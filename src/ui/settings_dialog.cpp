// libraries
#include <QDebug>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QDirIterator>
#include <QFileInfo>
#include <QMessageBox>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QProcess>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QHeaderView>
#include <QApplication>
#include <QIcon>
#include <QComboBox>
#include <QMap>
#include <QTableWidgetItem>

extern "C" {
#include <appimage/appimage.h>
}

// local
#include "settings_dialog.h"
#include "ui_settings_dialog.h"
#include "shared.h"
#include <translationmanager.h>

SettingsDialog::SettingsDialog(TranslationManager& translationManager, QWidget* parent)
    : QDialog(parent), ui(new Ui::SettingsDialog), settingsFile(getConfig(this)),
      translationManager(translationManager) {
    ui->setupUi(this);
    // QDialog omits the minimize and maximize buttons unless requested explicitly
    setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowMinMaxButtonsHint |
                   Qt::WindowCloseButtonHint);
    setSizeGripEnabled(true);
    ui->additionalDirsAddButton->setIcon(loadIconWithFallback("add"));
    ui->additionalDirsRemoveButton->setIcon(loadIconWithFallback("remove"));

    ui->languageComboBox->addItem(tr("System default"), "system");
    static const QMap<QString, QString> languageNames{
        {"en", "English"},
        {"ast", "Asturianu"},
        {"cs", "Čeština"},
        {"de", "Deutsch"},
        {"es", "Español De España"},
        {"fr", "Français"},
        {"it", "Italiano"},
        {"nb_NO", "Norsk Bokmål"},
        {"nl", "Nederlands"},
        {"pl", "Polski"},
        {"pt_BR", "Português Do Brasil"},
        {"pt_PT", "Português Europeu"},
        {"ru", "Русский"},
        {"tr", "Türkçe"},
        {"zh_Hans", "简体中文"},
    };
    for (const auto& languageCode : TranslationManager::availableLanguages()) {
        const auto languageName = languageNames.value(languageCode, languageCode);
        ui->languageComboBox->addItem(QString("%1 (%2)").arg(languageName, languageCode), languageCode);
    }
    auto selectedLanguage = settingsFile->value("AppImageController/language", "system").toString();
    if (selectedLanguage == "pt") {
        selectedLanguage = "pt_PT";
        settingsFile->setValue("AppImageController/language", selectedLanguage);
        settingsFile->sync();
        if (settingsFile->status() != QSettings::NoError)
            qWarning() << "Failed to migrate Portuguese language preference";
    }
    const auto languageIndex = ui->languageComboBox->findData(selectedLanguage);
    ui->languageComboBox->setCurrentIndex(languageIndex < 0 ? 0 : languageIndex);

    setWindowTitle(QStringLiteral("AppImage Controller Settings"));
    // logo and version live in the tab bar's free top-right corner so no vertical space is wasted
    auto* cornerWidget = new QWidget(ui->tabWidget);
    auto* cornerLayout = new QHBoxLayout(cornerWidget);
    cornerLayout->setContentsMargins(0, 0, 0, 0);
    logoLabel = new QLabel(cornerWidget);
    versionLabel = new QLabel(cornerWidget);
    cornerLayout->addWidget(versionLabel);
    cornerLayout->addWidget(logoLabel);
    ui->tabWidget->setCornerWidget(cornerWidget, Qt::TopRightCorner);
    versionLabel->setText(tr("Version %1").arg(APPIMAGELAUNCHER_VERSION));
    setWindowIcon(QIcon(":/AppImageController.svg"));
    logoLabel->setPixmap(QIcon(":/AppImageController.svg").pixmap(32, 32));
    ui->applicationsDirLineEdit->setPlaceholderText(integratedAppImagesDestination().absolutePath());
    ui->detectedAppImagesTableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->detectedAppImagesTableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->detectedAppImagesTableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->detectedAppImagesTableWidget->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    if (settingsFile->contains("AppImageLauncher/SettingsDialog/geometry"))
        restoreGeometry(settingsFile->value("AppImageLauncher/SettingsDialog/geometry").toByteArray());

    loadSettings();
    refreshDetectedAppImages();

