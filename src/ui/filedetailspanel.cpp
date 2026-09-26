#include "filedetailspanel.h"

#include "c64screenwidget.h"

#include "core/diskimagereader.h"
#include "core/fileactioncore.h"
#include "core/filemetadatacore.h"
#include "services/gamebase64service.h"
#include "services/hvscmetadataservice.h"
#include "services/songlengthsdatabaseservice.h"

#include <QFileInfo>
#include <QFont>
#include <QVBoxLayout>

namespace {

/// Characters `LOAD"` and `",8,1` take on the 40-column LOAD line.
constexpr int LoadLineOverhead = 10;

QString humanSize(qint64 size)
{
    if (size < 1024) {
        return QObject::tr("%1 bytes").arg(size);
    }
    if (size < qint64{1024} * 1024) {
        return QObject::tr("%1 KB").arg(size / 1024.0, 0, 'f', 1);
    }
    return QObject::tr("%1 MB").arg(size / (1024.0 * 1024.0), 0, 'f', 2);
}

/// `LOAD"<name>",8,1`, with the name cut so the line fits the screen width.
QString loadLine(const QString &fileName, int columns)
{
    const QString name = fileName.left(std::max(0, columns - LoadLineOverhead));
    return QStringLiteral("LOAD\"%1\",8,1").arg(name);
}

QStringList emptyStateLines()
{
    return {QStringLiteral("    **** R64U FILE DETAILS ****"), QString(),
            QObject::tr("SELECT A FILE TO VIEW ITS DETAILS."), QString(), QStringLiteral("READY.")};
}

QString errorLine(const QString &message)
{
    return QStringLiteral("?%1  ERROR").arg(message);
}

}  // namespace

FileDetailsPanel::FileDetailsPanel(QWidget *parent) : QWidget(parent)
{
    setupUi();
}

void FileDetailsPanel::setSonglengthsDatabase(SonglengthsDatabaseService *database)
{
    songlengthsDatabase_ = database;
}

void FileDetailsPanel::setHVSCMetadataService(HVSCMetadataService *service)
{
    hvscMetadataService_ = service;
}

void FileDetailsPanel::setGameBase64Service(GameBase64Service *service)
{
    gameBase64Service_ = service;
}

void FileDetailsPanel::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    stack_ = new QStackedWidget();

    // Screen page (page 0): file name above a C64 screen
    screenPage_ = new QWidget();
    auto *screenLayout = new QVBoxLayout(screenPage_);
    screenLayout->setContentsMargins(0, 0, 0, 0);

    fileNameLabel_ = new QLabel();
    fileNameLabel_->setWordWrap(true);
    QFont boldFont = fileNameLabel_->font();
    boldFont.setBold(true);
    boldFont.setPointSize(boldFont.pointSize() + 2);
    fileNameLabel_->setFont(boldFont);
    fileNameLabel_->setContentsMargins(0, 0, 0, 4);

    screen_ = new C64ScreenWidget();

    screenLayout->addWidget(fileNameLabel_);
    screenLayout->addWidget(screen_, 1);
    stack_->addWidget(screenPage_);

    // HTML page (page 1): basic HTML rendering
    htmlPage_ = new QWidget();
    auto *htmlLayout = new QVBoxLayout(htmlPage_);
    htmlLayout->setContentsMargins(0, 0, 0, 0);

    htmlBrowser_ = new QTextBrowser();
    htmlBrowser_->setReadOnly(true);
    htmlBrowser_->setOpenExternalLinks(true);

    htmlLayout->addWidget(htmlBrowser_);
    stack_->addWidget(htmlPage_);

    mainLayout->addWidget(stack_);

    showScreenLines(QString(), emptyStateLines());
}

void FileDetailsPanel::showScreenText(const QString &fileName, const QString &text)
{
    fileNameLabel_->setText(fileName);
    screen_->setText(text);
    stack_->setCurrentWidget(screenPage_);
}

void FileDetailsPanel::showScreenLines(const QString &fileName, const QStringList &lines)
{
    fileNameLabel_->setText(fileName);
    screen_->setLines(lines);
    stack_->setCurrentWidget(screenPage_);
}

void FileDetailsPanel::showFileDetails(const QString &path, qint64 size, const QString &type)
{
    currentPath_ = path;
    const QString fileName = QFileInfo(path).fileName();

    switch (fileaction::detectPreviewType(path)) {
    case fileaction::PreviewContentType::Html:
        htmlBrowser_->setHtml(tr("<p style='color:gray'>Loading...</p>"));
        stack_->setCurrentWidget(htmlPage_);
        emit contentRequested(path);
        return;
    case fileaction::PreviewContentType::DiskImage:
        showScreenText(fileName, C64ScreenWidget::petsciiUpper(tr("Loading disk directory...")));
        emit contentRequested(path);
        return;
    case fileaction::PreviewContentType::SidMusic:
        showScreenText(fileName, C64ScreenWidget::petsciiUpper(tr("Loading SID info...")));
        emit contentRequested(path);
        return;
    case fileaction::PreviewContentType::Text:
        showScreenText(fileName, C64ScreenWidget::petsciiUpper(tr("Loading...")));
        emit contentRequested(path);
        return;
    default:
        break;
    }

    // Info card — no content fetch needed
    const QStringList card{loadLine(fileName, screen_->columns()),
                           QString(),
                           tr("SIZE: %1").arg(humanSize(size)),
                           tr("TYPE: %1").arg(type),
                           QString(),
                           QStringLiteral("READY.")};
    QStringList lines;
    for (const QString &line : card) {
        lines.append(C64ScreenWidget::petsciiUpper(line));
    }
    showScreenLines(fileName, lines);
}

