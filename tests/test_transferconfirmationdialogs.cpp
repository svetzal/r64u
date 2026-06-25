/**
 * @file test_transferconfirmationdialogs.cpp
 * @brief Headless unit tests for TransferConfirmationDialogs.
 *
 * Tests verify that askOverwrite() and askFolderExists() map the integer index
 * returned by IMessagePresenter::confirm() to the correct response enum value,
 * and that the correct message text is passed for single- vs multi-folder cases.
 */

#include "mocks/mockmessagepresenter.h"
#include "ui/transferconfirmationdialogs.h"

#include <QtTest>

class TestTransferConfirmationDialogs : public QObject
{
    Q_OBJECT

private slots:

    // =========================================================================
    // askOverwrite
    // =========================================================================

    void testAskOverwrite_index0_returnsOverwrite()
    {
        MockMessagePresenter mock;
        mock.nextConfirmResult = 0;
        QCOMPARE(TransferConfirmationDialogs::askOverwrite(mock, nullptr, "file.prg"),
                 transfer::OverwriteResponse::Overwrite);
    }

    void testAskOverwrite_index1_returnsOverwriteAll()
    {
        MockMessagePresenter mock;
        mock.nextConfirmResult = 1;
        QCOMPARE(TransferConfirmationDialogs::askOverwrite(mock, nullptr, "file.prg"),
                 transfer::OverwriteResponse::OverwriteAll);
    }

    void testAskOverwrite_index2_returnsSkip()
    {
        MockMessagePresenter mock;
        mock.nextConfirmResult = 2;
        QCOMPARE(TransferConfirmationDialogs::askOverwrite(mock, nullptr, "file.prg"),
                 transfer::OverwriteResponse::Skip);
    }

    void testAskOverwrite_index3_returnsCancel()
    {
        MockMessagePresenter mock;
        mock.nextConfirmResult = 3;
        QCOMPARE(TransferConfirmationDialogs::askOverwrite(mock, nullptr, "file.prg"),
                 transfer::OverwriteResponse::Cancel);
    }

    void testAskOverwrite_negativeIndex_returnsCancel()
    {
        MockMessagePresenter mock;
        mock.nextConfirmResult = -1;
        QCOMPARE(TransferConfirmationDialogs::askOverwrite(mock, nullptr, "file.prg"),
                 transfer::OverwriteResponse::Cancel);
    }

    void testAskOverwrite_includesFileNameInMessage()
    {
        MockMessagePresenter mock;
        mock.nextConfirmResult = 0;
        TransferConfirmationDialogs::askOverwrite(mock, nullptr, "mygame.prg");

        QCOMPARE(mock.confirmCalls.size(), 1);
        QVERIFY(mock.confirmCalls[0].message.contains("mygame.prg"));
    }

    void testAskOverwrite_hasThreeNamedButtons()
    {
        MockMessagePresenter mock;
        mock.nextConfirmResult = 0;
        TransferConfirmationDialogs::askOverwrite(mock, nullptr, "x.prg");

        QCOMPARE(mock.confirmCalls.size(), 1);
        const auto &buttons = mock.confirmCalls[0].buttons;
        QCOMPARE(buttons.size(), 4);
        QCOMPARE(buttons[0].text, QString("Overwrite"));
        QCOMPARE(buttons[1].text, QString("Overwrite All"));
        QCOMPARE(buttons[2].text, QString("Skip"));
        QCOMPARE(buttons[3].text, QString("Cancel"));
    }

    void testAskOverwrite_defaultIndexIsSkip()
    {
        MockMessagePresenter mock;
        mock.nextConfirmResult = 0;
        TransferConfirmationDialogs::askOverwrite(mock, nullptr, "x.prg");

        QCOMPARE(mock.confirmCalls[0].defaultIndex, 2);  // Skip is index 2
    }

    // =========================================================================
    // askFolderExists — single folder
    // =========================================================================

    void testAskFolderExists_index0_returnsMerge()
    {
        MockMessagePresenter mock;
        mock.nextConfirmResult = 0;
        QCOMPARE(TransferConfirmationDialogs::askFolderExists(mock, nullptr, {"Games"}),
                 transfer::FolderExistsResponse::Merge);
    }

    void testAskFolderExists_index1_returnsReplace()
    {
        MockMessagePresenter mock;
        mock.nextConfirmResult = 1;
        QCOMPARE(TransferConfirmationDialogs::askFolderExists(mock, nullptr, {"Games"}),
                 transfer::FolderExistsResponse::Replace);
    }

    void testAskFolderExists_index2_returnsCancel()
    {
        MockMessagePresenter mock;
        mock.nextConfirmResult = 2;
        QCOMPARE(TransferConfirmationDialogs::askFolderExists(mock, nullptr, {"Games"}),
                 transfer::FolderExistsResponse::Cancel);
    }

    void testAskFolderExists_negativeIndex_returnsCancel()
    {
        MockMessagePresenter mock;
        mock.nextConfirmResult = -1;
        QCOMPARE(TransferConfirmationDialogs::askFolderExists(mock, nullptr, {"Games"}),
                 transfer::FolderExistsResponse::Cancel);
    }

    void testAskFolderExists_singleFolder_messageContainsFolderName()
    {
        MockMessagePresenter mock;
        mock.nextConfirmResult = 0;
        TransferConfirmationDialogs::askFolderExists(mock, nullptr, {"MyFolder"});

        QCOMPARE(mock.confirmCalls.size(), 1);
        QVERIFY(mock.confirmCalls[0].message.contains("MyFolder"));
    }

    void testAskFolderExists_multiFolder_messageContainsAllFolderNames()
    {
        MockMessagePresenter mock;
        mock.nextConfirmResult = 0;
        TransferConfirmationDialogs::askFolderExists(mock, nullptr, {"Alpha", "Beta", "Gamma"});

        QCOMPARE(mock.confirmCalls.size(), 1);
        const QString &msg = mock.confirmCalls[0].message;
        QVERIFY(msg.contains("Alpha"));
        QVERIFY(msg.contains("Beta"));
        QVERIFY(msg.contains("Gamma"));
    }

    void testAskFolderExists_hasThreeNamedButtons()
    {
        MockMessagePresenter mock;
        mock.nextConfirmResult = 0;
        TransferConfirmationDialogs::askFolderExists(mock, nullptr, {"Dir"});

        QCOMPARE(mock.confirmCalls.size(), 1);
        const auto &buttons = mock.confirmCalls[0].buttons;
        QCOMPARE(buttons.size(), 3);
        QCOMPARE(buttons[0].text, QString("Merge"));
        QCOMPARE(buttons[1].text, QString("Replace"));
        QCOMPARE(buttons[2].text, QString("Cancel"));
    }

    void testAskFolderExists_defaultIndexIsMerge()
    {
        MockMessagePresenter mock;
        mock.nextConfirmResult = 0;
        TransferConfirmationDialogs::askFolderExists(mock, nullptr, {"Dir"});

        QCOMPARE(mock.confirmCalls[0].defaultIndex, 0);  // Merge is index 0
    }

    void testAskFolderExists_replaceButtonIsDestructive()
    {
        MockMessagePresenter mock;
        mock.nextConfirmResult = 0;
        TransferConfirmationDialogs::askFolderExists(mock, nullptr, {"Dir"});

        const auto &buttons = mock.confirmCalls[0].buttons;
        QCOMPARE(buttons[1].role, IMessagePresenter::ButtonRole::Destructive);
    }
};

QTEST_MAIN(TestTransferConfirmationDialogs)
#include "test_transferconfirmationdialogs.moc"
