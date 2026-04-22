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
    enum class Format { Unknown, D64, D71, D81 };

    enum class FileType { DEL = 0, SEQ = 1, PRG = 2, USR = 3, REL = 4, CBM = 5, DIR = 6 };

    /**
     * @brief Represents a single directory entry
     *
     * Filenames are stored as raw PETSCII bytes to preserve C64-native
     * encoding. Use PetsciiConverter::toDisplayString() for display.
     */
    struct DirectoryEntry
    {
        QByteArray filename;  // 16-byte raw PETSCII filename
        FileType type{FileType::DEL};
        quint16 sizeInBlocks{0};
        bool isClosed{false};
        bool isLocked{false};
        quint8 firstTrack{0};
        quint8 firstSector{0};
    };

    /**
     * @brief Represents a complete disk directory
     *
     * All text fields are stored as raw PETSCII bytes to preserve C64-native
     * encoding. Use PetsciiConverter::toDisplayString() for display.
     */
    struct DiskDirectory
    {
        QByteArray diskName;
        QByteArray diskId;
        QByteArray dosType;
        quint16 freeBlocks;
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

    static QString fileTypeString(FileType type);
    static QString asciiToC64Font(const QString &text);

private:
    static Format detectFormat(const QByteArray &data, const QString &filename);
    static qint64 sectorOffset(Format format, int track, int sector);
    static int sectorsInTrack(Format format, int track);
    static QByteArray readSector(const QByteArray &data, Format format, int track, int sector);
    static void parseBam(const QByteArray &data, Format format, DiskDirectory &dir);
    static void parseDirectory(const QByteArray &data, Format format, DiskDirectory &dir);
    static DirectoryEntry parseEntry(const QByteArray &entryData);
    static QByteArray trimPetsciiPadding(const QByteArray &data);
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