// cosmetic changes in lite mode
#ifdef BUILD_LITE
    ui->daemonIsEnabledCheckBox->setChecked(true);
    ui->daemonIsEnabledCheckBox->setEnabled(false);

    ui->askMoveCheckBox->setChecked(false);
    ui->askMoveCheckBox->setEnabled(false);
#endif

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::onDialogAccepted);
    connect(ui->chooseAppsDirToolButton, &QToolButton::released, this, &SettingsDialog::onChooseAppsDirClicked);
    connect(ui->additionalDirsAddButton, &QToolButton::released, this, &SettingsDialog::onAddDirectoryToWatchButtonClicked);
    connect(ui->additionalDirsRemoveButton, &QToolButton::released, this, &SettingsDialog::onRemoveDirectoryToWatchButtonClicked);
    connect(ui->additionalDirsListWidget, &QListWidget::itemActivated, this, &SettingsDialog::onDirectoryToWatchItemActivated);
    connect(ui->additionalDirsListWidget, &QListWidget::itemClicked, this, &SettingsDialog::onDirectoryToWatchItemActivated);
    connect(ui->detectedAppImagesTableWidget, &QTableWidget::itemSelectionChanged,
            this, &SettingsDialog::onDetectedAppImageSelectionChanged);
    connect(ui->detectedAppImagesTableWidget, &QTableWidget::itemChanged,
            this, &SettingsDialog::onDetectedAppImageItemChanged);
    connect(ui->integrateDetectedAppImageButton, &QPushButton::clicked,
            this, &SettingsDialog::integrateSelectedAppImage);
    connect(ui->declineDetectedAppImageButton, &QPushButton::clicked,
            this, &SettingsDialog::declineSelectedAppImage);
    connect(ui->clearAppImageDecisionButton, &QPushButton::clicked,
            this, &SettingsDialog::clearSelectedAppImageDecision);
    connect(ui->launchDetectedAppImageButton, &QPushButton::clicked,
            this, &SettingsDialog::launchSelectedAppImage);
    connect(ui->deleteDetectedAppImageButton, &QPushButton::clicked,
            this, &SettingsDialog::deleteSelectedAppImage);
    connect(ui->refreshDetectedAppImagesButton, &QPushButton::clicked,
            this, [this]() { refreshDetectedAppImages(); });
    connect(ui->languageComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::onLanguageChanged);

    updateAvailableFeatures();
    ui->tabWidget->setCurrentWidget(ui->launcherTab);
}

void SettingsDialog::updateAvailableFeatures() {
    QStringList availableFeatures;
#ifdef ENABLE_UPDATE_HELPER
    availableFeatures << "<span style='color: green;'>✔</span> " + tr("updater available for AppImages supporting AppImageUpdate");
#else
    availableFeatures << "<span style='color: red;'>🞬</span> " + tr("updater unavailable");
#endif

#ifdef BUILD_LITE
    availableFeatures << "<br /><br />"
                      << tr("<strong>Note: this is an AppImage Controller Lite build, only supports a limited set of features</strong><br />"
                            "Please install the full version via the provided native packages to enjoy the full AppImage Controller experience");
#endif
    ui->featuresLabel->setText(availableFeatures.join('\n'));
}

