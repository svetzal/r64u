/**
 * @file ftpcommandqueue.cpp
 * @brief Implementation of FtpCommandQueue.
 */

#include "ftpcommandqueue.h"

#include <QtGlobal>

void FtpCommandQueue::enqueue(Command cmd, const QString &arg, const QString &localPath,
                              quint64 operationId)
{
    PendingCommand pending;
    pending.cmd = cmd;
    pending.arg = arg;
    pending.localPath = localPath;
    pending.operationId = operationId;
    queue_.enqueue(std::move(pending));
}

void FtpCommandQueue::enqueueRetr(const QString &remotePath, const QString &localPath,
                                  std::shared_ptr<QFile> file, bool isMemory, quint64 operationId)
{
    PendingCommand pending;
    pending.cmd = Command::Retr;
    pending.arg = remotePath;
    pending.localPath = localPath;
    pending.transferFile = std::move(file);
    pending.isMemoryDownload = isMemory;
    pending.operationId = operationId;
    queue_.enqueue(std::move(pending));
}

void FtpCommandQueue::enqueueStor(const QString &remotePath, const QString &localPath,
                                  std::shared_ptr<QFile> file, quint64 operationId)
{
    PendingCommand pending;
    pending.cmd = Command::Stor;
    pending.arg = remotePath;
    pending.localPath = localPath;
    pending.transferFile = std::move(file);
    pending.operationId = operationId;
    queue_.enqueue(std::move(pending));
}

bool FtpCommandQueue::isEmpty() const
{
    return queue_.isEmpty();
}

int FtpCommandQueue::size() const
{
    return queue_.size();
}

FtpCommandQueue::PendingCommand FtpCommandQueue::dequeueNext()
{
    Q_ASSERT(!queue_.isEmpty());
    PendingCommand cmd = std::move(queue_.head());
    queue_.dequeue();
    return cmd;
}

QList<FtpCommandQueue::PendingCommand> FtpCommandQueue::takeOperation(quint64 operationId)
{
    QList<PendingCommand> removed;
    QQueue<PendingCommand> kept;
    while (!queue_.isEmpty()) {
        PendingCommand cmd = queue_.dequeue();
        if (cmd.operationId == operationId) {
            removed.append(std::move(cmd));
        } else {
            kept.enqueue(std::move(cmd));
        }
    }
    queue_ = std::move(kept);
    return removed;
}

void FtpCommandQueue::drain()
{
    while (!queue_.isEmpty()) {
        PendingCommand cmd = std::move(queue_.head());
        queue_.dequeue();
        if (cmd.transferFile) {
            cmd.transferFile->close();
        }
    }
}
