#ifndef FILEDETAILSPANEL_H
#define FILEDETAILSPANEL_H

#include <QWidget>
#include <QStackedWidget>
#include <QTextBrowser>
#include <QWebEngineView>
#include <QLabel>

class FileDetailsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit FileDetailsPanel(QWidget *parent = nullptr);

    void showFileDetails(const QString &path, qint64 size, const QString &type);
    void showTextContent(const QString &content);
    void showLoading(const QString &path);
    void showError(const QString &message);
    void clear();

    bool isTextFile(const QString &path) const;
    bool isHtmlFile(const QString &path) const;

signals:
    void contentRequested(const QString &path);

private:
    void setupUi();

    QStackedWidget *stack_ = nullptr;
    QWidget *emptyPage_ = nullptr;
    QWidget *infoPage_ = nullptr;
    QWidget *textPage_ = nullptr;
    QWidget *htmlPage_ = nullptr;

    // Info page widgets
    QLabel *fileNameLabel_ = nullptr;
    QLabel *fileSizeLabel_ = nullptr;
    QLabel *fileTypeLabel_ = nullptr;

    // Text page widgets
    QLabel *textFileNameLabel_ = nullptr;
    QTextBrowser *textBrowser_ = nullptr;

    // HTML page widgets
    QWebEngineView *webView_ = nullptr;

    // Loading/error states
    QLabel *statusLabel_ = nullptr;

    QString currentPath_;
};

#endif // FILEDETAILSPANEL_H
