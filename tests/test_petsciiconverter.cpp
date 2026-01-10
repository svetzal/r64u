#include <QtTest>

#include "services/petsciiconverter.h"

class TestPetsciiConverter : public QObject
{
    Q_OBJECT

private slots:
    // ========== toAscii(QByteArray) tests ==========

    void testToAsciiEmptyData()
    {
        QByteArray empty;
        QCOMPARE(PetsciiConverter::toAscii(empty), QString());
    }

    void testToAsciiBasicUppercase()
    {
        // PETSCII uppercase A-Z is 0x41-0x5A (same as ASCII)
        QByteArray data;
        data.append(static_cast<char>(0x41)); // A
        data.append(static_cast<char>(0x42)); // B
        data.append(static_cast<char>(0x43)); // C
        QCOMPARE(PetsciiConverter::toAscii(data), QString("ABC"));
    }

    void testToAsciiBasicLowercase()
    {
        // PETSCII lowercase a-z is 0xC1-0xDA
        QByteArray data;
        data.append(static_cast<char>(0xC1)); // a
        data.append(static_cast<char>(0xC2)); // b
        data.append(static_cast<char>(0xC3)); // c
        QCOMPARE(PetsciiConverter::toAscii(data), QString("abc"));
    }

    void testToAsciiNumbers()
    {
        // Numbers 0-9 are 0x30-0x39 (same as ASCII)
        QByteArray data;
        data.append(static_cast<char>(0x30)); // 0
        data.append(static_cast<char>(0x31)); // 1
        data.append(static_cast<char>(0x39)); // 9
        QCOMPARE(PetsciiConverter::toAscii(data), QString("019"));
    }

    void testToAsciiPunctuation()
    {
        // Common punctuation maps directly
        QByteArray data;
        data.append(static_cast<char>(0x20)); // space
        data.append(static_cast<char>(0x21)); // !
        data.append(static_cast<char>(0x2E)); // .
        data.append(static_cast<char>(0x2C)); // ,
        QCOMPARE(PetsciiConverter::toAscii(data), QString(" !.,"));
    }

    void testToAsciiStopsAtPadding()
    {
        // 0xA0 is shift-space (padding character)
        QByteArray data;
        data.append(static_cast<char>(0x41)); // A
        data.append(static_cast<char>(0x42)); // B
        data.append(static_cast<char>(0xA0)); // padding
        data.append(static_cast<char>(0x43)); // C (should not appear)
        QCOMPARE(PetsciiConverter::toAscii(data, true), QString("AB"));
    }

    void testToAsciiIgnoresPaddingWhenDisabled()
    {
        QByteArray data;
        data.append(static_cast<char>(0x41)); // A
        data.append(static_cast<char>(0xA0)); // padding (becomes space)
        data.append(static_cast<char>(0x42)); // B
        QCOMPARE(PetsciiConverter::toAscii(data, false), QString("A B"));
    }

    void testToAsciiStopsAtNull()
    {
        QByteArray data;
        data.append(static_cast<char>(0x41)); // A
        data.append(static_cast<char>(0x00)); // null
        data.append(static_cast<char>(0x42)); // B (should not appear)
        QCOMPARE(PetsciiConverter::toAscii(data), QString("A"));
    }

    void testToAsciiControlCodesSkipped()
    {
        // Control codes 0x01-0x1F are skipped (mapped to 0)
        QByteArray data;
        data.append(static_cast<char>(0x41)); // A
        data.append(static_cast<char>(0x05)); // control code
        data.append(static_cast<char>(0x42)); // B
        QCOMPARE(PetsciiConverter::toAscii(data), QString("AB"));
    }

    void testToAsciiReturnCharacter()
    {
        // 0x0D (RETURN) maps to newline
        QByteArray data;
        data.append(static_cast<char>(0x41)); // A
        data.append(static_cast<char>(0x0D)); // RETURN
        data.append(static_cast<char>(0x42)); // B
        QCOMPARE(PetsciiConverter::toAscii(data), QString("A\nB"));
    }

    void testToAsciiGraphicsAsSpaces()
    {
        // Graphics characters 0x60-0x7F become spaces
        QByteArray data;
        data.append(static_cast<char>(0x41)); // A
        data.append(static_cast<char>(0x60)); // graphics
        data.append(static_cast<char>(0x6F)); // graphics
        data.append(static_cast<char>(0x42)); // B
        QCOMPARE(PetsciiConverter::toAscii(data), QString("A  B"));
    }

