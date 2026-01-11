#include "petsciiconverter.h"

/**
 * PETSCII to ASCII lookup table
 * Based on Ultimate64/Ultimate-II Control Library for Amiga OS 3.x
 *
 * Graphics characters are mapped to spaces for basic display.
 * Control codes are mapped to 0 (non-printable).
 */
const char PetsciiConverter::petsciiToAsciiTable[256] = {
    // 0x00-0x1F: Control codes
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '\n', 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    // 0x20-0x3F: Space, punctuation, numbers
    ' ', '!', '"', '#', '$', '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.', '/',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?',

    // 0x40-0x5F: @, PETSCII uppercase A-Z, special chars
    '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[', '\\', ']', '^', '_',

    // 0x60-0x7F: Graphics characters (Shift + letter keys)
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',

    // 0x80-0x9F: Control codes (colors, reverse, etc.)
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',

    // 0xA0-0xBF: Shifted graphics characters
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',

    // 0xC0-0xDF: PETSCII lowercase letters and symbols
    ' ',  // 0xC0
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',  // 0xC1-0xCF
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',  // 0xD0-0xDA
    ' ', ' ', ' ', ' ', ' ',  // 0xDB-0xDF

    // 0xE0-0xFF: Shifted graphics characters
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '
};

/**
 * ASCII to PETSCII lookup table
 * Based on Ultimate64/Ultimate-II Control Library for Amiga OS 3.x
 */
const quint8 PetsciiConverter::asciiToPetsciiTable[128] = {
    // 0x00-0x1F: Control codes
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0D, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,  // \n (0x0A) -> RETURN (0x0D)
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,

    // 0x20-0x3F: Space, punctuation, numbers (direct mapping)
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,  // !"#$%&'
    0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,  // ()*+,-./
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,  // 01234567
    0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,  // 89:;<=>?

    // 0x40-0x5F: @, uppercase A-Z -> PETSCII uppercase
    0x40,  // @
    0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,  // A-G
    0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,  // H-O
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,  // P-W
    0x58, 0x59, 0x5A,  // X-Z
    0x5B, 0x5C, 0x5D, 0x5E, 0x5F,  // [\]^_

    // 0x60-0x7F: backtick, lowercase a-z -> PETSCII lowercase (0xC1-0xDA)
    0x60,  // `
    0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,  // a-g
    0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,  // h-o
    0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7,  // p-w
    0xD8, 0xD9, 0xDA,  // x-z
    0x7B, 0x7C, 0x7D, 0x7E, 0x7F  // {|}~DEL
};

QString PetsciiConverter::toAscii(const QByteArray &data, bool stopAtPadding)
{
    QString result;
    result.reserve(data.size());

    for (int i = 0; i < data.size(); i++) {
        quint8 petscii = static_cast<quint8>(data[i]);

        // $A0 is shift-space (padding character in filenames)
        if (stopAtPadding && petscii == 0xA0) {
            break;
        }

        // Null - end of string
        if (petscii == 0x00) {
            break;
        }

        char ascii = petsciiToAsciiTable[petscii];
        if (ascii != 0) {
            result += QChar(ascii);
        }
        // Skip null mappings (control codes)
    }

    return result;
}

char PetsciiConverter::toAscii(quint8 petscii)
{
    return petsciiToAsciiTable[petscii];
}

QString PetsciiConverter::toDisplayString(const QByteArray &data)
{
    // C64 Pro font uses Private Use Area mapping:
    // PETSCII byte XX -> Unicode U+E0XX (Upper/Graph mode, Reverse Off)
    // Reference: https://style64.org/petscii/
    //
    // NOTE: The C64 Pro Mono font only has glyphs for printable characters.
    // Control codes (0x00-0x1F and 0x80-0x9F) are missing from the font.
    // We map these to space (0xE020) to prevent Qt font fallback issues.

    QString result;
    result.reserve(data.size());

    for (int i = 0; i < data.size(); i++) {
        quint8 petscii = static_cast<quint8>(data[i]);

        // Stop at null
        if (petscii == 0x00) {
            break;
        }

        // Control codes 0x01-0x1F and 0x80-0x9F don't have glyphs in C64 Pro font
        // Map them to space to maintain alignment and prevent font fallback
        if ((petscii >= 0x01 && petscii <= 0x1F) ||
            (petscii >= 0x80 && petscii <= 0x9F)) {
            result += QChar(0xE020);  // PUA space
        } else {
            // Map PETSCII to C64 Pro font's Private Use Area
            result += QChar(0xE000 + petscii);
        }
    }

    return result;
}

QByteArray PetsciiConverter::fromAscii(const QString &text)
{
    QByteArray result;
    result.reserve(text.size());

    for (int i = 0; i < text.size(); i++) {
        ushort unicode = text[i].unicode();

        if (unicode < 128) {
            result.append(static_cast<char>(asciiToPetsciiTable[unicode]));
        } else {
            // Non-ASCII character, use space
            result.append(static_cast<char>(0x20));
        }
    }

    return result;
}

quint8 PetsciiConverter::fromAscii(char ascii)
{
    if (static_cast<unsigned char>(ascii) < 128) {
        return asciiToPetsciiTable[static_cast<unsigned char>(ascii)];
    }
    return 0x20;  // Space for non-ASCII
}

bool PetsciiConverter::isPrintable(quint8 petscii)
{
    // Printable: 0x20-0x7F and 0xA0-0xFF (excluding pure control codes)
    if (petscii >= 0x20 && petscii <= 0x7F) {
        return true;
    }
    if (petscii >= 0xA0) {
        return true;
    }
    // 0x0D (RETURN) is printable as newline
    if (petscii == 0x0D) {
        return true;
    }
    return false;
}

bool PetsciiConverter::isGraphics(quint8 petscii)
{
    // Graphics characters:
    // 0x60-0x7F: Unshifted graphics (Commodore + letter keys)
    // 0xA0-0xBF: Shifted graphics
    // 0xE0-0xFF: Shifted graphics (repeat of 0x60-0x7F range)
    if (petscii >= 0x60 && petscii <= 0x7F) {
        return true;
    }
    if (petscii >= 0xA0 && petscii <= 0xBF) {
        return true;
    }
    if (petscii >= 0xE0) {
        return true;
    }
    return false;
}

bool PetsciiConverter::isControl(quint8 petscii)
{
    // Control codes: 0x00-0x1F and 0x80-0x9F
    if (petscii <= 0x1F) {
        return true;
    }
    if (petscii >= 0x80 && petscii <= 0x9F) {
        return true;
    }
    return false;
}

QString PetsciiConverter::toC64ProString(const QString &text)
{
    // Convert ASCII/Unicode text to C64 Pro font's PUA encoding
    // This ensures all characters render using the C64 Pro font
    // for consistent monospace alignment in directory listings.
    //
    // Maps ASCII 0x20-0x7F to U+E020-E07F in the font's PUA range.
    // Newlines (0x0A) are preserved as-is for Qt text rendering.

    QString result;
    result.reserve(text.size());

    for (int i = 0; i < text.size(); i++) {
        ushort ch = text[i].unicode();

        if (ch == '\n') {
            // Keep newlines as-is for proper line breaks
            result += QChar('\n');
        } else if (ch >= 0x20 && ch <= 0x7F) {
            // Map printable ASCII to PUA
            result += QChar(0xE000 + ch);
        } else if (ch >= 0xE000 && ch <= 0xE0FF) {
            // Already in PUA range (from toDisplayString), keep as-is
            result += QChar(ch);
        } else {
            // Other characters: map to PUA space
            result += QChar(0xE020);
        }
    }

    return result;
}
