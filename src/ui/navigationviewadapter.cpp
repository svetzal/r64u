#include "navigationviewadapter.h"

#include "pathnavigationwidget.h"

#include <QTreeView>

NavigationViewAdapter::NavigationViewAdapter(QTreeView *treeView, PathNavigationWidget *navWidget,
                                             QObject *parent)
    : QObject(parent), treeView_(treeView), navWidget_(navWidget)
{
}

void NavigationViewAdapter::setPath(const QString &path)
{
    if (navWidget_) {
        navWidget_->setPath(path);
    }
}

void NavigationViewAdapter::setUpEnabled(bool enabled)
{
    if (navWidget_) {
        navWidget_->setUpEnabled(enabled);
    }
}

QModelIndex NavigationViewAdapter::currentIndex() const
{
    if (treeView_) {
        return treeView_->currentIndex();
    }
    return {};
}