void SettingsDialog::onLanguageChanged(int index) {
    if (index < 0)
        return;

    const auto languageCode = ui->languageComboBox->itemData(index).toString();
    if (!translationManager.setLanguage(languageCode)) {
        QMessageBox::warning(this, tr("Language"), tr("Could not load the selected language."));
        const auto savedLanguage = settingsFile->value("AppImageController/language", "system").toString();
        const auto savedIndex = ui->languageComboBox->findData(savedLanguage);
        if (savedIndex >= 0) {
            const QSignalBlocker blocker(ui->languageComboBox);
            ui->languageComboBox->setCurrentIndex(savedIndex);
        }
        return;
    }

    settingsFile->setValue("AppImageController/language", languageCode);
    settingsFile->sync();
    if (settingsFile->status() != QSettings::NoError)
        qWarning() << "Failed to save language preference";

    ui->retranslateUi(this);
    ui->languageComboBox->setItemText(0, tr("System default"));
    QApplication::setApplicationDisplayName(QStringLiteral("AppImage Controller Settings"));
    setWindowTitle(QStringLiteral("AppImage Controller Settings"));
    versionLabel->setText(tr("Version %1").arg(APPIMAGELAUNCHER_VERSION));
    updateAvailableFeatures();
    refreshDetectedAppImages(false);
}

SettingsDialog::~SettingsDialog() {
    settingsFile->setValue("AppImageLauncher/SettingsDialog/geometry", saveGeometry());
    settingsFile->sync();
    if (settingsFile->status() != QSettings::NoError)
        qWarning() << "Failed to save Settings window geometry";
    delete ui;
}

void SettingsDialog::addDirectoryToWatchToListView(const QString& dirPath) {
    // empty paths are not permitted
    if (dirPath.isEmpty())
        return;

    const QDir dir(dirPath);

    // we don't want to redundantly add the main integration directory
    if (dir == integratedAppImagesDestination())
        return;

    QIcon icon;

    auto findIcon = [](const std::initializer_list<QString>& names) {
        for (const auto& i : names) {
            auto icon = QIcon::fromTheme(i, loadIconWithFallback(i));

            if (!icon.isNull())
                return icon;
        }

        return QIcon{};
    };

    if (dir.exists()) {
        icon = findIcon({"folder"});
    } else {
        // TODO: search for more meaningful icon, "remove" doesn't really show the directory is missing
        icon = findIcon({"remove"});
    }

    if (icon.isNull()) {
        qDebug() << "item icon unavailable, using fallback";
    }

    auto* item = new QListWidgetItem(icon, dirPath);
    ui->additionalDirsListWidget->addItem(item);
}

void SettingsDialog::loadSettings() {
    const auto daemonIsEnabled = settingsFile->value("AppImageLauncher/enable_daemon", "true").toBool();
    const auto askMoveChecked = settingsFile->value("AppImageLauncher/ask_to_move", "true").toBool();

    if (settingsFile) {
        ui->daemonIsEnabledCheckBox->setChecked(daemonIsEnabled);
        ui->askMoveCheckBox->setChecked(askMoveChecked);
        ui->applicationsDirLineEdit->setText(settingsFile->value("AppImageLauncher/destination").toString());

        const auto additionalDirsPath = settingsFile->value("appimagelauncherd/additional_directories_to_watch", "").toString();
        for (const auto& dirPath : additionalDirsPath.split(":")) {
            addDirectoryToWatchToListView(dirPath);
        }
    }
}

void SettingsDialog::onDialogAccepted() {
    saveSettings();
    toggleDaemon();
}

void SettingsDialog::saveSettings() {
    QStringList additionalDirsToWatch;

    {
        QListWidgetItem* currentItem;

        for (int i = 0; (currentItem = ui->additionalDirsListWidget->item(i)) != nullptr; ++i) {
            additionalDirsToWatch << currentItem->text();
        }
    }

    // temporary workaround to fill in the monitorMountedFilesystems with the same value it had in the old settings
    // this is supposed to support the option while hiding it in the settings
    int monitorMountedFilesystems = -1;
    {
        const auto oldSettings = getConfig();

        static constexpr auto oldKey = "appimagelauncherd/monitor_mounted_filesystems";

        // getConfig might return a null pointer if the config file doesn't exist
        // we have to handle this, obviously
        if (oldSettings != nullptr && oldSettings->contains(oldKey)) {
            const auto oldValue = oldSettings->value(oldKey).toBool();
            monitorMountedFilesystems = oldValue ? 1 : 0;
        }
    }

    createConfigFile(ui->askMoveCheckBox->isChecked(),
                     ui->applicationsDirLineEdit->text(),
                     ui->daemonIsEnabledCheckBox->isChecked(),
                     additionalDirsToWatch,
                     monitorMountedFilesystems);

    // reload settings
    loadSettings();
}

