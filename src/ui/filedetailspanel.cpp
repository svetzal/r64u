#include "filedetailspanel.h"
#include "services/songlengthsdatabase.h"
#include "services/hvscmetadataservice.h"
#include "services/gamebase64service.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QFont>
#include <QGuiApplication>
#include <QTextBlockFormat>
#include <QTextCursor>

FileDetailsPanel::FileDetailsPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void FileDetailsPanel::setSonglengthsDatabase(SonglengthsDatabase *database)
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

    // Empty page (page 0) - shown when nothing is selected
    emptyPage_ = new QWidget();
    auto *emptyLayout = new QVBoxLayout(emptyPage_);
    auto *emptyLabel = new QLabel(tr("Select a file to view details"));
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLabel->setStyleSheet("color: gray;");
    emptyLayout->addWidget(emptyLabel);
    stack_->addWidget(emptyPage_);

    // Info page (page 1) - shown for non-text files
    infoPage_ = new QWidget();
    auto *infoLayout = new QVBoxLayout(infoPage_);
    infoLayout->setAlignment(Qt::AlignTop);

    fileNameLabel_ = new QLabel();
    fileNameLabel_->setWordWrap(true);
    QFont boldFont = fileNameLabel_->font();
    boldFont.setBold(true);
    boldFont.setPointSize(boldFont.pointSize() + 2);
    fileNameLabel_->setFont(boldFont);

    fileSizeLabel_ = new QLabel();
    fileTypeLabel_ = new QLabel();

    infoLayout->addWidget(fileNameLabel_);
    infoLayout->addSpacing(8);
    infoLayout->addWidget(fileSizeLabel_);
    infoLayout->addWidget(fileTypeLabel_);
    infoLayout->addStretch();

    statusLabel_ = new QLabel();
    statusLabel_->setAlignment(Qt::AlignCenter);
    statusLabel_->setStyleSheet("color: gray;");
    statusLabel_->hide();
    infoLayout->addWidget(statusLabel_);

    stack_->addWidget(infoPage_);

    // Text page (page 2) - shown for text files
    textPage_ = new QWidget();
    auto *textLayout = new QVBoxLayout(textPage_);
    textLayout->setContentsMargins(0, 0, 0, 0);

    textFileNameLabel_ = new QLabel();
    textFileNameLabel_->setFont(boldFont);
    textFileNameLabel_->setContentsMargins(0, 0, 0, 4);

    textBrowser_ = new QTextBrowser();
    textBrowser_->setReadOnly(true);
    textBrowser_->setOpenExternalLinks(false);
    textBrowser_->setOpenLinks(false);

    // Apply C64 styling
    applyC64TextStyle();

    // Connect to system theme changes
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
            this, &FileDetailsPanel::onColorSchemeChanged);

    textLayout->addWidget(textFileNameLabel_);
    textLayout->addWidget(textBrowser_);

    stack_->addWidget(textPage_);

    // HTML page (page 3) - shown for HTML files (basic HTML rendering)
    htmlPage_ = new QWidget();
    auto *htmlLayout = new QVBoxLayout(htmlPage_);
    htmlLayout->setContentsMargins(0, 0, 0, 0);

    htmlBrowser_ = new QTextBrowser();
    htmlBrowser_->setReadOnly(true);
    htmlBrowser_->setOpenExternalLinks(true);

    htmlLayout->addWidget(htmlBrowser_);

    stack_->addWidget(htmlPage_);

    mainLayout->addWidget(stack_);
}

void FileDetailsPanel::showFileDetails(const QString &path, qint64 size, const QString &type)
{
    currentPath_ = path;
    QFileInfo fi(path);
    QString fileName = fi.fileName();

    if (isHtmlFile(path)) {
        // Show HTML page with loading state
        htmlBrowser_->setHtml(tr("<p style='color:gray'>Loading...</p>"));
        stack_->setCurrentWidget(htmlPage_);
        emit contentRequested(path);
    } else if (isDiskImageFile(path)) {
        // Show text page with loading state for disk directory
        textFileNameLabel_->setText(fileName);
        textBrowser_->setPlainText(tr("Loading disk directory..."));
        stack_->setCurrentWidget(textPage_);
        emit contentRequested(path);
    } else if (isSidFile(path)) {
        // Show text page with loading state for SID details
        textFileNameLabel_->setText(fileName);
        textBrowser_->setPlainText(tr("Loading SID info..."));
        stack_->setCurrentWidget(textPage_);
        emit contentRequested(path);
    } else if (isTextFile(path)) {
        // Show text page with loading state
        textFileNameLabel_->setText(fileName);
        textBrowser_->setPlainText(tr("Loading..."));
        stack_->setCurrentWidget(textPage_);
        emit contentRequested(path);
    } else {
        // Show info page
        fileNameLabel_->setText(fileName);

        QString sizeStr;
        if (size < 1024) {
            sizeStr = tr("Size: %1 bytes").arg(size);
        } else if (size < 1024 * 1024) {
            sizeStr = tr("Size: %1 KB").arg(size / 1024.0, 0, 'f', 1);
        } else {
            sizeStr = tr("Size: %1 MB").arg(size / (1024.0 * 1024.0), 0, 'f', 2);
        }
        fileSizeLabel_->setText(sizeStr);
        fileTypeLabel_->setText(tr("Type: %1").arg(type));
        statusLabel_->hide();

        stack_->setCurrentWidget(infoPage_);
    }
}