    void testToAsciiTypicalFilename()
    {
        // Simulate a typical C64 filename: "HELLO WORLD" with padding
        QByteArray data;
        const char filename[] = {0x48, 0x45, 0x4C, 0x4C, 0x4F, 0x20, // HELLO
                                  0x57, 0x4F, 0x52, 0x4C, 0x44,       // WORLD
                                  static_cast<char>(0xA0), static_cast<char>(0xA0), static_cast<char>(0xA0), static_cast<char>(0xA0), static_cast<char>(0xA0)}; // padding
        data = QByteArray(filename, 16);
        QCOMPARE(PetsciiConverter::toAscii(data), QString("HELLO WORLD"));
    }

    // ========== toAscii(quint8) tests ==========

    void testToAsciiSingleByte()
    {
        QCOMPARE(PetsciiConverter::toAscii(static_cast<quint8>(0x41)), 'A');
        QCOMPARE(PetsciiConverter::toAscii(static_cast<quint8>(0x5A)), 'Z');
        QCOMPARE(PetsciiConverter::toAscii(static_cast<quint8>(0xC1)), 'a');
        QCOMPARE(PetsciiConverter::toAscii(static_cast<quint8>(0xDA)), 'z');
    }

    void testToAsciiSingleByteControlCode()
    {
        QCOMPARE(PetsciiConverter::toAscii(static_cast<quint8>(0x00)), '\0');
        QCOMPARE(PetsciiConverter::toAscii(static_cast<quint8>(0x05)), '\0');
    }

    void testToAsciiSingleByteGraphics()
    {
        QCOMPARE(PetsciiConverter::toAscii(static_cast<quint8>(0x60)), ' ');
        QCOMPARE(PetsciiConverter::toAscii(static_cast<quint8>(0x7F)), ' ');
    }

    // ========== toDisplayString tests ==========

    void testToDisplayStringEmpty()
    {
        QByteArray empty;
        QCOMPARE(PetsciiConverter::toDisplayString(empty), QString());
    }

    void testToDisplayStringMapsToPrivateUseArea()
    {
        // Each PETSCII byte XX -> Unicode U+E0XX
        QByteArray data;
        data.append(static_cast<char>(0x41)); // A

        QString result = PetsciiConverter::toDisplayString(data);
        QCOMPARE(result.length(), 1);
        QCOMPARE(result[0].unicode(), static_cast<ushort>(0xE041));
    }

    void testToDisplayStringMultipleBytes()
    {
        QByteArray data;
        data.append(static_cast<char>(0x00)); // First valid PETSCII
        // Wait, 0x00 terminates - let me use different bytes
        data.clear();
        data.append(static_cast<char>(0x41)); // A
        data.append(static_cast<char>(0x42)); // B
        data.append(static_cast<char>(0x60)); // Graphics char

        QString result = PetsciiConverter::toDisplayString(data);
        QCOMPARE(result.length(), 3);
        QCOMPARE(result[0].unicode(), static_cast<ushort>(0xE041));
        QCOMPARE(result[1].unicode(), static_cast<ushort>(0xE042));
        QCOMPARE(result[2].unicode(), static_cast<ushort>(0xE060));
    }

    void testToDisplayStringStopsAtNull()
    {
        QByteArray data;
        data.append(static_cast<char>(0x41)); // A
        data.append(static_cast<char>(0x00)); // null
        data.append(static_cast<char>(0x42)); // B (should not appear)

        QString result = PetsciiConverter::toDisplayString(data);
        QCOMPARE(result.length(), 1);
        QCOMPARE(result[0].unicode(), static_cast<ushort>(0xE041));
    }

    void testToDisplayStringPreservesGraphics()
    {
        // Unlike toAscii, toDisplayString preserves graphics characters
        QByteArray data;
        data.append(static_cast<char>(0x60)); // graphics
        data.append(static_cast<char>(0xA0)); // shifted graphics

        QString result = PetsciiConverter::toDisplayString(data);
        QCOMPARE(result.length(), 2);
        QCOMPARE(result[0].unicode(), static_cast<ushort>(0xE060));
        QCOMPARE(result[1].unicode(), static_cast<ushort>(0xE0A0));
    }

    // ========== fromAscii(QString) tests ==========

    void testFromAsciiEmptyString()
    {
        QString empty;
        QCOMPARE(PetsciiConverter::fromAscii(empty), QByteArray());
    }

    void testFromAsciiUppercase()
    {
        QString text = "ABC";
        QByteArray result = PetsciiConverter::fromAscii(text);
        QCOMPARE(result.size(), 3);
        QCOMPARE(static_cast<quint8>(result[0]), static_cast<quint8>(0x41));
        QCOMPARE(static_cast<quint8>(result[1]), static_cast<quint8>(0x42));
        QCOMPARE(static_cast<quint8>(result[2]), static_cast<quint8>(0x43));
    }

