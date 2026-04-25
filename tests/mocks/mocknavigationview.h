#pragma once
#include "ui/inavigationview.h"

#include <QList>
#include <QModelIndex>
#include <QString>

class MockNavigationView : public INavigationView
{
public:
    void setPath(const QString &path) override { pathHistory.append(path); }
    void setUpEnabled(bool enabled) override { upEnabledHistory.append(enabled); }
    [[nodiscard]] QModelIndex currentIndex() const override { return stubbedCurrentIndex; }

    QList<QString> pathHistory;
    QList<bool> upEnabledHistory;
    QModelIndex stubbedCurrentIndex;  // default: invalid QModelIndex
};
