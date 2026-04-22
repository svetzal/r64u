#ifndef C64PREVIEWBASE_H
#define C64PREVIEWBASE_H

#include "filepreviewstrategy.h"

#include <QLabel>
#include <QObject>
#include <QTextBrowser>
#include <QVBoxLayout>

/**
 * @brief Intermediate base class for C64-styled preview strategies.
 *
 * Implements the shared widget creation, C64 colour scheme styling,
 * clear(), and showError() methods common to all three C64-styled
 * preview types. Subclasses only need to implement canHandle(),
 * showPreview(), and showLoading().
 */
class C64PreviewBase : public QObject, public FilePreviewStrategy
{
    Q_OBJECT

public:
    explicit C64PreviewBase(QObject *parent = nullptr);
    ~C64PreviewBase() override = default;

    [[nodiscard]] QWidget *createPreviewWidget(QWidget *parent) override;
    void showError(const QString &error) override;
    void clear() override;

protected:
    /**
     * @brief Applies C64 Pro Mono font and colour scheme to textBrowser_.
     *
     * Uses the system colour scheme to select between dark-mode and
     * light-mode C64 palettes.
     */
    void applyC64TextStyle();

    /**
     * @brief Sets proportional line height on the entire document.
     * @param percentage Line height as a percentage (e.g. 150 = 150%).
     */
    void applyLineHeight(int percentage);

    // UI widgets created by createPreviewWidget() and shared with subclasses.
    QWidget *previewWidget_ = nullptr;
    QLabel *fileNameLabel_ = nullptr;
    QTextBrowser *textBrowser_ = nullptr;
};

#endif  // C64PREVIEWBASE_H