void FileDetailsPanel::showTextContent(const QString &content)
{
    if (isHtmlFile(currentPath_)) {
        htmlBrowser_->setHtml(content);
    } else {
        textBrowser_->setPlainText(content);

        // Apply line height after content is set (must be done after setPlainText)
        QTextBlockFormat blockFormat;
        blockFormat.setLineHeight(150, QTextBlockFormat::ProportionalHeight);
        QTextCursor cursor = textBrowser_->textCursor();
        cursor.select(QTextCursor::Document);
        cursor.mergeBlockFormat(blockFormat);
    }
}

void FileDetailsPanel::showLoading(const QString &path)
{
    currentPath_ = path;
    QFileInfo fi(path);
    textFileNameLabel_->setText(fi.fileName());
    textBrowser_->setPlainText(tr("Loading..."));
    stack_->setCurrentWidget(textPage_);
}

void FileDetailsPanel::showError(const QString &message)
{
    if (stack_->currentWidget() == textPage_) {
        textBrowser_->setPlainText(tr("Error: %1").arg(message));
    } else if (stack_->currentWidget() == htmlPage_) {
        htmlBrowser_->setHtml(tr("<p style='color:red'>Error: %1</p>").arg(message));
    } else {
        statusLabel_->setText(tr("Error: %1").arg(message));
        statusLabel_->show();
    }
}

void FileDetailsPanel::clear()
{
    currentPath_.clear();
    textBrowser_->clear();
    stack_->setCurrentWidget(emptyPage_);
}

bool FileDetailsPanel::isTextFile(const QString &path) const
{
    QString lower = path.toLower();
    return lower.endsWith(".cfg") ||
           lower.endsWith(".txt") ||
           lower.endsWith(".log") ||
           lower.endsWith(".ini") ||
           lower.endsWith(".md") ||
           lower.endsWith(".json") ||
           lower.endsWith(".xml");
}

bool FileDetailsPanel::isHtmlFile(const QString &path) const
{
    QString lower = path.toLower();
    return lower.endsWith(".html") || lower.endsWith(".htm");
}

bool FileDetailsPanel::isDiskImageFile(const QString &path) const
{
    return DiskImageReader::isDiskImage(path);
}

bool FileDetailsPanel::isSidFile(const QString &path) const
{
    return SidFileParser::isSidFile(path);
}

