# C64 Pro Font Specification

This document describes how r64u uses the C64 Pro font family to display authentic PETSCII characters.

## Font Family

We use the **C64 Pro** font family from Style64:
- Website: https://style64.org/c64-truetype
- PETSCII Reference: https://style64.org/petscii/

The font provides pixel-perfect rendering of all 256 PETSCII characters, including graphics characters, in TrueType format.

## PETSCII to Unicode Mapping

### The Key Insight

The C64 Pro font uses Unicode Private Use Area (PUA) codepoints for PETSCII rendering. The mapping is elegantly simple:

```
PETSCII byte XX → Unicode U+E0XX
```

For example:
| PETSCII | Hex  | Unicode | Description |
|---------|------|---------|-------------|
| 32      | $20  | U+E020  | Space |
| 49      | $31  | U+E031  | Digit "1" |
| 65      | $41  | U+E041  | Letter "A" |
| 96      | $60  | U+E060  | Horizontal line (graphics) |
| 193     | $C1  | U+E0C1  | Spade symbol (Upper/Graph mode) |

### Character Set Modes

The C64 has two character set modes:

1. **Upper/Graph Mode** (default):
   - Uppercase letters A-Z at $41-$5A
   - Graphics characters at $C1-$DA
   - Font mapping: U+E0XX

2. **Lower/Upper Mode** (after SHIFT+Commodore):
   - Lowercase letters a-z at $41-$5A
   - Uppercase letters A-Z at $C1-$DA
   - Font mapping: U+E1XX

For directory listings and most file operations, we use **Upper/Graph mode** (U+E0XX).

### Reverse Video

The font also provides reverse video variants:
- Upper/Graph, Reverse Off: U+E0XX
- Lower/Upper, Reverse Off: U+E1XX
- Upper/Graph, Reverse On: U+E2XX
- Lower/Upper, Reverse On: U+E3XX

## Implementation

### Display String Conversion

The `PetsciiConverter::toDisplayString()` function converts raw PETSCII bytes to a QString for display with the C64 Pro font:

```cpp
QString PetsciiConverter::toDisplayString(const QByteArray &data)
{
    QString result;
    result.reserve(data.size());

    for (int i = 0; i < data.size(); i++) {
        quint8 petscii = static_cast<quint8>(data[i]);

        // Stop at null
        if (petscii == 0x00) {
            break;
        }

        // Map PETSCII to C64 Pro font's Private Use Area
        // U+E000 + petscii gives the correct glyph
        result += QChar(0xE000 + petscii);
    }

    return result;
}
```

### ASCII Conversion

For plain text operations (no special font), `toAscii()` provides a traditional mapping:
- $41-$5A → 'A'-'Z' (uppercase)
- $C1-$DA → 'a'-'z' (lowercase, following common convention)
- Graphics characters → space

### Qt Widget Setup

To display PETSCII text correctly, Qt widgets must use the C64 Pro font:

```cpp
QFont c64Font("C64 Pro Mono", 12);
label->setFont(c64Font);
label->setText(PetsciiConverter::toDisplayString(petsciiData));
```

## PETSCII Character Ranges

| Range     | Hex       | Description |
|-----------|-----------|-------------|
| 0-31      | $00-$1F   | Control codes (cursor, colors, etc.) |
| 32-63     | $20-$3F   | Space, punctuation, digits |
| 64-95     | $40-$5F   | @, uppercase A-Z, special chars |
| 96-127    | $60-$7F   | Graphics characters (Commodore+key) |
| 128-159   | $80-$9F   | Control codes (colors, reverse, etc.) |
| 160-191   | $A0-$BF   | Shifted graphics characters |
| 192-223   | $C0-$DF   | Graphics/lowercase (mode dependent) |
| 224-255   | $E0-$FF   | Shifted graphics (repeat of $A0-$BF) |

## Special Characters

| PETSCII | Hex  | Character |
|---------|------|-----------|
| 13      | $0D  | RETURN (carriage return) |
| 20      | $14  | DELETE |
| 147     | $93  | Clear screen |
| 160     | $A0  | Shift-space (often used as padding) |

## References

- [Style64 PETSCII Reference](https://style64.org/petscii/) - Interactive mapping tool
- [Style64 C64 TrueType Fonts](https://style64.org/c64-truetype) - Font downloads
- [C64 Wiki - PETSCII](https://www.c64-wiki.com/wiki/PETSCII) - General PETSCII information
