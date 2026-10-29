#include <windows.h>
#include <QApplication>
#include <QBoxLayout>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QStatusBar>
#include <QTcpSocket>
#include <QTimer>
#include "localization.hpp"

namespace {
using warudo_i18n::Text;
using warudo_i18n::UiStrings;
constexpr auto RhyLiveType = "d2a5d113-91b9-4840-94d1-06326e7dd93e";
constexpr int FinishedLoadingEvent = 26;
constexpr int ExitEvent = 17;
constexpr qsizetype MaxMessage = 8 * 1024 * 1024;

using FrontendCallback = void (*)(int, void *);
using GetWindowFn = void *(*)();
using AddCallbackFn = void (*)(FrontendCallback, void *);
GetWindowFn getMainWindow = nullptr;
AddCallbackFn addCallback = nullptr;
AddCallbackFn removeCallback = nullptr;

QString obsLocale()
{
    using LocaleFn = const char *(*)();
    const auto module = GetModuleHandleW(L"obs.dll");
    const auto function = module ? reinterpret_cast<LocaleFn>(GetProcAddress(module, "obs_get_locale")) : nullptr;
    if (function) {
        const auto locale = function();
        if (locale && *locale) return QString::fromUtf8(locale);
    }
    return QLocale::system().name();
}

struct LocalConnection {
    quint16 httpPort = 19052;
    quint16 webSocketPort = 19053;
    QString tokenFile = qEnvironmentVariable("LOCALAPPDATA") +
                        "/../LocalLow/HakuyaLabs/Warudo/Player.log";
};

QString readLocalToken(const QString &path)
{
    QFile log(path);
    if (!log.open(QIODevice::ReadOnly))
        return {};
    const QString beginning = QString::fromUtf8(log.read(512 * 1024));
    const QRegularExpression rx("One Time API Auth Token: ([A-Fa-f0-9]{32})");
    auto matches = rx.globalMatch(beginning);
    QString result;
    while (matches.hasNext())
        result = matches.next().captured(1).toUpper();
    return result;
}

QList<QJsonObject> sceneAssets(const QJsonValue &value)
{
    QList<QJsonObject> result;
    if (value.isArray()) {
        for (const auto &entry : value.toArray())
            if (entry.isObject()) result.append(entry.toObject());
    } else if (value.isObject()) {
        const auto object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it) {
            if (!it.value().isObject()) continue;
            auto asset = it.value().toObject();
            if (!asset.contains("id")) asset["id"] = it.key();
            result.append(asset);
        }
    }
    return result;
}

QByteArray maskedFrame(const QByteArray &payload, unsigned char opcode = 1)
{
    QByteArray frame;
    frame.append(char(0x80 | opcode));
    const auto size = quint64(payload.size());
    if (size < 126) {
        frame.append(char(0x80 | size));
    } else if (size <= 65535) {
        frame.append('\xFE');
        frame.append(char(size >> 8));
        frame.append(char(size));
    } else {
        frame.append('\xFF');
        for (int i = 7; i >= 0; --i) frame.append(char(size >> (8 * i)));
    }
    const quint32 random = QRandomGenerator::system()->generate();
    char mask[4];
    for (int i = 0; i < 4; ++i) {
        mask[i] = char(random >> (8 * i));
        frame.append(mask[i]);
    }
    for (qsizetype i = 0; i < payload.size(); ++i)
        frame.append(char(payload[i] ^ mask[i % 4]));
    return frame;
}

class CalibrateControl final : public QObject {
public:
    explicit CalibrateControl(QMainWindow *window, const QString &locale = obsLocale(),
                              const LocalConnection &settings = {})
        : QObject(window), main(window), ui(locale), connection(settings)
    {
        network.setProxy(QNetworkProxy::NoProxy);
        deadline.setSingleShot(true);
        resetLabel.setSingleShot(true);
        connect(&deadline, &QTimer::timeout, this, [this] {
            finish(false, ui(Text::TimeoutTitle),
                   ui(Text::TimeoutDetail));
        });
        connect(&resetLabel, &QTimer::timeout, this, [this] {
            if (button) {
                button->setText(ui(Text::Button));
                button->setToolTip(ui(Text::ReadyTooltip));
            }
        });
    }

