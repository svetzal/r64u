#ifndef PETSCIICONVERTER_H
#define PETSCIICONVERTER_H

#include <QByteArray>
#include <QString>

/**
 * @brief Converts between PETSCII and ASCII/Unicode
 *
 * PETSCII (PET Standard Code of Information Interchange) is the character
 * encoding used by Commodore 8-bit computers (C64, C128, VIC-20, etc.).
 *
 * This class provides conversion utilities for displaying PETSCII text
 * in modern applications, including disk directories, text files, and
 * any other text content from Commodore systems.
 *
 * Based on the Ultimate64/Ultimate-II Control Library for Amiga OS 3.x.
 */
class PetsciiConverter
{
public:
    /**
     * @brief Convert PETSCII bytes to plain ASCII (graphics become spaces)
     * @param data Raw PETSCII bytes
     * @param stopAtPadding If true, stop at $A0 (shift-space padding character)
     * @return QString with ASCII representation (graphics as spaces)
     */
    static QString toAscii(const QByteArray &data, bool stopAtPadding = true);

    /**
     * @brief Convert PETSCII bytes to displayable Unicode for C64 Pro font
     * @param data Raw PETSCII bytes
     * @return QString with Unicode characters that render correctly in C64 Pro font
     *
     * This method maps PETSCII graphics characters to their Unicode equivalents
     * (box drawing, block elements) that the C64 Pro font can display.
     */
    static QString toDisplayString(const QByteArray &data);

    /**
     * @brief Convert a single PETSCII byte to ASCII
     * @param petscii The PETSCII byte value
     * @return ASCII character, or 0 for non-printable
     */
    static char toAscii(quint8 petscii);

    /**
     * @brief Convert ASCII string to PETSCII bytes
     * @param text ASCII/Unicode text
     * @return QByteArray with PETSCII encoding
     */
    static QByteArray fromAscii(const QString &text);

    /**
     * @brief Convert a single ASCII character to PETSCII
     * @param ascii The ASCII character (0-127)
     * @return PETSCII byte value
     */
    static quint8 fromAscii(char ascii);

    /**
     * @brief Check if a PETSCII byte is a printable character
     * @param petscii The PETSCII byte value
     * @return true if the character has a visible representation
     */
    static bool isPrintable(quint8 petscii);

    /**
     * @brief Check if a PETSCII byte is a graphics character
     * @param petscii The PETSCII byte value
     * @return true if it's a graphics/drawing character
     */
    static bool isGraphics(quint8 petscii);

    /**
     * @brief Check if a PETSCII byte is a control character
     * @param petscii The PETSCII byte value
     * @return true if it's a control code (colors, cursor, etc.)
     */
    static bool isControl(quint8 petscii);

private:
    // Lookup tables
    static const char petsciiToAsciiTable[256];
    static const quint8 asciiToPetsciiTable[128];
};

#endif // PETSCIICONVERTER_H
