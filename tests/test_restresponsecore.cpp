/**
 * @file test_restresponsecore.cpp
 * @brief Unit tests for pure REST API JSON response parsing functions.
 *
 * Tests the restresponse:: namespace functions which are pure (no I/O, no network).
 * Exercises all parsing paths including type-switching logic in
 * parseConfigCategoryItemsResponse.
 */

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include <services/restresponsecore.h>

class TestRestResponseCore : public QObject
{
    Q_OBJECT

private slots:
    // =========================================================
    // extractErrors
    // =========================================================

    void extractErrors_noErrorsKey()
    {
        QJsonObject json;
        auto errors = restresponse::extractErrors(json);
        QVERIFY(errors.isEmpty());
    }

    void extractErrors_emptyArray()
    {
        QJsonObject json;
        json["errors"] = QJsonArray();
        auto errors = restresponse::extractErrors(json);
        QVERIFY(errors.isEmpty());
    }

    void extractErrors_singleError()
    {
        QJsonObject json;
        json["errors"] = QJsonArray{QJsonValue("Something went wrong")};
        auto errors = restresponse::extractErrors(json);
        QCOMPARE(errors.size(), 1);
        QCOMPARE(errors[0], QString("Something went wrong"));
    }

    void extractErrors_multipleErrors()
    {
        QJsonObject json;
        json["errors"] = QJsonArray{QJsonValue("Error 1"), QJsonValue("Error 2")};
        auto errors = restresponse::extractErrors(json);
        QCOMPARE(errors.size(), 2);
    }

    void extractErrors_emptyStringFiltered()
    {
        QJsonObject json;
        json["errors"] = QJsonArray{QJsonValue(""), QJsonValue("Real error"), QJsonValue("")};
        auto errors = restresponse::extractErrors(json);
        QCOMPARE(errors.size(), 1);
        QCOMPARE(errors[0], QString("Real error"));
    }

    // =========================================================
    // parseVersionResponse
    // =========================================================

    void parseVersionResponse_returnsVersionString()
    {
        QJsonObject json;
        json["version"] = "3.11.0";
        QCOMPARE(restresponse::parseVersionResponse(json), QString("3.11.0"));
    }

    void parseVersionResponse_missingKey_returnsEmpty()
    {
        QJsonObject json;
        QCOMPARE(restresponse::parseVersionResponse(json), QString(""));
    }

    // =========================================================
    // parseInfoResponse
    // =========================================================

    void parseInfoResponse_allFields()
    {
        QJsonObject json;
        json["product"] = "Ultimate 64";
        json["firmware_version"] = "3.11.0";
        json["fpga_version"] = "1.2";
        json["core_version"] = "5.0";
        json["hostname"] = "mydevice";
        json["unique_id"] = "AABBCCDD";

        auto info = restresponse::parseInfoResponse(json);

        QCOMPARE(info.product, QString("Ultimate 64"));
        QCOMPARE(info.firmwareVersion, QString("3.11.0"));
        QCOMPARE(info.fpgaVersion, QString("1.2"));
        QCOMPARE(info.coreVersion, QString("5.0"));
        QCOMPARE(info.hostname, QString("mydevice"));
        QCOMPARE(info.uniqueId, QString("AABBCCDD"));
    }

    void parseInfoResponse_missingFields_defaultToEmpty()
    {
        QJsonObject json;
        auto info = restresponse::parseInfoResponse(json);
        QCOMPARE(info.product, QString(""));
        QCOMPARE(info.firmwareVersion, QString(""));
    }

    // =========================================================
    // parseDrivesResponse
    // =========================================================

    void parseDrivesResponse_singleDrive()
    {
        QJsonObject driveData;
        driveData["enabled"] = true;
        driveData["bus_id"] = 8;
        driveData["type"] = "1541";
        driveData["rom"] = "dos1541.rom";
        driveData["image_file"] = "game.d64";
        driveData["image_path"] = "/SD/game.d64";
        driveData["last_error"] = "";

        QJsonObject drive;
        drive["A"] = driveData;

        QJsonObject json;
        json["drives"] = QJsonArray{drive};

        auto drives = restresponse::parseDrivesResponse(json);

        QCOMPARE(drives.size(), 1);
        QCOMPARE(drives[0].name, QString("A"));
        QCOMPARE(drives[0].enabled, true);
        QCOMPARE(drives[0].busId, 8);
        QCOMPARE(drives[0].type, QString("1541"));
        QCOMPARE(drives[0].imageFile, QString("game.d64"));
        QCOMPARE(drives[0].imagePath, QString("/SD/game.d64"));
    }

