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
        quint8 petscii = static_cast<quint8>(data[i]);

        // $A0 is shift-space (padding character) - stop here
        if (petscii == 0xA0) {
            break;
        }

        // Null - end of string
        if (petscii == 0x00) {
            break;
        }

        // Convert PETSCII to screen code, then to Unicode
        quint8 screenCode = petsciiToScreenCode(petscii);
        result += screenCodeToUnicode(screenCode);
    }

    return result;
}

QChar DiskImageReader::screenCodeToUnicode(quint8 screenCode) const
{
    // C64 screen code to Unicode mapping
    // Based on Style64's suggested Unicode mappings
    // Reference: https://style64.org/petscii/

    // Screen codes 0x00-0x1F: @ A-Z [ £ ] ↑ ←
    if (screenCode == 0x00) return QChar('@');
    if (screenCode >= 0x01 && screenCode <= 0x1A) return QChar('A' + screenCode - 1);
    if (screenCode == 0x1B) return QChar('[');
    if (screenCode == 0x1C) return QChar(0x00A3); // £
    if (screenCode == 0x1D) return QChar(']');
    if (screenCode == 0x1E) return QChar(0x2191); // ↑
    if (screenCode == 0x1F) return QChar(0x2190); // ←

    // Screen codes 0x20-0x3F: space and ASCII punctuation/numbers
    if (screenCode >= 0x20 && screenCode <= 0x3F) {
        return QChar(screenCode); // Direct ASCII mapping
    }

    // Screen codes 0x40-0x5F: Graphics characters
    // These map to Unicode box drawing and block elements
    static const ushort graphics1[] = {
        0x2500, // 40: horizontal line ─
        0x2660, // 41: spade ♠
        0x2502, // 42: vertical line │
        0x2500, // 43: horizontal line ─
        0x2500, // 44: horizontal line (lower) ─
        0x2500, // 45: horizontal line (upper) ─
        0x2500, // 46: horizontal line ─
        0x2500, // 47: horizontal line ─
        0x2500, // 48: horizontal line ─
        0x256E, // 49: round corner ╮
        0x2570, // 4A: round corner ╰
        0x256F, // 4B: round corner ╯
        0x2500, // 4C: horizontal line ─
        0x2572, // 4D: diagonal ╲
        0x2571, // 4E: diagonal ╱
        0x2500, // 4F: horizontal line ─
        0x256D, // 50: round corner ╭
        0x2022, // 51: bullet •
        0x2500, // 52: horizontal line ─
        0x2665, // 53: heart ♥
        0x2500, // 54: horizontal line ─
        0x256D, // 55: round corner ╭
        0x2573, // 56: cross ╳
        0x25CB, // 57: circle ○
        0x2663, // 58: club ♣
        0x2500, // 59: horizontal line ─
        0x2666, // 5A: diamond ♦
        0x253C, // 5B: cross ┼
        0x2502, // 5C: vertical line │
        0x03C0, // 5D: pi π
        0x25E5, // 5E: upper right triangle ◥
        0x2500, // 5F: horizontal line ─
    };
    if (screenCode >= 0x40 && screenCode <= 0x5F) {
        return QChar(graphics1[screenCode - 0x40]);
    }

    // Screen codes 0x60-0x7F: More graphics (block elements)
    static const ushort graphics2[] = {
        0x00A0, // 60: non-breaking space
        0x258C, // 61: left half block ▌
        0x2584, // 62: lower half block ▄
        0x2594, // 63: upper one eighth block ▔
        0x2581, // 64: lower one eighth block ▁
        0x258E, // 65: left one quarter block ▎
        0x2592, // 66: medium shade ▒
        0x2595, // 67: right one eighth block ▕
        0x2500, // 68: horizontal line ─
        0x25E4, // 69: upper left triangle ◤
        0x2500, // 6A: horizontal line ─
        0x251C, // 6B: left tee ├
        0x2597, // 6C: quadrant lower right ▗
        0x2514, // 6D: corner └
        0x2510, // 6E: corner ┐
        0x2582, // 6F: lower one quarter block ▂
        0x250C, // 70: corner ┌
        0x2534, // 71: tee ┴
        0x252C, // 72: tee ┬
        0x2524, // 73: right tee ┤
        0x258E, // 74: left one quarter block ▎
        0x258D, // 75: left three eighths block ▍
        0x2500, // 76: horizontal line ─
        0x2500, // 77: horizontal line ─
        0x2500, // 78: horizontal line ─
        0x2583, // 79: lower three eighths block ▃
        0x2713, // 7A: check mark ✓
        0x2596, // 7B: quadrant lower left ▖
        0x259D, // 7C: quadrant upper right ▝
        0x2518, // 7D: corner ┘
        0x2598, // 7E: quadrant upper left ▘
        0x259A, // 7F: quadrant upper left and lower right ▚
    };
    if (screenCode >= 0x60 && screenCode <= 0x7F) {
        return QChar(graphics2[screenCode - 0x60]);
    }

    // Screen codes 0x80-0xFF: Reverse video versions
    // For now, map to the same as non-reverse (0x00-0x7F)
    if (screenCode >= 0x80) {
        return screenCodeToUnicode(screenCode - 0x80);
    }

    return QChar(' '); // Fallback
}

