#ifndef FILEDETAILSPANEL_H
#define FILEDETAILSPANEL_H

#include <QWidget>
#include <QStackedWidget>
#include <QTextBrowser>
#include <QLabel>
#include <QStyleHints>

#include "services/diskimagereader.h"
#include "services/sidfileparser.h"

class SonglengthsDatabase;
class HVSCMetadataService;

class FileDetailsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit FileDetailsPanel(QWidget *parent = nullptr);

    /**
     * @brief Sets the songlengths database for SID duration lookup.
     */
    void setSonglengthsDatabase(SonglengthsDatabase *database);

    /**
     * @brief Sets the HVSC metadata service for STIL/BUGlist lookup.
     */
    void setHVSCMetadataService(HVSCMetadataService *service);

    void showFileDetails(const QString &path, qint64 size, const QString &type);
    void showTextContent(const QString &content);
    void showDiskDirectory(const QByteArray &diskImageData, const QString &filename);
    void showSidDetails(const QByteArray &sidData, const QString &filename);
    void showLoading(const QString &path);
    void showError(const QString &message);
    void clear();

    bool isTextFile(const QString &path) const;
    bool isHtmlFile(const QString &path) const;
    bool isDiskImageFile(const QString &path) const;
    bool isSidFile(const QString &path) const;

signals:
    void contentRequested(const QString &path);

private slots:
    void onColorSchemeChanged(Qt::ColorScheme scheme);

private:
    void setupUi();
    void applyC64TextStyle();

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
    QTextBrowser *htmlBrowser_ = nullptr;

    // Loading/error states
    QLabel *statusLabel_ = nullptr;

    QString currentPath_;

    // Optional songlengths database (not owned)
    SonglengthsDatabase *songlengthsDatabase_ = nullptr;

    // Optional HVSC metadata service (not owned)
    HVSCMetadataService *hvscMetadataService_ = nullptr;
};

#endif // FILEDETAILSPANEL_H
