/**
 * @file textfilepreview.h
 * @brief Preview strategy for text and HTML files.
 *
 * Handles plain text files (.txt, .cfg, .log, .ini, .md, .json, .xml)
 * and HTML files (.html, .htm) with appropriate rendering.
 */

#ifndef TEXTFILEPREVIEW_H
#define TEXTFILEPREVIEW_H

#include "filepreviewstrategy.h"
#include <QObject>
#include <QTextBrowser>
#include <QLabel>
#include <QVBoxLayout>

/**
 * @brief Preview strategy for text and HTML files.
 *
 * Displays text files in a QTextBrowser with C64-style formatting
 * and HTML files with basic HTML rendering.
 */
class TextFilePreview : public QObject, public FilePreviewStrategy
{
    Q_OBJECT

public:
    TextFilePreview(QObject *parent = nullptr) : QObject(parent) {}
    ~TextFilePreview() override = default;

    [[nodiscard]] bool canHandle(const QString &path) const override;
    [[nodiscard]] QWidget* createPreviewWidget(QWidget *parent) override;
    void showPreview(const QString &path, const QByteArray &data) override;
    void showLoading(const QString &path) override;
    void showError(const QString &error) override;
    void clear() override;

private:
    [[nodiscard]] bool isTextFile(const QString &path) const;
    [[nodiscard]] bool isHtmlFile(const QString &path) const;
    void applyC64TextStyle();
    void applyLineHeight(int percentage);

    // UI widgets (created in createPreviewWidget)
    QWidget *previewWidget_ = nullptr;
    QLabel *fileNameLabel_ = nullptr;
    QTextBrowser *textBrowser_ = nullptr;
    bool isHtml_ = false;
};

#endif // TEXTFILEPREVIEW_H
