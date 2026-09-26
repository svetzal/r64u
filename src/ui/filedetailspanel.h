#ifndef FILEDETAILSPANEL_H
#define FILEDETAILSPANEL_H

#include "idetailsdisplay.h"

#include <QLabel>
#include <QStackedWidget>
#include <QTextBrowser>
#include <QWidget>

class C64ScreenWidget;
class SonglengthsDatabaseService;
class HVSCMetadataService;
class GameBase64Service;

/**
 * @brief Shows the selected file the way a C64 would: on a C64 screen.
 *
 * Every state except HTML pages (which keep a QTextBrowser) is a set of
 * screen lines on one C64ScreenWidget: the empty prompt, the BASIC-style
 * info card for programs, text files, disk directories, SID details,
 * loading notices and errors.
 */
class FileDetailsPanel : public QWidget, public IDetailsDisplay
{
    Q_OBJECT

public:
    explicit FileDetailsPanel(QWidget *parent = nullptr);

    /**
     * @brief Sets the songlengths database for SID duration lookup.
     */
    void setSonglengthsDatabase(SonglengthsDatabaseService *database);

    /**
     * @brief Sets the HVSC metadata service for STIL/BUGlist lookup.
     */
    void setHVSCMetadataService(HVSCMetadataService *service);

    /**
     * @brief Sets the GameBase64 service for game metadata lookup.
     */
    void setGameBase64Service(GameBase64Service *service);

    void clear() override;
    void showFileDetails(const QString &path, qint64 size, const QString &type) override;
    void showTextContent(const QString &content) override;
    void showDiskDirectory(const QByteArray &diskImageData, const QString &filename) override;
    void showSidDetails(const QByteArray &sidData, const QString &filename) override;
    void showLoading(const QString &path);
    void showError(const QString &message) override;

    // File type detection — delegates to fileaction::detectPreviewType()
    bool isTextFile(const QString &path) const;
    bool isHtmlFile(const QString &path) const;
    bool isDiskImageFile(const QString &path) const;
    bool isSidFile(const QString &path) const;

signals:
    void contentRequested(const QString &path);

private:
    void setupUi();
    void showScreenText(const QString &fileName, const QString &text);
    void showScreenLines(const QString &fileName, const QStringList &lines);

    QStackedWidget *stack_ = nullptr;
    QWidget *screenPage_ = nullptr;
    QWidget *htmlPage_ = nullptr;

    // Screen page widgets
    QLabel *fileNameLabel_ = nullptr;
    C64ScreenWidget *screen_ = nullptr;

    // HTML page widgets
    QTextBrowser *htmlBrowser_ = nullptr;

    QString currentPath_;

    // Optional songlengths database (not owned)
    SonglengthsDatabaseService *songlengthsDatabase_ = nullptr;

    // Optional HVSC metadata service (not owned)
    HVSCMetadataService *hvscMetadataService_ = nullptr;

    // Optional GameBase64 service (not owned)
    GameBase64Service *gameBase64Service_ = nullptr;
};

#endif  // FILEDETAILSPANEL_H
