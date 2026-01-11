#ifndef VIEWPANEL_H
#define VIEWPANEL_H

#include <QWidget>
#include <QToolBar>
#include <QLabel>
#include <QButtonGroup>
#include <QRadioButton>

class DeviceConnection;
class VideoDisplayWidget;
class StreamingManager;

class ViewPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ViewPanel(DeviceConnection *connection, QWidget *parent = nullptr);
    ~ViewPanel() override;

    // Public API for MainWindow coordination
    void stopStreamingIfActive();

    // Settings
    void loadSettings();
    void saveSettings();
    int scalingMode() const;

signals:
    void statusMessage(const QString &message, int timeout = 0);

private slots:
    void onConnectionStateChanged();
    void onStartStreaming();
    void onStopStreaming();
    void onVideoFormatDetected(int format);
    void onStreamingStarted(const QString &targetHost);
    void onStreamingStopped();
    void onStreamingError(const QString &error);
    void onScalingModeChanged(int id);

private:
    void setupUi();
    void setupConnections();
    void updateActions();

    // Dependencies (not owned)
    DeviceConnection *deviceConnection_ = nullptr;

    // Owned streaming manager
    StreamingManager *streamingManager_ = nullptr;

    // UI widgets
    QToolBar *toolBar_ = nullptr;
    VideoDisplayWidget *videoDisplayWidget_ = nullptr;
    QAction *startStreamAction_ = nullptr;
    QAction *stopStreamAction_ = nullptr;
    QLabel *streamStatusLabel_ = nullptr;
    QButtonGroup *scalingModeGroup_ = nullptr;
    QRadioButton *sharpRadio_ = nullptr;
    QRadioButton *smoothRadio_ = nullptr;
    QRadioButton *integerRadio_ = nullptr;
};

#endif // VIEWPANEL_H
