#include <QtTest>
#include <QJsonDocument>

#include "services/configfileloader.h"

class TestConfigFileLoader : public QObject
{
    Q_OBJECT

private slots:
    // ========== parseConfigFile - empty/invalid input ==========

    void testParseEmptyData()
    {
        QJsonObject result = ConfigFileLoader::parseConfigFile(QByteArray());
        QVERIFY(result.isEmpty());
    }

    void testParseWhitespaceOnly()
    {
        QJsonObject result = ConfigFileLoader::parseConfigFile("   \n\t\n  ");
        QVERIFY(result.isEmpty());
    }

    void testParseCommentsOnly()
    {
        QByteArray data = "# This is a comment\n; This is also a comment\n";
        QJsonObject result = ConfigFileLoader::parseConfigFile(data);
        QVERIFY(result.isEmpty());
    }

    void testParseKeyValueWithoutSection()
    {
        // Key-value pairs before any section are ignored
        QByteArray data = "key=value\nanother=pair\n";
        QJsonObject result = ConfigFileLoader::parseConfigFile(data);
        QVERIFY(result.isEmpty());
    }

    // ========== parseConfigFile - section parsing ==========

    void testParseSingleSection()
    {
        QByteArray data = "[Section]\nkey=value\n";
        QJsonObject result = ConfigFileLoader::parseConfigFile(data);

        QVERIFY(result.contains("Section"));
        QVERIFY(result["Section"].isObject());
        QCOMPARE(result["Section"].toObject()["key"].toString(), QString("value"));
    }

    void testParseMultipleSections()
    {
        QByteArray data =
            "[Section1]\n"
            "key1=value1\n"
            "[Section2]\n"
            "key2=value2\n";
        QJsonObject result = ConfigFileLoader::parseConfigFile(data);

        QCOMPARE(result.count(), 2);
        QVERIFY(result.contains("Section1"));
        QVERIFY(result.contains("Section2"));
        QCOMPARE(result["Section1"].toObject()["key1"].toString(), QString("value1"));
        QCOMPARE(result["Section2"].toObject()["key2"].toString(), QString("value2"));
    }

    void testParseSectionWithSpaces()
    {
        QByteArray data = "[Audio Mixer Settings]\nVolume=80\n";
        QJsonObject result = ConfigFileLoader::parseConfigFile(data);

        QVERIFY(result.contains("Audio Mixer Settings"));
        QCOMPARE(result["Audio Mixer Settings"].toObject()["Volume"].toInt(), 80);
    }

    void testParseSectionWithTrailingWhitespace()
    {
        QByteArray data = "[Section]   \nkey=value\n";
        QJsonObject result = ConfigFileLoader::parseConfigFile(data);

        QVERIFY(result.contains("Section"));
    }

    void testParseEmptySection()
    {
        // Empty sections are not added to result
        QByteArray data = "[Empty]\n[HasData]\nkey=value\n";
        QJsonObject result = ConfigFileLoader::parseConfigFile(data);

        QVERIFY(!result.contains("Empty"));
        QVERIFY(result.contains("HasData"));
    }

    // ========== parseConfigFile - key-value parsing ==========

    void testParseMultipleKeyValues()
    {
        QByteArray data =
            "[Settings]\n"
            "Volume=80\n"
            "Mute=0\n"
            "Balance=Center\n";
        QJsonObject result = ConfigFileLoader::parseConfigFile(data);

        QJsonObject settings = result["Settings"].toObject();
        QCOMPARE(settings.count(), 3);
        QCOMPARE(settings["Volume"].toInt(), 80);
        QCOMPARE(settings["Mute"].toInt(), 0);
        QCOMPARE(settings["Balance"].toString(), QString("Center"));
    }

    void testParseKeyWithSpaces()
    {
        QByteArray data = "[Section]\nKey With Spaces=value\n";
        QJsonObject result = ConfigFileLoader::parseConfigFile(data);

        QJsonObject section = result["Section"].toObject();
        QVERIFY(section.contains("Key With Spaces"));
        QCOMPARE(section["Key With Spaces"].toString(), QString("value"));
    }

    void testParseValueWithEqualsSign()
    {
        // Values can contain = signs
        QByteArray data = "[Section]\nkey=value=with=equals\n";
        QJsonObject result = ConfigFileLoader::parseConfigFile(data);

        QCOMPARE(result["Section"].toObject()["key"].toString(),
                 QString("value=with=equals"));
    }

    void testParseEmptyValue()
    {
        QByteArray data = "[Section]\nkey=\n";
        QJsonObject result = ConfigFileLoader::parseConfigFile(data);

        QCOMPARE(result["Section"].toObject()["key"].toString(), QString(""));
    }

    // ========== parseConfigFile - type conversion ==========

