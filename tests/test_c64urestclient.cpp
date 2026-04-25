#include <QtTest>

// Use angle brackets to force include path search order
#include <services/c64urestclient.h>

class TestC64URestClient : public QObject
{
    Q_OBJECT

private:
    C64URestClient *client;

private slots:
    void init() { client = new C64URestClient(); }

    void cleanup()
    {
        delete client;
        client = nullptr;
    }

    // === Initial State Tests ===

    void testInitialState_HostIsEmpty()
    {
        QCOMPARE(client->host(), QString(""));
    }

    void testInitialState_HasNoPassword()
    {
        QVERIFY(!client->hasPassword());
    }

    // === setHost() Normalization Tests ===

    void testSetHost_BareHostname_AddsHttpScheme()
    {
        client->setHost("192.168.1.64");
        QCOMPARE(client->host(), QString("http://192.168.1.64"));
    }

    void testSetHost_AlreadyHasHttp_NotDoubled()
    {
        client->setHost("http://192.168.1.64");
        QCOMPARE(client->host(), QString("http://192.168.1.64"));
    }

    void testSetHost_AlreadyHasHttps_Unchanged()
    {
        client->setHost("https://192.168.1.64");
        QCOMPARE(client->host(), QString("https://192.168.1.64"));
    }

    void testSetHost_TrailingSlash_IsStripped()
    {
        client->setHost("192.168.1.64/");
        QCOMPARE(client->host(), QString("http://192.168.1.64"));
    }

    void testSetHost_HttpWithTrailingSlash_SlashStripped()
    {
        client->setHost("http://192.168.1.64/");
        QCOMPARE(client->host(), QString("http://192.168.1.64"));
    }

    void testSetHost_MultipleCallsOverwritePreviousValue()
    {
        client->setHost("192.168.1.1");
        QCOMPARE(client->host(), QString("http://192.168.1.1"));

        client->setHost("10.0.0.1");
        QCOMPARE(client->host(), QString("http://10.0.0.1"));
    }

    // === Password State Tests ===

    void testSetPassword_NonEmpty_HasPasswordTrue()
    {
        client->setPassword("secret");
        QVERIFY(client->hasPassword());
    }

    void testSetPassword_Empty_HasPasswordFalse()
    {
        client->setPassword("secret");
        client->setPassword("");
        QVERIFY(!client->hasPassword());
    }
};

QTEST_MAIN(TestC64URestClient)
#include "test_c64urestclient.moc"
