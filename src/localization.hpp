#pragma once
#include <QLocale>
#include <QString>
#include "generated_locales.hpp"

namespace warudo_i18n {
inline const Locale &selectLocale(QString name)
{
    name.replace('_', '-');
    name = name.toLower();
    if (name == "zh" || name.startsWith("zh-")) {
        const bool traditional = name.contains("hant") || name.startsWith("zh-tw") ||
                                 name.startsWith("zh-hk") || name.startsWith("zh-mo");
        name = traditional ? "zh-tw" : "zh-cn";
    }
    for (const auto &locale : Locales)
        if (name == QString::fromLatin1(locale.name).toLower()) return locale;
    const auto language = name.section('-', 0, 0);
    for (const auto &locale : Locales)
        if (language == QString::fromLatin1(locale.name).section('-', 0, 0).toLower())
            return locale;
    return Locales.front();
}

class UiStrings {
public:
    explicit UiStrings(const QString &locale) : selected(selectLocale(locale)) {}
    QString operator()(Text key) const
    {
        return QString::fromUtf8(selected.values[static_cast<std::size_t>(key)]);
    }
    const char *localeName() const { return selected.name; }

private:
    const Locale &selected;
};
}