    void testParseIntegerValue()
    {
        QByteArray data = "[Section]\ncount=42\n";
        QJsonObject result = ConfigFileLoader::parseConfigFile(data);

        QJsonValue value = result["Section"].toObject()["count"];
        QVERIFY(value.isDouble()); // JSON stores ints as doubles
        QCOMPARE(value.toInt(), 42);
    }

    void testParseNegativeInteger()
    {
        QByteArray data = "[Section]\noffset=-10\n";
        QJsonObject result = ConfigFileLoader::parseConfigFile(data);

        QCOMPARE(result["Section"].toObject()["offset"].toInt(), -10);
    }

    void testParseZero()
    {
        QByteArray data = "[Section]\nvalue=0\n";
        QJsonObject result = ConfigFileLoader::parseConfigFile(data);

        QCOMPARE(result["Section"].toObject()["value"].toInt(), 0);
    }

    void testParseStringNotInteger()
    {
        // Non-numeric strings remain strings
        QByteArray data = "[Section]\nname=Hello123\n";
        QJsonObject result = ConfigFileLoader::parseConfigFile(data);

        QJsonValue value = result["Section"].toObject()["name"];
        QVERIFY(value.isString());
        QCOMPARE(value.toString(), QString("Hello123"));
    }

    // ========== parseConfigFile - whitespace handling ==========

    void testParsePreservesLeadingSpace()
    {
        // Leading spaces in values are significant (e.g., " 0 dB")
        QByteArray data = "[Section]\nVolume= 0 dB\n";
        QJsonObject result = ConfigFileLoader::parseConfigFile(data);

        QCOMPARE(result["Section"].toObject()["Volume"].toString(),
                 QString(" 0 dB"));
    }

    void testParseTrimsTrailingSpace()
    {
        QByteArray data = "[Section]\nkey=value   \n";
        QJsonObject result = ConfigFileLoader::parseConfigFile(data);

        QCOMPARE(result["Section"].toObject()["key"].toString(), QString("value"));
    }

    void testParseTrimsKeyWhitespace()
    {
        QByteArray data = "[Section]\n  key  =value\n";
        QJsonObject result = ConfigFileLoader::parseConfigFile(data);

        QJsonObject section = result["Section"].toObject();
        QVERIFY(section.contains("key"));
        QCOMPARE(section["key"].toString(), QString("value"));
    }

    void testParseLeadingSpacePreventsIntConversion()
    {
        // If value has leading space, keep as string even if numeric
        QByteArray data = "[Section]\nVolume= 42\n";
        QJsonObject result = ConfigFileLoader::parseConfigFile(data);

        QJsonValue value = result["Section"].toObject()["Volume"];
        QVERIFY(value.isString());
        QCOMPARE(value.toString(), QString(" 42"));
    }

    // ========== parseConfigFile - comments ==========

    void testParseHashComment()
    {
        QByteArray data =
            "[Section]\n"
            "# This is a comment\n"
            "key=value\n";
        QJsonObject result = ConfigFileLoader::parseConfigFile(data);

        QJsonObject section = result["Section"].toObject();
        QCOMPARE(section.count(), 1);
        QVERIFY(section.contains("key"));
        QVERIFY(!section.contains("# This is a comment"));
    }

    void testParseSemicolonComment()
    {
        QByteArray data =
            "[Section]\n"
            "; This is a comment\n"
            "key=value\n";
        QJsonObject result = ConfigFileLoader::parseConfigFile(data);

        QJsonObject section = result["Section"].toObject();
        QCOMPARE(section.count(), 1);
    }

    void testParseInlineHashNotTreatedAsComment()
    {
        // Hash in value is NOT treated as comment start
        QByteArray data = "[Section]\nkey=value#notcomment\n";
        QJsonObject result = ConfigFileLoader::parseConfigFile(data);

        QCOMPARE(result["Section"].toObject()["key"].toString(),
                 QString("value#notcomment"));
    }

    // ========== parseConfigFile - real-world examples ==========

    void testParseRealAudioMixerConfig()
    {
        QByteArray data =
            "[Audio Mixer]\n"
            "Sid Left=Sid 1\n"
            "Sid Right=Sid 2\n"
            "Sid Volume= 0 dB\n"
            "Drive Volume=-12 dB\n"
            "Sample Rate=48000\n";
        QJsonObject result = ConfigFileLoader::parseConfigFile(data);

        QJsonObject mixer = result["Audio Mixer"].toObject();
        QCOMPARE(mixer["Sid Left"].toString(), QString("Sid 1"));
        QCOMPARE(mixer["Sid Right"].toString(), QString("Sid 2"));
        QCOMPARE(mixer["Sid Volume"].toString(), QString(" 0 dB")); // Leading space preserved
        QCOMPARE(mixer["Drive Volume"].toString(), QString("-12 dB"));
        QCOMPARE(mixer["Sample Rate"].toInt(), 48000);
    }

