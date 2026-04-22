#ifndef SIDFILEPREVIEW_H
#define SIDFILEPREVIEW_H

#include "c64previewbase.h"

/**
 * @brief Preview strategy for SID music files.
 *
 * Parses SID files and displays metadata in a formatted view.
 */
class SidFilePreview : public C64PreviewBase
{
    Q_OBJECT

public:
    explicit SidFilePreview(QObject *parent = nullptr) : C64PreviewBase(parent) {}
    ~SidFilePreview() override = default;

    [[nodiscard]] bool canHandle(const QString &path) const override;
    void showPreview(const QString &path, const QByteArray &data) override;
    void showLoading(const QString &path) override;
};

#endif  // SIDFILEPREVIEW_H
