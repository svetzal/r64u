#include "diskimagereader.h"

DiskImageReader::DiskImageReader()
{
}

bool DiskImageReader::isDiskImage(const QString &filename)
{
    QString lower = filename.toLower();
    return lower.endsWith(".d64") ||
           lower.endsWith(".d71") ||
           lower.endsWith(".d81");
}

DiskImageReader::DiskDirectory DiskImageReader::parse(const QByteArray &data, const QString &filename)
{
    DiskDirectory dir;
    dir.format = detectFormat(data, filename);

    if (dir.format == Format::Unknown) {
        return dir;
    }

    parseBam(data, dir.format, dir);
    parseDirectory(data, dir.format, dir);

    return dir;
}

DiskImageReader::Format DiskImageReader::detectFormat(const QByteArray &data, const QString &filename) const
{
    // Try filename extension first
    QString lower = filename.toLower();
    if (lower.endsWith(".d64")) {
        return Format::D64;
    } else if (lower.endsWith(".d71")) {
        return Format::D71;
    } else if (lower.endsWith(".d81")) {
        return Format::D81;
    }

    // Fall back to size detection
    qint64 size = data.size();

    // D64: 683 sectors * 256 = 174848 bytes (without error bytes)
    // or 174848 + 683 = 175531 (with error bytes)
    if (size == 174848 || size == 175531 ||
        size == 196608 || size == 197376) {  // 40-track extended
        return Format::D64;
    }

    // D71: 1366 sectors * 256 = 349696 bytes (without error bytes)
    if (size == 349696 || size == 351062) {
        return Format::D71;
    }

    // D81: 3200 sectors * 256 = 819200 bytes
    if (size == 819200 || size == 822400) {
        return Format::D81;
    }

    return Format::Unknown;
}

int DiskImageReader::sectorsInTrack(Format format, int track) const
{
    if (format == Format::D81) {
        return D81_SECTORS_PER_TRACK; // All tracks have 40 sectors
    }

    // D64/D71 zone-bit recording
    // For D71, tracks 36-70 mirror the layout of tracks 1-35
    int effectiveTrack = (track > D64_TRACKS) ? track - D64_TRACKS : track;

    if (effectiveTrack >= 1 && effectiveTrack <= 17) {
        return 21;
    } else if (effectiveTrack >= 18 && effectiveTrack <= 24) {
        return 19;
    } else if (effectiveTrack >= 25 && effectiveTrack <= 30) {
        return 18;
    } else { // tracks 31-35
        return 17;
    }
}

qint64 DiskImageReader::sectorOffset(Format format, int track, int sector) const
{
    if (track < 1) return -1;

    if (format == Format::D81) {
        // D81: uniform 40 sectors per track
        return static_cast<qint64>((track - 1) * D81_SECTORS_PER_TRACK + sector) * SECTOR_SIZE;
    }

    // D64/D71: variable sectors per track
    qint64 offset = 0;
    for (int t = 1; t < track; t++) {
        offset += sectorsInTrack(format, t);
    }
    offset += sector;
    return offset * SECTOR_SIZE;
}

QByteArray DiskImageReader::readSector(const QByteArray &data, Format format, int track, int sector) const
{
    qint64 offset = sectorOffset(format, track, sector);
    if (offset < 0 || offset + SECTOR_SIZE > data.size()) {
        return QByteArray();
    }
    return data.mid(offset, SECTOR_SIZE);
}

void DiskImageReader::parseBam(const QByteArray &data, Format format, DiskDirectory &dir) const
{
    QByteArray bam;
    int nameOffset, idOffset, dosTypeOffset;

    if (format == Format::D81) {
        // D81: Header at track 40, sector 0
        bam = readSector(data, format, D81_BAM_TRACK, D81_BAM_SECTOR);
        nameOffset = 0x04;      // Disk name at offset 4
        idOffset = 0x16;        // Disk ID at offset 22
        dosTypeOffset = 0x19;   // DOS type at offset 25
    } else {
        // D64/D71: BAM at track 18, sector 0
        bam = readSector(data, format, D64_BAM_TRACK, D64_BAM_SECTOR);
        nameOffset = 0x90;      // Disk name at offset 144
        idOffset = 0xA2;        // Disk ID at offset 162
        dosTypeOffset = 0xA5;   // DOS type at offset 165
    }

    if (bam.isEmpty()) {
        return;
    }

    // Extract disk name (16 bytes, PETSCII, padded with $A0)
    dir.diskName = petsciiToString(bam.mid(nameOffset, 16));

    // Extract disk ID (2 bytes)
    dir.diskId = petsciiToString(bam.mid(idOffset, 2));

    // Extract DOS type (2 bytes)
    dir.dosType = petsciiToString(bam.mid(dosTypeOffset, 2));

    // Count free blocks
    dir.freeBlocks = countFreeBlocks(data, format);
}