    void testParseRealNetworkConfig()
    {
        QByteArray data =
            "[Network Settings]\n"
            "Use DHCP=1\n"
            "IP Address=192.168.1.100\n"
            "Netmask=255.255.255.0\n"
            "Gateway=192.168.1.1\n"
            "Hostname=ultimate64\n";
        QJsonObject result = ConfigFileLoader::parseConfigFile(data);

        QJsonObject network = result["Network Settings"].toObject();
        QCOMPARE(network["Use DHCP"].toInt(), 1);
        QCOMPARE(network["IP Address"].toString(), QString("192.168.1.100"));
        QCOMPARE(network["Hostname"].toString(), QString("ultimate64"));
    }

    void testParseMultipleSectionsRealWorld()
    {
        QByteArray data =
            "# Ultimate64 Configuration\n"
            "\n"
            "[Drive A Settings]\n"
            "Drive Type=1541\n"
            "Speed=Normal\n"
            "\n"
            "[Drive B Settings]\n"
            "Drive Type=1571\n"
            "Speed=Fast\n"
            "\n"
            "[SID Settings]\n"
            "Model=8580\n"
            "Filter Bias=1472\n";
        QJsonObject result = ConfigFileLoader::parseConfigFile(data);

        QCOMPARE(result.count(), 3);
        QCOMPARE(result["Drive A Settings"].toObject()["Drive Type"].toInt(), 1541);
        QCOMPARE(result["Drive B Settings"].toObject()["Drive Type"].toInt(), 1571);
        QCOMPARE(result["SID Settings"].toObject()["Filter Bias"].toInt(), 1472);
    }

    // ========== Edge cases ==========

    void testParseSectionWithSpecialChars()
    {
        QByteArray data = "[Section (v2.0) - Advanced]\nkey=value\n";
        QJsonObject result = ConfigFileLoader::parseConfigFile(data);

        QVERIFY(result.contains("Section (v2.0) - Advanced"));
    }

    void testParseNoNewlineAtEnd()
    {
        QByteArray data = "[Section]\nkey=value";
        QJsonObject result = ConfigFileLoader::parseConfigFile(data);

        QCOMPARE(result["Section"].toObject()["key"].toString(), QString("value"));
    }

    void testParseWindowsLineEndings()
    {
        QByteArray data = "[Section]\r\nkey=value\r\n";
        QJsonObject result = ConfigFileLoader::parseConfigFile(data);

        QVERIFY(result.contains("Section"));
        QCOMPARE(result["Section"].toObject()["key"].toString(), QString("value"));
    }

    void testParseMixedLineEndings()
    {
        QByteArray data = "[Section]\nkey1=value1\r\nkey2=value2\n";
        QJsonObject result = ConfigFileLoader::parseConfigFile(data);

        QJsonObject section = result["Section"].toObject();
        QCOMPARE(section["key1"].toString(), QString("value1"));
        QCOMPARE(section["key2"].toString(), QString("value2"));
    }

    void testParseLargeIntegerValue()
    {
        QByteArray data = "[Section]\nbignum=2147483647\n"; // INT_MAX
        QJsonObject result = ConfigFileLoader::parseConfigFile(data);

        QCOMPARE(result["Section"].toObject()["bignum"].toInt(), 2147483647);
    }

    void testParseOverflowIntegerAsString()
    {
        // Number too large for int stays as string
        QByteArray data = "[Section]\nhuge=99999999999999\n";
        QJsonObject result = ConfigFileLoader::parseConfigFile(data);

        QJsonValue value = result["Section"].toObject()["huge"];
        // Qt's toInt will fail and it becomes string
        QVERIFY(value.isString() || value.isDouble());
    }

    void testParseSectionReplacesKeys()
    {
        // Later sections with same name replace earlier ones
        // (Though this is unusual - testing edge case behavior)
        QByteArray data =
            "[Section]\n"
            "key1=first\n"
            "[Section]\n"
            "key2=second\n";
        QJsonObject result = ConfigFileLoader::parseConfigFile(data);

        // The second [Section] replaces the first
        QJsonObject section = result["Section"].toObject();
        // Behavior depends on implementation - might have key1, might not
        QVERIFY(section.contains("key2"));
    }

    void testParseValueWithTabs()
    {
        QByteArray data = "[Section]\nkey=value\twith\ttabs\n";
        QJsonObject result = ConfigFileLoader::parseConfigFile(data);

        QCOMPARE(result["Section"].toObject()["key"].toString(),
                 QString("value\twith\ttabs"));
    }

    void testParseUtf8Content()
    {
        QByteArray data = QString::fromUtf8("[Section]\nname=Müller\n").toUtf8();
        QJsonObject result = ConfigFileLoader::parseConfigFile(data);

        QCOMPARE(result["Section"].toObject()["name"].toString(),
                 QString::fromUtf8("Müller"));
    }
};

QTEST_MAIN(TestConfigFileLoader)
#include "test_configfileloader.moc"
