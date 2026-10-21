#pragma once

// system
#include <memory>

// libraries
#include <QDialog>
#include <QLabel>
#include <QListWidgetItem>
#include <QSettings>


namespace Ui {
    class SettingsDialog;
}

class TranslationManager;
class QTableWidgetItem;

class SettingsDialog : public QDialog {
Q_OBJECT

public:
    explicit SettingsDialog(TranslationManager& translationManager, QWidget* parent = nullptr);

    ~SettingsDialog() override;

protected slots:
    void onChooseAppsDirClicked();
    void onAddDirectoryToWatchButtonClicked();
    void onRemoveDirectoryToWatchButtonClicked();
    void onDirectoryToWatchItemActivated(QListWidgetItem* item);

    void onDialogAccepted();
    void onDetectedAppImageSelectionChanged();
    void onDetectedAppImageItemChanged(QTableWidgetItem* item);
    void integrateSelectedAppImage();
    void declineSelectedAppImage();
    void clearSelectedAppImageDecision();
    void launchSelectedAppImage();
    void deleteSelectedAppImage();
    void refreshDetectedAppImages(bool scanWatchedDirectories = true);
    void onLanguageChanged(int index);

private:
    void loadSettings();

    void saveSettings();

    void toggleDaemon();

    void addDirectoryToWatchToListView(const QString& dirPath);
    void updateDetectedAppImageActions();
    void updateAvailableFeatures();

    Ui::SettingsDialog* ui;
    QLabel* logoLabel = nullptr;
    QLabel* versionLabel = nullptr;
    QSettings* settingsFile;
    TranslationManager& translationManager;
};