quint8 DiskImageReader::petsciiToScreenCode(quint8 petscii) const
{
    // PETSCII to C64 screen code conversion
    // Reference: https://sta.c64.org/cbm64pet.html

    if (petscii <= 0x1F) {
        // $00-$1F: Control codes - map to reverse @ and letters
        return petscii + 0x80;
    } else if (petscii <= 0x3F) {
        // $20-$3F: Space, punctuation, numbers - same as screen code
        return petscii;
    } else if (petscii <= 0x5F) {
        // $40-$5F: @ and uppercase A-Z, [, £, ], ↑, ←
        // Screen code = PETSCII - $40
        return petscii - 0x40;
    } else if (petscii <= 0x7F) {
        // $60-$7F: Graphics characters (horizontal line, etc.)
        // Screen code = PETSCII - $20
        return petscii - 0x20;
    } else if (petscii <= 0x9F) {
        // $80-$9F: Control codes (colors, etc.) - not printable
        // Map to space
        return 0x20;
    } else if (petscii <= 0xBF) {
        // $A0-$BF: Shifted space and graphics - reverse video versions
        // Screen code = PETSCII - $40
        return petscii - 0x40;
    } else if (petscii <= 0xDF) {
        // $C0-$DF: Lowercase letters and some symbols
        // In uppercase mode (default): these show as graphics
        // Screen code = PETSCII - $80
        return petscii - 0x80;
    } else if (petscii <= 0xFE) {
        // $E0-$FE: More shifted graphics
        // Screen code = PETSCII - $80
        return petscii - 0x80;
    } else {
        // $FF: Pi symbol
        return 0x5E; // Pi is at screen code $5E
    }
}

QString DiskImageReader::fileTypeString(FileType type)
{
    switch (type) {
    case FileType::DEL: return QStringLiteral("DEL");
    case FileType::SEQ: return QStringLiteral("SEQ");
    case FileType::PRG: return QStringLiteral("PRG");
    case FileType::USR: return QStringLiteral("USR");
    case FileType::REL: return QStringLiteral("REL");
    case FileType::CBM: return QStringLiteral("CBM");
    case FileType::DIR: return QStringLiteral("DIR");
    default: return QStringLiteral("???");
    }
}

QString DiskImageReader::asciiToC64Font(const QString &text)
{
    // For the C64 Pro font with standard Unicode mappings,
    // ASCII characters display correctly as-is since the font
    // has C64-styled glyphs at standard ASCII code points.
    // Just return the text directly - no conversion needed.
    return text;
}

QString DiskImageReader::formatDirectoryListing(const DiskDirectory &dir)
{
    QString result;

    // Use regular space for padding (font renders ASCII correctly)
    QChar c64Space(' ');

    // Header line: disk name and ID (like: 0 "DISK NAME       " ID 2A)
    // Format: 0 "diskname" id,dostype
    // Pad the disk name with C64 spaces (filenames are already in C64 font)
    QString paddedName = dir.diskName;
    while (paddedName.length() < 16) {
        paddedName += c64Space;
    }

    result += asciiToC64Font(QString("0 \"")) + paddedName + asciiToC64Font(QString("\" "));
    result += dir.diskId + asciiToC64Font(QString(" ")) + dir.dosType;
    result += QChar('\n');

    // File entries
    for (const DirectoryEntry &entry : dir.entries) {
        // Skip truly empty entries
        if (entry.filename.isEmpty() && entry.firstTrack == 0) {
            continue;
        }

        // Format: blocks "filename" type
        // Blocks are left-justified in a 5-character field
        QString blocksStr = asciiToC64Font(QString::number(entry.sizeInBlocks).leftJustified(5, ' '));

        // Filename is quoted and padded to 16 characters with C64 spaces
        QString paddedFilename = entry.filename;
        while (paddedFilename.length() < 16) {
            paddedFilename += c64Space;
        }
        QString quotedName = asciiToC64Font(QString("\"")) + paddedFilename + asciiToC64Font(QString("\""));

        // Type string with optional modifiers
        QString typeStr;
        if (!entry.isClosed) {
            typeStr += asciiToC64Font(QString("*")); // Splat file (not properly closed)
        }
        typeStr += asciiToC64Font(fileTypeString(entry.type));
        if (entry.isLocked) {
            typeStr += asciiToC64Font(QString("<")); // Locked file
        }

        result += blocksStr + quotedName + asciiToC64Font(QString(" ")) + typeStr;
        result += QChar('\n');
    }

    // Footer: blocks free
    result += asciiToC64Font(QString("%1 BLOCKS FREE.").arg(dir.freeBlocks));
    result += QChar('\n');

    return result;
}
