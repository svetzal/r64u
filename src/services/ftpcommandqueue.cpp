/**
 * @file ftpcommandqueue.cpp
 * @brief Implementation of FtpCommandQueue.
 */

#include "ftpcommandqueue.h"

#include <QtGlobal>

void FtpCommandQueue::enqueue(Command cmd, const QString &arg, const QString &localPath)
{
    PendingCommand pending;
    pending.cmd = cmd;
    pending.arg = arg;
    pending.localPath = localPath;
    queue_.enqueue(std::move(pending));
}

void FtpCommandQueue::enqueueRetr(const QString &remotePath, const QString &localPath,
                                  std::shared_ptr<QFile> file, bool isMemory)
{
    PendingCommand pending;
    pending.cmd = Command::Retr;
    pending.arg = remotePath;
    pending.localPath = localPath;
    pending.transferFile = std::move(file);
    pending.isMemoryDownload = isMemory;
    queue_.enqueue(std::move(pending));
}

void FtpCommandQueue::enqueueStor(const QString &remotePath, const QString &localPath,
                                  std::shared_ptr<QFile> file)
{
    PendingCommand pending;
    pending.cmd = Command::Stor;
    pending.arg = remotePath;
    pending.localPath = localPath;
    pending.transferFile = std::move(file);
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
