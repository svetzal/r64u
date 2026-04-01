/**
 * @file textfilepreview.h
 * @brief Preview strategy for text and HTML files.
 *
 * Handles plain text files (.txt, .cfg, .log, .ini, .md, .json, .xml)
 * and HTML files (.html, .htm) with appropriate rendering.
 */

#ifndef TEXTFILEPREVIEW_H
#define TEXTFILEPREVIEW_H

#include "c64previewbase.h"

/**
 * @brief Preview strategy for text and HTML files.
 *
 * Displays text files in a QTextBrowser with C64-style formatting
 * and HTML files with basic HTML rendering.
 */
class TextFilePreview : public C64PreviewBase
{
    Q_OBJECT

public:
    explicit TextFilePreview(QObject *parent = nullptr) : C64PreviewBase(parent) {}
    ~TextFilePreview() override = default;

    [[nodiscard]] bool canHandle(const QString &path) const override;
    void showPreview(const QString &path, const QByteArray &data) override;
    void showLoading(const QString &path) override;
    void showError(const QString &error) override;

private:
    bool isHtml_ = false;
};

#endif  // TEXTFILEPREVIEW_H
