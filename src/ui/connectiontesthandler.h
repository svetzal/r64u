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
    /**
     * @brief Constructs a ConnectionTestHandler.
     * @param parentWidget Widget used as parent for QMessageBox dialogs.
     * @param injectedClient Optional IRestClient to use instead of creating a C64URestClient.
     *        When non-null the injected client is used and NOT owned or deleted by this handler.
     *        Pass nullptr (default) to use the production C64URestClient.
     * @param parent QObject parent.
     */
    explicit ConnectionTestHandler(QWidget *parentWidget, IRestClient *injectedClient = nullptr,
                                   QObject *parent = nullptr);

    void startTest(const QString &host, const QString &password);

private slots:
    void onTestConnectionSuccess(const DeviceInfo &info);
    void onTestConnectionError(const QString &error);

private:
    QWidget *parentWidget_ = nullptr;
    IRestClient *testClient_ = nullptr;
    IRestClient *injectedClient_ = nullptr;  ///< Non-owning; set via constructor for testing.
};

#endif  // CONNECTIONTESTHANDLER_H
