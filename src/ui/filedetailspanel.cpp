#include "filedetailspanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QFont>

FileDetailsPanel::FileDetailsPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
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
    QFont monoFont("Menlo, Monaco, Consolas, monospace");
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setPointSize(11);
    textBrowser_->setFont(monoFont);

    textLayout->addWidget(textFileNameLabel_);
    textLayout->addWidget(textBrowser_);

    stack_->addWidget(textPage_);

    // HTML page (page 3) - shown for HTML files with JavaScript support
    htmlPage_ = new QWidget();
    auto *htmlLayout = new QVBoxLayout(htmlPage_);
    htmlLayout->setContentsMargins(0, 0, 0, 0);

    webView_ = new QWebEngineView();

    htmlLayout->addWidget(webView_);

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
        webView_->setHtml(tr("<p style='color:gray'>Loading...</p>"));
        stack_->setCurrentWidget(htmlPage_);
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
        webView_->setHtml(content);
    } else {
        textBrowser_->setPlainText(content);
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
        webView_->setHtml(tr("<p style='color:red'>Error: %1</p>").arg(message));
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
