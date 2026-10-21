// system headers
#include <iostream>
#include <memory>

// library headers
#include <QDebug>
#include <QDir>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QString>

// local headers
#include <shared.h>
#include "translationmanager.h"

TranslationManager::TranslationManager(QCoreApplication& app) : app(app) {
    qtTranslator.load("qt_" + QLocale::system().name(), QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    app.installTranslator(&qtTranslator);

    std::unique_ptr<QSettings> settings(getConfig());
    setLanguage(settings->value("AppImageController/language", "system").toString());
}

TranslationManager::~TranslationManager() {
    app.removeTranslator(&appTranslator);
    app.removeTranslator(&qtTranslator);
}

bool TranslationManager::setLanguage(const QString& languageCode) {
    app.removeTranslator(&appTranslator);

    const auto useSystemDefault = languageCode.isEmpty() || languageCode == "system";
    auto localeName = languageCode;
    if (useSystemDefault)
        localeName = QLocale::system().name();
    else if (localeName == "pt")
        localeName = "pt_PT";

    const auto translationDir = getTranslationDir();
    const auto exactPath = translationDir + "/ui." + localeName + ".qm";
    auto loaded = appTranslator.load(exactPath);

    if (!loaded && localeName.contains('_'))
        loaded = appTranslator.load(translationDir + "/ui." + localeName.section('_', 0, 0) + ".qm");
    if (!loaded && useSystemDefault)
        loaded = appTranslator.load(translationDir + "/ui.en.qm");

    if (!loaded) {
        std::cerr << "[AppImageController] Warning: translation could not be loaded for "
                  << localeName.toStdString() << std::endl;
        return false;
    }

    app.installTranslator(&appTranslator);
    return true;
}

QStringList TranslationManager::availableLanguages() {
    const QDir translationDir(getTranslationDir());
    QStringList languages;
    for (const auto& filename : translationDir.entryList({"ui.*.qm"}, QDir::Files, QDir::Name)) {
        auto locale = filename.mid(3, filename.size() - 6);
        if (locale != "en" && locale != "pt")
            languages.append(locale);
    }
    languages.prepend("en");
    return languages;
}

QString TranslationManager::getTranslationDir() {
    const auto binaryDirPath = QApplication::applicationDirPath();
    auto translationDir = binaryDirPath + "/../../i18n/generated/l10n";

    if (!QDir(translationDir).exists()) {
        const auto privateDataDir = pathToPrivateDataDirectory();
        if (!privateDataDir.isEmpty())
            translationDir = privateDataDir + "/l10n";
    }

    if (!QDir(translationDir).exists()) {
        std::cerr << "[AppImageController] Warning: "
                  << "Translation directory could not be found, translations are likely not available" << std::endl;
    }

    return translationDir;
}
