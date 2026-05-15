#pragma once
#include "ui/idetailsdisplay.h"

#include <QByteArray>
#include <QList>
#include <QPair>
#include <QString>

/**
 * @brief Test double for IDetailsDisplay that records all method calls.
 *
 * Header-only mock — no Q_OBJECT needed since the interface has no signals or slots.
 * Use reset() in test cleanup to clear recorded call state between test cases.
 */
class MockDetailsDisplay : public IDetailsDisplay
{
public:
    void clear() override { clearCount++; }

    void showFileDetails(const QString &path, qint64 size, const QString &type) override
    {
        fileDetailsCalls.append({path, QString::number(size) + "|" + type});
    }

    void showDiskDirectory(const QByteArray &data, const QString &path) override
    {
        diskDirectoryCalls.append({path, data});
    }

    void showSidDetails(const QByteArray &data, const QString &path) override
    {
        sidDetailsCalls.append({path, data});
    }

    void showTextContent(const QString &content) override { textContentCalls.append(content); }

    void showError(const QString &message) override { errorCalls.append(message); }

    void reset()
    {
        clearCount = 0;
        fileDetailsCalls.clear();
        diskDirectoryCalls.clear();
        sidDetailsCalls.clear();
        textContentCalls.clear();
        errorCalls.clear();
    }

    int clearCount = 0;
    QList<QPair<QString, QString>> fileDetailsCalls;
    QList<QPair<QString, QByteArray>> diskDirectoryCalls;
    QList<QPair<QString, QByteArray>> sidDetailsCalls;
    QList<QString> textContentCalls;
    QList<QString> errorCalls;
};
