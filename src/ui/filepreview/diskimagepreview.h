#ifndef DISKIMAGEPREVIEW_H
#define DISKIMAGEPREVIEW_H

#include "c64previewbase.h"

/**
 * @brief Preview strategy for disk image files.
 *
 * Parses disk images and displays the directory listing
 * with C64-style PETSCII graphics.
 */
class DiskImagePreview : public C64PreviewBase
{
    Q_OBJECT

public:
    explicit DiskImagePreview(QObject *parent = nullptr) : C64PreviewBase(parent) {}
    ~DiskImagePreview() override = default;

    [[nodiscard]] bool canHandle(const QString &path) const override;
    void showPreview(const QString &path, const QByteArray &data) override;
    void showLoading(const QString &path) override;
};

#endif  // DISKIMAGEPREVIEW_H