    void testFromAsciiLowercase()
    {
        QString text = "abc";
        QByteArray result = PetsciiConverter::fromAscii(text);
        QCOMPARE(result.size(), 3);
        // Lowercase a-z maps to PETSCII 0xC1-0xDA
        QCOMPARE(static_cast<quint8>(result[0]), static_cast<quint8>(0xC1));
        QCOMPARE(static_cast<quint8>(result[1]), static_cast<quint8>(0xC2));
        QCOMPARE(static_cast<quint8>(result[2]), static_cast<quint8>(0xC3));
    }

    void testFromAsciiNumbers()
    {
        QString text = "123";
        QByteArray result = PetsciiConverter::fromAscii(text);
        QCOMPARE(result.size(), 3);
        QCOMPARE(static_cast<quint8>(result[0]), static_cast<quint8>(0x31));
        QCOMPARE(static_cast<quint8>(result[1]), static_cast<quint8>(0x32));
        QCOMPARE(static_cast<quint8>(result[2]), static_cast<quint8>(0x33));
    }

    void testFromAsciiNewline()
    {
        QString text = "A\nB";
        QByteArray result = PetsciiConverter::fromAscii(text);
        QCOMPARE(result.size(), 3);
        QCOMPARE(static_cast<quint8>(result[0]), static_cast<quint8>(0x41)); // A
        QCOMPARE(static_cast<quint8>(result[1]), static_cast<quint8>(0x0D)); // RETURN
        QCOMPARE(static_cast<quint8>(result[2]), static_cast<quint8>(0x42)); // B
    }

    void testFromAsciiNonAsciiBecomesSpace()
    {
        QString text = QString::fromUtf8("Aéß");
        QByteArray result = PetsciiConverter::fromAscii(text);
        QCOMPARE(result.size(), 3);
        QCOMPARE(static_cast<quint8>(result[0]), static_cast<quint8>(0x41)); // A
        QCOMPARE(static_cast<quint8>(result[1]), static_cast<quint8>(0x20)); // space (for é)
        QCOMPARE(static_cast<quint8>(result[2]), static_cast<quint8>(0x20)); // space (for ß)
    }

    // ========== fromAscii(char) tests ==========

    void testFromAsciiSingleCharUppercase()
    {
        QCOMPARE(PetsciiConverter::fromAscii('A'), static_cast<quint8>(0x41));
        QCOMPARE(PetsciiConverter::fromAscii('Z'), static_cast<quint8>(0x5A));
    }

    void testFromAsciiSingleCharLowercase()
    {
        QCOMPARE(PetsciiConverter::fromAscii('a'), static_cast<quint8>(0xC1));
        QCOMPARE(PetsciiConverter::fromAscii('z'), static_cast<quint8>(0xDA));
    }

    void testFromAsciiSingleCharNonAscii()
    {
        // High bit set chars become space
        QCOMPARE(PetsciiConverter::fromAscii(static_cast<char>(0x80)), static_cast<quint8>(0x20));
        QCOMPARE(PetsciiConverter::fromAscii(static_cast<char>(0xFF)), static_cast<quint8>(0x20));
    }

    // ========== Round-trip tests ==========

    void testRoundTripUppercase()
    {
        QString original = "HELLO WORLD";
        QByteArray petscii = PetsciiConverter::fromAscii(original);
        QString roundTrip = PetsciiConverter::toAscii(petscii);
        QCOMPARE(roundTrip, original);
    }

    void testRoundTripLowercase()
    {
        QString original = "hello world";
        QByteArray petscii = PetsciiConverter::fromAscii(original);
        QString roundTrip = PetsciiConverter::toAscii(petscii);
        QCOMPARE(roundTrip, original);
    }

    void testRoundTripMixed()
    {
        QString original = "Hello World 123!";
        QByteArray petscii = PetsciiConverter::fromAscii(original);
        QString roundTrip = PetsciiConverter::toAscii(petscii);
        QCOMPARE(roundTrip, original);
    }

    // ========== isPrintable tests ==========

    void testIsPrintableSpace()
    {
        QVERIFY(PetsciiConverter::isPrintable(0x20)); // space
    }

    void testIsPrintableLetters()
    {
        QVERIFY(PetsciiConverter::isPrintable(0x41)); // A
        QVERIFY(PetsciiConverter::isPrintable(0x5A)); // Z
        QVERIFY(PetsciiConverter::isPrintable(0xC1)); // a
        QVERIFY(PetsciiConverter::isPrintable(0xDA)); // z
    }

    void testIsPrintableGraphics()
    {
        QVERIFY(PetsciiConverter::isPrintable(0x60)); // graphics
        QVERIFY(PetsciiConverter::isPrintable(0x7F)); // graphics
        QVERIFY(PetsciiConverter::isPrintable(0xA0)); // shifted graphics
        QVERIFY(PetsciiConverter::isPrintable(0xFF)); // shifted graphics
    }