    ~CalibrateControl() override
    {
        disconnectTransport();
        delete button.data();
    }

    bool attach()
    {
        if (button) return true;
        if (!main) return false;
        auto *settings = main->findChild<QPushButton *>("settingsButton");
        if (!settings || !settings->parentWidget()) return false;
        auto *layout = qobject_cast<QBoxLayout *>(settings->parentWidget()->layout());
        if (!layout || layout->indexOf(settings) < 0) return false;
        button = new QPushButton(ui(Text::Button), settings->parentWidget());
        button->setObjectName("warudoCalibrateButton");
        button->setAccessibleName(ui(Text::AccessibleName));
        button->setSizePolicy(settings->sizePolicy());
        button->setMinimumSize(settings->minimumSize());
        button->setToolTip(ui(Text::ReadyTooltip));
        layout->insertWidget(layout->indexOf(settings) + 1, button);
        connect(button, &QPushButton::clicked, this, [this] { calibrate(); });
        return true;
    }

    void calibrate()
    {
        if (busy || !button) return;
        busy = true;
        succeeded = false;
        detail.clear();
        resetLabel.stop();
        button->setEnabled(false);
        button->setText(ui(Text::Working));
        token = readLocalToken(connection.tokenFile);
        QNetworkRequest request(QUrl(QString("http://localhost:%1/api/openedScene").arg(connection.httpPort)));
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::ManualRedirectPolicy);
        if (!token.isEmpty()) request.setRawHeader("X-Token", token.toUtf8());
        reply = network.get(request);
        deadline.start(6000);
        connect(reply, &QNetworkReply::finished, this, [this] { receivedScene(); });
    }

    bool isBusy() const { return busy; }
    bool lastSucceeded() const { return succeeded; }
    QString lastDetail() const { return detail; }