void FileDetailsPanel::showDiskDirectory(const QByteArray &diskImageData, const QString &filename)
{
    DiskImageReader reader;
    DiskImageReader::DiskDirectory dir = reader.parse(diskImageData, filename);

    if (dir.format == DiskImageReader::Format::Unknown) {
        showError(tr("Unable to parse disk image"));
        return;
    }

    QString listing = DiskImageReader::formatDirectoryListing(dir);

    // Look up game info from GameBase64
    if (gameBase64Service_ != nullptr && gameBase64Service_->isLoaded()) {
        GameBase64Service::GameInfo gameInfo = gameBase64Service_->lookupByFilename(filename);
        if (gameInfo.found) {
            listing += "\n\n";
            listing += tr("════════════════════════════════════════\n");
            listing += tr("GAMEBASE64 INFO:\n");
            listing += tr("────────────────────────────────────────\n");
            listing += tr("  Game: %1\n").arg(gameInfo.name);
            if (gameInfo.year > 0) {
                listing += tr("  Year: %1\n").arg(gameInfo.year);
            }
            if (!gameInfo.publisher.isEmpty()) {
                listing += tr("  Publisher: %1\n").arg(gameInfo.publisher);
            }
            if (!gameInfo.genre.isEmpty()) {
                QString genre = gameInfo.genre;
                if (!gameInfo.parentGenre.isEmpty() && gameInfo.parentGenre != gameInfo.genre) {
                    genre = gameInfo.parentGenre + " / " + gameInfo.genre;
                }
                listing += tr("  Genre: %1\n").arg(genre);
            }
            if (!gameInfo.musician.isEmpty()) {
                QString musician = gameInfo.musician;
                if (!gameInfo.musicianGroup.isEmpty()) {
                    musician += " (" + gameInfo.musicianGroup + ")";
                }
                listing += tr("  Musician: %1\n").arg(musician);
            }
            if (gameInfo.rating > 0) {
                listing += tr("  Rating: %1/10\n").arg(gameInfo.rating);
            }
            if (gameInfo.playersFrom > 0) {
                if (gameInfo.playersTo > gameInfo.playersFrom) {
                    listing += tr("  Players: %1-%2\n").arg(gameInfo.playersFrom).arg(gameInfo.playersTo);
                } else {
                    listing += tr("  Players: %1\n").arg(gameInfo.playersFrom);
                }
            }
            if (!gameInfo.memo.isEmpty()) {
                listing += tr("\n  %1\n").arg(gameInfo.memo);
            }
        }
    }

    QFileInfo fi(filename);
    textFileNameLabel_->setText(fi.fileName());
    textBrowser_->setPlainText(listing);

    // Note: No extra line height for disk directories - PETSCII graphics
    // require characters to touch vertically with no gaps

    stack_->setCurrentWidget(textPage_);
}

