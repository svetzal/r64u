#ifndef CONNECTIONTESTHANDLER_H
#define CONNECTIONTESTHANDLER_H

#include "../services/irestclient.h"

#include <QObject>

class QWidget;

/// Encapsulates the one-shot test-connection workflow for the preferences dialog.
class ConnectionTestHandler : public QObject
{
    Q_OBJECT

public:
    explicit ConnectionTestHandler(QWidget *parentWidget, QObject *parent = nullptr);

    void startTest(const QString &host, const QString &password);

private slots:
    void onTestConnectionSuccess(const DeviceInfo &info);
    void onTestConnectionError(const QString &error);

private:
    QWidget *parentWidget_ = nullptr;
    IRestClient *testClient_ = nullptr;
};

#endif  // CONNECTIONTESTHANDLER_H