void FileDetailsPanel::showTextContent(const QString &content)
{
    if (isHtmlFile(currentPath_)) {
        htmlBrowser_->setHtml(content);
    } else {
        screen_->setText(C64ScreenWidget::petsciiUpper(content));
    }
}

void FileDetailsPanel::showLoading(const QString &path)
{
    currentPath_ = path;
    showScreenText(QFileInfo(path).fileName(), C64ScreenWidget::petsciiUpper(tr("Loading...")));
}

void FileDetailsPanel::showError(const QString &message)
{
    if (stack_->currentWidget() == htmlPage_) {
        htmlBrowser_->setHtml(tr("<p style='color:red'>Error: %1</p>").arg(message));
        return;
    }
    screen_->setText(C64ScreenWidget::petsciiUpper(errorLine(message)));
}

void FileDetailsPanel::clear()
{
    currentPath_.clear();
    showScreenLines(QString(), emptyStateLines());
}

bool FileDetailsPanel::isTextFile(const QString &path) const
{
    return fileaction::detectPreviewType(path) == fileaction::PreviewContentType::Text;
}

bool FileDetailsPanel::isHtmlFile(const QString &path) const
{
    return fileaction::detectPreviewType(path) == fileaction::PreviewContentType::Html;
}

bool FileDetailsPanel::isDiskImageFile(const QString &path) const
{
    return fileaction::detectPreviewType(path) == fileaction::PreviewContentType::DiskImage;
}

bool FileDetailsPanel::isSidFile(const QString &path) const
{
    return fileaction::detectPreviewType(path) == fileaction::PreviewContentType::SidMusic;
}

void FileDetailsPanel::showDiskDirectory(const QByteArray &diskImageData, const QString &filename)
{
    DiskImageReader::DiskDirectory dir = DiskImageReader::parse(diskImageData, filename);

    if (dir.format == DiskImageReader::Format::Unknown) {
        showError(tr("Unable to parse disk image"));
        return;
    }

    // Build display context for the pure formatter
    filemetadata::DiskDisplayContext ctx;
    ctx.directoryListing = DiskImageReader::formatDirectoryListing(dir);
    ctx.gameBase64ServicePresent = (gameBase64Service_ != nullptr);
    ctx.gameBase64DatabaseLoaded =
        (gameBase64Service_ != nullptr) && gameBase64Service_->isLoaded();
    if (ctx.gameBase64DatabaseLoaded) {
        ctx.gameInfo = gameBase64Service_->lookupByFilename(filename);
    }

    // The PETSCII listing is already screen-exact; only the enrichment that
    // follows it is folded into the C64's uppercase set.
    QString details = filemetadata::formatDiskDetails(ctx);
    if (details.startsWith(ctx.directoryListing)) {
        details = ctx.directoryListing +
                  C64ScreenWidget::petsciiUpper(details.mid(ctx.directoryListing.size()));
    }

    showScreenText(QFileInfo(filename).fileName(), details);
}

void FileDetailsPanel::showSidDetails(const QByteArray &sidData, const QString &filename)
{
    SidFileParser::SidInfo info = SidFileParser::parse(sidData);

    if (!info.valid) {
        showError(tr("Unable to parse SID file"));
        return;
    }

    // Build display context for the pure formatter
    filemetadata::SidDisplayContext ctx;
    ctx.sidInfo = info;

    // Songlengths
    ctx.songlengthsServicePresent = (songlengthsDatabase_ != nullptr);
    ctx.songlengthsDatabaseLoaded =
        (songlengthsDatabase_ != nullptr) && songlengthsDatabase_->isLoaded();
    if (ctx.songlengthsDatabaseLoaded) {
        ctx.songLengths = songlengthsDatabase_->lookupByData(sidData);
    }

    // HVSC metadata (STIL + BUGlist) — only accessible if we have an HVSC path
    if (hvscMetadataService_ != nullptr && ctx.songLengths.found) {
        const QString &hvscPath = ctx.songLengths.hvscPath;
        ctx.stilLoaded = hvscMetadataService_->isStilLoaded();
        ctx.buglistLoaded = hvscMetadataService_->isBuglistLoaded();
        if (ctx.buglistLoaded) {
            ctx.bugInfo = hvscMetadataService_->lookupBuglist(hvscPath);
        }
        if (ctx.stilLoaded) {
            ctx.stilInfo = hvscMetadataService_->lookupStil(hvscPath);
        }
    }

    // GameBase64
    ctx.gameBase64ServicePresent = (gameBase64Service_ != nullptr);
    ctx.gameBase64DatabaseLoaded =
        (gameBase64Service_ != nullptr) && gameBase64Service_->isLoaded();
    if (ctx.gameBase64DatabaseLoaded) {
        ctx.gameInfo = gameBase64Service_->lookupBySidFilename(filename);
    }

    showScreenText(QFileInfo(filename).fileName(),
                   C64ScreenWidget::petsciiUpper(filemetadata::formatSidDetails(ctx)));
}
