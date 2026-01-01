#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QTreeView>
#include <QSplitter>
#include <QToolBar>
#include <QComboBox>
#include <QLabel>
#include <QFileSystemModel>

class PreferencesDialog;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    enum class Mode {
        ExploreRun,
        Transfer
    };

    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onModeChanged(int index);
    void onPreferences();
    void onPlay();
    void onRun();
    void onMount();
    void onReset();
    void onUpload();
    void onDownload();
    void onNewFolder();
    void onRefresh();

private:
    void setupUi();
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupExploreRunMode();
    void setupTransferMode();
    void switchToMode(Mode mode);
    void updateWindowTitle();

    Mode currentMode_ = Mode::ExploreRun;
    QString connectedHost_;
    QString firmwareVersion_;

    // Central widget
    QStackedWidget *stackedWidget_ = nullptr;

    // Explore/Run mode widgets
    QWidget *exploreRunWidget_ = nullptr;
    QTreeView *remoteTreeView_ = nullptr;

    // Transfer mode widgets
    QWidget *transferWidget_ = nullptr;
    QSplitter *transferSplitter_ = nullptr;
    QTreeView *localTreeView_ = nullptr;
    QTreeView *remoteTransferTreeView_ = nullptr;
    QFileSystemModel *localFileModel_ = nullptr;

    // Toolbar
    QToolBar *mainToolBar_ = nullptr;
    QComboBox *modeCombo_ = nullptr;

    // Mode-specific actions
    QAction *playAction_ = nullptr;
    QAction *runAction_ = nullptr;
    QAction *mountAction_ = nullptr;
    QAction *resetAction_ = nullptr;
    QAction *uploadAction_ = nullptr;
    QAction *downloadAction_ = nullptr;
    QAction *newFolderAction_ = nullptr;

    // Status bar
    QLabel *driveALabel_ = nullptr;
    QLabel *driveBLabel_ = nullptr;
    QLabel *connectionLabel_ = nullptr;

    // Dialogs
    PreferencesDialog *preferencesDialog_ = nullptr;
};

#endif // MAINWINDOW_H
