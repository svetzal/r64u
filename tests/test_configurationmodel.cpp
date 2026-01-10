#include <QtTest>
#include <QSignalSpy>

#include "models/configurationmodel.h"

class TestConfigurationModel : public QObject
{
    Q_OBJECT

private:
    ConfigurationModel *model;

private slots:
    void init()
    {
        model = new ConfigurationModel(this);
    }

    void cleanup()
    {
        delete model;
        model = nullptr;
    }

    // Test initial state
    void testInitialState()
    {
        QVERIFY(model->categories().isEmpty());
        QVERIFY(!model->isDirty());
    }

    // Test setCategories
    void testSetCategories()
    {
        QSignalSpy spy(model, &ConfigurationModel::categoriesChanged);

        QStringList categories = {"Audio Mixer", "Network Settings", "Drive A Settings"};
        model->setCategories(categories);

        QCOMPARE(model->categories(), categories);
        QCOMPARE(spy.count(), 1);
    }

    // Test hasCategory
    void testHasCategory()
    {
        model->setCategories({"Audio Mixer", "Network Settings"});

        QVERIFY(model->hasCategory("Audio Mixer"));
        QVERIFY(model->hasCategory("Network Settings"));
        QVERIFY(!model->hasCategory("Nonexistent"));
    }

    // Test setCategoryItems
    void testSetCategoryItems()
    {
        model->setCategories({"Audio Mixer"});

        QSignalSpy spy(model, &ConfigurationModel::categoryItemsChanged);

        QHash<QString, QVariant> items;
        items["Volume"] = 80;
        items["Mute"] = false;
        items["Balance"] = "Center";
        model->setCategoryItems("Audio Mixer", items);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toString(), QString("Audio Mixer"));