private:
    void receivedScene()
    {
        if (!busy || !reply) return;
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 401 || status == 403) {
            finish(false, ui(Text::AuthTitle),
                   ui(Text::AuthDetail));
            return;
        }
        if (reply->error() != QNetworkReply::NoError || status != 200) {
            finish(false, ui(Text::OfflineTitle),
                   ui(Text::OfflineDetail));
            return;
        }
        const auto raw = reply->read(MaxMessage + 1);
        QJsonParseError parseError;
        const auto scene = QJsonDocument::fromJson(raw, &parseError);
        if (raw.size() > MaxMessage || parseError.error != QJsonParseError::NoError || !scene.isObject()) {
            finish(false, ui(Text::SceneTitle),
                   ui(Text::SceneDetail));
            return;
        }
        QList<QJsonObject> receivers;
        for (const auto &asset : sceneAssets(scene.object().value("assets"))) {
            if (asset.value("typeId").toString() == QLatin1String(RhyLiveType) &&
                asset.value("enabled").toBool(true) && asset.value("active").toBool(true))
                receivers.append(asset);
        }
        if (receivers.isEmpty()) {
            finish(false, ui(Text::NoReceiverTitle),
                   ui(Text::NoReceiverDetail));
            return;
        }
        if (receivers.size() != 1 || receivers.first().value("id").toString().isEmpty()) {
            finish(false, ui(Text::MultipleTitle),
                   ui(Text::MultipleDetail));
            return;
        }
        const auto receiver = receivers.first();
        const auto triggers = receiver.value("triggers").toObject();
        const auto calibration = triggers.constFind("TriggerCalibrate");
        if (calibration == triggers.constEnd() || !calibration.value().isObject()) {
            finish(false, ui(Text::FailedTitle), ui(Text::MissingTriggerDetail));
            return;
        }
        assetId = receiver.value("id").toString();
        calibrationTrigger = calibration.key();
        reply->deleteLater();
        reply = nullptr;
        openWebSocket();
    }

    void openWebSocket()
    {
        input.clear();
        fragment.clear();
        handshaken = false;
        socket = new QTcpSocket(this);
        socket->setProxy(QNetworkProxy::NoProxy);
        connect(socket, &QTcpSocket::connected, this, [this] {
            QByteArray randomKey;
            for (int i = 0; i < 16; ++i)
                randomKey.append(char(QRandomGenerator::system()->generate()));
            const auto key = randomKey.toBase64();
            expectedAccept = QCryptographicHash::hash(
                key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11", QCryptographicHash::Sha1).toBase64();
            QByteArray headers = "GET / HTTP/1.1\r\nHost: localhost:" +
                QByteArray::number(connection.webSocketPort) + "\r\n" +
                "Upgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 13\r\n"
                "Sec-WebSocket-Key: " + key + "\r\n";
            if (!token.isEmpty()) headers += "Sec-WebSocket-Protocol: " + token.toUtf8() + "\r\n";
            socket->write(headers + "\r\n");
        });
        connect(socket, &QTcpSocket::readyRead, this, [this] { receiveWebSocket(); });
        connect(socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
            if (busy) finish(false, ui(Text::OfflineTitle),
                              ui(Text::ConnectDetail));
        });
        connect(socket, &QTcpSocket::disconnected, this, [this] {
            if (busy) finish(false, ui(Text::ClosedTitle),
                              ui(Text::ClosedDetail));
        });
        socket->connectToHost("localhost", connection.webSocketPort);
    }

    void receiveWebSocket()
    {
        if (!socket || !busy) return;
        input += socket->readAll();
        if (input.size() > MaxMessage) {
            finish(false, ui(Text::SizeTitle), ui(Text::SizeDetail));
            return;
        }
        if (!handshaken) {
            const auto end = input.indexOf("\r\n\r\n");
            if (end < 0) return;
            const auto headers = input.left(end).split('\n');
            QByteArray accepted;
            for (auto header : headers) {
                const auto colon = header.indexOf(':');
                if (colon > 0 && header.left(colon).trimmed().toLower() == "sec-websocket-accept")
                    accepted = header.mid(colon + 1).trimmed();
            }
            if (!headers.first().startsWith("HTTP/1.1 101 ") || accepted != expectedAccept) {
                finish(false, ui(Text::AuthTitle), ui(Text::ControlAuthDetail));
                return;
            }
            input.remove(0, end + 4);
            handshaken = true;
            const QJsonObject command{{"action", "invokeEntityTriggerPort"},
                {"data", QJsonObject{{"id", assetId}, {"port", calibrationTrigger}}}};
            socket->write(maskedFrame(QJsonDocument(command).toJson(QJsonDocument::Compact)));
        }
        while (busy && socket && input.size() >= 2) {
            const auto first = static_cast<unsigned char>(input[0]);
            const auto second = static_cast<unsigned char>(input[1]);
            const auto opcode = first & 15;
            quint64 length = second & 127;
            qsizetype offset = 2;
            if (length == 126) {
                if (input.size() < 4) return;
                length = (quint64(static_cast<unsigned char>(input[2])) << 8) |
                          quint64(static_cast<unsigned char>(input[3]));
                offset = 4;
            } else if (length == 127) {
                if (input.size() < 10) return;
                length = 0;
                for (int i = 2; i < 10; ++i)
                    length = (length << 8) | static_cast<unsigned char>(input[i]);
                offset = 10;
            }
            if ((second & 128) || length > quint64(MaxMessage) || (first & 0x70)) {
                finish(false, ui(Text::ProtocolTitle), ui(Text::ProtocolDetail));
                return;
            }
            if (quint64(input.size() - offset) < length) return;
            const auto payload = input.mid(offset, qsizetype(length));
            input.remove(0, offset + qsizetype(length));
            if (opcode == 8) {
                finish(false, ui(Text::ClosedTitle), ui(Text::RemoteClosedDetail));
                return;
            }
            if (opcode == 9) { socket->write(maskedFrame(payload, 10)); continue; }
            if (opcode == 10) continue;
            if (opcode == 1 || opcode == 0) {
                fragment += payload;
                if (fragment.size() > MaxMessage) {
                    finish(false, ui(Text::SizeTitle), ui(Text::SizeDetail));
                    return;
                }
                if (!(first & 128)) continue;
                const auto message = QJsonDocument::fromJson(fragment).object();
                fragment.clear();
                if (message.value("action").toString() != "invokeEntityTriggerPort") continue;
                if (message.value("error").isString() && !message.value("error").toString().isEmpty()) {
                    finish(false, ui(Text::FailedTitle), ui(Text::FailedDetail));
                } else if (message.value("data").toBool(false)) {
                    finish(true, ui(Text::SuccessTitle), ui(Text::SuccessDetail));
                } else {
                    finish(false, ui(Text::UnconfirmedTitle), ui(Text::UnconfirmedDetail));
                }
            }
        }
    }

    void disconnectTransport()
    {
        deadline.stop();
        if (reply) {
            reply->disconnect(this);
            reply->abort();
            reply->deleteLater();
            reply = nullptr;
        }
        if (socket) {
            socket->disconnect(this);
            socket->abort();
            socket->deleteLater();
            socket = nullptr;
        }
        token.clear();
    }

    void finish(bool ok, const QString &label, const QString &message)
    {
        busy = false;
        succeeded = ok;
        detail = message;
        disconnectTransport();
        if (button) {
            button->setEnabled(true);
            button->setText(label);
            button->setToolTip(message);
            resetLabel.start(ok ? 1500 : 2500);
        }
        if (main && main->statusBar()) main->statusBar()->showMessage(message, 6000);
    }

    QPointer<QMainWindow> main;
    QPointer<QPushButton> button;
    UiStrings ui;
    LocalConnection connection;
    QNetworkAccessManager network;
    QPointer<QNetworkReply> reply;
    QPointer<QTcpSocket> socket;
    QTimer deadline;
    QTimer resetLabel;
    QString token;
    QString assetId;
    QString calibrationTrigger;
    QString detail;
    QByteArray input;
    QByteArray fragment;
    QByteArray expectedAccept;
    bool busy = false;
    bool succeeded = false;
    bool handshaken = false;
};

