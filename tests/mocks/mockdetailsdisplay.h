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
        diskDirectoryCalls.clear();
        sidDetailsCalls.clear();
        textContentCalls.clear();
        errorCalls.clear();
    }

    QList<QPair<QString, QByteArray>> diskDirectoryCalls;
    QList<QPair<QString, QByteArray>> sidDetailsCalls;
    QList<QString> textContentCalls;
    QList<QString> errorCalls;
};
