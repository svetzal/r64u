#pragma once
#include <QModelIndex>
#include <QString>

class INavigationView {
public:
    virtual ~INavigationView() = default;
    virtual void setPath(const QString &path) = 0;
    virtual void setUpEnabled(bool enabled) = 0;
    [[nodiscard]] virtual QModelIndex currentIndex() const = 0;
};