QPointer<CalibrateControl> control;

void installControl()
{
    if (!getMainWindow) return;
    auto *window = static_cast<QMainWindow *>(getMainWindow());
    if (!window) return;
    if (!control) control = new CalibrateControl(window);
    control->attach();
}

void frontendEvent(int event, void *)
{
    if (event == FinishedLoadingEvent) installControl();
    if (event == ExitEvent) { delete control.data(); control = nullptr; }
}
}

#ifndef WARUDO_CALIBRATE_TEST
extern "C" {
__declspec(dllexport) void obs_module_set_pointer(void *) {}
__declspec(dllexport) unsigned long obs_module_ver()
{
    using VersionFn = unsigned long (*)();
    auto fn = reinterpret_cast<VersionFn>(GetProcAddress(GetModuleHandleW(L"obs.dll"), "obs_get_version"));
    return fn ? fn() : (33UL << 24);
}
__declspec(dllexport) const char *obs_module_name() { return "Warudo RhyLive Calibrate"; }
__declspec(dllexport) const char *obs_module_description() { return "Adds RhyLive calibration below Settings in OBS Controls."; }
__declspec(dllexport) const char *obs_module_author() { return "Warudo Calibrate contributors"; }
__declspec(dllexport) bool obs_module_load()
{
    const auto frontend = GetModuleHandleW(L"obs-frontend-api.dll");
    if (!frontend) return false;
    getMainWindow = reinterpret_cast<GetWindowFn>(GetProcAddress(frontend, "obs_frontend_get_main_window"));
    addCallback = reinterpret_cast<AddCallbackFn>(GetProcAddress(frontend, "obs_frontend_add_event_callback"));
    removeCallback = reinterpret_cast<AddCallbackFn>(GetProcAddress(frontend, "obs_frontend_remove_event_callback"));
    if (!getMainWindow || !addCallback || !removeCallback) return false;
    addCallback(frontendEvent, nullptr);
    return true;
}
__declspec(dllexport) void obs_module_post_load()
{
    if (qApp) QTimer::singleShot(0, qApp, [] { installControl(); });
}
__declspec(dllexport) void obs_module_unload()
{
    if (removeCallback) removeCallback(frontendEvent, nullptr);
    delete control.data();
    control = nullptr;
}
}
#endif
