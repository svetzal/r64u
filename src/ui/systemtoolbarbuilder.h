#ifndef SYSTEMTOOLBARBUILDER_H
#define SYSTEMTOOLBARBUILDER_H

class QMainWindow;
class QToolBar;
class DeviceConnection;
class SystemCommandController;
class ConnectionStatusWidget;
class ConnectionUIController;
class QAction;

/// Result of building the system toolbar.
struct SystemToolBarResult
{
    QAction *connectAction = nullptr;
    QAction *resetAction = nullptr;
    QAction *rebootAction = nullptr;
    QAction *pauseAction = nullptr;
    QAction *resumeAction = nullptr;
    QAction *menuAction = nullptr;
    QAction *powerOffAction = nullptr;
    QAction *prefsAction = nullptr;
    ConnectionStatusWidget *connectionStatus = nullptr;
    ConnectionUIController *connectionUiController = nullptr;
};

/// Constructs the system toolbar and returns all created objects.
/// The caller is responsible for wiring the connectAction trigger (which needs private
/// MainWindow slots) and the windowTitleUpdateNeeded connection.
class SystemToolBarBuilder
{
public:
    /// Populate @a toolBar with all system actions and return the created objects.
    /// @param window         The MainWindow (parent for ConnectionUIController)
    /// @param toolBar        The QToolBar to populate (must already be added to the window)
    /// @param deviceConnection  The device connection (for ConnectionUIController)
    /// @param sysCtrl        The SystemCommandController for hardware actions
    /// @param refreshAction  The refresh action from the menu bar (passed to
    /// ConnectionUIController)
    static SystemToolBarResult build(QMainWindow *window, QToolBar *toolBar,
                                     DeviceConnection *deviceConnection,
                                     SystemCommandController *sysCtrl, QAction *refreshAction);
};

#endif  // SYSTEMTOOLBARBUILDER_H
