#ifndef EXPLORENAVIGATIONCONTROLLER_H
#define EXPLORENAVIGATIONCONTROLLER_H

#include <QObject>
#include <QString>

class DeviceConnection;
class RemoteFileModel;
class ExploreFavoritesController;
class PathNavigationWidget;
class QTreeView;

/// Owns the current-directory state for ExplorePanel and encapsulates
/// all navigation operations: set directory, navigate to parent, refresh.
class ExploreNavigationController : public QObject
{
    Q_OBJECT

public:
    explicit ExploreNavigationController(DeviceConnection *connection, RemoteFileModel *model,
                                         QTreeView *treeView, PathNavigationWidget *navWidget,
                                         ExploreFavoritesController *favoritesController,
                                         QObject *parent = nullptr);

    void setCurrentDirectory(const QString &path);
    [[nodiscard]] QString currentDirectory() const;

    void navigateToParent();
    void refresh();
    void refreshIfStale();

signals:
    void statusMessage(const QString &message, int timeout = 0);

private:
    DeviceConnection *deviceConnection_ = nullptr;
    RemoteFileModel *remoteFileModel_ = nullptr;
    QTreeView *treeView_ = nullptr;
    PathNavigationWidget *navWidget_ = nullptr;
    ExploreFavoritesController *favoritesController_ = nullptr;

    QString currentDirectory_;
};

#endif  // EXPLORENAVIGATIONCONTROLLER_H
