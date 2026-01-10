/**
 * @file sidfileparser.h
 * @brief Parser for Commodore SID music file format.
 *
 * Parses PSID/RSID file headers to extract metadata for display.
 * Supports v1-v4 formats including multi-SID (2SID/3SID) files.
 */

#ifndef SIDFILEPARSER_H
#define SIDFILEPARSER_H

#include <QByteArray>
#include <QString>
#include <QList>

/**
 * @brief Parser for Commodore SID music files.
 *
 * This class parses the header of SID files to extract metadata
 * such as title, author, release info, technical addresses, and
 * chip/timing requirements.
 */
class SidFileParser
{
public:
    /// @name Header Constants
    /// @{
    static constexpr int MinHeaderSize = 0x76;      ///< Minimum v1 header size
    static constexpr int V2HeaderSize = 0x7C;       ///< v2+ header size
    static constexpr int MagicOffset = 0x00;        ///< Magic ID offset
    static constexpr int VersionOffset = 0x04;      ///< Version field offset
    static constexpr int DataOffsetOffset = 0x06;   ///< Data offset field offset
    static constexpr int LoadAddrOffset = 0x08;     ///< Load address offset
    static constexpr int InitAddrOffset = 0x0A;     ///< Init address offset
    static constexpr int PlayAddrOffset = 0x0C;     ///< Play address offset
    static constexpr int SongsOffset = 0x0E;        ///< Number of songs offset
    static constexpr int StartSongOffset = 0x10;    ///< Start song offset
    static constexpr int SpeedOffset = 0x12;        ///< Speed flags offset
    static constexpr int TitleOffset = 0x16;        ///< Title string offset
    static constexpr int AuthorOffset = 0x36;       ///< Author string offset
    static constexpr int ReleasedOffset = 0x56;     ///< Released string offset
    static constexpr int FlagsOffset = 0x76;        ///< Flags offset (v2+)
    static constexpr int SecondSidOffset = 0x7A;    ///< Second SID address (v3+)
    static constexpr int ThirdSidOffset = 0x7B;     ///< Third SID address (v4)
    static constexpr int StringLength = 32;         ///< Length of text fields
    /// @}

    /**
     * @brief SID file format type.
     */
    enum class Format {
        Unknown,    ///< Not a valid SID file
        PSID,       ///< PlaySID format (C64 compatible)
        RSID        ///< Real SID format (requires real C64 environment)
    };

    /**
     * @brief SID chip model.
     */
    enum class SidModel {
        Unknown,    ///< Not specified
        MOS6581,    ///< Original SID chip
        MOS8580,    ///< New SID chip (C64C/128)
        Both        ///< Works on both models
    };

    /**
     * @brief Video standard / clock speed.
     */
    enum class VideoStandard {
        Unknown,    ///< Not specified
        PAL,        ///< European 50Hz
        NTSC,       ///< US/Japan 60Hz
        Both        ///< Works on both
    };

    /**
     * @brief Parsed SID file information.
     */
    struct SidInfo {
        bool valid = false;             ///< True if parsing succeeded
        Format format = Format::Unknown;
        quint16 version = 0;            ///< Format version (1-4)
        quint16 dataOffset = 0;         ///< Offset to music data
        quint16 loadAddress = 0;        ///< Memory load address
        quint16 initAddress = 0;        ///< Init routine address
        quint16 playAddress = 0;        ///< Play routine address
        quint16 songs = 0;              ///< Number of sub-tunes
        quint16 startSong = 0;          ///< Default song (1-indexed)
        quint32 speed = 0;              ///< Speed flags per song
        QString title;                  ///< Song title
        QString author;                 ///< Composer name
        QString released;               ///< Release/copyright info

        // v2+ extended fields
        SidModel sidModel = SidModel::Unknown;
        VideoStandard videoStandard = VideoStandard::Unknown;
        bool musPlayer = false;         ///< Uses external MUS player
        bool playsSamples = false;      ///< Uses PlaySID sample tricks
        bool basicFlag = false;         ///< RSID with BASIC program

        // Multi-SID (v3/v4)
        quint8 secondSidAddress = 0;    ///< Second SID at $D4xx (0 = none)
        quint8 thirdSidAddress = 0;     ///< Third SID at $D4xx (0 = none)
        SidModel secondSidModel = SidModel::Unknown;
        SidModel thirdSidModel = SidModel::Unknown;
    };

    /**
     * @brief Checks if a file path appears to be a SID file.
     * @param path File path to check.
     * @return True if the file has a .sid extension.
     */
    [[nodiscard]] static bool isSidFile(const QString &path);

    /**
     * @brief Parses SID file data.
     * @param data Raw file contents.
     * @return Parsed SID information.
     */
    [[nodiscard]] static SidInfo parse(const QByteArray &data);

    /**
     * @brief Formats SID info for display.
     * @param info Parsed SID information.
     * @return Formatted text string for display.
     */
    [[nodiscard]] static QString formatForDisplay(const SidInfo &info);

    /**
     * @brief Converts SidModel enum to display string.
     */
    [[nodiscard]] static QString sidModelToString(SidModel model);

    /**
     * @brief Converts VideoStandard enum to display string.
     */
    [[nodiscard]] static QString videoStandardToString(VideoStandard standard);

private:
    /**
     * @brief Reads a big-endian 16-bit word from data.
     */
    [[nodiscard]] static quint16 readWord(const QByteArray &data, int offset);

    /**
     * @brief Reads a big-endian 32-bit longword from data.
     */
    [[nodiscard]] static quint32 readLongword(const QByteArray &data, int offset);

    /**
     * @brief Reads a null-terminated string from data.
     */
    [[nodiscard]] static QString readString(const QByteArray &data, int offset, int maxLen);

    /**
     * @brief Decodes SID model from 2-bit flag value.
     */
    [[nodiscard]] static SidModel decodeSidModel(int bits);

    /**
     * @brief Decodes video standard from 2-bit flag value.
     */
    [[nodiscard]] static VideoStandard decodeVideoStandard(int bits);
};

#endif // SIDFILEPARSER_H
