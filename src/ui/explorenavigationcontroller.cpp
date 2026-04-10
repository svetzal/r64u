#include "explorenavigationcontroller.h"

#include "explorefavoritescontroller.h"
#include "pathnavigationwidget.h"

#include "models/remotefilemodel.h"
#include "services/deviceconnection.h"

#include <QTreeView>

ExploreNavigationController::ExploreNavigationController(
    DeviceConnection *connection, RemoteFileModel *model, QTreeView *treeView,
    PathNavigationWidget *navWidget, ExploreFavoritesController *favoritesController,
    QObject *parent)
    : QObject(parent), deviceConnection_(connection), remoteFileModel_(model), treeView_(treeView),
      navWidget_(navWidget), favoritesController_(favoritesController), currentDirectory_("/")
{
}

void ExploreNavigationController::setCurrentDirectory(const QString &path)
{
    currentDirectory_ = path;

    if (remoteFileModel_) {
        remoteFileModel_->setRootPath(path);
    }

    if (navWidget_) {
        navWidget_->setPath(path);
    }

    emit statusMessage(tr("Navigated to: %1").arg(path), 2000);

    bool canGoUp = (path != "/" && !path.isEmpty());
    if (navWidget_) {
        navWidget_->setUpEnabled(canGoUp);
    }

    if (favoritesController_) {
        favoritesController_->updateForPath(path);
    }
}

QString ExploreNavigationController::currentDirectory() const
{
    return currentDirectory_;
}

void ExploreNavigationController::navigateToParent()
{
    if (currentDirectory_.isEmpty() || currentDirectory_ == "/") {
        return;
    }

    QString parentPath = currentDirectory_;
    int lastSlash = parentPath.lastIndexOf('/');
    if (lastSlash > 0) {
        parentPath = parentPath.left(lastSlash);
    } else {
        parentPath = "/";
    }

    setCurrentDirectory(parentPath);
}

void ExploreNavigationController::refresh()
{
    if (!deviceConnection_ || !deviceConnection_->canPerformOperations()) {
        return;
    }

    if (!remoteFileModel_ || !treeView_) {
        return;
    }

    QModelIndex index = treeView_->currentIndex();
    if (index.isValid() && remoteFileModel_->isDirectory(index)) {
        remoteFileModel_->refresh(index);
    } else {
        remoteFileModel_->refresh();
    }

    deviceConnection_->refreshDriveInfo();
}

void ExploreNavigationController::refreshIfStale()
{
    if (!deviceConnection_ || !deviceConnection_->canPerformOperations()) {
        return;
    }

    if (remoteFileModel_) {
        remoteFileModel_->refreshIfStale();
    }
}