    void parseDrivesResponse_multipleDrives()
    {
        QJsonObject driveDataA;
        driveDataA["enabled"] = true;
        driveDataA["bus_id"] = 8;
        QJsonObject driveA;
        driveA["A"] = driveDataA;

        QJsonObject driveDataB;
        driveDataB["enabled"] = false;
        driveDataB["bus_id"] = 9;
        QJsonObject driveB;
        driveB["B"] = driveDataB;

        QJsonObject json;
        json["drives"] = QJsonArray{driveA, driveB};

        auto drives = restresponse::parseDrivesResponse(json);
        QCOMPARE(drives.size(), 2);
    }

    void parseDrivesResponse_emptyArray()
    {
        QJsonObject json;
        json["drives"] = QJsonArray();
        auto drives = restresponse::parseDrivesResponse(json);
        QVERIFY(drives.isEmpty());
    }

    void parseDrivesResponse_missingKey()
    {
        QJsonObject json;
        auto drives = restresponse::parseDrivesResponse(json);
        QVERIFY(drives.isEmpty());
    }

    // =========================================================
    // parseFileInfoResponse
    // =========================================================

    void parseFileInfoResponse_allFields()
    {
        QJsonObject files;
        files["path"] = "/SD/game.prg";
        files["size"] = 12345;
        files["extension"] = "prg";

        QJsonObject json;
        json["files"] = files;

        auto result = restresponse::parseFileInfoResponse(json);

        QCOMPARE(result.path, QString("/SD/game.prg"));
        QCOMPARE(result.size, static_cast<qint64>(12345));
        QCOMPARE(result.extension, QString("prg"));
    }

    void parseFileInfoResponse_largeSize()
    {
        QJsonObject files;
        files["path"] = "/SD/large.d81";
        files["size"] = static_cast<qint64>(819200);
        files["extension"] = "d81";

        QJsonObject json;
        json["files"] = files;

        auto result = restresponse::parseFileInfoResponse(json);
        QCOMPARE(result.size, static_cast<qint64>(819200));
    }

    // =========================================================
    // parseConfigCategoriesResponse
    // =========================================================

    void parseConfigCategoriesResponse_multipleCategories()
    {
        QJsonObject json;
        json["categories"] =
            QJsonArray{QJsonValue("Video"), QJsonValue("Audio"), QJsonValue("Network")};

        auto categories = restresponse::parseConfigCategoriesResponse(json);

        QCOMPARE(categories.size(), 3);
        QVERIFY(categories.contains("Video"));
        QVERIFY(categories.contains("Audio"));
    }

    void parseConfigCategoriesResponse_emptyArray()
    {
        QJsonObject json;
        json["categories"] = QJsonArray();
        auto categories = restresponse::parseConfigCategoriesResponse(json);
        QVERIFY(categories.isEmpty());
    }

    // =========================================================
    // parseConfigCategoryItemsResponse
    // =========================================================

    void parseConfigCategoryItemsResponse_stringValue()
    {
        QJsonObject item;
        item["current"] = "NTSC";
        item["default"] = "PAL";

        QJsonObject category;
        category["VideoFormat"] = item;

        QJsonObject json;
        json["Video"] = category;

        auto items = restresponse::parseConfigCategoryItemsResponse("Video", json);

        QVERIFY(items.contains("VideoFormat"));
        QCOMPARE(items["VideoFormat"].current.toString(), QString("NTSC"));
        QCOMPARE(items["VideoFormat"].defaultValue.toString(), QString("PAL"));
    }

    void parseConfigCategoryItemsResponse_boolValue()
    {
        QJsonObject item;
        item["current"] = true;
        item["default"] = false;

        QJsonObject category;
        category["SoundEnabled"] = item;

        QJsonObject json;
        json["Audio"] = category;

        auto items = restresponse::parseConfigCategoryItemsResponse("Audio", json);

        QVERIFY(items["SoundEnabled"].current.toBool());
        QVERIFY(!items["SoundEnabled"].defaultValue.toBool());
    }

