#ifndef PIXELICONCORE_H
#define PIXELICONCORE_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>

/**
 * @brief The r64u pixel icon set (visual.md section 7), as pure data.
 *
 * Every icon is a 16x16 grid drawn with one character per pixel. Each character
 * names a colour from the VIC-II "Pepto" palette ('.' is transparent, 'k' is the
 * black outline every shape carries). This header has no Qt dependency; rendering
 * to a QIcon lives in `ui/pixelicons.h`.
 *
 * Drawing rules for the set:
 * - one 1px 'k' outline around every shape, hard edges only;
 * - one to four fill colours per icon, shapes centred with 1px of breathing room;
 * - icons that come in pairs (Start/Stop, Save/Load, Up/Download/Upload, ...)
 *   share a silhouette so the pair reads as related.
 */
namespace pixeliconcore {

/// Icons are drawn on a square grid this many pixels on a side.
inline constexpr int Size = 16;

/// An opaque 8-bit RGB colour.
struct Rgb
{
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
};

/// One palette character and the colour it paints.
struct PaletteEntry
{
    char key;
    Rgb rgb;
};

/// The transparent pixel character.
inline constexpr char Transparent = '.';

/// The outline character; every icon must use it.
inline constexpr char Outline = 'k';

/// The VIC-II Pepto palette keyed by the characters used in the icon rows.
inline constexpr std::array<PaletteEntry, 16> Palette{{
    {'k', {0x00, 0x00, 0x00}},  // black (outline)
    {'w', {0xFF, 0xFF, 0xFF}},  // white
    {'r', {0x9F, 0x4E, 0x44}},  // red
    {'c', {0x6A, 0xBF, 0xC6}},  // cyan
    {'p', {0xA0, 0x57, 0xA3}},  // purple
    {'g', {0x5C, 0xAB, 0x5E}},  // green
    {'b', {0x50, 0x45, 0x9B}},  // blue
    {'y', {0xC9, 0xD4, 0x87}},  // yellow
    {'o', {0xA1, 0x68, 0x3C}},  // orange
    {'n', {0x6D, 0x54, 0x12}},  // brown
    {'R', {0xCB, 0x7E, 0x75}},  // light red
    {'d', {0x62, 0x62, 0x62}},  // dark grey
    {'m', {0x89, 0x89, 0x89}},  // mid grey
    {'G', {0x9A, 0xE2, 0x9B}},  // light green
    {'l', {0x88, 0x7E, 0xCB}},  // light blue
    {'L', {0xAD, 0xAD, 0xAD}},  // light grey
}};

/**
 * @brief The colour a palette character paints, or nullopt for transparent /
 *        unknown characters.
 */
[[nodiscard]] inline std::optional<Rgb> colourFor(char key)
{
    const auto entry = std::find_if(Palette.begin(), Palette.end(),
                                    [key](const PaletteEntry &e) { return e.key == key; });
    if (entry == Palette.end()) {
        return std::nullopt;
    }
    return entry->rgb;
}

/// True when @p key is either transparent or one of the palette colours.
[[nodiscard]] inline bool isPaletteChar(char key)
{
    return key == Transparent || colourFor(key).has_value();
}

/// Every icon in the set, grouped by where it is used.
enum class Icon {
    // System toolbar
    Connect,
    Disconnect,
    Reset,
    Reboot,
    Pause,
    Resume,
    Menu,
    PowerOff,
    Preferences,
    // Explore
    Play,
    Run,
    Mount,
    Refresh,
    StarOutline,
    StarFilled,
    Favorites,
    Up,
    Eject,
    // Transfer
    Download,
    Upload,
    NewFolder,
    Rename,
    Delete,
    Cancel,
    // View
    StartStream,
    StopStream,
    Screenshot,
    Record,
    StopRecording,
    Stats,
    FullScreen,
    // Config
    SaveToFlash,
    LoadFromFlash,
    ResetDefaults,
    // Playlist
    PlaylistPlay,
    PlaylistStop,
    Previous,
    Next,
    Shuffle,
    Repeat,
    RepeatOne,
    Save,
    Load,
    Clear,
    // File types
    Folder,
    File,
    Program,
    Sid,
    Mod,
    Cartridge,
    DiskImage,
    Tape,
    Rom,
    Config,
};

/// Every Icon value, for iteration in tests and tooling.
inline constexpr std::array<Icon, 54> AllIcons{
    Icon::Connect,
    Icon::Disconnect,
    Icon::Reset,
    Icon::Reboot,
    Icon::Pause,
    Icon::Resume,
    Icon::Menu,
    Icon::PowerOff,
    Icon::Preferences,
    Icon::Play,
    Icon::Run,
    Icon::Mount,
    Icon::Refresh,
    Icon::StarOutline,
    Icon::StarFilled,
    Icon::Favorites,
    Icon::Up,
    Icon::Eject,
    Icon::Download,
    Icon::Upload,
    Icon::NewFolder,
    Icon::Rename,
    Icon::Delete,
    Icon::Cancel,
    Icon::StartStream,
    Icon::StopStream,
    Icon::Screenshot,
    Icon::Record,
    Icon::StopRecording,
    Icon::Stats,
    Icon::FullScreen,
    Icon::SaveToFlash,
    Icon::LoadFromFlash,
    Icon::ResetDefaults,
    Icon::PlaylistPlay,
    Icon::PlaylistStop,
    Icon::Previous,
    Icon::Next,
    Icon::Shuffle,
    Icon::Repeat,
    Icon::RepeatOne,
    Icon::Save,
    Icon::Load,
    Icon::Clear,
    Icon::Folder,
    Icon::File,
    Icon::Program,
    Icon::Sid,
    Icon::Mod,
    Icon::Cartridge,
    Icon::DiskImage,
    Icon::Tape,
    Icon::Rom,
    Icon::Config,
};

/// The 16 rows of an icon, top to bottom; each row is exactly 16 characters.
using Rows = std::array<const char *, Size>;

namespace detail {

// ---------------------------------------------------------------------------
// System toolbar
// ---------------------------------------------------------------------------

/// A plug seated in its socket bar.
inline constexpr Rows kConnect{
    "................", ".kkkkkkkkkkkkkk.", ".kdddkddddkdddk.", ".kkkkkkkkkkkkkk.",
    "....kmk..kmk....", "....kmk..kmk....", "..kkkkkkkkkkkk..", "..kwwwwwwwwwwk..",
    "..kwLLLLLLLLLk..", "..kLLLLLLLLLLk..", "..kLLLLLLLLLLk..", "...kkkkkkkkkk...",
    "......kddk......", "......kddk......", ".......kk.......", "................",
};

/// The same plug pulled a pixel clear of its socket.
inline constexpr Rows kDisconnect{
    "................", ".kkkkkkkkkkkkkk.", ".kdddkddddkdddk.", ".kkkkkkkkkkkkkk.",
    "................", "....kmk..kmk....", "....kmk..kmk....", "..kkkkkkkkkkkk..",
    "..kwwwwwwwwwwk..", "..kwLLLLLLLLLk..", "..kLLLLLLLLLLk..", "..kLLLLLLLLLLk..",
    "...kkkkkkkkkk...", "......kddk......", ".......kk.......", "................",
};

/// One clockwise circular arrow.
inline constexpr Rows kReset{
    "................", ".......kkkk.....", ".....kkkcck.....", "....kcccccck....",
    "...kcckkcck.....", "..kcck.kkk......", "..kck......kkk..", "..kck......kck..",
    "..kck......kck..", "..kck......kck..", "..kcck....kcck..", "...kcckkkkcck...",
    "....kcccccck....", ".....kkkkkk.....", "................", "................",
};

/// Two chasing circular arrows.
inline constexpr Rows kRefresh{
    "................", ".......kkkk.....", ".....kkkcck.....", "....kcccccck....",
    "...kcckkcck.....", "..kcck.kkk......", "..kck......kkk..", "..kck......kck..",
    "..kck......kck..", "..kkk......kck..", "......kkk.kcck..", ".....kcckkcck...",
    "....kcccccck....", ".....kcckkk.....", ".....kkkk.......", "................",
};

/// The power symbol: a broken ring with a bar through the gap.
inline constexpr Rows kReboot{
    "................", "......kkkk......", "...kkkkcckkkk...", "...kcckcckcck...",
    "..kcckkcckkcck..", ".kcck.kcck.kcck.", ".kck..kcck..kck.", ".kck..kcck..kck.",
    ".kck..kkkk..kck.", ".kck........kck.", ".kcck......kcck.", "..kcck....kcck..",
    "...kcckkkkcck...", "....kcccccck....", ".....kkkkkk.....", "................",
};

/// The power symbol again, in red.
inline constexpr Rows kPowerOff{
    "................", "......kkkk......", "...kkkkRRkkkk...", "...krrkRRkrrk...",
    "..krrkkRRkkrrk..", ".krrk.kRRk.krrk.", ".krk..kRRk..krk.", ".krk..kRRk..krk.",
    ".krk..kkkk..krk.", ".krk........krk.", ".krrk......krrk.", "..krrk....krrk..",
    "...krrkkkkrrk...", "....krrrrrrk....", ".....kkkkkk.....", "................",
};

inline constexpr Rows kPause{
    "................", "................", "..kkkkk..kkkkk..", "..kooRk..kooRk..",
    "..kooRk..kooRk..", "..kooRk..kooRk..", "..kooRk..kooRk..", "..kooRk..kooRk..",
    "..kooRk..kooRk..", "..kooRk..kooRk..", "..kooRk..kooRk..", "..kooRk..kooRk..",
    "..kooRk..kooRk..", "..kkkkk..kkkkk..", "................", "................",
};

/// A play triangle; shared by Resume and PlaylistPlay.
inline constexpr Rows kPlayTriangle{
    "................", "................", "...kk...........", "...kgkk.........",
    "...kgggkk.......", "...kggGggkk.....", "...kggGggggkk...", "...kggGgggggggk.",
    "...kggGggggkk...", "...kggGggkk.....", "...kgggkk.......", "...kgkk.........",
    "...kk...........", "................", "................", "................",
};

/// A stop square; shared by PlaylistStop and StopRecording.
inline constexpr Rows kStopSquare{
    "................", "................", "...kkkkkkkkkk...", "...krrrrrrrrk...",
    "...krrrrrrrrk...", "...krrrrrrrrk...", "...krrrrrrrrk...", "...krrrrrrrrk...",
    "...krrrrrrrrk...", "...krrrrrrrrk...", "...krrrrrrrrk...", "...krrrrrrrrk...",
    "...kkkkkkkkkk...", "................", "................", "................",
};

/// The Ultimate menu as it appears: a C64 screen listing three items.
inline constexpr Rows kMenu{
    "................", "................", "................", ".kkkkkkkkkkkkkk.",
    ".kllllllllllllk.", ".klbbbbbbbbbblk.", ".klbllllllbbblk.", ".klbbbbbbbbbblk.",
    ".klbllllllllblk.", ".klbbbbbbbbbblk.", ".klblllllbbbblk.", ".klbbbbbbbbbblk.",
    ".kllllllllllllk.", ".kkkkkkkkkkkkkk.", "................", "................",
};

/// An eight-toothed gear.
inline constexpr Rows kPreferences{
    "................", "......kkkk......", "...kk.kmmk.kk...", "..kmmkkmmkkmmk..",
    "..kmmmmmmmmmmk..", "...kmmkkkkmmk...", ".kkkmmk..kmmkkk.", ".kmmmmk..kmmmmk.",
    ".kmmmmk..kmmmmk.", ".kkkmmk..kmmkkk.", "...kmmkkkkmmk...", "..kmmmmmmmmmmk..",
    "..kmmkkmmkkmmk..", "...kk.kmmk.kk...", "......kkkk......", "................",
};

// ---------------------------------------------------------------------------
// Explore
// ---------------------------------------------------------------------------

/// A beamed pair of quavers.
inline constexpr Rows kPlay{
    "................", "................", ".....kkkkkkkk...", ".....kGGGGGGk...",
    ".....kggggggk...", ".....kgkkkkgk...", ".....kgk..kgk...", ".....kgk..kgk...",
    ".....kgk..kgk...", "..kkkkgk.kkkgk..", ".kggggggkgggggk.", ".kggggggkgggggk.",
    ".kgggggkkggggk..", "..kkkkk..kkkkk..", "................", "................",
};

/// A BASIC prompt: a bold chevron and a cursor block.
inline constexpr Rows kRun{
    "................", "..kk............", "..klkk..........", "..klllkk........",
    "...kllllkk......", "....kllllkk.....", ".....kllllk.....", "....kllllkk.....",
    "...kllllkk......", "..klllkk........", "..klkk..........", "..kk............",
    "........kkkkkk..", "........kbbbbk..", "........kkkkkk..", "................",
};

/// A 5.25" disk sliding into a drive slot.
inline constexpr Rows kMount{
    "................", "....kkkkkkkk....", "....kddddddk....", "....kdkkkkdk....",
    "....kdkwwkdk....", "....kdkwwkdk....", "....kdkkkkdk....", ".kkkkddddddkkkk.",
    ".kLLkddddddkLLk.", ".kLLkkkkkkkkLLk.", ".kLLLLLLLLLLLLk.", ".kLLLLLLLLLGLLk.",
    ".kLLLLLLLLLLLLk.", ".kkkkkkkkkkkkkk.", "................", "................",
};

/// A five-pointed star in light grey (the "not a favourite" state).
inline constexpr Rows kStarOutline{
    "................", ".......k........", "......kLk.......", "......kLk.......",
    ".....kLLLk......", ".kkkkkLLLkkkkk..", "..kLLLLLLLLLk...", "...kLLLLLLLk....",
    "....kLLLLLk.....", "....kLLLLLk.....", "...kLLLLLLLk....", "...kLLLkLLLk....",
    "..kLLk...kLLk...", ".kkkk.....kkkk..", "................", "................",
};

/// The same star filled yellow.
inline constexpr Rows kStarFilled{
    "................", ".......k........", "......kyk.......", "......kyk.......",
    ".....kyyyk......", ".kkkkkyyykkkkk..", "..kyyyyyyyyyk...", "...kyyyyyyyk....",
    "....kyyyyyk.....", "....kyyyyyk.....", "...kyyyyyyyk....", "...kyyykyyyk....",
    "..kyyk...kyyk...", ".kkkk.....kkkk..", "................", "................",
};

/// An orange folder wearing a yellow star.
inline constexpr Rows kFavorites{
    "................", "................", ".kkkkkk.........", ".knnnnnkkkkkkk..",
    ".knnnnnnnnnnnnk.", ".kkkkkkkkkkkkkk.", ".koooooyooooook.", ".kooooyyyoooook.",
    ".kooyyyyyyyoook.", ".koooyyyyyooook.", ".koooyyyyyooook.", ".kooyyyoyyyoook.",
    ".kkkkkkkkkkkkkk.", "................", "................", "................",
};

/// A plain up arrow.
inline constexpr Rows kUp{
    "................", "................", ".......k........", "......kck.......",
    ".....kccck......", "....kccccck.....", "...kccccccck....", "..kkkkccckkkk...",
    ".....kccck......", ".....kccck......", ".....kccck......", ".....kccck......",
    ".....kccck......", ".....kkkkk......", "................", "................",
};

/// Triangle over a bar.
inline constexpr Rows kEject{
    "................", "................", ".......k........", "......kck.......",
    ".....kccck......", "....kccccck.....", "...kccccccck....", "..kccccccccck...",
    ".kkkkkkkkkkkkk..", "................", ".kkkkkkkkkkkkk..", ".kccccccccccck..",
    ".kkkkkkkkkkkkk..", "................", "................", "................",
};

// ---------------------------------------------------------------------------
// Transfer
// ---------------------------------------------------------------------------

/// Arrow down into a tray.
inline constexpr Rows kDownload{
    "................", ".....kkkkk......", ".....kccck......", ".....kccck......",
    ".....kccck......", "..kkkkccckkkk...", "...kccccccck....", "....kccccck.....",
    ".....kccck......", "......kck.......", ".......k........", ".kkk.......kkk..",
    ".kLkkkkkkkkkLk..", ".kLLLLLLLLLLLk..", ".kkkkkkkkkkkkk..", "................",
};

/// Arrow up out of a tray.
inline constexpr Rows kUpload{
    "................", ".......k........", "......kck.......", ".....kccck......",
    "....kccccck.....", "...kccccccck....", "..kkkkccckkkk...", ".....kccck......",
    ".....kccck......", ".....kccck......", ".....kkkkk......", ".kkk.......kkk..",
    ".kLkkkkkkkkkLk..", ".kLLLLLLLLLLLk..", ".kkkkkkkkkkkkk..", "................",
};

/// A folder with a green plus.
inline constexpr Rows kNewFolder{
    "................", "................", ".kkkkkk.........", ".koooookkkkkkk..",
    ".kooooooooooook.", ".kkkkkkkkkkkkkk.", ".kyyyyykkkyyyyk.", ".kyyyyykgkyyyyk.",
    ".kyykkkkgkkkkyk.", ".kyykgggggggkyk.", ".kyykkkkgkkkkyk.", ".kyyyyykgkyyyyk.",
    ".kkkkkkkkkkkkkk.", "................", "................", "................",
};

/// A pencil.
inline constexpr Rows kRename{
    "................", "...........kkk..", "..........kRRRk.", ".........kRRRRk.",
    "........kyykRk..", ".......kyyyyk...", "......kyyyyk....", ".....kyyyyk.....",
    "....kyyyyk......", "...kyyyyk.......", "..kkyyyk........", "..kdkkk.........",
    "..kkk...........", "................", "................", "................",
};

/// A bin with a lid.
inline constexpr Rows kDelete{
    "................", "......kkkk......", ".kkkkkkddkkkkkk.", ".krrrrrrrrrrrrk.",
    ".kkkkkkkkkkkkkk.", "..krrrrrrrrrrk..", "..krrrRrrRrrrk..", "..krrrRrrRrrrk..",
    "..krrrRrrRrrrk..", "..krrrRrrRrrrk..", "..krrrRrrRrrrk..", "..krrrRrrRrrrk..",
    "..krrrrrrrrrrk..", "..kkkkkkkkkkkk..", "................", "................",
};

/// A cross in a grey disc.
inline constexpr Rows kCancel{
    "................", "................", ".....kkkkkk.....", "...kkddddddkk...",
    "..kdwddddddwdk..", "..kddwddddwddk..", ".kddddwddwddddk.", ".kdddddwwdddddk.",
    ".kdddddwwdddddk.", ".kddddwddwddddk.", "..kddwddddwddk..", "..kdwddddddwdk..",
    "...kkddddddkk...", ".....kkkkkk.....", "................", "................",
};

// ---------------------------------------------------------------------------
// View
// ---------------------------------------------------------------------------

/// A monitor showing a play triangle.
inline constexpr Rows kStartStream{
    "................", ".kkkkkkkkkkkkkk.", ".kllllllllllllk.", ".klbbkkbbbbbblk.",
    ".klbbkgkkbbbblk.", ".klbbkgggkkbblk.", ".klbbkgggggkblk.", ".klbbkgggkkbblk.",
    ".klbbkgkkbbbblk.", ".klbbkkbbbbbblk.", ".kllllllllllllk.", ".kkkkkkkkkkkkkk.",
    "......kddk......", "....kkkddkkk....", "....kkkkkkkk....", "................",
};

/// The same monitor showing a stop square.
inline constexpr Rows kStopStream{
    "................", ".kkkkkkkkkkkkkk.", ".kllllllllllllk.", ".klbbkkkkkkkblk.",
    ".klbbkrrrrrkblk.", ".klbbkrrrrrkblk.", ".klbbkrrrrrkblk.", ".klbbkrrrrrkblk.",
    ".klbbkrrrrrkblk.", ".klbbkkkkkkkblk.", ".kllllllllllllk.", ".kkkkkkkkkkkkkk.",
    "......kddk......", "....kkkddkkk....", "....kkkkkkkk....", "................",
};

/// A camera.
inline constexpr Rows kScreenshot{
    "................", "................", "....kkkkk.......", "....kmmmmk..kkk.",
    ".kkkkkkkkkkkkkk.", ".kmmmmmkkkmmmmk.", ".kmmmmkLLLkmmmk.", ".kmmmkLwccLkmmk.",
    ".kmmmkLcccLkmmk.", ".kmmmkLcccLkmmk.", ".kmmmmkLLLkmmmk.", ".kmmmmmkkkmmmmk.",
    ".kkkkkkkkkkkkkk.", "................", "................", "................",
};

/// A red recording dot.
inline constexpr Rows kRecord{
    "................", "................", ".....kkkkkk.....", "...kkrRRrrrkk...",
    "..krRRrrrrrrrk..", "..krRrrrrrrrrk..", ".krRrrrrrrrrrrk.", ".krrrrrrrrrrrrk.",
    ".krrrrrrrrrrrrk.", ".krrrrrrrrrrrrk.", "..krrrrrrrrrrk..", "..krrrrrrrrrrk..",
    "...kkrrrrrrkk...", ".....kkkkkk.....", "................", "................",
};

/// Three rising bars.
inline constexpr Rows kStats{
    "................", "................", "...........kkkk.", "...........kggk.",
    "...........kggk.", "......kkkk.kggk.", "......kyyk.kggk.", "......kyyk.kggk.",
    ".kkkk.kyyk.kggk.", ".krrk.kyyk.kggk.", ".krrk.kyyk.kggk.", ".krrk.kyyk.kggk.",
    ".krrk.kyyk.kggk.", ".kkkk.kkkk.kkkk.", "................", "................",
};

/// Four corner brackets.
inline constexpr Rows kFullScreen{
    "................", ".kkkkk....kkkkk.", ".kcccck..kcccck.", ".kckkkk..kkkkck.",
    ".kck........kck.", ".kck........kck.", ".kkk........kkk.", "................",
    "................", ".kkk........kkk.", ".kck........kck.", ".kck........kck.",
    ".kckkkk..kkkkck.", ".kcccck..kcccck.", ".kkkkk....kkkkk.", "................",
};

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------

/// Arrow down into a DIP chip.
inline constexpr Rows kSaveToFlash{
    "................", ".....kkkkk......", ".....kgggk......", ".....kgggk......",
    "..kkkkgggkkkk...", "...kgggggggk....", "....kgggggk.....", ".....kgggk......",
    "......kgk.......", "...kkkkkkkkkk...", ".kkkddddddddkkk.", "...kddddddddk...",
    ".kkkddddddddkkk.", "...kddddddddk...", "...kkkkkkkkkk...", "................",
};

/// Arrow up out of a DIP chip.
inline constexpr Rows kLoadFromFlash{
    "................", ".......k........", "......kgk.......", ".....kgggk......",
    "....kgggggk.....", "...kgggggggk....", "..kkkkgggkkkk...", ".....kgggk......",
    ".....kgggk......", "...kkkkkkkkkk...", ".kkkddddddddkkk.", "...kddddddddk...",
    ".kkkddddddddkkk.", "...kddddddddk...", "...kkkkkkkkkk...", "................",
};

/// A return arrow pointing back the way it came.
inline constexpr Rows kResetDefaults{
    "................", "................", "................", "....kk..........",
    "...kcck.........", "..kccckkkkkkkkk.", ".kcccccccccccck.", ".kcccccccccccck.",
    "..kccckkkkkkcck.", "...kcck....kcck.", "....kk.....kcck.", "...........kcck.",
    "...........kkkk.", "................", "................", "................",
};

// ---------------------------------------------------------------------------
// Playlist
// ---------------------------------------------------------------------------

/// Bar then a left-pointing triangle.
inline constexpr Rows kPrevious{
    "................", "................", "..kkk.......kk..", "..kgk.....kkgk..",
    "..kgk...kkgggk..", "..kgk.kkgggggk..", "..kgkkgggggggk..", "..kgkggggggggk..",
    "..kgkkgggggggk..", "..kgk.kkgggggk..", "..kgk...kkgggk..", "..kgk.....kkgk..",
    "..kkk.......kk..", "................", "................", "................",
};

/// Right-pointing triangle then a bar.
inline constexpr Rows kNext{
    "................", "................", "..kk.......kkk..", "..kgkk.....kgk..",
    "..kgggkk...kgk..", "..kgggggkk.kgk..", "..kgggggggkkgk..", "..kggggggggkgk..",
    "..kgggggggkkgk..", "..kgggggkk.kgk..", "..kgggkk...kgk..", "..kgkk.....kgk..",
    "..kk.......kkk..", "................", "................", "................",
};

/// Two crossing arrows.
inline constexpr Rows kShuffle{
    "................", "................", ".kk........kkkk.", ".kck.......kcck.",
    "..kck.....kkcck.", "...kck...kckkk..", "....kck.kck.....", ".....kckck......",
    "......kck.......", ".....kckck......", "....kck.kck.....", "...kck...kckkk..",
    "..kck.....kkcck.", ".kck.......kcck.", ".kk........kkkk.", "................",
};

/// A loop with an arrowhead at each end.
inline constexpr Rows kRepeat{
    "................", "................", "..........kk....", ".kkkkkkkkkkck...",
    ".kcccccccccccck.", ".kckkkkkkkkccck.", ".kck......kkck..", ".kck.......kck..",
    ".kck.......kck..", ".kccckkkkkkkck..", ".kccccccccccck..", "...kckkkkkkkkkk.",
    "....kk..........", "................", "................", "................",
};

/// The loop with a "1" inside.
inline constexpr Rows kRepeatOne{
    "................", "................", "..........kk....", ".kkkkkkkkkkck...",
    ".kcccccccccccck.", ".kckkkkkkkkccck.", ".kck..oo..kkck..", ".kck...o...kck..",
    ".kck..ooo..kck..", ".kccckkkkkkkck..", ".kccccccccccck..", "...kckkkkkkkkkk.",
    "....kk..........", "................", "................", "................",
};

/// A 3.5" floppy with an arrow going into its label.
inline constexpr Rows kSave{
    "................", ".kkkkkkkkkkkkk..", ".kbbkLLLLLkbbk..", ".kbbkLkkLLkbbk..",
    ".kbbkLkkLLkbbk..", ".kbbkkkkkkkbbk..", ".kbbbbbbbbbbbk..", ".kbkkkkkkkkkbk..",
    ".kbkwwwkwwwkbk..", ".kbkwwwkwwwkbk..", ".kbkwkkkkkwkbk..", ".kbkwwkkkwwkbk..",
    ".kbkwwwkwwwkbk..", ".kkkkkkkkkkkkk..", "................", "................",
};

/// The same floppy with the arrow coming out.
inline constexpr Rows kLoad{
    "................", ".kkkkkkkkkkkkk..", ".kbbkLLLLLkbbk..", ".kbbkLkkLLkbbk..",
    ".kbbkLkkLLkbbk..", ".kbbkkkkkkkbbk..", ".kbbbbbbbbbbbk..", ".kbkkkkkkkkkbk..",
    ".kbkwwwkwwwkbk..", ".kbkwwkkkwwkbk..", ".kbkwkkkkkwkbk..", ".kbkwwwkwwwkbk..",
    ".kbkwwwkwwwkbk..", ".kkkkkkkkkkkkk..", "................", "................",
};

/// A list with a red cross over it.
inline constexpr Rows kClear{
    "................", "................", ".kkkkkkk........", ".kLLLLLk........",
    ".kkkkkkk........", "................", ".kkkkkkk........", ".kLLLLLk........",
    ".kkkkkkkkk...kk.", "........krk.krk.", ".kkkkkkk.krkrk..", ".kLLLLLk..krk...",
    ".kkkkkkk.krkrk..", "........krk.krk.", "........kk...kk.", "................",
};

// ---------------------------------------------------------------------------
// File types
// ---------------------------------------------------------------------------

inline constexpr Rows kFolder{
    "................", "................", ".kkkkkk.........", ".koooookkkkkkk..",
    ".kooooooooooook.", ".kkkkkkkkkkkkkk.", ".kyyyyyyyyyyyyk.", ".kyyyyyyyyyyyyk.",
    ".kyyyyyyyyyyyyk.", ".kyyyyyyyyyyyyk.", ".kyyyyyyyyyyyyk.", ".kyyyyyyyyyyyyk.",
    ".kkkkkkkkkkkkkk.", "................", "................", "................",
};

/// A white page with a folded corner.
inline constexpr Rows kFile{
    "................", "...kkkkkkkk.....", "...kwwwwwwkk....", "...kwwwwwwkLk...",
    "...kwwwwwwkkkk..", "...kwwwwwwwwwk..", "...kwLLLLLLLwk..", "...kwwwwwwwwwk..",
    "...kwLLLLLLLwk..", "...kwwwwwwwwwk..", "...kwLLLLLLLwk..", "...kwwwwwwwwwk..",
    "...kwwwwwwwwwk..", "...kkkkkkkkkkk..", "................", "................",
};

/// A blue page with a run chevron.
inline constexpr Rows kProgram{
    "................", "...kkkkkkkk.....", "...kbbbbbbkk....", "...kbbbbbbklk...",
    "...kbbbbbbkkkk..", "...kbbbbbbbbbk..", "...kbkkbbbbbbk..", "...kbkGkkbbbbk..",
    "...kbkGGGkbbbk..", "...kbkGkkbbbbk..", "...kbkkbbbbbbk..", "...kbbbbbbbbbk..",
    "...kbbbbbbbbbk..", "...kkkkkkkkkkk..", "................", "................",
};

/// A yellow page with a quaver.
inline constexpr Rows kSid{
    "................", "...kkkkkkkk.....", "...kyyyyyykk....", "...kyyyyyykLk...",
    "...kyyyyyykkkk..", "...kyyyyykkyyk..", "...kyyyyykykyk..", "...kyyyyykykyk..",
    "...kyyyyykyyyk..", "...kyyykkkyyyk..", "...kyykkkkyyyk..", "...kyyykkkyyyk..",
    "...kyyyyyyyyyk..", "...kkkkkkkkkkk..", "................", "................",
};

/// A light green page with a waveform.
inline constexpr Rows kMod{
    "................", "...kkkkkkkk.....", "...kGGGGGGkk....", "...kGGGGGGkLk...",
    "...kGGGGGGkkkk..", "...kGGGGGGGGGk..", "...kGGGkGGGGGk..", "...kGGkGkGGGGk..",
    "...kGkGGGkGkGk..", "...kGGGGGGkGGk..", "...kGGGGGGGGGk..", "...kGGGGGGGGGk..",
    "...kGGGGGGGGGk..", "...kkkkkkkkkkk..", "................", "................",
};

/// A purple cartridge with its edge connector.
inline constexpr Rows kCartridge{
    "................", "................", "..kkkkkkkkkkkk..", "..kppppppppppk..",
    "..kpkkkkkkkkpk..", "..kpkwwwwwwkpk..", "..kpkwwwwwwkpk..", "..kpkkkkkkkkpk..",
    "..kppppppppppk..", "..kppppppppppk..", "..kkkkkkkkkkkk..", "....kppppppk....",
    "....kkkkkkkk....", "................", "................", "................",
};

/// A 5.25" floppy: dark sleeve, white label, hub hole.
inline constexpr Rows kDiskImage{
    "................", ".kkkkkkkkkkkkkk.", ".kdkkkkkkkkkkdk.", ".kdkwwwwwwwwkdk.",
    ".kdkwwwwwwwwkdk.", ".kdkkkkkkkkkkdk.", ".kddddddddddddk.", ".kddddkkkkddddk.",
    ".kdddknnnnkdddk.", ".kdddknnnnkdddk.", ".kddddkkkkddddk.", ".kddddddddddddk.",
    ".kddddddddddddk.", ".kkkkkkkkkkkkkk.", "................", "................",
};

/// A cassette.
inline constexpr Rows kTape{
    "................", "................", "................", ".kkkkkkkkkkkkkk.",
    ".kooooooooooook.", ".kokkkkkkkkkkok.", ".kokwwwwwwwwkok.", ".kokwkkwwkkwkok.",
    ".kokwkkwwkkwkok.", ".kokkkkkkkkkkok.", ".kooooooooooook.", ".kooknnnnnnkook.",
    ".kkkkkkkkkkkkkk.", "................", "................", "................",
};

/// A DIP chip.
inline constexpr Rows kRom{
    "................", "................", "................", "...kkkkkkkkkk...",
    ".kkkdddmmdddkkk.", "...kddddddddk...", ".kkkddddddddkkk.", "...kddddddddk...",
    ".kkkddddddddkkk.", "...kddddddddk...", ".kkkddddddddkkk.", "...kddddddddk...",
    "...kkkkkkkkkk...", "................", "................", "................",
};

/// A wrench.
inline constexpr Rows kConfig{
    "................", "........kk..kk..", ".......kmmk.kmk.", ".......kmmkkkmk.",
    ".......kmmmmmmk.", "........kmmmmk..", ".......kmmmkk...", "......kmmmk.....",
    ".....kmmmk......", "....kmmmk.......", "...kmmmk........", "..kmmmk.........",
    ".kmmmk..........", ".kkkk...........", "................", "................",
};

}  // namespace detail

/**
 * @brief The 16 rows of pixels that make up @p icon.
 */
[[nodiscard]] inline const Rows &rows(Icon icon)
{
    using namespace detail;
    switch (icon) {
    case Icon::Connect:
        return kConnect;
    case Icon::Disconnect:
        return kDisconnect;
    case Icon::Reset:
        return kReset;
    case Icon::Reboot:
        return kReboot;
    case Icon::Pause:
        return kPause;
    case Icon::Resume:
        return kPlayTriangle;
    case Icon::Menu:
        return kMenu;
    case Icon::PowerOff:
        return kPowerOff;
    case Icon::Preferences:
        return kPreferences;
    case Icon::Play:
        return kPlay;
    case Icon::Run:
        return kRun;
    case Icon::Mount:
        return kMount;
    case Icon::Refresh:
        return kRefresh;
    case Icon::StarOutline:
        return kStarOutline;
    case Icon::StarFilled:
        return kStarFilled;
    case Icon::Favorites:
        return kFavorites;
    case Icon::Up:
        return kUp;
    case Icon::Eject:
        return kEject;
    case Icon::Download:
        return kDownload;
    case Icon::Upload:
        return kUpload;
    case Icon::NewFolder:
        return kNewFolder;
    case Icon::Rename:
        return kRename;
    case Icon::Delete:
        return kDelete;
    case Icon::Cancel:
        return kCancel;
    case Icon::StartStream:
        return kStartStream;
    case Icon::StopStream:
        return kStopStream;
    case Icon::Screenshot:
        return kScreenshot;
    case Icon::Record:
        return kRecord;
    case Icon::StopRecording:
        return kStopSquare;
    case Icon::Stats:
        return kStats;
    case Icon::FullScreen:
        return kFullScreen;
    case Icon::SaveToFlash:
        return kSaveToFlash;
    case Icon::LoadFromFlash:
        return kLoadFromFlash;
    case Icon::ResetDefaults:
        return kResetDefaults;
    case Icon::PlaylistPlay:
        return kPlayTriangle;
    case Icon::PlaylistStop:
        return kStopSquare;
    case Icon::Previous:
        return kPrevious;
    case Icon::Next:
        return kNext;
    case Icon::Shuffle:
        return kShuffle;
    case Icon::Repeat:
        return kRepeat;
    case Icon::RepeatOne:
        return kRepeatOne;
    case Icon::Save:
        return kSave;
    case Icon::Load:
        return kLoad;
    case Icon::Clear:
        return kClear;
    case Icon::Folder:
        return kFolder;
    case Icon::File:
        return kFile;
    case Icon::Program:
        return kProgram;
    case Icon::Sid:
        return kSid;
    case Icon::Mod:
        return kMod;
    case Icon::Cartridge:
        return kCartridge;
    case Icon::DiskImage:
        return kDiskImage;
    case Icon::Tape:
        return kTape;
    case Icon::Rom:
        return kRom;
    case Icon::Config:
        return kConfig;
    }
    return kFile;
}

}  // namespace pixeliconcore

#endif  // PIXELICONCORE_H
