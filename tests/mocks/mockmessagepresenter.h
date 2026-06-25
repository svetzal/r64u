#ifndef MOCKMESSAGEPRESENTER_H
#define MOCKMESSAGEPRESENTER_H

#include "ui/imessagepresenter.h"

#include <QList>
#include <QString>

/**
 * @brief Recorded call to showInfo or showWarning.
 */
struct MessageCall
{
    QString kind;  ///< "info" or "warning"
    QString title;
    QString message;
};

/**
 * @brief Recorded call to confirm().
 */
struct ConfirmCall
{
    QString title;
    QString message;
    QList<IMessagePresenter::DialogButton> buttons;
    IMessagePresenter::MessageIcon icon;
    int defaultIndex;
};

/**
 * @brief Test double for IMessagePresenter that records all calls and returns scripted results.
 *
 * Usage:
 * @code
 *   MockMessagePresenter mock;
 *   mock.nextConfirmResult = 1;   // next confirm() returns index 1
 *   presenter.confirm(...);       // returns 1
 *   QCOMPARE(mock.confirmCalls.size(), 1);
 * @endcode
 */
class MockMessagePresenter : public IMessagePresenter
{
public:
    /// Set before calling confirm() to control the return value.
    int nextConfirmResult = 0;

    /// All showInfo / showWarning calls in order.
    QVector<MessageCall> calls;

    /// All confirm() calls in order.
    QVector<ConfirmCall> confirmCalls;

    void showInfo(QWidget * /*parent*/, const QString &title, const QString &message) override
    {
        calls.push_back({"info", title, message});
    }

    void showWarning(QWidget * /*parent*/, const QString &title, const QString &message) override
    {
        calls.push_back({"warning", title, message});
    }

    int confirm(QWidget * /*parent*/, const QString &title, const QString &message,
                const QList<DialogButton> &buttons, MessageIcon icon, int defaultIndex) override
    {
        confirmCalls.push_back({title, message, buttons, icon, defaultIndex});
        return nextConfirmResult;
    }
};

#endif  // MOCKMESSAGEPRESENTER_H
