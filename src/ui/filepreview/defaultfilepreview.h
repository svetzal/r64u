/**
 * @file defaultfilepreview.h
 * @brief Default fallback preview strategy for unrecognized file types.
 *
 * Displays basic file information (name, size, type) for files
 * that don't have a specialized preview handler.
 */

#ifndef DEFAULTFILEPREVIEW_H
#define DEFAULTFILEPREVIEW_H

#include "filepreviewstrategy.h"
#include <QObject>
#include <QLabel>
#include <QVBoxLayout>

/**
 * @brief Default fallback preview strategy.
 *
 * Shows basic file metadata for files without specialized
 * preview support.
 */
class DefaultFilePreview : public QObject, public FilePreviewStrategy
{
    Q_OBJECT

public:
    DefaultFilePreview(QObject *parent = nullptr) : QObject(parent) {}
    ~DefaultFilePreview() override = default;

    [[nodiscard]] bool canHandle(const QString &path) const override;
    [[nodiscard]] QWidget* createPreviewWidget(QWidget *parent) override;
    void showPreview(const QString &path, const QByteArray &data) override;
    void showLoading(const QString &path) override;
    void showError(const QString &error) override;
    void clear() override;

    /**
     * @brief Sets the file details to display.
     * @param path The file path.
     * @param size The file size in bytes.
     * @param type The file type description.
     */
    void setFileDetails(const QString &path, qint64 size, const QString &type);

private:
    // UI widgets (created in createPreviewWidget)
    QWidget *previewWidget_ = nullptr;
    QLabel *fileNameLabel_ = nullptr;
    QLabel *fileSizeLabel_ = nullptr;
    QLabel *fileTypeLabel_ = nullptr;
    QLabel *statusLabel_ = nullptr;
};

#endif // DEFAULTFILEPREVIEW_H
