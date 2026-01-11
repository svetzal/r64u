/**
 * @file sidfilepreview.h
 * @brief Preview strategy for C64 SID music files.
 *
 * Handles SID files (.sid) and displays metadata including title,
 * author, copyright, and technical information.
 */

#ifndef SIDFILEPREVIEW_H
#define SIDFILEPREVIEW_H

#include "filepreviewstrategy.h"
#include "services/sidfileparser.h"
#include <QObject>
#include <QTextBrowser>
#include <QLabel>
#include <QVBoxLayout>

/**
 * @brief Preview strategy for SID music files.
 *
 * Parses SID files and displays metadata in a formatted view.
 */
class SidFilePreview : public QObject, public FilePreviewStrategy
{
    Q_OBJECT

public:
    SidFilePreview(QObject *parent = nullptr) : QObject(parent) {}
    ~SidFilePreview() override = default;

    [[nodiscard]] bool canHandle(const QString &path) const override;
    [[nodiscard]] QWidget* createPreviewWidget(QWidget *parent) override;
    void showPreview(const QString &path, const QByteArray &data) override;
    void showLoading(const QString &path) override;
    void showError(const QString &error) override;
    void clear() override;

private:
    void applyC64TextStyle();
    void applyLineHeight(int percentage);

    // UI widgets (created in createPreviewWidget)
    QWidget *previewWidget_ = nullptr;
    QLabel *fileNameLabel_ = nullptr;
    QTextBrowser *textBrowser_ = nullptr;
};

#endif // SIDFILEPREVIEW_H
