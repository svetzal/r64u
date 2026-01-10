#ifndef DISKIMAGEREADER_H
#define DISKIMAGEREADER_H

#include <QByteArray>
#include <QChar>
#include <QString>
#include <QVector>

/**
 * @brief Reads and parses Commodore D64, D71, and D81 disk image files
 *
 * Supports extracting disk name, ID, and directory listings from
 * disk images used by Commodore 8-bit systems (C64, C128).
 */
class DiskImageReader
{
public:
    /**
     * @brief Supported disk image formats
     */
    enum class Format {
        Unknown,
        D64,    // 35-track single-sided 1541 disk (170KB)
        D71,    // 70-track double-sided 1571 disk (340KB)
        D81     // 80-track double-sided 1581 disk (800KB)
    };

    /**
     * @brief File type codes from directory entries
     */
    enum class FileType {
        DEL = 0,    // Deleted
        SEQ = 1,    // Sequential
        PRG = 2,    // Program
        USR = 3,    // User
        REL = 4,    // Relative
        CBM = 5,    // Partition (1581 only)
        DIR = 6     // Directory (1581 only)
    };

    /**
     * @brief Represents a single directory entry
     *
     * Filenames are stored as raw PETSCII bytes to preserve C64-native
     * encoding. Use PetsciiConverter::toDisplayString() for display.
     */
    struct DirectoryEntry {
        QByteArray filename;        // 16-byte raw PETSCII filename
        FileType type;              // File type
        quint16 sizeInBlocks;       // Size in 254-byte blocks
        bool isClosed;              // True if file is properly closed
        bool isLocked;              // True if file is write-protected
        quint8 firstTrack;          // First data track
        quint8 firstSector;         // First data sector
    };

    /**
     * @brief Represents a complete disk directory
     *
     * All text fields are stored as raw PETSCII bytes to preserve C64-native
     * encoding. Use PetsciiConverter::toDisplayString() for display.
     */
    struct DiskDirectory {
        QByteArray diskName;        // 16-byte raw PETSCII disk name
        QByteArray diskId;          // 2-byte raw PETSCII disk ID
        QByteArray dosType;         // 2-byte raw PETSCII DOS type (e.g., "2A", "3D")
        quint16 freeBlocks;         // Number of free blocks
        QVector<DirectoryEntry> entries;
        Format format;
    };

    DiskImageReader();

    /**
     * @brief Parse a disk image from raw data
     * @param data The raw disk image bytes
     * @param filename Optional filename to help determine format
     * @return DiskDirectory structure with parsed information
     */
    DiskDirectory parse(const QByteArray &data, const QString &filename = QString());

    /**
     * @brief Detect disk image format from file extension
     * @param filename The filename to check
     * @return True if the file is a supported disk image
     */
    static bool isDiskImage(const QString &filename);

    /**
     * @brief Format a directory listing as C64-style text
     * @param dir The parsed directory
     * @return Multi-line string formatted like a C64 directory listing
     */
    static QString formatDirectoryListing(const DiskDirectory &dir);

    /**
     * @brief Get file type string (e.g., "PRG", "SEQ")
     */
    static QString fileTypeString(FileType type);

    /**
     * @brief Convert ASCII text to C64 Pro font Unicode (PUA)
     */
    static QString asciiToC64Font(const QString &text);

private:
    /**
     * @brief Detect format from data size and optional filename
     */
    Format detectFormat(const QByteArray &data, const QString &filename) const;

    /**
     * @brief Calculate sector offset in image for given track/sector
     */
    qint64 sectorOffset(Format format, int track, int sector) const;

    /**
     * @brief Get number of sectors for a given track (D64/D71 zone recording)
     */
    int sectorsInTrack(Format format, int track) const;

    /**
     * @brief Read a sector from the image data
     */
    QByteArray readSector(const QByteArray &data, Format format, int track, int sector) const;

    /**
     * @brief Parse BAM sector for disk name, ID, and free block count
     */
    void parseBam(const QByteArray &data, Format format, DiskDirectory &dir) const;

    /**
     * @brief Parse all directory sectors
     */
    void parseDirectory(const QByteArray &data, Format format, DiskDirectory &dir) const;

    /**
     * @brief Parse a single directory entry (32 bytes)
     */
    DirectoryEntry parseEntry(const QByteArray &entryData) const;

    /**
     * @brief Trim PETSCII padding ($A0) from end of byte array
     */
    QByteArray trimPetsciiPadding(const QByteArray &data) const;

    /**
     * @brief Count free blocks from BAM bitmap
     */
    quint16 countFreeBlocks(const QByteArray &data, Format format) const;

    // Constants for disk geometry
    static constexpr int SECTOR_SIZE = 256;
    static constexpr int ENTRIES_PER_SECTOR = 8;
    static constexpr int ENTRY_SIZE = 32;

    // D64/D71 constants
    static constexpr int D64_DIR_TRACK = 18;
    static constexpr int D64_DIR_SECTOR = 1;
    static constexpr int D64_BAM_TRACK = 18;
    static constexpr int D64_BAM_SECTOR = 0;
    static constexpr int D64_TRACKS = 35;
    static constexpr int D71_TRACKS = 70;

    // D81 constants
    static constexpr int D81_DIR_TRACK = 40;
    static constexpr int D81_DIR_SECTOR = 3;
    static constexpr int D81_BAM_TRACK = 40;
    static constexpr int D81_BAM_SECTOR = 0;
    static constexpr int D81_TRACKS = 80;
    static constexpr int D81_SECTORS_PER_TRACK = 40;
};

#endif // DISKIMAGEREADER_H
