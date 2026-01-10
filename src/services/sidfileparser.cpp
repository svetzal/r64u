#include "sidfileparser.h"

#include <QFileInfo>

bool SidFileParser::isSidFile(const QString &path)
{
    return path.toLower().endsWith(".sid");
}

SidFileParser::SidInfo SidFileParser::parse(const QByteArray &data)
{
    SidInfo info;

    // Check minimum size
    if (data.size() < MinHeaderSize) {
        return info;
    }

    // Check magic ID
    QByteArray magic = data.mid(MagicOffset, 4);
    if (magic == "PSID") {
        info.format = Format::PSID;
    } else if (magic == "RSID") {
        info.format = Format::RSID;
    } else {
        return info;
    }

    // Read version
    info.version = readWord(data, VersionOffset);
    if (info.version < 1 || info.version > 4) {
        return info;
    }

    // Read core header fields
    info.dataOffset = readWord(data, DataOffsetOffset);
    info.loadAddress = readWord(data, LoadAddrOffset);
    info.initAddress = readWord(data, InitAddrOffset);
    info.playAddress = readWord(data, PlayAddrOffset);
    info.songs = readWord(data, SongsOffset);
    info.startSong = readWord(data, StartSongOffset);
    info.speed = readLongword(data, SpeedOffset);

    // Read text fields
    info.title = readString(data, TitleOffset, StringLength);
    info.author = readString(data, AuthorOffset, StringLength);
    info.released = readString(data, ReleasedOffset, StringLength);

    // Read v2+ extended fields if present
    if (info.version >= 2 && data.size() >= V2HeaderSize) {
        quint16 flags = readWord(data, FlagsOffset);

        // Bit 0: MUS player
        info.musPlayer = (flags & 0x01) != 0;

        // Bit 1: PlaySID samples (PSID) or BASIC flag (RSID)
        if (info.format == Format::RSID) {
            info.basicFlag = (flags & 0x02) != 0;
        } else {
            info.playsSamples = (flags & 0x02) != 0;
        }

        // Bits 2-3: Video standard
        info.videoStandard = decodeVideoStandard((flags >> 2) & 0x03);

        // Bits 4-5: SID model
        info.sidModel = decodeSidModel((flags >> 4) & 0x03);

        // Multi-SID support (v3+)
        if (info.version >= 3) {
            info.secondSidAddress = static_cast<quint8>(data.at(SecondSidOffset));
            // Bits 6-7: Second SID model
            info.secondSidModel = decodeSidModel((flags >> 6) & 0x03);
        }

        if (info.version >= 4) {
            info.thirdSidAddress = static_cast<quint8>(data.at(ThirdSidOffset));
            // Bits 8-9: Third SID model
            info.thirdSidModel = decodeSidModel((flags >> 8) & 0x03);
        }
    }

    info.valid = true;
    return info;
}

