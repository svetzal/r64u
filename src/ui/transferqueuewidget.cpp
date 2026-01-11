#include "transferqueuewidget.h"
#include "../models/transferqueue.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QApplication>
#include <QStyleOptionProgressBar>

class TransferItemDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        painter->save();

        // Background
        if (opt.state & QStyle::State_Selected) {
            painter->fillRect(opt.rect, opt.palette.highlight());
        }

        QRect rect = opt.rect.adjusted(4, 4, -4, -4);

        // File name and direction
        QString fileName = index.data(TransferQueue::FileNameRole).toString();
        int direction = index.data(TransferQueue::DirectionRole).toInt();
        QString dirText = (direction == 0) ? "[UP]" : "[DN]";

        int status = index.data(TransferQueue::StatusRole).toInt();
        QString statusText;
        QColor statusColor;
        switch (static_cast<TransferItem::Status>(status)) {
        case TransferItem::Status::Pending:
            statusText = "Pending";
            statusColor = Qt::gray;
            break;
        case TransferItem::Status::InProgress:
            statusText = "Transferring";
            statusColor = Qt::blue;
            break;
        case TransferItem::Status::Completed:
            statusText = "Done";
            statusColor = Qt::darkGreen;
            break;
        case TransferItem::Status::Failed:
            statusText = "Failed";
            statusColor = Qt::red;
            break;
        }

        // Draw text
        QFont font = opt.font;
        painter->setFont(font);

        QRect textRect = rect;
        textRect.setHeight(rect.height() / 2);

        painter->setPen(statusColor);
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                          QString("%1 %2 - %3").arg(dirText, fileName, statusText));

        // Draw progress bar for in-progress items
        if (status == static_cast<int>(TransferItem::Status::InProgress)) {
            int progress = index.data(TransferQueue::ProgressRole).toInt();

            QRect progressRect = rect;
            progressRect.setTop(rect.top() + rect.height() / 2 + 2);
            progressRect.setHeight(rect.height() / 2 - 4);

            QStyleOptionProgressBar progressBarOption;
            progressBarOption.rect = progressRect;
            progressBarOption.minimum = 0;
            progressBarOption.maximum = 100;
            progressBarOption.progress = progress;
            progressBarOption.text = QString("%1%").arg(progress);
            progressBarOption.textVisible = true;

            QApplication::style()->drawControl(QStyle::CE_ProgressBar,
                                                &progressBarOption, painter);
        }

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override
    {
        Q_UNUSED(index)
        return QSize(option.rect.width(), 50);
    }
};

TransferQueueWidget::TransferQueueWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void TransferQueueWidget::setTransferQueue(TransferQueue *queue)
{
    if (queue_) {
        disconnect(queue_, nullptr, this, nullptr);
    }

    queue_ = queue;

    if (queue_) {
        listView_->setModel(queue_);
        connect(queue_, &TransferQueue::queueChanged,
                this, &TransferQueueWidget::onQueueChanged);
        connect(queue_, &TransferQueue::dataChanged,
                this, [this]() { listView_->viewport()->update(); });
    }

    onQueueChanged();
}

void TransferQueueWidget::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // Header with status
    auto *headerLayout = new QHBoxLayout();

    statusLabel_ = new QLabel(tr("Transfer Queue"));
    statusLabel_->setObjectName("heading");
    headerLayout->addWidget(statusLabel_);

    headerLayout->addStretch();

    clearButton_ = new QPushButton(tr("Clear Done"));
    clearButton_->setEnabled(false);
    connect(clearButton_, &QPushButton::clicked,
            this, &TransferQueueWidget::onClearCompleted);
    headerLayout->addWidget(clearButton_);

    cancelButton_ = new QPushButton(tr("Cancel All"));
    cancelButton_->setEnabled(false);
    connect(cancelButton_, &QPushButton::clicked,
            this, &TransferQueueWidget::onCancelAll);
    headerLayout->addWidget(cancelButton_);

    layout->addLayout(headerLayout);

    // List view
    listView_ = new QListView();
    listView_->setItemDelegate(new TransferItemDelegate(listView_));
    listView_->setSelectionMode(QAbstractItemView::NoSelection);
    listView_->setAlternatingRowColors(true);
    layout->addWidget(listView_);
}

void TransferQueueWidget::onQueueChanged()
{
    updateButtons();

    if (queue_) {
        int pending = queue_->pendingCount();
        int active = queue_->activeCount();
        int total = queue_->rowCount();

        if (total == 0) {
            statusLabel_->setText(tr("Transfer Queue"));
        } else if (active > 0) {
            statusLabel_->setText(tr("Transferring (%1 pending)").arg(pending));
        } else {
            statusLabel_->setText(tr("Transfer Queue (%1 items)").arg(total));
        }
    }
}

void TransferQueueWidget::onClearCompleted()
{
    if (queue_) {
        queue_->removeCompleted();
    }
}

void TransferQueueWidget::onCancelAll()
{
    if (queue_) {
        queue_->cancelAll();
    }
}

void TransferQueueWidget::updateButtons()
{
    if (!queue_) {
        clearButton_->setEnabled(false);
        cancelButton_->setEnabled(false);
        return;
    }

    bool hasItems = queue_->rowCount() > 0;
    bool hasActive = queue_->isProcessing();

    clearButton_->setEnabled(hasItems);
    cancelButton_->setEnabled(hasActive);
}
