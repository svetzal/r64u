#ifndef CONNECTIONTESTER_H
#define CONNECTIONTESTER_H

#include "../services/irestclient.h"

#include <QObject>

class QWidget;

/// Encapsulates the one-shot test-connection workflow for the preferences dialog.
class ConnectionTester : public QObject
{
    Q_OBJECT

public:
    explicit ConnectionTester(QWidget *parentWidget, QObject *parent = nullptr);

    void startTest(const QString &host, const QString &password);

private slots:
    void onTestConnectionSuccess(const DeviceInfo &info);
    void onTestConnectionError(const QString &error);

private:
    QWidget *parentWidget_ = nullptr;
    IRestClient *testClient_ = nullptr;
};

#endif  // CONNECTIONTESTER_H