void SettingsDialog::toggleDaemon() {
    // assumes defaults if config doesn't exist or lacks the related key(s)
    if (settingsFile) {
        if (settingsFile->value("AppImageLauncher/enable_daemon", "true").toBool()) {
            system("systemctl --user enable  appimagelauncherd.service");
            // we want to actually restart the service to apply the new configuration
            system("systemctl --user restart appimagelauncherd.service");
        } else {
            system("systemctl --user disable appimagelauncherd.service");
            system("systemctl --user stop    appimagelauncherd.service");
        }
    }
}

void SettingsDialog::onChooseAppsDirClicked() {
    QFileDialog fileDialog(this);

    fileDialog.setFileMode(QFileDialog::DirectoryOnly);
    fileDialog.setWindowTitle(tr("Select Applications directory"));
    fileDialog.setDirectory(integratedAppImagesDestination().absolutePath());

    // Gtk+ >= 3 segfaults when trying to use the native dialog, therefore we need to enforce the Qt one
    // See #218 for more information
    fileDialog.setOption(QFileDialog::DontUseNativeDialog, true);

    if (fileDialog.exec()) {
        QString dirPath = fileDialog.selectedFiles().first();
        ui->applicationsDirLineEdit->setText(dirPath);
    }
}

void SettingsDialog::onAddDirectoryToWatchButtonClicked() {
    QFileDialog fileDialog(this);

    fileDialog.setFileMode(QFileDialog::DirectoryOnly);
    fileDialog.setWindowTitle(tr("Select additional directory to watch"));
    fileDialog.setDirectory(QStandardPaths::locate(QStandardPaths::HomeLocation, ".", QStandardPaths::LocateDirectory));

    // Gtk+ >= 3 segfaults when trying to use the native dialog, therefore we need to enforce the Qt one
    // See #218 for more information
    fileDialog.setOption(QFileDialog::DontUseNativeDialog, true);

    if (fileDialog.exec()) {
        QString dirPath = fileDialog.selectedFiles().first();
        addDirectoryToWatchToListView(dirPath);
    }
}

void SettingsDialog::onRemoveDirectoryToWatchButtonClicked() {
    auto* widget = ui->additionalDirsListWidget;

    auto* currentItem = widget->currentItem();

    if (currentItem == nullptr)
        return;

    const auto index = widget->row(currentItem);

    // after taking it, we have to delete it ourselves, Qt docs say
    auto deletedItem = widget->takeItem(index);
    delete deletedItem;

    // we should deactivate the remove button once the last item is gone
    if (widget->item(0) == nullptr) {
        ui->additionalDirsRemoveButton->setEnabled(false);
    }
}

void SettingsDialog::onDirectoryToWatchItemActivated(QListWidgetItem* item) {
    // we activate the button based on whether there's an item selected
    ui->additionalDirsRemoveButton->setEnabled(item != nullptr);
}

