/**
 * @file test_preferencesdialog.cpp
 * @brief Unit tests for PreferencesDialog construction and basic widget state.
 *
 * Tests verify:
 * - Construction does not crash (with empty QSettings — avoids keychain access)
 * - scalingModeCombo_ has exactly 3 items (Sharp, Smooth, Integer)
 * - hostEdit_ is empty when no host is stored in settings
 */

#include "ui/preferencesdialog.h"

#include <QComboBox>
#include <QLineEdit>
#include <QSettings>
#include <QtTest>

class TestPreferencesDialog : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        QCoreApplication::setOrganizationName("r64utest");
        QCoreApplication::setApplicationName("test_preferencesdialog");
        QSettings settings;
        settings.clear();
        settings.sync();
    }

    void cleanup()
    {
        QSettings settings;
        settings.clear();
        settings.sync();
    }

    void testConstruct_doesNotCrash()
    {
        PreferencesDialog dialog;
        QVERIFY(true);
    }

    void testLoadSettings_defaultHost_isEmpty()
    {
        PreferencesDialog dialog;
        // When settings are empty the host field must be empty (no keychain read occurs)
        const auto edits = dialog.findChildren<QLineEdit *>();
        bool foundEmpty = false;
        for (auto *edit : edits) {
            // hostEdit_ is the first QLineEdit with no echo mode set to Password
            if (edit->echoMode() == QLineEdit::Normal) {
                QVERIFY(edit->text().isEmpty());
                foundEmpty = true;
                break;
            }
        }
        QVERIFY(foundEmpty);
    }

    void testScalingModeCombo_hasThreeItems()
    {
        PreferencesDialog dialog;
        const auto combos = dialog.findChildren<QComboBox *>();
        bool found = false;
        for (auto *combo : combos) {
            if (combo->count() == 3) {
                found = true;
                break;
            }
        }
        QVERIFY(found);
    }
};

QTEST_MAIN(TestPreferencesDialog)
#include "test_preferencesdialog.moc"
