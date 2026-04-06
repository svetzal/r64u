/**
 * @file ftptransferstate.h
 * @brief Tracks all state for active FTP data transfers.
 */

#ifndef FTPTRANSFERSTATE_H
#define FTPTRANSFERSTATE_H

#include <QByteArray>
#include <QFile>
#include <QString>

#include <memory>
#include <optional>

/**
 * @brief Tracks all state for active FTP data transfers.
 *
 * Manages separate buffers for LIST and RETR operations to prevent
 * data corruption across concurrent commands. Provides pending-state
 * structs for handling the race condition where the server sends a
 * 226 Transfer Complete response before the data socket closes.
 */
class FtpTransferState
{
public:
    /// @brief Saved state for a LIST operation awaiting data-socket closure.
    struct PendingListData
    {
        QString path;       ///< Directory path being listed
        QByteArray buffer;  ///< Accumulated listing data
    };

    /// @brief Saved state for a RETR operation awaiting data-socket closure.
    struct PendingRetrData
    {
        QString remotePath;           ///< Remote file path
        QString localPath;            ///< Local destination path
        std::shared_ptr<QFile> file;  ///< Destination file (nullptr for memory)
        bool isMemory = false;        ///< True when delivering to memory
    };

    // -----------------------------------------------------------------------
    // LIST buffer
    // -----------------------------------------------------------------------

    /**
     * @brief Appends data to the LIST accumulation buffer.
     * @param data Raw bytes received from the data socket.
     */
    void appendListData(const QByteArray &data);

    /**
     * @brief Returns the current LIST buffer contents.
     * @return The accumulated LIST data.
     */
    [[nodiscard]] const QByteArray &listBuffer() const { return listBuffer_; }

    /// @brief Clears the LIST buffer.
    void clearListBuffer();

    // -----------------------------------------------------------------------
    // RETR buffer
    // -----------------------------------------------------------------------

    /**
     * @brief Appends data to the RETR in-memory accumulation buffer.
     * @param data Raw bytes received from the data socket.
     */
    void appendRetrData(const QByteArray &data);

    /**
     * @brief Returns the current RETR buffer contents.
     * @return The accumulated RETR data.
     */
    [[nodiscard]] const QByteArray &retrBuffer() const { return retrBuffer_; }

    /// @brief Clears the RETR buffer.
    void clearRetrBuffer();

    // -----------------------------------------------------------------------
    // Pending LIST (race: 226 before data socket closes)
    // -----------------------------------------------------------------------

    /**
     * @brief Returns true if a pending LIST is saved.
     * @return True when a LIST operation is awaiting data-socket closure.
     */
    [[nodiscard]] bool hasPendingList() const { return pendingList_.has_value(); }

    /**
     * @brief Saves the current LIST state while waiting for the data socket to close.
     * @param path Directory path being listed.
     * @param buffer Accumulated data received so far.
     */
    void savePendingList(const QString &path, const QByteArray &buffer);

    /**
     * @brief Appends additional data to the saved pending LIST buffer.
     * @param data Data to append.
     */
    void appendToPendingList(const QByteArray &data);

    /**
     * @brief Removes and returns the pending LIST state.
     * @return The saved PendingListData, or std::nullopt if none was saved.
     */
    std::optional<PendingListData> takePendingList();

    // -----------------------------------------------------------------------
    // Pending RETR (race: 226 before data socket closes)
    // -----------------------------------------------------------------------

    /**
     * @brief Returns true if a pending RETR is saved.
     * @return True when a RETR operation is awaiting data-socket closure.
     */
    [[nodiscard]] bool hasPendingRetr() const { return pendingRetr_.has_value(); }

    /**
     * @brief Saves the current RETR state while waiting for the data socket to close.
     * @param remotePath Remote file path.
     * @param localPath Local destination path.
     * @param file File handle (may be nullptr for memory downloads).
     * @param isMemory True when delivering to memory.
     */
    void savePendingRetr(const QString &remotePath, const QString &localPath,
                         std::shared_ptr<QFile> file, bool isMemory);

    /**
     * @brief Removes and returns the pending RETR state.
     * @return The saved PendingRetrData, or std::nullopt if none was saved.
     */
    std::optional<PendingRetrData> takePendingRetr();

