/**
 * @file ftptransferstate.cpp
 * @brief Implementation of FtpTransferState.
 */

#include "ftptransferstate.h"

// --- LIST buffer ---

void FtpTransferState::appendListData(const QByteArray &data)
{
    listBuffer_.append(data);
}

void FtpTransferState::clearListBuffer()
{
    listBuffer_.clear();
}

// --- RETR buffer ---

void FtpTransferState::appendRetrData(const QByteArray &data)
{
    retrBuffer_.append(data);
}

void FtpTransferState::clearRetrBuffer()
{
    retrBuffer_.clear();
}

// --- Pending LIST ---

void FtpTransferState::savePendingList(const QString &path, const QByteArray &buffer)
{
    pendingList_ = PendingListData{path, buffer};
}

void FtpTransferState::appendToPendingList(const QByteArray &data)
{
    if (pendingList_) {
        pendingList_->buffer.append(data);
    }
}

std::optional<FtpTransferState::PendingListData> FtpTransferState::takePendingList()
{
    auto result = std::move(pendingList_);
    pendingList_.reset();
    return result;
}

// --- Pending RETR ---

void FtpTransferState::savePendingRetr(const QString &remotePath, const QString &localPath,
                                       std::shared_ptr<QFile> file, bool isMemory)
{
    pendingRetr_ = PendingRetrData{remotePath, localPath, std::move(file), isMemory};
}

std::optional<FtpTransferState::PendingRetrData> FtpTransferState::takePendingRetr()
{
    auto result = std::move(pendingRetr_);
    pendingRetr_.reset();
    return result;
}

QFile *FtpTransferState::pendingRetrFile() const
{
    return pendingRetr_ ? pendingRetr_->file.get() : nullptr;
}

bool FtpTransferState::pendingRetrIsMemory() const
{
    return pendingRetr_ && pendingRetr_->isMemory;
}

// --- Current RETR file ---

void FtpTransferState::setCurrentRetrFile(std::shared_ptr<QFile> file, bool isMemory)
{
    currentRetrFile_ = std::move(file);
    currentRetrIsMemory_ = isMemory;
}

void FtpTransferState::clearCurrentRetrFile()
{
    if (currentRetrFile_) {
        currentRetrFile_->close();
    }
    currentRetrFile_.reset();
    currentRetrIsMemory_ = false;
}

// --- Current STOR file ---

void FtpTransferState::setCurrentStorFile(std::shared_ptr<QFile> file)
{
    currentStorFile_ = std::move(file);
}

void FtpTransferState::clearCurrentStorFile()
{
    if (currentStorFile_) {
        currentStorFile_->close();
    }
    currentStorFile_.reset();
}

// --- Reset ---

void FtpTransferState::reset()
{
    if (currentRetrFile_) {
        currentRetrFile_->close();
        currentRetrFile_.reset();
    }
    if (currentStorFile_) {
        currentStorFile_->close();
        currentStorFile_.reset();
    }
    currentRetrIsMemory_ = false;

    if (pendingRetr_ && pendingRetr_->file) {
        pendingRetr_->file->close();
    }
    pendingRetr_.reset();
    pendingList_.reset();

    listBuffer_.clear();
    retrBuffer_.clear();
    transferSize_ = 0;
    downloading_ = false;
}
