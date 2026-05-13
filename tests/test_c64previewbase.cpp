/**
 * @file test_c64previewbase.cpp
 * @brief Unit tests for C64PreviewBase and DefaultFilePreview.
 *
 * Tests verify:
 * - C64PreviewBase: createPreviewWidget, showError, clear via a concrete subclass
 * - DefaultFilePreview: canHandle, createPreviewWidget, setFileDetails size formatting,
 *   showLoading, showError, and clear
 */

#include "ui/filepreview/c64previewbase.h"
#include "ui/filepreview/defaultfilepreview.h"

#include <QLabel>
#include <QtTest>

// Minimal concrete subclass of C64PreviewBase for testing the base class.
class TestableC64Preview : public C64PreviewBase
{
    Q_OBJECT

public:
    explicit TestableC64Preview(QObject *parent = nullptr) : C64PreviewBase(parent) {}

    [[nodiscard]] bool canHandle(const QString & /*path*/) const override { return true; }
    void showPreview(const QString & /*path*/, const QByteArray & /*data*/) override {}
    void showLoading(const QString & /*path*/) override {}
};

class TestC64PreviewBase : public QObject
{
    Q_OBJECT

private slots:

    // =========================================================================
    // C64PreviewBase via TestableC64Preview
    // =========================================================================

    void testCreatePreviewWidget_returnsNonNull()
    {
        TestableC64Preview preview;
        QWidget *widget = preview.createPreviewWidget(nullptr);
        QVERIFY(widget != nullptr);
        delete widget;
    }

    void testCreatePreviewWidget_calledTwice_doesNotCrash()
    {
        TestableC64Preview preview;
        QWidget *first = preview.createPreviewWidget(nullptr);
        // Second call replaces internal widget pointer — no crash expected
        preview.createPreviewWidget(nullptr);
        delete first;
    }

    void testShowError_doesNotCrash()
    {
        TestableC64Preview preview;
        preview.createPreviewWidget(nullptr);
        preview.showError("some error");
        delete preview.createPreviewWidget(nullptr);
    }

    void testClear_doesNotCrash()
    {
        TestableC64Preview preview;
        QWidget *widget = preview.createPreviewWidget(nullptr);
        preview.clear();
        QVERIFY(true);
        delete widget;
    }

    // =========================================================================
    // DefaultFilePreview
    // =========================================================================

    void testDefaultFilePreview_canHandle_alwaysTrue()
    {
        DefaultFilePreview preview;
        QVERIFY(preview.canHandle("any.file"));
        QVERIFY(preview.canHandle("game.d64"));
        QVERIFY(preview.canHandle(""));
    }

    void testDefaultFilePreview_createPreviewWidget_returnsNonNull()
    {
        DefaultFilePreview preview;
        QWidget *widget = preview.createPreviewWidget(nullptr);
        QVERIFY(widget != nullptr);
        delete widget;
    }

    void testDefaultFilePreview_setFileDetails_setsFileName()
    {
        DefaultFilePreview preview;
        QWidget *widget = preview.createPreviewWidget(nullptr);
        preview.setFileDetails("/path/to/game.d64", 1024, "D64");

        bool found = false;
        const auto labels = widget->findChildren<QLabel *>();
        for (auto *label : labels) {
            if (label->text() == "game.d64") {
                found = true;
                break;
            }
        }
        QVERIFY(found);
        delete widget;
    }

    void testDefaultFilePreview_setFileDetails_smallSize_showsBytes()
    {
        DefaultFilePreview preview;
        QWidget *widget = preview.createPreviewWidget(nullptr);
        preview.setFileDetails("/path/file.prg", 512, "PRG");

        bool found = false;
        const auto labels = widget->findChildren<QLabel *>();
        for (auto *label : labels) {
            if (label->text().contains("bytes")) {
                found = true;
                break;
            }
        }
        QVERIFY(found);
        delete widget;
    }

    void testDefaultFilePreview_setFileDetails_mediumSize_showsKB()
    {
        DefaultFilePreview preview;
        QWidget *widget = preview.createPreviewWidget(nullptr);
        preview.setFileDetails("/path/file.d64", 1024, "D64");

        bool found = false;
        const auto labels = widget->findChildren<QLabel *>();
        for (auto *label : labels) {
            if (label->text().contains("KB")) {
                found = true;
                break;
            }
        }
        QVERIFY(found);
        delete widget;
    }

    void testDefaultFilePreview_setFileDetails_largeSize_showsMB()
    {
        DefaultFilePreview preview;
        QWidget *widget = preview.createPreviewWidget(nullptr);
        preview.setFileDetails("/path/file.d81", qint64{1024} * 1024, "D81");

        bool found = false;
        const auto labels = widget->findChildren<QLabel *>();
        for (auto *label : labels) {
            if (label->text().contains("MB")) {
                found = true;
                break;
            }
        }
        QVERIFY(found);
        delete widget;
    }

    void testDefaultFilePreview_showLoading_doesNotCrash()
    {
        DefaultFilePreview preview;
        QWidget *widget = preview.createPreviewWidget(nullptr);
        preview.showLoading("/path/file.sid");
        QVERIFY(true);
        delete widget;
    }

    void testDefaultFilePreview_showError_doesNotCrash()
    {
        DefaultFilePreview preview;
        QWidget *widget = preview.createPreviewWidget(nullptr);
        preview.showError("not found");
        QVERIFY(true);
        delete widget;
    }

    void testDefaultFilePreview_clear_doesNotCrash()
    {
        DefaultFilePreview preview;
        QWidget *widget = preview.createPreviewWidget(nullptr);
        preview.clear();
        QVERIFY(true);
        delete widget;
    }
};

QTEST_MAIN(TestC64PreviewBase)
#include "test_c64previewbase.moc"
