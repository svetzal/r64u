#pragma once
#include "inavigationview.h"
#include <QObject>

class PathNavigationWidget;
class QTreeView;

class NavigationViewAdapter : public QObject, public INavigationView {
    Q_OBJECT
public:
    explicit NavigationViewAdapter(QTreeView *treeView, PathNavigationWidget *navWidget,
                                   QObject *parent = nullptr);
    void setPath(const QString &path) override;
    void setUpEnabled(bool enabled) override;
    [[nodiscard]] QModelIndex currentIndex() const override;

private:
    QTreeView *treeView_;
    PathNavigationWidget *navWidget_;
};
