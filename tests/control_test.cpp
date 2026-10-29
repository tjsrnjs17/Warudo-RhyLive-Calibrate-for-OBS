#define WARUDO_CALIBRATE_TEST
#include "../src/warudo-calibrate.cpp"
#include <QEventLoop>
#include <QTextStream>
#include <cstdio>

namespace {
void output(const QJsonObject &result)
{
    const auto bytes = QJsonDocument(result).toJson(QJsonDocument::Compact);
    std::fwrite(bytes.constData(), 1, std::size_t(bytes.size()), stdout);
    std::fwrite("\n", 1, 1, stdout);
}

QPushButton *makeControls(QMainWindow &window)
{
    auto *panel = new QWidget(&window);
    auto *layout = new QVBoxLayout(panel);
    layout->addWidget(new QPushButton("Studio Mode", panel));
    auto *settings = new QPushButton("Settings", panel);
    settings->setObjectName("settingsButton");
    settings->setMinimumWidth(150);
    layout->addWidget(settings);
    layout->addStretch();
    window.setCentralWidget(panel);
    return settings;
}

int localizationTest()
{
    struct Case { const char *requested; const char *resolved; const char *label; };
    const Case cases[] = {
        {"en-US", "en-US", "Recalibrate"},
        {"ko_KR", "ko-KR", "캘리"},
        {"ja-JP", "ja-JP", "再調整"},
        {"zh-CN", "zh-CN", "重新校准"},
        {"zh-TW", "zh-TW", "重新校正"},
        {"de-DE", "de-DE", "Neu kalibrieren"},
        {"fr-FR", "fr-FR", "Recalibrer"},
        {"es-ES", "es-ES", "Recalibrar"},
        {"pt-BR", "pt-BR", "Recalibrar"},
        {"zh-Hant-HK", "zh-TW", "重新校正"},
        {"zh-Hans-SG", "zh-CN", "重新校准"},
        {"pt-PT", "pt-BR", "Recalibrar"},
        {"en-GB", "en-US", "Recalibrate"},
        {"ru-RU", "en-US", "Recalibrate"},
        {"", "en-US", "Recalibrate"},
    };
    for (const auto &test : cases) {
        QMainWindow window;
        auto *settings = makeControls(window);
        CalibrateControl controller(&window, QString::fromUtf8(test.requested));
        if (!controller.attach() || !controller.attach()) return 1;
        const auto buttons = window.findChildren<QPushButton *>("warudoCalibrateButton");
        if (buttons.size() != 1) return 2;
        const auto *button = buttons.first();
        const auto *layout = settings->parentWidget()->layout();
        if (layout->indexOf(button) != layout->indexOf(settings) + 1) return 3;
        if (button->text() != QString::fromUtf8(test.label)) return 4;
        if (button->toolTip().isEmpty() || button->accessibleName().isEmpty()) return 5;
        UiStrings ui(QString::fromUtf8(test.requested));
        if (QByteArray(ui.localeName()) != test.resolved) return 6;
        for (std::size_t i = 0; i < std::size_t(Text::Count); ++i)
            if (ui(static_cast<Text>(i)).isEmpty()) return 7;
    }
    output({{"test", "localization-and-placement"}, {"passed", 15}, {"locales", 9}});
    return 0;
}

Text expectedTitle(const QString &scenario)
{
    if (scenario == "auth") return Text::AuthTitle;
    if (scenario == "missing") return Text::NoReceiverTitle;
    if (scenario == "multiple") return Text::MultipleTitle;
    if (scenario == "malformed") return Text::SceneTitle;
    if (scenario == "handshake") return Text::AuthTitle;
    if (scenario == "trigger-error") return Text::FailedTitle;
    if (scenario == "missing-trigger" || scenario == "legacy-trigger") return Text::FailedTitle;
    if (scenario == "unconfirmed") return Text::UnconfirmedTitle;
    if (scenario == "timeout") return Text::TimeoutTitle;
    if (scenario == "offline") return Text::OfflineTitle;
    return Text::SuccessTitle;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.size() == 2 && args[1] == "localization") return localizationTest();
    if (args.size() != 7) return 20;
    const auto scenario = args[1];
    const auto locale = args[2];
    LocalConnection connection;
    connection.httpPort = quint16(args[3].toUInt());
    connection.webSocketPort = quint16(args[4].toUInt());
    connection.tokenFile = args[5];
    const bool expectedSuccess = args[6] == "true";
    QMainWindow window;
    auto *settings = makeControls(window);
    CalibrateControl controller(&window, locale, connection);
    if (!controller.attach()) return 21;
    auto *button = window.findChild<QPushButton *>("warudoCalibrateButton");
    button->click();
    controller.calibrate();
    if (!controller.isBusy() || button->isEnabled()) return 22;
    QEventLoop loop;
    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
        if (!controller.isBusy()) loop.quit();
    });
    QTimer::singleShot(9000, &loop, &QEventLoop::quit);
    poll.start(5);
    loop.exec();
    UiStrings ui(locale);
    const bool ok = !controller.isBusy() && controller.lastSucceeded() == expectedSuccess &&
                    button->isEnabled() && button->text() == ui(expectedTitle(scenario)) &&
                    !controller.lastDetail().isEmpty() &&
                    settings->parentWidget()->layout()->indexOf(button) ==
                        settings->parentWidget()->layout()->indexOf(settings) + 1;
    output({{"scenario", scenario}, {"ok", ok}, {"locale", ui.localeName()},
            {"success", controller.lastSucceeded()}, {"label", button->text()}});
    return ok ? 0 : 23;
}
