#pragma once

// library includes
#include <QApplication>
#include <QTranslator>
#include <QStringList>

/*
 * Installs translations for AppImage Controller UIs in a Qt application.
 *
 * You need to keep instances of this alive until the application is terminated.
 */
class TranslationManager {
private:
    const QCoreApplication& app;
    QTranslator qtTranslator;
    QTranslator appTranslator;

public:
    explicit TranslationManager(QCoreApplication& app);
    ~TranslationManager();

    bool setLanguage(const QString& languageCode);
    static QStringList availableLanguages();
    static QString getTranslationDir();
};