void SettingsDialog::refreshDetectedAppImages(bool scanWatchedDirectories) {
    if (scanWatchedDirectories) {
        for (const auto& directory : daemonDirectoriesToWatch(settingsFile)) {
            QDirIterator iterator(directory.absolutePath(), QDir::Files | QDir::NoDotAndDotDot,
                                  QDirIterator::NoIteratorFlags);
            while (iterator.hasNext()) {
                const auto path = iterator.next();
                const QFileInfo fileInfo(path);
                if (fileInfo.isSymLink() || !fileInfo.isFile() || !isAppImage(path))
                    continue;

                if (!recordDetectedAppImage(fileInfo.absoluteFilePath()))
                    qWarning() << "Failed to record detected AppImage:" << fileInfo.absoluteFilePath();
            }
        }
    }

    const auto selectedRow = ui->detectedAppImagesTableWidget->currentRow();
    const auto selectedPath = selectedRow < 0
                                  ? QString{}
                                  : ui->detectedAppImagesTableWidget->item(selectedRow, 0)->data(Qt::UserRole).toString();
    const QSignalBlocker tableUpdates(ui->detectedAppImagesTableWidget);
    ui->detectedAppImagesTableWidget->setRowCount(0);

    for (const auto& appImage : detectedAppImages()) {
        QString actionText;
        switch (appImage.decision) {
            case AppImageIntegrationDecision::Integrate:
                actionText = tr("Integrate");
                break;
            case AppImageIntegrationDecision::DoNotIntegrate:
                actionText = tr("Do not integrate");
                break;
            case AppImageIntegrationDecision::Undecided:
                actionText = tr("New - decision needed");
                break;
        }

        const auto row = ui->detectedAppImagesTableWidget->rowCount();
        ui->detectedAppImagesTableWidget->insertRow(row);
        auto* nameItem = new QTableWidgetItem(appImage.name);
        nameItem->setData(Qt::UserRole, appImage.path);
        nameItem->setData(Qt::UserRole + 1, static_cast<int>(appImage.decision));
        nameItem->setToolTip(appImage.path);
        auto* actionItem = new QTableWidgetItem(actionText);
        auto* locationItem = new QTableWidgetItem(appImage.path);
        locationItem->setToolTip(appImage.path);
        auto* argumentsItem = new QTableWidgetItem(appImage.launchArguments);
        argumentsItem->setToolTip(tr("Optional arguments passed to the AppImage when launched from its desktop entry."));
        argumentsItem->setFlags(argumentsItem->flags() | Qt::ItemIsEditable);
        for (auto* readOnlyItem : {nameItem, actionItem, locationItem})
            readOnlyItem->setFlags(readOnlyItem->flags() & ~Qt::ItemIsEditable);
        if (appImage.decision == AppImageIntegrationDecision::Undecided) {
            auto font = nameItem->font();
            font.setBold(true);
            nameItem->setFont(font);
            actionItem->setFont(font);
        }
        ui->detectedAppImagesTableWidget->setItem(row, 0, nameItem);
        ui->detectedAppImagesTableWidget->setItem(row, 1, actionItem);
        ui->detectedAppImagesTableWidget->setItem(row, 2, locationItem);
        ui->detectedAppImagesTableWidget->setItem(row, 3, argumentsItem);

        if (appImage.path == selectedPath) {
            ui->detectedAppImagesTableWidget->setCurrentCell(row, 0);
            ui->detectedAppImagesTableWidget->selectRow(row);
        }
    }

    updateDetectedAppImageActions();
}

void SettingsDialog::updateDetectedAppImageActions() {
    const auto row = ui->detectedAppImagesTableWidget->currentRow();
    const auto hasSelection = row >= 0;
    const auto decision = hasSelection
                              ? static_cast<AppImageIntegrationDecision>(
                                    ui->detectedAppImagesTableWidget->item(row, 0)->data(Qt::UserRole + 1).toInt())
                              : AppImageIntegrationDecision::Undecided;
    ui->integrateDetectedAppImageButton->setEnabled(
        hasSelection && decision != AppImageIntegrationDecision::Integrate);
    ui->declineDetectedAppImageButton->setEnabled(
        hasSelection && decision != AppImageIntegrationDecision::DoNotIntegrate);
    ui->clearAppImageDecisionButton->setEnabled(
        hasSelection && decision != AppImageIntegrationDecision::Undecided);
    ui->launchDetectedAppImageButton->setEnabled(hasSelection);
    ui->deleteDetectedAppImageButton->setEnabled(hasSelection);
}

