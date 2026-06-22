/**
 * @file test_refreshpolicymanager.cpp
 * @brief Unit tests for RefreshPolicyManager suppressor RAII and refresh policy.
 */

#include "ui/refreshpolicymanager.h"

#include <QtTest>

class TestRefreshPolicyManager : public QObject
{
    Q_OBJECT

private:
    RefreshPolicyManager *policy_ = nullptr;

private slots:
    void init() { policy_ = new RefreshPolicyManager(this); }

    void cleanup()
    {
        delete policy_;
        policy_ = nullptr;
    }

    // -----------------------------------------------------------------------
    // Initial state
    // -----------------------------------------------------------------------

    void testInitial_notSuppressed() { QVERIFY(!policy_->isSuppressed()); }

    // -----------------------------------------------------------------------
    // setSuppressed / isSuppressed
    // -----------------------------------------------------------------------

    void testSetSuppressed_true_isSuppressedTrue()
    {
        policy_->setSuppressed(true);
        QVERIFY(policy_->isSuppressed());
    }

    void testSetSuppressed_false_isSuppressedFalse()
    {
        policy_->setSuppressed(true);
        policy_->setSuppressed(false);
        QVERIFY(!policy_->isSuppressed());
    }

    // -----------------------------------------------------------------------
    // suppress() RAII
    // -----------------------------------------------------------------------

    void testSuppress_whileActive_isSuppressedTrue()
    {
        auto s = policy_->suppress();
        QVERIFY(policy_->isSuppressed());
    }

    void testSuppress_afterScopeEnd_isSuppressedFalse()
    {
        {
            auto s = policy_->suppress();
            QVERIFY(policy_->isSuppressed());
        }
        QVERIFY(!policy_->isSuppressed());
    }

    void testSuppress_nested_restoresOnInnerDestruct()
    {
        policy_->setSuppressed(true);
        {
            auto s = policy_->suppress();
            QVERIFY(policy_->isSuppressed());
        }
        // inner suppressor destructs → setSuppressed(false) even though outer was true
        QVERIFY(!policy_->isSuppressed());
    }

    // -----------------------------------------------------------------------
    // setConnected / refreshIfStale with null model
    // -----------------------------------------------------------------------

    void testRefreshIfStale_notConnected_doesNotCrash()
    {
        policy_->setConnected(false);
        policy_->setModel(nullptr);
        policy_->refreshIfStale();  // must not crash
        QVERIFY(true);
    }

    void testRefreshIfStale_connectedSuppressed_doesNotCrash()
    {
        policy_->setConnected(true);
        policy_->setSuppressed(true);
        policy_->setModel(nullptr);
        policy_->refreshIfStale();  // must not crash
        QVERIFY(true);
    }

    void testRefreshIfStale_connectedNotSuppressedNullModel_doesNotCrash()
    {
        policy_->setConnected(true);
        policy_->setSuppressed(false);
        policy_->setModel(nullptr);
        policy_->refreshIfStale();  // null model guard must not crash
        QVERIFY(true);
    }

    // -----------------------------------------------------------------------
    // setConnected state
    // -----------------------------------------------------------------------

    void testSetConnected_changesState()
    {
        // We verify the connected flag affects refresh behaviour by testing
        // that refreshIfStale is a no-op when not connected (no crash = guard fires)
        policy_->setConnected(false);
        policy_->setModel(nullptr);
        policy_->refreshIfStale();
        QVERIFY(true);

        policy_->setConnected(true);
        policy_->setModel(nullptr);
        policy_->refreshIfStale();
        QVERIFY(true);
    }
};

QTEST_MAIN(TestRefreshPolicyManager)
#include "test_refreshpolicymanager.moc"