quint16 DiskImageReader::countFreeBlocks(const QByteArray &data, Format format) const
{
    quint16 freeBlocks = 0;

    if (format == Format::D81) {
        // D81: BAM is in sectors 40/1 and 40/2
        QByteArray bam1 = readSector(data, format, D81_BAM_TRACK, 1);
        QByteArray bam2 = readSector(data, format, D81_BAM_TRACK, 2);

        if (bam1.isEmpty() || bam2.isEmpty()) {
            return 0;
        }

        // Each track entry is 6 bytes: 1 byte free count + 5 bytes bitmap
        // Tracks 1-40 in BAM sector 1 (starting at offset 0x10)
        for (int t = 0; t < 40; t++) {
            int offset = 0x10 + t * 6;
            if (offset < bam1.size()) {
                freeBlocks += static_cast<quint8>(bam1[offset]);
            }
        }

        // Tracks 41-80 in BAM sector 2 (starting at offset 0x10)
        for (int t = 0; t < 40; t++) {
            int offset = 0x10 + t * 6;
            if (offset < bam2.size()) {
                freeBlocks += static_cast<quint8>(bam2[offset]);
            }
        }
    } else {
        // D64/D71: BAM at track 18, sector 0
        // Each track entry is 4 bytes: 1 byte free count + 3 bytes bitmap
        QByteArray bam = readSector(data, format, D64_BAM_TRACK, D64_BAM_SECTOR);
        if (bam.isEmpty()) {
            return 0;
        }

        int bamOffset = 0x04; // BAM entries start at offset 4

        for (int t = 0; t < D64_TRACKS && (bamOffset + t * 4) < bam.size(); t++) {
            freeBlocks += static_cast<quint8>(bam[bamOffset + t * 4]);
        }

        // For D71, also read second BAM at track 53, sector 0
        if (format == Format::D71) {
            QByteArray bam2 = readSector(data, format, 53, 0);
            if (!bam2.isEmpty()) {
                // Second BAM for tracks 36-70 (stored as track entries for side 2)
                for (int t = 0; t < D64_TRACKS && (bamOffset + t * 4) < bam2.size(); t++) {
                    freeBlocks += static_cast<quint8>(bam2[bamOffset + t * 4]);
                }
            }
        }
    }

    return freeBlocks;
}

void DiskImageReader::parseDirectory(const QByteArray &data, Format format, DiskDirectory &dir) const
{
    int track, sector;

    if (format == Format::D81) {
        track = D81_DIR_TRACK;
        sector = D81_DIR_SECTOR;
    } else {
        track = D64_DIR_TRACK;
        sector = D64_DIR_SECTOR;
    }

    // Follow the linked list of directory sectors
    while (track != 0) {
        QByteArray sectorData = readSector(data, format, track, sector);
        if (sectorData.isEmpty()) {
            break;
        }

        // First 2 bytes are the link to next sector
        int nextTrack = static_cast<quint8>(sectorData[0]);
        int nextSector = static_cast<quint8>(sectorData[1]);

        // Parse 8 directory entries per sector
        for (int i = 0; i < ENTRIES_PER_SECTOR; i++) {
            int entryOffset = i * ENTRY_SIZE;
            QByteArray entryData = sectorData.mid(entryOffset, ENTRY_SIZE);

            DirectoryEntry entry = parseEntry(entryData);

            // Skip empty/deleted entries (type byte 0 with no track pointer)
            quint8 typeByte = static_cast<quint8>(entryData[2]);
            quint8 firstTrack = static_cast<quint8>(entryData[3]);

            if ((typeByte & 0x07) != 0 || firstTrack != 0) {
                // Only add entries that have a valid file type or track pointer
                if (!entry.filename.isEmpty() || firstTrack != 0) {
                    dir.entries.append(entry);
                }
            }
        }

        // Move to next sector
        track = nextTrack;
        sector = nextSector;
    }
}

