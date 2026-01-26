/**
 * @file streamingdiagnosticswidget.cpp
 * @brief Implementation of the streaming diagnostics widget.
 */

#include "streamingdiagnosticswidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QPainter>

StreamingDiagnosticsWidget::StreamingDiagnosticsWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void StreamingDiagnosticsWidget::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 2, 4, 2);
    mainLayout->setSpacing(0);

    // === Compact mode frame ===
    compactFrame_ = new QFrame();
    compactFrame_->setFrameShape(QFrame::NoFrame);
    auto *compactLayout = new QHBoxLayout(compactFrame_);
    compactLayout->setContentsMargins(0, 0, 0, 0);
    compactLayout->setSpacing(8);

    // Quality indicator dot
    qualityDot_ = new QWidget();
    qualityDot_->setFixedSize(10, 10);
    qualityDot_->setStyleSheet("background-color: #808080; border-radius: 5px;");
    compactLayout->addWidget(qualityDot_);

    // Summary label
    summaryLabel_ = new QLabel(tr("--"));
    summaryLabel_->setStyleSheet("font-family: monospace;");
    compactLayout->addWidget(summaryLabel_);

    compactLayout->addStretch();

    // Expand button
    expandButton_ = new QPushButton(tr("Details"));
    expandButton_->setFlat(true);
    expandButton_->setMaximumHeight(20);
    connect(expandButton_, &QPushButton::clicked, this, &StreamingDiagnosticsWidget::toggleDisplayMode);
    compactLayout->addWidget(expandButton_);

    mainLayout->addWidget(compactFrame_);

    // === Detailed mode frame ===
    detailedFrame_ = new QFrame();
    detailedFrame_->setFrameShape(QFrame::StyledPanel);
    detailedFrame_->setVisible(false);
    auto *detailedLayout = new QVBoxLayout(detailedFrame_);
    detailedLayout->setContentsMargins(8, 8, 8, 8);
    detailedLayout->setSpacing(4);

    // Header with quality and uptime
    auto *headerLayout = new QHBoxLayout();
    qualityLabel_ = new QLabel(tr("Quality: --"));
    qualityLabel_->setStyleSheet("font-weight: bold;");
    headerLayout->addWidget(qualityLabel_);
    headerLayout->addStretch();
    uptimeLabel_ = new QLabel(tr("Uptime: 0:00"));
    headerLayout->addWidget(uptimeLabel_);
    detailedLayout->addLayout(headerLayout);

    // Separator
    auto *separator = new QFrame();
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    detailedLayout->addWidget(separator);

    // Metrics grid
    auto *metricsLayout = new QGridLayout();
    metricsLayout->setSpacing(4);

    // Video section header
    auto *videoHeader = new QLabel(tr("Video"));
    videoHeader->setStyleSheet("font-weight: bold;");
    metricsLayout->addWidget(videoHeader, 0, 0, 1, 2);

    // Audio section header
    auto *audioHeader = new QLabel(tr("Audio"));
    audioHeader->setStyleSheet("font-weight: bold;");
    metricsLayout->addWidget(audioHeader, 0, 2, 1, 2);

    // Video metrics
    metricsLayout->addWidget(new QLabel(tr("Packets:")), 1, 0);
    videoPacketsLabel_ = new QLabel("0");
    videoPacketsLabel_->setStyleSheet("font-family: monospace;");
    metricsLayout->addWidget(videoPacketsLabel_, 1, 1);

    metricsLayout->addWidget(new QLabel(tr("Loss:")), 2, 0);
    videoLossLabel_ = new QLabel("0.00%");
    videoLossLabel_->setStyleSheet("font-family: monospace;");
    metricsLayout->addWidget(videoLossLabel_, 2, 1);

    metricsLayout->addWidget(new QLabel(tr("Frames:")), 3, 0);
    videoFramesLabel_ = new QLabel("0");
    videoFramesLabel_->setStyleSheet("font-family: monospace;");
    metricsLayout->addWidget(videoFramesLabel_, 3, 1);

    metricsLayout->addWidget(new QLabel(tr("Complete:")), 4, 0);
    videoCompletionLabel_ = new QLabel("100.0%");
    videoCompletionLabel_->setStyleSheet("font-family: monospace;");
    metricsLayout->addWidget(videoCompletionLabel_, 4, 1);

    metricsLayout->addWidget(new QLabel(tr("Jitter:")), 5, 0);
    videoJitterLabel_ = new QLabel("0.0 ms");
    videoJitterLabel_->setStyleSheet("font-family: monospace;");
    metricsLayout->addWidget(videoJitterLabel_, 5, 1);

    metricsLayout->addWidget(new QLabel(tr("Assembly:")), 6, 0);
    videoAssemblyLabel_ = new QLabel("0.0 ms");
    videoAssemblyLabel_->setStyleSheet("font-family: monospace;");
    metricsLayout->addWidget(videoAssemblyLabel_, 6, 1);

    metricsLayout->addWidget(new QLabel(tr("Disp Buf:")), 7, 0);
    videoDisplayBufferLabel_ = new QLabel("0");
    videoDisplayBufferLabel_->setStyleSheet("font-family: monospace;");
    metricsLayout->addWidget(videoDisplayBufferLabel_, 7, 1);

    metricsLayout->addWidget(new QLabel(tr("Disp Jitter:")), 8, 0);
    videoDisplayJitterLabel_ = new QLabel("0.0 ms");
    videoDisplayJitterLabel_->setStyleSheet("font-family: monospace;");
    metricsLayout->addWidget(videoDisplayJitterLabel_, 8, 1);

    // Audio metrics
    metricsLayout->addWidget(new QLabel(tr("Packets:")), 1, 2);
    audioPacketsLabel_ = new QLabel("0");
    audioPacketsLabel_->setStyleSheet("font-family: monospace;");
    metricsLayout->addWidget(audioPacketsLabel_, 1, 3);

    metricsLayout->addWidget(new QLabel(tr("Loss:")), 2, 2);
    audioLossLabel_ = new QLabel("0.00%");
    audioLossLabel_->setStyleSheet("font-family: monospace;");
    metricsLayout->addWidget(audioLossLabel_, 2, 3);

    metricsLayout->addWidget(new QLabel(tr("Buffer:")), 3, 2);
    audioBufferLabel_ = new QLabel("0 / 0");
    audioBufferLabel_->setStyleSheet("font-family: monospace;");
    metricsLayout->addWidget(audioBufferLabel_, 3, 3);

    metricsLayout->addWidget(new QLabel(tr("Underruns:")), 4, 2);
    audioUnderrunsLabel_ = new QLabel("0");
    audioUnderrunsLabel_->setStyleSheet("font-family: monospace;");
    metricsLayout->addWidget(audioUnderrunsLabel_, 4, 3);

    metricsLayout->addWidget(new QLabel(tr("Jitter:")), 5, 2);
    audioJitterLabel_ = new QLabel("0.0 ms");
    audioJitterLabel_->setStyleSheet("font-family: monospace;");
    metricsLayout->addWidget(audioJitterLabel_, 5, 3);

    metricsLayout->addWidget(new QLabel(tr("Write Jitter:")), 6, 2);
    audioWriteJitterLabel_ = new QLabel("0.0 ms");
    audioWriteJitterLabel_->setStyleSheet("font-family: monospace;");
    metricsLayout->addWidget(audioWriteJitterLabel_, 6, 3);

    metricsLayout->addWidget(new QLabel(tr("Dropped:")), 7, 2);
    audioDroppedLabel_ = new QLabel("0 B");
    audioDroppedLabel_->setStyleSheet("font-family: monospace;");
    metricsLayout->addWidget(audioDroppedLabel_, 7, 3);

    detailedLayout->addLayout(metricsLayout);

    // Collapse button
    auto *collapseButton = new QPushButton(tr("Collapse"));
    collapseButton->setFlat(true);
    connect(collapseButton, &QPushButton::clicked, this, &StreamingDiagnosticsWidget::toggleDisplayMode);
    detailedLayout->addWidget(collapseButton);

    mainLayout->addWidget(detailedFrame_);
}

