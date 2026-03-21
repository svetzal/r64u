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
        D64,  // 35-track single-sided 1541 disk (170KB)
        D71,  // 70-track double-sided 1571 disk (340KB)
        D81   // 80-track double-sided 1581 disk (800KB)
    };

    /**
     * @brief File type codes from directory entries
     */
    enum class FileType {
        DEL = 0,  // Deleted
        SEQ = 1,  // Sequential
        PRG = 2,  // Program
        USR = 3,  // User
        REL = 4,  // Relative
        CBM = 5,  // Partition (1581 only)
        DIR = 6   // Directory (1581 only)
    };

    /**
     * @brief Represents a single directory entry
     *
     * Filenames are stored as raw PETSCII bytes to preserve C64-native
     * encoding. Use PetsciiConverter::toDisplayString() for display.
     */
    struct DirectoryEntry
    {
        QByteArray filename;           // 16-byte raw PETSCII filename
        FileType type{FileType::DEL};  // File type
        quint16 sizeInBlocks{0};       // Size in 254-byte blocks
        bool isClosed{false};          // True if file is properly closed
        bool isLocked{false};          // True if file is write-protected
        quint8 firstTrack{0};          // First data track
        quint8 firstSector{0};         // First data sector
    };

    /**
     * @brief Represents a complete disk directory
     *
     * All text fields are stored as raw PETSCII bytes to preserve C64-native
     * encoding. Use PetsciiConverter::toDisplayString() for display.
     */
    struct DiskDirectory
    {
        QByteArray diskName;  // 16-byte raw PETSCII disk name
        QByteArray diskId;    // 2-byte raw PETSCII disk ID
        QByteArray dosType;   // 2-byte raw PETSCII DOS type (e.g., "2A", "3D")
        quint16 freeBlocks;   // Number of free blocks
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
    static DiskDirectory parse(const QByteArray &data, const QString &filename = QString());

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
    static Format detectFormat(const QByteArray &data, const QString &filename);

    /**
     * @brief Calculate sector offset in image for given track/sector
     */
    static qint64 sectorOffset(Format format, int track, int sector);

    /**
     * @brief Get number of sectors for a given track (D64/D71 zone recording)
     */
    static int sectorsInTrack(Format format, int track);

    /**
     * @brief Read a sector from the image data
     */
    static QByteArray readSector(const QByteArray &data, Format format, int track, int sector);

    /**
     * @brief Parse BAM sector for disk name, ID, and free block count
     */
    static void parseBam(const QByteArray &data, Format format, DiskDirectory &dir);

    /**
     * @brief Parse all directory sectors
     */
    static void parseDirectory(const QByteArray &data, Format format, DiskDirectory &dir);

    /**
     * @brief Parse a single directory entry (32 bytes)
     */
    static DirectoryEntry parseEntry(const QByteArray &entryData);

    /**
     * @brief Trim PETSCII padding ($A0) from end of byte array
     */
    static QByteArray trimPetsciiPadding(const QByteArray &data);

    /**
     * @brief Count free blocks from BAM bitmap
     */
    static quint16 countFreeBlocks(const QByteArray &data, Format format);

    // Constants for disk geometry
    static constexpr int SectorSize = 256;
    static constexpr int EntriesPerSector = 8;
    static constexpr int EntrySize = 32;

    // D64/D71 constants
    static constexpr int D64DirTrack = 18;
    static constexpr int D64DirSector = 1;
    static constexpr int D64BamTrack = 18;
    static constexpr int D64BamSector = 0;
    static constexpr int D64Tracks = 35;
    static constexpr int D71Tracks = 70;

    // D81 constants
    static constexpr int D81DirTrack = 40;
    static constexpr int D81DirSector = 3;
    static constexpr int D81BamTrack = 40;
    static constexpr int D81BamSector = 0;
    static constexpr int D81Tracks = 80;
    static constexpr int D81SectorsPerTrack = 40;
};

#endif  // DISKIMAGEREADER_H