void SettingsDialog::onDetectedAppImageSelectionChanged() {
    updateDetectedAppImageActions();
}

void SettingsDialog::onDetectedAppImageItemChanged(QTableWidgetItem* item) {
    if (item == nullptr || item->column() != 3)
        return;

    const auto row = item->row();
    auto* nameItem = ui->detectedAppImagesTableWidget->item(row, 0);
    if (nameItem == nullptr)
        return;

    const auto path = nameItem->data(Qt::UserRole).toString();
    const auto oldArguments = appImageLaunchArguments(path);
    if (!setAppImageLaunchArguments(path, item->text())) {
        QMessageBox::warning(this, tr("Launch arguments"),
                             tr("Could not save launch arguments. Ensure arguments are valid and separated with spaces."));
        const QSignalBlocker tableUpdates(ui->detectedAppImagesTableWidget);
        item->setText(oldArguments);
        return;
    }

    if (!hasAlreadyBeenIntegrated(path)) {
        if (hasExistingDesktopEntry(path)) {
            QMessageBox::warning(this, tr("Launch arguments"),
                                 tr("A desktop entry for this AppImage already exists and was not created by AppImage "
                                    "Controller, so it will not be modified. The arguments are saved and are used "
                                    "when launching from this window."));
        }
        return;
    }

    if (!updateDesktopFileAndIcons(path) || !updateDesktopDatabaseAndIconCaches()) {
        if (!setAppImageLaunchArguments(path, oldArguments))
            qCritical() << "Could not restore previous AppImage launch arguments for" << path;
        if (!updateDesktopFileAndIcons(path) || !updateDesktopDatabaseAndIconCaches())
            qCritical() << "Could not restore integrated desktop entry after launch argument update for" << path;
        QMessageBox::critical(this, tr("Launch arguments"),
                              tr("The arguments could not be applied to the integrated desktop entry."));
        const QSignalBlocker tableUpdates(ui->detectedAppImagesTableWidget);
        item->setText(oldArguments);
    }
}

void SettingsDialog::integrateSelectedAppImage() {
    const auto row = ui->detectedAppImagesTableWidget->currentRow();
    if (row < 0)
        return;

    const auto path = ui->detectedAppImagesTableWidget->item(row, 0)->data(Qt::UserRole).toString();
    const auto pathString = path.toStdString();
    const auto shallNotBeIntegrated = appimage_shall_not_be_integrated(pathString.c_str());
    if (shallNotBeIntegrated != 0) {
        const auto message = shallNotBeIntegrated > 0
                                 ? tr("This AppImage's desktop entry disables integration.")
                                 : tr("Could not determine whether this AppImage permits integration.");
        QMessageBox::warning(this, tr("Integration unavailable"), message);
        return;
    }
    const auto isTerminalApp = appimage_is_terminal_app(pathString.c_str());
    if (isTerminalApp != 0) {
        const auto message = isTerminalApp > 0
                                 ? tr("Terminal AppImages cannot be integrated.")
                                 : tr("Could not determine whether this AppImage is a terminal application.");
        QMessageBox::warning(this, tr("Integration unavailable"), message);
        return;
    }

    const auto previousDecision = appImageIntegrationDecision(path);
    if (!setAppImageIntegrationDecision(path, AppImageIntegrationDecision::Integrate)) {
        QMessageBox::critical(this, tr("Integration"), tr("Could not save the integration preference."));
        return;
    }

    const auto integratedPath = buildPathToIntegratedAppImage(path);
    const auto integrationResult = integrateAppImage(path, integratedPath);
    if (integrationResult != INTEGRATION_SUCCESSFUL) {
        if (!setAppImageIntegrationDecision(path, previousDecision))
            qCritical() << "Could not restore AppImage integration preference for" << path;
        if (integrationResult == INTEGRATION_FAILED)
            QMessageBox::critical(this, tr("Integration"), tr("Could not integrate the selected AppImage."));
        return;
    }

    if (!updateDesktopDatabaseAndIconCaches()) {
        QMessageBox::warning(this, tr("Integration"), tr("The AppImage was integrated, but desktop caches could not be updated."));
    }
    if (!setAppImageIntegrationDecision(integratedPath, AppImageIntegrationDecision::Integrate)) {
        QMessageBox::critical(this, tr("Integration"), tr("The AppImage was integrated, but its preference could not be saved."));
    }
    refreshDetectedAppImages(false);
}