void StreamingDiagnosticsWidget::setDisplayMode(DisplayMode mode)
{
    displayMode_ = mode;
    compactFrame_->setVisible(mode == DisplayMode::Compact);
    detailedFrame_->setVisible(mode == DisplayMode::Detailed);
}

void StreamingDiagnosticsWidget::toggleDisplayMode()
{
    if (displayMode_ == DisplayMode::Compact) {
        setDisplayMode(DisplayMode::Detailed);
    } else {
        setDisplayMode(DisplayMode::Compact);
    }
}

void StreamingDiagnosticsWidget::updateDiagnostics(const DiagnosticsSnapshot &snapshot)
{
    updateQualityIndicator(snapshot.overallQuality);

    if (displayMode_ == DisplayMode::Compact) {
        updateCompactDisplay(snapshot);
    } else {
        updateDetailedDisplay(snapshot);
    }
}

void StreamingDiagnosticsWidget::clear()
{
    updateQualityIndicator(QualityLevel::Unknown);
    summaryLabel_->setText(tr("--"));

    videoPacketsLabel_->setText("0");
    videoLossLabel_->setText("0.00%");
    videoFramesLabel_->setText("0");
    videoCompletionLabel_->setText("100.0%");
    videoJitterLabel_->setText("0.0 ms");
    videoAssemblyLabel_->setText("0.0 ms");
    videoDisplayBufferLabel_->setText("0");
    videoDisplayJitterLabel_->setText("0.0 ms");

    audioPacketsLabel_->setText("0");
    audioLossLabel_->setText("0.00%");
    audioBufferLabel_->setText("0 / 0");
    audioUnderrunsLabel_->setText("0");
    audioJitterLabel_->setText("0.0 ms");
    audioWriteJitterLabel_->setText("0.0 ms");
    audioDroppedLabel_->setText("0 B");

    qualityLabel_->setText(tr("Quality: --"));
    uptimeLabel_->setText(tr("Uptime: 0:00"));
}

