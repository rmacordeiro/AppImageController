// libraries
#include <QApplication>

// local
#include <translationmanager.h>
#include <shared.h>
#include "settings_dialog.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationDisplayName("AppImage Controller Settings");
    QApplication::setApplicationVersion(APPIMAGELAUNCHER_VERSION);
    QApplication::setWindowIcon(QIcon(":/AppImageController.svg"));

    TranslationManager mgr(app);
//
//    // we ship some very basic fallbacks for icons used in the settings dialog
//    // this should fix missing icons on some distros

    SettingsDialog dialog(mgr);

    setUpFallbackIconPaths(&dialog);

    dialog.show();

    return app.exec();
}
