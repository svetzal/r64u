#ifndef IPANEL_H
#define IPANEL_H

#include <QObject>
#include <QString>

class IPanel
{
public:
    virtual ~IPanel() = default;
    virtual QObject *asQObject() = 0;

    // No-op defaults — override in panels that participate in directory sync
    virtual void refreshIfEmpty() {}
    virtual QString currentDirectory() const { return {}; }
    virtual void setCurrentDirectory(const QString &) {}
    virtual QString currentRemoteDir() const { return {}; }
    virtual void setCurrentRemoteDir(const QString &) {}
};

#endif  // IPANEL_H