void SettingsDialog::declineSelectedAppImage() {
    const auto row = ui->detectedAppImagesTableWidget->currentRow();
    if (row < 0)
        return;

    const auto path = ui->detectedAppImagesTableWidget->item(row, 0)->data(Qt::UserRole).toString();
    if (hasAlreadyBeenIntegrated(path)) {
        if (!unregisterAppImage(path) || !updateDesktopDatabaseAndIconCaches()) {
            QMessageBox::critical(this, tr("Integration"), tr("Could not remove the existing integration."));
            return;
        }
    }

    if (!setAppImageIntegrationDecision(path, AppImageIntegrationDecision::DoNotIntegrate)) {
        QMessageBox::critical(this, tr("AppImage preference"), tr("Could not save the integration preference."));
        return;
    }
    refreshDetectedAppImages(false);
}

void SettingsDialog::launchSelectedAppImage() {
    const auto row = ui->detectedAppImagesTableWidget->currentRow();
    if (row < 0)
        return;

    const auto path = ui->detectedAppImagesTableWidget->item(row, 0)->data(Qt::UserRole).toString();
    if (!QFileInfo::exists(path)) {
        QMessageBox::warning(this, tr("Launch"), tr("The AppImage file no longer exists."));
        refreshDetectedAppImages(false);
        return;
    }

    // launched directly, so that the saved launch arguments are always applied
    const auto arguments = QProcess::splitCommand(appImageLaunchArguments(path));
    if (!QProcess::startDetached(path, arguments, QFileInfo(path).absolutePath()))
        QMessageBox::critical(this, tr("Launch"), tr("Could not launch the AppImage."));
}

void SettingsDialog::deleteSelectedAppImage() {
    const auto row = ui->detectedAppImagesTableWidget->currentRow();
    if (row < 0)
        return;

    const auto path = ui->detectedAppImagesTableWidget->item(row, 0)->data(Qt::UserRole).toString();
    const auto answer = QMessageBox::question(
        this, tr("Delete from disk"),
        tr("Permanently delete this AppImage from disk?\n\n%1\n\nThis cannot be undone. Any desktop integration "
           "will be removed.").arg(path),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    if (hasAlreadyBeenIntegrated(path) && (!unregisterAppImage(path) || !updateDesktopDatabaseAndIconCaches())) {
        QMessageBox::critical(this, tr("Delete from disk"), tr("Could not remove the existing integration."));
        return;
    }

    if (QFileInfo::exists(path) && !QFile::remove(path)) {
        QMessageBox::critical(this, tr("Delete from disk"), tr("Could not delete the AppImage file."));
        return;
    }

    forgetDetectedAppImage(path);
    refreshDetectedAppImages(false);
}

void SettingsDialog::clearSelectedAppImageDecision() {
    const auto row = ui->detectedAppImagesTableWidget->currentRow();
    if (row < 0)
        return;

    if (!setAppImageIntegrationDecision(ui->detectedAppImagesTableWidget->item(row, 0)->data(Qt::UserRole).toString(),
                                        AppImageIntegrationDecision::Undecided)) {
        QMessageBox::critical(this, tr("AppImage preference"), tr("Could not clear the integration preference."));
        return;
    }
    refreshDetectedAppImages(false);
}
