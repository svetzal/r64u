/**
 * @file diskimagepreview.h
 * @brief Preview strategy for C64 disk image files.
 *
 * Handles disk image files (.d64, .g64, .d71, .d81) and displays
 * the disk directory with PETSCII graphics.
 */

#ifndef DISKIMAGEPREVIEW_H
#define DISKIMAGEPREVIEW_H

#include "filepreviewstrategy.h"
#include "services/diskimagereader.h"
#include <QObject>
#include <QTextBrowser>
#include <QLabel>
#include <QVBoxLayout>

/**
 * @brief Preview strategy for disk image files.
 *
 * Parses disk images and displays the directory listing
 * with C64-style PETSCII graphics.
 */
class DiskImagePreview : public QObject, public FilePreviewStrategy
{
    Q_OBJECT

public:
    DiskImagePreview(QObject *parent = nullptr) : QObject(parent) {}
    ~DiskImagePreview() override = default;

    [[nodiscard]] bool canHandle(const QString &path) const override;
    [[nodiscard]] QWidget* createPreviewWidget(QWidget *parent) override;
    void showPreview(const QString &path, const QByteArray &data) override;
    void showLoading(const QString &path) override;
    void showError(const QString &error) override;
    void clear() override;

private:
    void applyC64TextStyle();

    // UI widgets (created in createPreviewWidget)
    QWidget *previewWidget_ = nullptr;
    QLabel *fileNameLabel_ = nullptr;
    QTextBrowser *textBrowser_ = nullptr;
};

#endif // DISKIMAGEPREVIEW_H