void FileDetailsPanel::showSidDetails(const QByteArray &sidData, const QString &filename)
{
    SidFileParser::SidInfo info = SidFileParser::parse(sidData);

    if (!info.valid) {
        showError(tr("Unable to parse SID file"));
        return;
    }

    QString details = SidFileParser::formatForDisplay(info);
    QString hvscPath;

    // Look up song lengths from database
    if (songlengthsDatabase_ != nullptr && songlengthsDatabase_->isLoaded()) {
        SonglengthsDatabase::SongLengths lengths = songlengthsDatabase_->lookupByData(sidData);

        details += "\n";
        if (lengths.found) {
            hvscPath = lengths.hvscPath;
            details += tr("─────────────────────────────────\n");
            details += tr("HVSC Database: Found\n");
            details += tr("Song Lengths:\n");

            for (int i = 0; i < lengths.formattedTimes.size(); ++i) {
                details += tr("  Song %1: %2\n")
                    .arg(i + 1)
                    .arg(lengths.formattedTimes.at(i));
            }
        } else {
            details += tr("─────────────────────────────────\n");
            details += tr("HVSC Database: Not found\n");
            details += tr("(Using default 3:00 duration)\n");
        }
    } else if (songlengthsDatabase_ != nullptr && !songlengthsDatabase_->isLoaded()) {
        details += "\n";
        details += tr("─────────────────────────────────\n");
        details += tr("HVSC Database: Not loaded\n");
    }

    // Look up STIL and BUGlist metadata if we have the HVSC path
    if (hvscMetadataService_ != nullptr && !hvscPath.isEmpty()) {
        // Check for bug reports first (important warnings)
        if (hvscMetadataService_->isBuglistLoaded()) {
            HVSCMetadataService::BugInfo bugInfo = hvscMetadataService_->lookupBuglist(hvscPath);
            if (bugInfo.found && !bugInfo.entries.isEmpty()) {
                details += tr("\n─────────────────────────────────\n");
                details += tr("⚠ KNOWN ISSUES:\n");
                for (const HVSCMetadataService::BugEntry &bug : bugInfo.entries) {
                    if (bug.subtune > 0) {
                        details += tr("  Song #%1: %2\n").arg(bug.subtune).arg(bug.description);
                    } else {
                        details += tr("  %1\n").arg(bug.description);
                    }
                }
            }
        }

        // Show STIL commentary and cover info
        if (hvscMetadataService_->isStilLoaded()) {
            HVSCMetadataService::StilInfo stilInfo = hvscMetadataService_->lookupStil(hvscPath);
            if (stilInfo.found && !stilInfo.entries.isEmpty()) {
                details += tr("\n─────────────────────────────────\n");
                details += tr("STIL INFORMATION:\n");

                for (const HVSCMetadataService::SubtuneEntry &entry : stilInfo.entries) {
                    // Show subtune header if specific to a song
                    if (entry.subtune > 0) {
                        details += tr("\n  Song #%1:\n").arg(entry.subtune);
                    }

                    // Show tune name if different from header
                    if (!entry.name.isEmpty()) {
                        details += tr("  Name: %1\n").arg(entry.name);
                    }

                    // Show author if different from header
                    if (!entry.author.isEmpty()) {
                        details += tr("  Author: %1\n").arg(entry.author);
                    }

                    // Show cover information
                    for (const HVSCMetadataService::CoverInfo &cover : entry.covers) {
                        QString coverLine = tr("  Cover: %1").arg(cover.title);
                        if (!cover.artist.isEmpty()) {
                            coverLine += tr(" by %1").arg(cover.artist);
                        }
                        if (!cover.timestamp.isEmpty()) {
                            coverLine += tr(" (%1)").arg(cover.timestamp);
                        }
                        details += coverLine + "\n";
                    }

                    // Show commentary
                    if (!entry.comment.isEmpty()) {
                        // Word-wrap long comments
                        QString comment = entry.comment;
                        if (entry.subtune > 0 || !entry.name.isEmpty()) {
                            details += tr("  Comment: %1\n").arg(comment);
                        } else {
                            details += tr("  %1\n").arg(comment);
                        }
                    }
                }
            }
        }
    }

    // Look up game info from GameBase64 by SID filename
    if (gameBase64Service_ != nullptr && gameBase64Service_->isLoaded()) {
        GameBase64Service::GameInfo gameInfo = gameBase64Service_->lookupBySidFilename(filename);
        if (gameInfo.found) {
            details += tr("\n─────────────────────────────────\n");
            details += tr("GAME INFO (GameBase64):\n");
            details += tr("  Game: %1\n").arg(gameInfo.name);
            if (gameInfo.year > 0) {
                details += tr("  Year: %1\n").arg(gameInfo.year);
            }
            if (!gameInfo.publisher.isEmpty()) {
                details += tr("  Publisher: %1\n").arg(gameInfo.publisher);
            }
            if (!gameInfo.genre.isEmpty()) {
                QString genre = gameInfo.genre;
                if (!gameInfo.parentGenre.isEmpty() && gameInfo.parentGenre != gameInfo.genre) {
                    genre = gameInfo.parentGenre + " / " + gameInfo.genre;
                }
                details += tr("  Genre: %1\n").arg(genre);
            }
        }
    }

    QFileInfo fi(filename);
    textFileNameLabel_->setText(fi.fileName());
    textBrowser_->setPlainText(details);

    // Apply line height for better readability
    QTextBlockFormat blockFormat;
    blockFormat.setLineHeight(140, QTextBlockFormat::ProportionalHeight);
    QTextCursor cursor = textBrowser_->textCursor();
    cursor.select(QTextCursor::Document);
    cursor.mergeBlockFormat(blockFormat);

    stack_->setCurrentWidget(textPage_);
}

void FileDetailsPanel::applyC64TextStyle()
{
    // Set C64 Pro Mono font
    QFont c64Font("C64 Pro Mono");
    c64Font.setStyleHint(QFont::Monospace);
    c64Font.setPointSize(12);
    textBrowser_->setFont(c64Font);

    // Determine color scheme based on system theme
    Qt::ColorScheme scheme = QGuiApplication::styleHints()->colorScheme();
    bool isDarkMode = (scheme == Qt::ColorScheme::Dark);

    // C64 color constants
    const QString c64Blue = "#4040E8";
    const QString c64LightBlue = "#887ECB";

    QString stylesheet;
    if (isDarkMode) {
        // Dark mode: blue text on black background
        stylesheet = QString(
            "QTextBrowser {"
            "  background-color: #000000;"
            "  color: %1;"
            "  border: 1px solid %1;"
            "  padding: 8px;"
            "}"
        ).arg(c64LightBlue);
    } else {
        // Light mode: white text on blue background (classic C64 look)
        stylesheet = QString(
            "QTextBrowser {"
            "  background-color: %1;"
            "  color: #FFFFFF;"
            "  border: 1px solid #2020A8;"
            "  padding: 8px;"
            "}"
        ).arg(c64Blue);
    }

    textBrowser_->setStyleSheet(stylesheet);
}

void FileDetailsPanel::onColorSchemeChanged(Qt::ColorScheme /*scheme*/)
{
    applyC64TextStyle();
}