    /**
     * @brief Returns the file handle from the pending RETR (non-destructive).
     * @return Raw pointer to the file, or nullptr if no pending RETR or no file.
     * @pre hasPendingRetr()
     */
    [[nodiscard]] QFile *pendingRetrFile() const;

    /**
     * @brief Returns the isMemory flag from the pending RETR (non-destructive).
     * @return True if the pending RETR is a memory download.
     * @pre hasPendingRetr()
     */
    [[nodiscard]] bool pendingRetrIsMemory() const;

    // -----------------------------------------------------------------------
    // Current RETR file
    // -----------------------------------------------------------------------

    /**
     * @brief Sets the file handle for the currently-executing RETR command.
     * @param file Open QFile for writing (may be nullptr for memory downloads).
     * @param isMemory True when delivering to memory.
     */
    void setCurrentRetrFile(std::shared_ptr<QFile> file, bool isMemory);

    /**
     * @brief Returns the current RETR file handle.
     * @return Shared pointer to the file, or nullptr if no RETR is active.
     */
    [[nodiscard]] std::shared_ptr<QFile> currentRetrFile() const { return currentRetrFile_; }

    /**
     * @brief Returns true when the current RETR is a memory download.
     * @return True if the active RETR delivers data to memory rather than disk.
     */
    [[nodiscard]] bool isCurrentRetrMemory() const { return currentRetrIsMemory_; }

    /// @brief Releases and closes the current RETR file handle.
    void clearCurrentRetrFile();

    // -----------------------------------------------------------------------
    // Current STOR file
    // -----------------------------------------------------------------------

    /**
     * @brief Sets the file handle for the currently-executing STOR command.
     * @param file Open QFile for reading.
     */
    void setCurrentStorFile(std::shared_ptr<QFile> file);

    /**
     * @brief Returns the current STOR file handle.
     * @return Shared pointer to the file, or nullptr if no STOR is active.
     */
    [[nodiscard]] std::shared_ptr<QFile> currentStorFile() const { return currentStorFile_; }

    /// @brief Releases and closes the current STOR file handle.
    void clearCurrentStorFile();

    // -----------------------------------------------------------------------
    // Transfer metadata
    // -----------------------------------------------------------------------

    /**
     * @brief Returns the expected transfer size in bytes (0 if unknown).
     * @return The expected number of bytes for the current transfer.
     */
    [[nodiscard]] qint64 transferSize() const { return transferSize_; }

    /**
     * @brief Sets the expected transfer size in bytes.
     * @param size Expected number of bytes for the current transfer.
     */
    void setTransferSize(qint64 size) { transferSize_ = size; }

    /**
     * @brief Returns true while a download is in progress.
     * @return True when a data transfer is actively receiving data.
     */
    [[nodiscard]] bool isDownloading() const { return downloading_; }

    /**
     * @brief Sets the downloading flag.
     * @param v True to indicate a download is in progress.
     */
    void setDownloading(bool v) { downloading_ = v; }

    // -----------------------------------------------------------------------
    // Reset: close all files, clear all state
    // -----------------------------------------------------------------------

    /**
     * @brief Closes all open file handles and resets all transfer state.
     *
     * Safe to call when no transfer is in progress.
     */
    void reset();

private:
    QByteArray listBuffer_;     ///< Accumulation buffer for LIST data
    QByteArray retrBuffer_;     ///< Accumulation buffer for in-memory RETR data
    qint64 transferSize_ = 0;   ///< Expected transfer size in bytes
    bool downloading_ = false;  ///< True while a data transfer is active

    std::shared_ptr<QFile> currentRetrFile_;  ///< File handle for active RETR
    std::shared_ptr<QFile> currentStorFile_;  ///< File handle for active STOR
    bool currentRetrIsMemory_ = false;        ///< True when active RETR goes to memory

    std::optional<PendingListData> pendingList_;  ///< Saved LIST state (226 race)
    std::optional<PendingRetrData> pendingRetr_;  ///< Saved RETR state (226 race)
};

#endif  // FTPTRANSFERSTATE_H
