#include "databasedownloadview.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

DatabaseDownloadView::DatabaseDownloadView(QObject *parent) : QObject(parent) {}

QWidget *DatabaseDownloadView::createWidget()
{
    auto *container = new QWidget();
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    // HVSC Databases group
    auto *databaseGroup = new QGroupBox(QObject::tr("HVSC Databases"));
    auto *databaseLayout = new QVBoxLayout(databaseGroup);

    // Songlengths section
    auto *songlengthsLabel =
        new QLabel(QObject::tr("<b>Songlengths</b> - Accurate song durations"));
    databaseLayout->addWidget(songlengthsLabel);

    dbWidgets_.statusLabel = new QLabel(QObject::tr("Not loaded"));
    dbWidgets_.itemName = QObject::tr("HVSC Songlengths database");
    dbWidgets_.unitName = QObject::tr("entries");
    databaseLayout->addWidget(dbWidgets_.statusLabel);

    dbWidgets_.progressBar = new QProgressBar();
    dbWidgets_.progressBar->setVisible(false);
    databaseLayout->addWidget(dbWidgets_.progressBar);

    auto *songlengthsButtonLayout = new QHBoxLayout();
    dbWidgets_.button = new QPushButton(QObject::tr("Download/Update"));
    dbWidgets_.button->setToolTip(
        QObject::tr("Download the HVSC Songlengths database for accurate SID song durations"));
    songlengthsButtonLayout->addWidget(dbWidgets_.button);
    songlengthsButtonLayout->addStretch();
    databaseLayout->addLayout(songlengthsButtonLayout);

    databaseLayout->addSpacing(8);

    // STIL section
    auto *stilLabel = new QLabel(QObject::tr("<b>STIL</b> - Tune commentary and cover info"));
    databaseLayout->addWidget(stilLabel);

    stilWidgets_.statusLabel = new QLabel(QObject::tr("Not loaded"));
    stilWidgets_.itemName = QObject::tr("STIL database");
    stilWidgets_.unitName = QObject::tr("entries");
    databaseLayout->addWidget(stilWidgets_.statusLabel);

    stilWidgets_.progressBar = new QProgressBar();
    stilWidgets_.progressBar->setVisible(false);
    databaseLayout->addWidget(stilWidgets_.progressBar);

    auto *stilButtonLayout = new QHBoxLayout();
    stilWidgets_.button = new QPushButton(QObject::tr("Download/Update"));
    stilWidgets_.button->setToolTip(
        QObject::tr("Download STIL.txt for tune commentary, history, and cover information"));
    stilButtonLayout->addWidget(stilWidgets_.button);
    stilButtonLayout->addStretch();
    databaseLayout->addLayout(stilButtonLayout);

    databaseLayout->addSpacing(8);

    // BUGlist section
    auto *buglistLabel = new QLabel(QObject::tr("<b>BUGlist</b> - Known playback issues"));
    databaseLayout->addWidget(buglistLabel);

    buglistWidgets_.statusLabel = new QLabel(QObject::tr("Not loaded"));
    buglistWidgets_.itemName = QObject::tr("BUGlist database");
    buglistWidgets_.unitName = QObject::tr("entries");
    databaseLayout->addWidget(buglistWidgets_.statusLabel);

    buglistWidgets_.progressBar = new QProgressBar();
    buglistWidgets_.progressBar->setVisible(false);
    databaseLayout->addWidget(buglistWidgets_.progressBar);

    auto *buglistButtonLayout = new QHBoxLayout();
    buglistWidgets_.button = new QPushButton(QObject::tr("Download/Update"));
    buglistWidgets_.button->setToolTip(
        QObject::tr("Download BUGlist.txt for known SID playback issues"));
    buglistButtonLayout->addWidget(buglistWidgets_.button);
    buglistButtonLayout->addStretch();
    databaseLayout->addLayout(buglistButtonLayout);

    layout->addWidget(databaseGroup);

    // GameBase64 Database group
    auto *gamebaseGroup = new QGroupBox(QObject::tr("GameBase64 Database"));
    auto *gamebaseLayout = new QVBoxLayout(gamebaseGroup);

    auto *gamebaseLabel = new QLabel(QObject::tr("<b>Game Database</b> - ~29,000 C64 games"));
    gamebaseLayout->addWidget(gamebaseLabel);

    gameBase64Widgets_.statusLabel = new QLabel(QObject::tr("Not loaded"));
    gameBase64Widgets_.itemName = QObject::tr("GameBase64 database");
    gameBase64Widgets_.unitName = QObject::tr("games");
    gamebaseLayout->addWidget(gameBase64Widgets_.statusLabel);

    gameBase64Widgets_.progressBar = new QProgressBar();
    gameBase64Widgets_.progressBar->setVisible(false);
    gamebaseLayout->addWidget(gameBase64Widgets_.progressBar);

    auto *gamebaseButtonLayout = new QHBoxLayout();
    gameBase64Widgets_.button = new QPushButton(QObject::tr("Download/Update"));
    gameBase64Widgets_.button->setToolTip(QObject::tr(
        "Download GameBase64 database for game information (publisher, year, genre, etc.)"));
    gamebaseButtonLayout->addWidget(gameBase64Widgets_.button);
    gamebaseButtonLayout->addStretch();
    gamebaseLayout->addLayout(gamebaseButtonLayout);

    layout->addWidget(gamebaseGroup);
    layout->addStretch();

    return container;
}
