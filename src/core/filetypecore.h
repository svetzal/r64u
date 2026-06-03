#ifndef FILETYPECORE_H
#define FILETYPECORE_H

#include <QMetaType>
#include <QString>

namespace filetype {

Q_NAMESPACE

/**
 * @brief C64 ecosystem file types recognized by the application.
 *
 * Canonical definition used throughout the application.
 */
enum class FileType {
    Unknown,
    Directory,
    SidMusic,
    ModMusic,
    Program,
    Cartridge,
    DiskImage,
    TapeImage,
    Rom,
    Config
};
Q_ENUM_NS(FileType)

/**
 * @brief Capabilities that can be performed on a given file type.
 *
 * Used by UI to determine which actions (toolbar buttons, context menu items,
 * double-click behavior) to enable for a selected file.
 */
struct Capabilities
{
    bool canPlay = false;        ///< SID/MOD music playback
    bool canRun = false;         ///< Execute PRG/CRT/disk image
    bool canMount = false;       ///< Mount as disk in drive
    bool canLoadConfig = false;  ///< Load device configuration
};

/**
 * @brief Default action to perform on double-click for a file type.
 */
enum class DefaultAction {
    None,       ///< No default action
    Play,       ///< Play music (SID/MOD)
    Run,        ///< Run program (PRG/CRT)
    Mount,      ///< Mount disk image (D64/D71/D81/G64/G71)
    LoadConfig  ///< Load device configuration (CFG)
};

/**
 * @brief Detect the C64 file type from a filename based on its extension.
 * @param filename The filename (with extension) to classify.
 * @return The detected FileType, or FileType::Unknown if unrecognized.
 */
[[nodiscard]] FileType detectFromFilename(const QString &filename);

/**
 * @brief Get a translatable display name for a file type.
 * @param type The file type to describe.
 * @return Human-readable type string (e.g., "SID Music", "Disk Image").
 */
[[nodiscard]] QString displayName(FileType type);

/**
 * @brief Get the action capabilities for a file type.
 * @param type The file type to query.
 * @return Capabilities struct indicating which operations are valid.
 */
[[nodiscard]] Capabilities capabilities(FileType type);

/**
 * @brief Get the default action for a file type on double-click.
 * @param type The file type.
 * @return The action to perform, or DefaultAction::None.
 */
[[nodiscard]] DefaultAction defaultAction(FileType type);

}  // namespace filetype

#endif  // FILETYPECORE_H