        QCOMPARE(model->itemCount("Audio Mixer"), 3);
        QVERIFY(model->hasItem("Audio Mixer", "Volume"));
        QVERIFY(model->hasItem("Audio Mixer", "Mute"));
        QVERIFY(model->hasItem("Audio Mixer", "Balance"));
    }

    // Test setCategoryItemsWithInfo
    void testSetCategoryItemsWithInfo()
    {
        model->setCategories({"Audio Mixer"});

        QHash<QString, ConfigItemInfo> items;

        ConfigItemInfo volumeInfo;
        volumeInfo.value = 80;
        volumeInfo.defaultValue = 75;
        volumeInfo.minValue = 0;
        volumeInfo.maxValue = 100;
        items["Volume"] = volumeInfo;

        ConfigItemInfo muteInfo;
        muteInfo.value = false;
        muteInfo.options = {"On", "Off"};
        items["Mute"] = muteInfo;

        model->setCategoryItemsWithInfo("Audio Mixer", items);

        ConfigItemInfo retrieved = model->itemInfo("Audio Mixer", "Volume");
        QCOMPARE(retrieved.value.toInt(), 80);
        QCOMPARE(retrieved.defaultValue.toInt(), 75);
        QCOMPARE(retrieved.minValue.toInt(), 0);
        QCOMPARE(retrieved.maxValue.toInt(), 100);
        QVERIFY(!retrieved.isDirty);

        ConfigItemInfo muteRetrieved = model->itemInfo("Audio Mixer", "Mute");
        QCOMPARE(muteRetrieved.options, QStringList({"On", "Off"}));
    }

    // Test value()
    void testValue()
    {
        model->setCategories({"Audio Mixer"});

        QHash<QString, QVariant> items;
        items["Volume"] = 80;
        model->setCategoryItems("Audio Mixer", items);

        QCOMPARE(model->value("Audio Mixer", "Volume").toInt(), 80);

        // Nonexistent category
        QVERIFY(!model->value("Nonexistent", "Volume").isValid());

        // Nonexistent item
        QVERIFY(!model->value("Audio Mixer", "Nonexistent").isValid());
    }

    // Test itemNames()
    void testItemNames()
    {
        model->setCategories({"Audio Mixer"});

        QHash<QString, QVariant> items;
        items["Volume"] = 80;
        items["Mute"] = false;
        model->setCategoryItems("Audio Mixer", items);

        QStringList names = model->itemNames("Audio Mixer");
        QCOMPARE(names.size(), 2);
        QVERIFY(names.contains("Volume"));
        QVERIFY(names.contains("Mute"));

        // Nonexistent category
        QVERIFY(model->itemNames("Nonexistent").isEmpty());
    }

    // Test setValue and dirty tracking
    void testSetValueAndDirty()
    {
        model->setCategories({"Audio Mixer"});

        QHash<QString, QVariant> items;
        items["Volume"] = 80;
        model->setCategoryItems("Audio Mixer", items);

        QVERIFY(!model->isDirty());
        QVERIFY(!model->isItemDirty("Audio Mixer", "Volume"));

        QSignalSpy valueSpy(model, &ConfigurationModel::itemValueChanged);
        QSignalSpy dirtySpy(model, &ConfigurationModel::dirtyStateChanged);

        // Change the value
        bool changed = model->setValue("Audio Mixer", "Volume", 90);
        QVERIFY(changed);

        QCOMPARE(valueSpy.count(), 1);
        QCOMPARE(valueSpy.first().at(0).toString(), QString("Audio Mixer"));
        QCOMPARE(valueSpy.first().at(1).toString(), QString("Volume"));
        QCOMPARE(valueSpy.first().at(2).toInt(), 90);

        QCOMPARE(dirtySpy.count(), 1);
        QCOMPARE(dirtySpy.first().first().toBool(), true);

        QVERIFY(model->isDirty());
        QVERIFY(model->isItemDirty("Audio Mixer", "Volume"));
        QCOMPARE(model->value("Audio Mixer", "Volume").toInt(), 90);
    }

    // Test setValue with same value (should not mark dirty)
    void testSetValueSameValue()
    {
        model->setCategories({"Audio Mixer"});

        QHash<QString, QVariant> items;
        items["Volume"] = 80;
        model->setCategoryItems("Audio Mixer", items);

        QSignalSpy valueSpy(model, &ConfigurationModel::itemValueChanged);

        bool changed = model->setValue("Audio Mixer", "Volume", 80);
        QVERIFY(!changed);
        QCOMPARE(valueSpy.count(), 0);
        QVERIFY(!model->isDirty());
    }

    // Test setValue on nonexistent item
    void testSetValueNonexistent()
    {
        model->setCategories({"Audio Mixer"});

        bool changed = model->setValue("Audio Mixer", "Nonexistent", 50);
        QVERIFY(!changed);

        changed = model->setValue("Nonexistent", "Volume", 50);
        QVERIFY(!changed);
    }

    // Test dirtyItems()
    void testDirtyItems()
    {
        model->setCategories({"Audio Mixer", "Network"});

        QHash<QString, QVariant> audioItems;
        audioItems["Volume"] = 80;
        audioItems["Mute"] = false;
        model->setCategoryItems("Audio Mixer", audioItems);

        QHash<QString, QVariant> networkItems;
        networkItems["IP"] = "192.168.1.1";
        model->setCategoryItems("Network", networkItems);

        // Modify some items
        model->setValue("Audio Mixer", "Volume", 90);
        model->setValue("Network", "IP", "10.0.0.1");

        QHash<QString, QVariant> dirty = model->dirtyItems();
        QCOMPARE(dirty.size(), 2);
        QCOMPARE(dirty["Audio Mixer/Volume"].toInt(), 90);
        QCOMPARE(dirty["Network/IP"].toString(), QString("10.0.0.1"));
    }

    // Test clearDirtyFlags()
    void testClearDirtyFlags()
    {
        model->setCategories({"Audio Mixer"});

        QHash<QString, QVariant> items;
        items["Volume"] = 80;
        model->setCategoryItems("Audio Mixer", items);

        model->setValue("Audio Mixer", "Volume", 90);
        QVERIFY(model->isDirty());

        QSignalSpy dirtySpy(model, &ConfigurationModel::dirtyStateChanged);

        model->clearDirtyFlags();

        QVERIFY(!model->isDirty());
        QVERIFY(!model->isItemDirty("Audio Mixer", "Volume"));
        QCOMPARE(dirtySpy.count(), 1);
        QCOMPARE(dirtySpy.first().first().toBool(), false);

        // Value should still be the new value
        QCOMPARE(model->value("Audio Mixer", "Volume").toInt(), 90);
    }

    // Test clearDirtyFlags when not dirty
    void testClearDirtyFlagsWhenNotDirty()
    {
        model->setCategories({"Audio Mixer"});

        QHash<QString, QVariant> items;
        items["Volume"] = 80;
        model->setCategoryItems("Audio Mixer", items);

        QSignalSpy dirtySpy(model, &ConfigurationModel::dirtyStateChanged);

        model->clearDirtyFlags();

        // No signal should be emitted
        QCOMPARE(dirtySpy.count(), 0);
    }

    // Test clearItemDirtyFlag()
    void testClearItemDirtyFlag()
    {
        model->setCategories({"Audio Mixer"});

        QHash<QString, QVariant> items;
        items["Volume"] = 80;
        items["Mute"] = false;
        model->setCategoryItems("Audio Mixer", items);

        model->setValue("Audio Mixer", "Volume", 90);
        model->setValue("Audio Mixer", "Mute", true);

        QVERIFY(model->isDirty());

        // Clear just one item
        model->clearItemDirtyFlag("Audio Mixer", "Volume");

        QVERIFY(!model->isItemDirty("Audio Mixer", "Volume"));
        QVERIFY(model->isItemDirty("Audio Mixer", "Mute"));
        QVERIFY(model->isDirty());  // Still dirty (Mute)

        QSignalSpy dirtySpy(model, &ConfigurationModel::dirtyStateChanged);

        // Clear the last dirty item
        model->clearItemDirtyFlag("Audio Mixer", "Mute");

        QVERIFY(!model->isDirty());
        QCOMPARE(dirtySpy.count(), 1);
        QCOMPARE(dirtySpy.first().first().toBool(), false);
    }

    // Test clearItemDirtyFlag on nonexistent items
    void testClearItemDirtyFlagNonexistent()
    {
        model->setCategories({"Audio Mixer"});

        QHash<QString, QVariant> items;
        items["Volume"] = 80;
        model->setCategoryItems("Audio Mixer", items);

        model->setValue("Audio Mixer", "Volume", 90);

        // Should not crash
        model->clearItemDirtyFlag("Nonexistent", "Volume");
        model->clearItemDirtyFlag("Audio Mixer", "Nonexistent");

        QVERIFY(model->isDirty());  // Still dirty
    }

    // Test clear()
    void testClear()
    {
        model->setCategories({"Audio Mixer", "Network"});

        QHash<QString, QVariant> items;
        items["Volume"] = 80;
        model->setCategoryItems("Audio Mixer", items);

        model->setValue("Audio Mixer", "Volume", 90);

        QSignalSpy catSpy(model, &ConfigurationModel::categoriesChanged);
        QSignalSpy dirtySpy(model, &ConfigurationModel::dirtyStateChanged);

        model->clear();

        QVERIFY(model->categories().isEmpty());
        QVERIFY(!model->isDirty());
        QCOMPARE(catSpy.count(), 1);
        QCOMPARE(dirtySpy.count(), 1);
        QCOMPARE(dirtySpy.first().first().toBool(), false);
    }

    // Test clear() when already empty
    void testClearWhenEmpty()
    {
        QSignalSpy dirtySpy(model, &ConfigurationModel::dirtyStateChanged);

        model->clear();

        // No dirty state signal should be emitted (wasn't dirty)
        QCOMPARE(dirtySpy.count(), 0);
    }

    // Test that setCategories clears existing data
    void testSetCategoriesClearsExisting()
    {
        model->setCategories({"Audio Mixer"});

        QHash<QString, QVariant> items;
        items["Volume"] = 80;
        model->setCategoryItems("Audio Mixer", items);

        model->setValue("Audio Mixer", "Volume", 90);
        QVERIFY(model->isDirty());

        QSignalSpy dirtySpy(model, &ConfigurationModel::dirtyStateChanged);

        // Set new categories
        model->setCategories({"Network"});

        QVERIFY(!model->hasCategory("Audio Mixer"));
        QVERIFY(!model->isDirty());
        QCOMPARE(dirtySpy.count(), 1);
        QCOMPARE(dirtySpy.first().first().toBool(), false);
    }

    // Test that setCategoryItems clears dirty count for replaced items
    void testSetCategoryItemsUpdatesDirtyCount()
    {
        model->setCategories({"Audio Mixer"});

        QHash<QString, QVariant> items;
        items["Volume"] = 80;
        items["Mute"] = false;
        model->setCategoryItems("Audio Mixer", items);

        model->setValue("Audio Mixer", "Volume", 90);
        model->setValue("Audio Mixer", "Mute", true);
        QVERIFY(model->isDirty());

        QSignalSpy dirtySpy(model, &ConfigurationModel::dirtyStateChanged);

        // Replace items (simulating reload from server)
        QHash<QString, QVariant> newItems;
        newItems["Volume"] = 90;  // Server now has the new value
        newItems["Mute"] = true;
        model->setCategoryItems("Audio Mixer", newItems);

        // Items should no longer be dirty
        QVERIFY(!model->isDirty());
        QCOMPARE(dirtySpy.count(), 1);
    }

    // Test itemInfo on nonexistent items
    void testItemInfoNonexistent()
    {
        model->setCategories({"Audio Mixer"});

        ConfigItemInfo info = model->itemInfo("Nonexistent", "Volume");
        QVERIFY(!info.value.isValid());

        info = model->itemInfo("Audio Mixer", "Nonexistent");
        QVERIFY(!info.value.isValid());
    }

    // Test hasItem
    void testHasItem()
    {
        model->setCategories({"Audio Mixer"});

        QHash<QString, QVariant> items;
        items["Volume"] = 80;
        model->setCategoryItems("Audio Mixer", items);

        QVERIFY(model->hasItem("Audio Mixer", "Volume"));
        QVERIFY(!model->hasItem("Audio Mixer", "Nonexistent"));
        QVERIFY(!model->hasItem("Nonexistent", "Volume"));
    }

    // Test itemCount on nonexistent category
    void testItemCountNonexistent()
    {
        QCOMPARE(model->itemCount("Nonexistent"), 0);
    }

    // Test isItemDirty on nonexistent items
    void testIsItemDirtyNonexistent()
    {
        model->setCategories({"Audio Mixer"});

        QVERIFY(!model->isItemDirty("Nonexistent", "Volume"));
        QVERIFY(!model->isItemDirty("Audio Mixer", "Nonexistent"));
    }

    // Test multiple setValue calls on same item (only first marks dirty)
    void testMultipleSetValue()
    {
        model->setCategories({"Audio Mixer"});

        QHash<QString, QVariant> items;
        items["Volume"] = 80;
        model->setCategoryItems("Audio Mixer", items);

        QSignalSpy dirtySpy(model, &ConfigurationModel::dirtyStateChanged);

        model->setValue("Audio Mixer", "Volume", 90);
        model->setValue("Audio Mixer", "Volume", 95);
        model->setValue("Audio Mixer", "Volume", 100);

        // Only one dirtyStateChanged signal (when first became dirty)
        QCOMPARE(dirtySpy.count(), 1);

        QCOMPARE(model->value("Audio Mixer", "Volume").toInt(), 100);
    }

    // Test with various data types
    void testVariousDataTypes()
    {
        model->setCategories({"Settings"});

        QHash<QString, QVariant> items;
        items["IntValue"] = 42;
        items["BoolValue"] = true;
        items["StringValue"] = "Hello";
        items["DoubleValue"] = 3.14;
        model->setCategoryItems("Settings", items);

        QCOMPARE(model->value("Settings", "IntValue").toInt(), 42);
        QCOMPARE(model->value("Settings", "BoolValue").toBool(), true);
        QCOMPARE(model->value("Settings", "StringValue").toString(), QString("Hello"));
        QCOMPARE(model->value("Settings", "DoubleValue").toDouble(), 3.14);

        // Modify each type
        model->setValue("Settings", "IntValue", 100);
        model->setValue("Settings", "BoolValue", false);
        model->setValue("Settings", "StringValue", "World");
        model->setValue("Settings", "DoubleValue", 2.71);

        QCOMPARE(model->value("Settings", "IntValue").toInt(), 100);
        QCOMPARE(model->value("Settings", "BoolValue").toBool(), false);
        QCOMPARE(model->value("Settings", "StringValue").toString(), QString("World"));
        QCOMPARE(model->value("Settings", "DoubleValue").toDouble(), 2.71);
    }
};

QTEST_MAIN(TestConfigurationModel)
#include "test_configurationmodel.moc"
