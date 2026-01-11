#include "filepreviewfactory.h"
#include "textfilepreview.h"
#include "diskimagepreview.h"
#include "sidfilepreview.h"
#include "defaultfilepreview.h"

std::unique_ptr<FilePreviewStrategy> FilePreviewFactory::createStrategy(const QString &path)
{
    // Check strategies in priority order
    // Note: We use temporary instances to check canHandle()

    // Check disk image first (specific file types)
    auto diskStrategy = std::make_unique<DiskImagePreview>();
    if (diskStrategy->canHandle(path)) {
        return diskStrategy;
    }

    // Check SID files
    auto sidStrategy = std::make_unique<SidFilePreview>();
    if (sidStrategy->canHandle(path)) {
        return sidStrategy;
    }

    // Check text and HTML files
    auto textStrategy = std::make_unique<TextFilePreview>();
    if (textStrategy->canHandle(path)) {
        return textStrategy;
    }

    // Default fallback for all other files
    return std::make_unique<DefaultFilePreview>();
}
