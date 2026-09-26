#ifndef MENUBARBUILDER_H
#define MENUBARBUILDER_H

class QMainWindow;
class QTabWidget;
class SystemCommandController;
class QAction;

namespace menubar {

/// Constructs the application menu bar and returns the one action that MainWindow retains.
class Builder
{
public:
    /// Build all menus on @a window and return the refresh QAction.
    /// @param fullScreenAction Optional window-owned action added to the View menu.
    static QAction *build(QMainWindow *window, SystemCommandController *sysCtrl,
                          QTabWidget *modeTabWidget, QAction *fullScreenAction = nullptr);
};

}  // namespace menubar

// Backward-compatible alias for call sites using the old name
using MenuBarBuilder = menubar::Builder;

#endif  // MENUBARBUILDER_H
