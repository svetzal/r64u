/**
 * @file ftpcommandqueue.h
 * @brief Manages queued FTP commands with associated state.
 */

#ifndef FTPCOMMANDQUEUE_H
#define FTPCOMMANDQUEUE_H

#include <QFile>
#include <QQueue>
#include <QString>

#include <memory>

/**
 * @brief Manages queued FTP commands with associated state.
 *
 * Holds the sequence of FTP commands awaiting execution, along with
 * any file handles attached to transfer commands. Provides safe
 * draining that closes all file handles.
 */
class FtpCommandQueue
{
public:
    /// @brief FTP commands that can be queued for execution.
    enum class Command {
        None,  ///< Placeholder / no-op
        User,  ///< USER command
        Pass,  ///< PASS command
        Pwd,   ///< PWD command
        Cwd,   ///< CWD command
        Type,  ///< TYPE command
        Pasv,  ///< PASV command
        List,  ///< LIST command
        Retr,  ///< RETR command (download)
        Stor,  ///< STOR command (upload)
        Mkd,   ///< MKD command
        Rmd,   ///< RMD command
        Dele,  ///< DELE command
        RnFr,  ///< RNFR command
        RnTo,  ///< RNTO command
        Quit   ///< QUIT command
    };

    /// @brief A single pending FTP command with its associated state.
    struct PendingCommand
    {
        Command cmd = Command::None;          ///< The FTP command type
        QString arg;                          ///< Primary argument (remote path, type string, etc.)
        QString localPath;                    ///< Local filesystem path for transfer commands
        std::shared_ptr<QFile> transferFile;  ///< File handle for RETR/STOR commands
        bool isMemoryDownload = false;        ///< True when RETR result goes to memory, not disk
    };

    /**
     * @brief Enqueues a simple command with optional argument and local path.
     * @param cmd The command to queue.
     * @param arg Optional argument string.
     * @param localPath Optional local filesystem path.
     */
    void enqueue(Command cmd, const QString &arg = {}, const QString &localPath = {});

    /**
     * @brief Enqueues a RETR command with a pre-opened file handle.
     * @param remotePath Remote path to retrieve.
     * @param localPath Local destination path.
     * @param file Pre-opened QFile for writing (may be nullptr for memory downloads).
     * @param isMemory True when data should be delivered to memory rather than disk.
     */
    void enqueueRetr(const QString &remotePath, const QString &localPath,
                     std::shared_ptr<QFile> file, bool isMemory);

    /**
     * @brief Enqueues a STOR command with a pre-opened file handle.
     * @param remotePath Remote destination path.
     * @param localPath Local source path.
     * @param file Pre-opened QFile for reading.
     */
    void enqueueStor(const QString &remotePath, const QString &localPath,
                     std::shared_ptr<QFile> file);

    /**
     * @brief Returns true if the queue contains no pending commands.
     * @return True when empty.
     */
    [[nodiscard]] bool isEmpty() const;

    /**
     * @brief Returns the number of pending commands.
     * @return Queue depth.
     */
    [[nodiscard]] int size() const;

    /**
     * @brief Removes and returns the next command from the front of the queue.
     *
     * @pre !isEmpty() — caller must check before calling.
     * @return The next PendingCommand.
     */
    PendingCommand dequeueNext();

    /**
     * @brief Closes all file handles and empties the queue.
     *
     * Safe to call on an already-empty queue.
     */
    void drain();

private:
    QQueue<PendingCommand> queue_;  ///< Internal FIFO queue
};

#endif  // FTPCOMMANDQUEUE_H