    void testIsPrintableReturn()
    {
        QVERIFY(PetsciiConverter::isPrintable(0x0D)); // RETURN is printable
    }

    void testIsPrintableControlCodes()
    {
        QVERIFY(!PetsciiConverter::isPrintable(0x00)); // null
        QVERIFY(!PetsciiConverter::isPrintable(0x01)); // control
        QVERIFY(!PetsciiConverter::isPrintable(0x1F)); // control
    }

    // ========== isGraphics tests ==========

    void testIsGraphicsUnshifted()
    {
        // 0x60-0x7F are unshifted graphics
        QVERIFY(PetsciiConverter::isGraphics(0x60));
        QVERIFY(PetsciiConverter::isGraphics(0x6F));
        QVERIFY(PetsciiConverter::isGraphics(0x7F));
    }

    void testIsGraphicsShifted()
    {
        // 0xA0-0xBF are shifted graphics
        QVERIFY(PetsciiConverter::isGraphics(0xA0));
        QVERIFY(PetsciiConverter::isGraphics(0xAF));
        QVERIFY(PetsciiConverter::isGraphics(0xBF));
    }

    void testIsGraphicsHighRange()
    {
        // 0xE0-0xFF are shifted graphics (repeat)
        QVERIFY(PetsciiConverter::isGraphics(0xE0));
        QVERIFY(PetsciiConverter::isGraphics(0xEF));
        QVERIFY(PetsciiConverter::isGraphics(0xFF));
    }

    void testIsGraphicsNotGraphics()
    {
        QVERIFY(!PetsciiConverter::isGraphics(0x20)); // space
        QVERIFY(!PetsciiConverter::isGraphics(0x41)); // A
        QVERIFY(!PetsciiConverter::isGraphics(0xC1)); // a
        QVERIFY(!PetsciiConverter::isGraphics(0x00)); // null
    }

    // ========== isControl tests ==========

    void testIsControlLowRange()
    {
        // 0x00-0x1F are control codes
        QVERIFY(PetsciiConverter::isControl(0x00));
        QVERIFY(PetsciiConverter::isControl(0x0D)); // RETURN (still a control code)
        QVERIFY(PetsciiConverter::isControl(0x1F));
    }

    void testIsControlHighRange()
    {
        // 0x80-0x9F are control codes (colors, reverse, etc.)
        QVERIFY(PetsciiConverter::isControl(0x80));
        QVERIFY(PetsciiConverter::isControl(0x8F));
        QVERIFY(PetsciiConverter::isControl(0x9F));
    }

    void testIsControlNotControl()
    {
        QVERIFY(!PetsciiConverter::isControl(0x20)); // space
        QVERIFY(!PetsciiConverter::isControl(0x41)); // A
        QVERIFY(!PetsciiConverter::isControl(0x60)); // graphics
        QVERIFY(!PetsciiConverter::isControl(0xA0)); // shifted graphics
        QVERIFY(!PetsciiConverter::isControl(0xC1)); // a
    }

    // ========== Edge cases and boundary tests ==========

    void testAllUppercaseLetters()
    {
        for (int i = 0; i < 26; i++) {
            quint8 petscii = 0x41 + i;
            char expected = 'A' + i;
            QCOMPARE(PetsciiConverter::toAscii(petscii), expected);
        }
    }

    void testAllLowercaseLetters()
    {
        for (int i = 0; i < 26; i++) {
            quint8 petscii = 0xC1 + i;
            char expected = 'a' + i;
            QCOMPARE(PetsciiConverter::toAscii(petscii), expected);
        }
    }

    void testAllDigits()
    {
        for (int i = 0; i < 10; i++) {
            quint8 petscii = 0x30 + i;
            char expected = '0' + i;
            QCOMPARE(PetsciiConverter::toAscii(petscii), expected);
        }
    }

    void testLookupTableCompleteness()
    {
        // Verify all 256 bytes can be converted without crash
        for (int i = 0; i < 256; i++) {
            quint8 petscii = static_cast<quint8>(i);
            // Just ensure no crash - the result can be anything
            PetsciiConverter::toAscii(petscii);
            PetsciiConverter::isPrintable(petscii);
            PetsciiConverter::isGraphics(petscii);
            PetsciiConverter::isControl(petscii);
        }
    }

    void testFromAsciiTableCompleteness()
    {
        // Verify all 128 ASCII chars can be converted without crash
        for (int i = 0; i < 128; i++) {
            char ascii = static_cast<char>(i);
            PetsciiConverter::fromAscii(ascii);
        }
    }
};

QTEST_MAIN(TestPetsciiConverter)
#include "test_petsciiconverter.moc"
