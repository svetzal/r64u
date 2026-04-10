#include "connectiontester.h"

#include "../services/c64urestclient.h"

#include <QApplication>
#include <QMessageBox>

ConnectionTester::ConnectionTester(QWidget *parentWidget, QObject *parent)
    : QObject(parent), parentWidget_(parentWidget)
{
}

void ConnectionTester::startTest(const QString &host, const QString &password)
{
    if (host.isEmpty()) {
        QMessageBox::warning(parentWidget_, tr("Test Connection"),
                             tr("Please enter a host address."));
        return;
    }

    if (testClient_) {
        testClient_->deleteLater();
    }

    testClient_ = new C64URestClient(parentWidget_);
    testClient_->setHost(host);
    testClient_->setPassword(password);

    connect(testClient_, &IRestClient::infoReceived, this,
            &ConnectionTester::onTestConnectionSuccess);
    connect(testClient_, &IRestClient::connectionError, this,
            &ConnectionTester::onTestConnectionError);

    QApplication::setOverrideCursor(Qt::WaitCursor);
    testClient_->getInfo();
}

void ConnectionTester::onTestConnectionSuccess(const DeviceInfo &info)
{
    QApplication::restoreOverrideCursor();

    QString message = tr("Connection successful!\n\n"
                         "Device: %1\n"
                         "Firmware: %2\n"
                         "Hostname: %3")
                          .arg(info.product)
                          .arg(info.firmwareVersion)
                          .arg(info.hostname);

    QMessageBox::information(parentWidget_, tr("Test Connection"), message);

    if (testClient_) {
        testClient_->deleteLater();
        testClient_ = nullptr;
    }
}

void ConnectionTester::onTestConnectionError(const QString &error)
{
    QApplication::restoreOverrideCursor();

    QMessageBox::critical(parentWidget_, tr("Test Connection"),
                          tr("Connection failed:\n%1").arg(error));

    if (testClient_) {
        testClient_->deleteLater();
        testClient_ = nullptr;
    }
}