void StreamingDiagnosticsWidget::updateCompactDisplay(const DiagnosticsSnapshot &snapshot)
{
    // Format: "99.8% | 0.1% loss | 2.3ms jitter"
    double totalPackets = static_cast<double>(snapshot.videoPacketsReceived + snapshot.audioPacketsReceived);
    double totalLost = static_cast<double>(snapshot.videoPacketsLost + snapshot.audioPacketsLost);
    double overallLoss = (totalPackets > 0) ? (totalLost / (totalPackets + totalLost) * 100.0) : 0.0;
    double maxJitter = qMax(snapshot.videoPacketJitterMs, snapshot.audioPacketJitterMs);

    QString summary = QString("%1% | %2% loss | %3ms jitter")
        .arg(snapshot.videoFrameCompletionPercent, 0, 'f', 1)
        .arg(overallLoss, 0, 'f', 2)
        .arg(maxJitter, 0, 'f', 1);

    summaryLabel_->setText(summary);
}

void StreamingDiagnosticsWidget::updateDetailedDisplay(const DiagnosticsSnapshot &snapshot)
{
    // Update header
    qualityLabel_->setText(tr("Quality: %1")
        .arg(StreamingDiagnostics::qualityLevelString(snapshot.overallQuality)));

    // Format uptime
    qint64 uptimeSecs = snapshot.uptimeMs / 1000;
    auto minutes = static_cast<int>(uptimeSecs / 60);
    auto seconds = static_cast<int>(uptimeSecs % 60);
    uptimeLabel_->setText(tr("Uptime: %1:%2")
        .arg(minutes)
        .arg(seconds, 2, 10, QChar('0')));

    // Video network metrics
    videoPacketsLabel_->setText(QString::number(snapshot.videoPacketsReceived));
    videoLossLabel_->setText(QString("%1%").arg(snapshot.videoPacketLossPercent, 0, 'f', 2));
    videoFramesLabel_->setText(QString::number(snapshot.videoFramesCompleted));
    videoCompletionLabel_->setText(QString("%1%").arg(snapshot.videoFrameCompletionPercent, 0, 'f', 1));
    videoJitterLabel_->setText(QString("%1 ms").arg(snapshot.videoPacketJitterMs, 0, 'f', 1));
    videoAssemblyLabel_->setText(QString("%1 ms").arg(snapshot.videoFrameAssemblyTimeMs, 0, 'f', 1));

    // Video playback metrics
    videoDisplayBufferLabel_->setText(QString::number(snapshot.videoFrameBufferLevel));
    videoDisplayJitterLabel_->setText(QString("%1 ms").arg(snapshot.videoDisplayJitterMs, 0, 'f', 1));

    // Audio network metrics
    audioPacketsLabel_->setText(QString::number(snapshot.audioPacketsReceived));
    audioLossLabel_->setText(QString("%1%").arg(snapshot.audioPacketLossPercent, 0, 'f', 2));
    audioBufferLabel_->setText(QString("%1 / %2")
        .arg(snapshot.audioBufferLevel)
        .arg(snapshot.audioBufferTarget));
    audioUnderrunsLabel_->setText(QString::number(snapshot.audioBufferUnderruns));
    audioJitterLabel_->setText(QString("%1 ms").arg(snapshot.audioPacketJitterMs, 0, 'f', 1));

    // Audio playback metrics
    audioWriteJitterLabel_->setText(QString("%1 ms").arg(snapshot.audioWriteJitterMs, 0, 'f', 1));
    audioDroppedLabel_->setText(formatBytes(snapshot.audioSamplesDropped));

    // Color code values based on thresholds
    auto colorForLoss = [](double loss) -> QString {
        if (loss < 0.1) {
            return "color: green;";
        }
        if (loss < 1.0) {
            return "color: #9BC800;";
        }
        if (loss < 5.0) {
            return "color: orange;";
        }
        return "color: red;";
    };

    auto colorForCompletion = [](double completion) -> QString {
        if (completion > 99.9) {
            return "color: green;";
        }
        if (completion > 99.0) {
            return "color: #9BC800;";
        }
        if (completion > 95.0) {
            return "color: orange;";
        }
        return "color: red;";
    };

    videoLossLabel_->setStyleSheet(QString("font-family: monospace; %1").arg(colorForLoss(snapshot.videoPacketLossPercent)));
    audioLossLabel_->setStyleSheet(QString("font-family: monospace; %1").arg(colorForLoss(snapshot.audioPacketLossPercent)));
    videoCompletionLabel_->setStyleSheet(QString("font-family: monospace; %1").arg(colorForCompletion(snapshot.videoFrameCompletionPercent)));
}

void StreamingDiagnosticsWidget::updateQualityIndicator(QualityLevel level)
{
    QColor color = StreamingDiagnostics::qualityLevelColor(level);
    qualityDot_->setStyleSheet(QString("background-color: %1; border-radius: 5px;").arg(color.name()));
}

QString StreamingDiagnosticsWidget::formatBytes(quint64 bytes)
{
    constexpr quint64 KB = 1024;
    constexpr quint64 MB = static_cast<quint64>(1024) * 1024;

    if (bytes < KB) {
        return QString("%1 B").arg(bytes);
    }
    if (bytes < MB) {
        return QString("%1 KB").arg(static_cast<double>(bytes) / static_cast<double>(KB), 0, 'f', 1);
    }
    return QString("%1 MB").arg(static_cast<double>(bytes) / static_cast<double>(MB), 0, 'f', 1);
}