DiskImageReader::DirectoryEntry DiskImageReader::parseEntry(const QByteArray &entryData) const
{
    DirectoryEntry entry;

    if (entryData.size() < ENTRY_SIZE) {
        return entry;
    }

    // Offset 2: File type byte
    quint8 typeByte = static_cast<quint8>(entryData[2]);
    entry.type = static_cast<FileType>(typeByte & 0x07);
    entry.isClosed = (typeByte & 0x80) != 0;
    entry.isLocked = (typeByte & 0x40) != 0;

    // Offset 3-4: First track/sector
    entry.firstTrack = static_cast<quint8>(entryData[3]);
    entry.firstSector = static_cast<quint8>(entryData[4]);

    // Offset 5-20: Filename (16 bytes PETSCII)
    entry.filename = petsciiToString(entryData.mid(5, 16));

    // Offset 29-30: File size in blocks (little-endian)
    entry.sizeInBlocks = static_cast<quint8>(entryData[29]) |
                         (static_cast<quint8>(entryData[30]) << 8);

    return entry;
}

QString DiskImageReader::petsciiToString(const QByteArray &data) const
{
    QString result;
    result.reserve(data.size());

    for (int i = 0; i < data.size(); i++) {
        quint8 c = static_cast<quint8>(data[i]);

        // $A0 is shift-space (padding character) - stop here
        if (c == 0xA0) {
            break;
        }

        // Convert PETSCII to Unicode
        // PETSCII uppercase letters are same positions as ASCII uppercase
        // Lowercase in PETSCII is $C1-$DA, but we display uppercase
        if (c >= 0x41 && c <= 0x5A) {
            // A-Z (uppercase in PETSCII default mode)
            result += QChar(c);
        } else if (c >= 0xC1 && c <= 0xDA) {
            // PETSCII lowercase letters - convert to uppercase for display
            result += QChar(c - 0xC1 + 'A');
        } else if (c >= 0x30 && c <= 0x39) {
            // 0-9
            result += QChar(c);
        } else if (c >= 0x20 && c <= 0x3F) {
            // Space and various punctuation
            result += QChar(c);
        } else if (c == 0x00) {
            // Null - end of string
            break;
        } else {
            // Other characters - use space or the character as-is if printable
            if (c >= 0x20 && c < 0x7F) {
                result += QChar(c);
            } else {
                result += ' ';
            }
        }
    }

    return result;
}

QString DiskImageReader::fileTypeString(FileType type)
{
    switch (type) {
    case FileType::DEL: return "DEL";
    case FileType::SEQ: return "SEQ";
    case FileType::PRG: return "PRG";
    case FileType::USR: return "USR";
    case FileType::REL: return "REL";
    case FileType::CBM: return "CBM";
    case FileType::DIR: return "DIR";
    default: return "???";
    }
}

QString DiskImageReader::formatDirectoryListing(const DiskDirectory &dir)
{
    QString result;

    // Header line: disk name and ID (like: 0 "DISK NAME       " ID 2A)
    // Format: 0 "diskname" id,dostype
    result += QString("0 \"%1\" %2 %3\n")
              .arg(dir.diskName.leftJustified(16, ' '))
              .arg(dir.diskId)
              .arg(dir.dosType);

    // File entries
    for (const DirectoryEntry &entry : dir.entries) {
        // Skip truly empty entries
        if (entry.filename.isEmpty() && entry.firstTrack == 0) {
            continue;
        }

        // Format: blocks "filename" type
        // Blocks are left-justified in a 5-character field
        QString blocksStr = QString::number(entry.sizeInBlocks).leftJustified(5, ' ');

        // Filename is quoted and padded to 16 characters
        QString quotedName = QString("\"%1\"").arg(entry.filename.leftJustified(16, ' '));

        // Type string with optional modifiers
        QString typeStr;
        if (!entry.isClosed) {
            typeStr += '*'; // Splat file (not properly closed)
        }
        typeStr += fileTypeString(entry.type);
        if (entry.isLocked) {
            typeStr += '<'; // Locked file
        }

        result += QString("%1%2 %3\n").arg(blocksStr, quotedName, typeStr);
    }

    // Footer: blocks free
    result += QString("%1 BLOCKS FREE.\n").arg(dir.freeBlocks);

    return result;
}
