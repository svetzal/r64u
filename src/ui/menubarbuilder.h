#ifndef MENUBARBUILDER_H
#define MENUBARBUILDER_H

class QMainWindow;
class QTabWidget;
class SystemCommandController;
class QAction;

/// Constructs the application menu bar and returns the one action that MainWindow retains.
class MenuBarBuilder
{
public:
    /// Build all menus on @a window and return the refresh QAction.
    static QAction *build(QMainWindow *window, SystemCommandController *sysCtrl,
                          QTabWidget *modeTabWidget);
};

#endif  // MENUBARBUILDER_H
