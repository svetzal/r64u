#ifndef FTPENTRY_H
#define FTPENTRY_H

#include <QDateTime>
#include <QString>

/**
 * @brief Represents a single entry in an FTP directory listing.
 */
struct FtpEntry {
    QString name;              ///< Name of the file or directory
    bool isDirectory = false;  ///< True if this entry is a directory
    qint64 size = 0;           ///< Size in bytes (0 for directories)
    QString permissions;       ///< Unix-style permission string
    QDateTime modified;        ///< Last modification timestamp
};

#endif // FTPENTRY_H