    void parseConfigCategoryItemsResponse_intValue()
    {
        QJsonObject item;
        item["current"] = 50.0;  // Whole double → int
        item["default"] = 100.0;

        QJsonObject category;
        category["Volume"] = item;

        QJsonObject json;
        json["Audio"] = category;

        auto items = restresponse::parseConfigCategoryItemsResponse("Audio", json);

        QCOMPARE(items["Volume"].current.typeId(), QMetaType::Int);
        QCOMPARE(items["Volume"].current.toInt(), 50);
    }

    void parseConfigCategoryItemsResponse_doubleValue()
    {
        QJsonObject item;
        item["current"] = 1.5;  // Non-integer double → double
        item["default"] = 1.0;

        QJsonObject category;
        category["Multiplier"] = item;

        QJsonObject json;
        json["Test"] = category;

        auto items = restresponse::parseConfigCategoryItemsResponse("Test", json);

        QCOMPARE(items["Multiplier"].current.typeId(), QMetaType::Double);
        QCOMPARE(items["Multiplier"].current.toDouble(), 1.5);
    }

    void parseConfigCategoryItemsResponse_withEnumValues()
    {
        QJsonObject item;
        item["current"] = "NTSC";
        item["default"] = "PAL";
        item["values"] = QJsonArray{QJsonValue("PAL"), QJsonValue("NTSC")};

        QJsonObject category;
        category["Format"] = item;

        QJsonObject json;
        json["Video"] = category;

        auto items = restresponse::parseConfigCategoryItemsResponse("Video", json);

        QCOMPARE(items["Format"].values.size(), 2);
        QVERIFY(items["Format"].values.contains("PAL"));
        QVERIFY(items["Format"].values.contains("NTSC"));
    }

    void parseConfigCategoryItemsResponse_withPresets()
    {
        QJsonObject item;
        item["current"] = "dos1541.rom";
        item["default"] = "";
        item["presets"] = QJsonArray{QJsonValue("dos1541.rom"), QJsonValue("dos1571.rom")};

        QJsonObject category;
        category["DosRom"] = item;

        QJsonObject json;
        json["Drive"] = category;

        auto items = restresponse::parseConfigCategoryItemsResponse("Drive", json);

        QCOMPARE(items["DosRom"].presets.size(), 2);
    }

    void parseConfigCategoryItemsResponse_withRange()
    {
        QJsonObject item;
        item["current"] = 50.0;
        item["default"] = 50.0;
        item["min"] = 0;
        item["max"] = 100;

        QJsonObject category;
        category["Volume"] = item;

        QJsonObject json;
        json["Audio"] = category;

        auto items = restresponse::parseConfigCategoryItemsResponse("Audio", json);

        QVERIFY(items["Volume"].hasRange);
        QCOMPARE(items["Volume"].min, 0);
        QCOMPARE(items["Volume"].max, 100);
    }

    void parseConfigCategoryItemsResponse_withFormat()
    {
        QJsonObject item;
        item["current"] = 20.0;
        item["default"] = 20.0;
        item["format"] = "%d00 ms";

        QJsonObject category;
        category["Delay"] = item;

        QJsonObject json;
        json["SID"] = category;

        auto items = restresponse::parseConfigCategoryItemsResponse("SID", json);

        QCOMPARE(items["Delay"].format, QString("%d00 ms"));
    }

    void parseConfigCategoryItemsResponse_multipleItems()
    {
        QJsonObject item1;
        item1["current"] = "NTSC";
        item1["default"] = "PAL";

        QJsonObject item2;
        item2["current"] = true;
        item2["default"] = false;

        QJsonObject category;
        category["Format"] = item1;
        category["Enabled"] = item2;

        QJsonObject json;
        json["Video"] = category;

        auto items = restresponse::parseConfigCategoryItemsResponse("Video", json);

        QCOMPARE(items.size(), 2);
        QVERIFY(items.contains("Format"));
        QVERIFY(items.contains("Enabled"));
    }

    void parseConfigCategoryItemsResponse_emptyCategory()
    {
        QJsonObject json;
        json["Empty"] = QJsonObject();

        auto items = restresponse::parseConfigCategoryItemsResponse("Empty", json);

        QVERIFY(items.isEmpty());
    }
};

QTEST_MAIN(TestRestResponseCore)
#include "test_restresponsecore.moc"