QString SidFileParser::formatForDisplay(const SidInfo &info)
{
    if (!info.valid) {
        return QString("Invalid SID file");
    }

    QString output;

    // Title block
    output += QString("%1\n").arg(info.title.isEmpty() ? "(Untitled)" : info.title);
    output += QString("by %1\n").arg(info.author.isEmpty() ? "(Unknown)" : info.author);
    if (!info.released.isEmpty()) {
        output += QString("%1\n").arg(info.released);
    }
    output += "\n";

    // Format info
    QString formatStr = (info.format == Format::PSID) ? "PSID" : "RSID";
    output += QString("Format: %1 v%2").arg(formatStr).arg(info.version);

    // Multi-SID indicator
    if (info.version >= 4 && info.thirdSidAddress != 0) {
        output += " (3SID)";
    } else if (info.version >= 3 && info.secondSidAddress != 0) {
        output += " (2SID)";
    }
    output += "\n";

    // Songs section
    output += QString("\nSongs: %1\n").arg(info.songs);
    if (info.songs > 1) {
        int maxSongsToShow = qMin(static_cast<int>(info.songs), 32);
        for (int i = 1; i <= maxSongsToShow; ++i) {
            QString marker = (i == info.startSong) ? "*" : " ";
            output += QString("  %1 Song %2\n").arg(marker).arg(i);
        }
        if (info.songs > 32) {
            output += QString("  ... and %1 more\n").arg(info.songs - 32);
        }
    }

    // Technical details
    output += "\n--- Technical Info ---\n";

    // SID chip and video standard
    if (info.version >= 2) {
        output += QString("SID Chip: %1\n").arg(sidModelToString(info.sidModel));
        output += QString("Video: %1\n").arg(videoStandardToString(info.videoStandard));

        // Multi-SID addresses
        if (info.secondSidAddress != 0) {
            quint16 addr = 0xD400 + (info.secondSidAddress & 0xFE) * 16;
            output += QString("2nd SID: $%1 (%2)\n")
                .arg(addr, 4, 16, QChar('0')).toUpper()
                .arg(sidModelToString(info.secondSidModel));
        }
        if (info.thirdSidAddress != 0) {
            quint16 addr = 0xD400 + (info.thirdSidAddress & 0xFE) * 16;
            output += QString("3rd SID: $%1 (%2)\n")
                .arg(addr, 4, 16, QChar('0')).toUpper()
                .arg(sidModelToString(info.thirdSidModel));
        }
    }

    // Memory addresses
    output += QString("\nLoad:  $%1\n").arg(info.loadAddress, 4, 16, QChar('0')).toUpper();
    output += QString("Init:  $%1\n").arg(info.initAddress, 4, 16, QChar('0')).toUpper();
    if (info.playAddress != 0) {
        output += QString("Play:  $%1\n").arg(info.playAddress, 4, 16, QChar('0')).toUpper();
    } else {
        output += "Play:  (uses IRQ)\n";
    }

    // Special flags
    if (info.format == Format::RSID) {
        output += "\nRSID: Requires real C64 environment\n";
        if (info.basicFlag) {
            output += "Contains BASIC program\n";
        }
    }

    return output;
}

QString SidFileParser::sidModelToString(SidModel model)
{
    switch (model) {
    case SidModel::MOS6581:
        return "MOS 6581";
    case SidModel::MOS8580:
        return "MOS 8580";
    case SidModel::Both:
        return "6581/8580";
    default:
        return "Unknown";
    }
}

QString SidFileParser::videoStandardToString(VideoStandard standard)
{
    switch (standard) {
    case VideoStandard::PAL:
        return "PAL (50Hz)";
    case VideoStandard::NTSC:
        return "NTSC (60Hz)";
    case VideoStandard::Both:
        return "PAL/NTSC";
    default:
        return "Unknown";
    }
}

quint16 SidFileParser::readWord(const QByteArray &data, int offset)
{
    if (offset + 1 >= data.size()) {
        return 0;
    }
    // Big-endian
    return (static_cast<quint8>(data.at(offset)) << 8) |
           static_cast<quint8>(data.at(offset + 1));
}

quint32 SidFileParser::readLongword(const QByteArray &data, int offset)
{
    if (offset + 3 >= data.size()) {
        return 0;
    }
    // Big-endian
    return (static_cast<quint8>(data.at(offset)) << 24) |
           (static_cast<quint8>(data.at(offset + 1)) << 16) |
           (static_cast<quint8>(data.at(offset + 2)) << 8) |
           static_cast<quint8>(data.at(offset + 3));
}

QString SidFileParser::readString(const QByteArray &data, int offset, int maxLen)
{
    if (offset >= data.size()) {
        return QString();
    }

    int len = 0;
    while (len < maxLen && (offset + len) < data.size()) {
        if (data.at(offset + len) == '\0') {
            break;
        }
        ++len;
    }

    // SID files use ISO-8859-1 encoding
    return QString::fromLatin1(data.mid(offset, len));
}

SidFileParser::SidModel SidFileParser::decodeSidModel(int bits)
{
    switch (bits) {
    case 1:
        return SidModel::MOS6581;
    case 2:
        return SidModel::MOS8580;
    case 3:
        return SidModel::Both;
    default:
        return SidModel::Unknown;
    }
}

SidFileParser::VideoStandard SidFileParser::decodeVideoStandard(int bits)
{
    switch (bits) {
    case 1:
        return VideoStandard::PAL;
    case 2:
        return VideoStandard::NTSC;
    case 3:
        return VideoStandard::Both;
    default:
        return VideoStandard::Unknown;
    }
}
