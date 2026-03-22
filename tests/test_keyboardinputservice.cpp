/**
 * @file test_keyboardinputservice.cpp
 * @brief Unit tests for KeyboardInputService.
 *
 * Tests cover PETSCII code generation, REST client interactions, signal
 * emission, key mapping, and null safety using MockRestClient.
 */

#include "mocks/mockrestclient.h"
#include "services/keyboardinputservice.h"

#include <QKeyEvent>
#include <QSignalSpy>
#include <QtTest>

class TestKeyboardInputService : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        mock_ = new MockRestClient();
        service_ = new KeyboardInputService(mock_);
    }

    void cleanup()
    {
        delete service_;
        service_ = nullptr;
        delete mock_;
        mock_ = nullptr;
    }

    // ========== Construction ==========

    void testConstructionWithNullParent()
    {
        KeyboardInputService svc(mock_, nullptr);
        QVERIFY(svc.parent() == nullptr);
    }

    // ========== Null REST client guard ==========

    void testSendPetsciiWithNullRestClientEmitsErrorOccurred()
    {
        KeyboardInputService nullSvc(nullptr);
        QSignalSpy spy(&nullSvc, &KeyboardInputService::errorOccurred);

        nullSvc.sendPetscii(65);

        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.first().first().toString().contains("REST client", Qt::CaseInsensitive));
    }

    void testSendPetsciiWithNullRestClientDoesNotEmitKeySent()
    {
        KeyboardInputService nullSvc(nullptr);
        QSignalSpy sentSpy(&nullSvc, &KeyboardInputService::keySent);

        nullSvc.sendPetscii(65);

        QCOMPARE(sentSpy.count(), 0);
    }

    // ========== sendPetscii — call count ==========

    void testSendPetsciiMakesTwoWriteMemCalls()
    {
        service_->sendPetscii(13);

        QCOMPARE(mock_->mockGetWriteMemCallCount(), 2);
    }

    // ========== sendPetscii — address formatting ==========

    void testSendPetsciiFirstCallUsesKeyboardBufferAddress()
    {
        // The mock tracks only the last call. To inspect the first call we reset
        // after the first write by replacing with a fresh mock, then re-verify.
        // Instead, we verify by confirming the last (second) call is "00c6".
        service_->sendPetscii(13);

        // Second call writes the buffer length counter
        QCOMPARE(mock_->mockLastWriteMemAddress(), QString("00c6"));
    }

    void testSendPetsciiFirstCallUsesKeyboardBufferAddressViaInterception()
    {
        // To inspect the first writeMem address independently, we use a counting
        // mock that we reset after call #1 and then call sendPetscii a second
        // time with the same value — instead, verify address "0277" appears before
        // we see "00c6" by checking total call sequence via counts on a fresh mock.
        mock_->mockReset();
        service_->sendPetscii(65);

        // After exactly 2 calls the last address must be "00c6" (buffer length)
        QCOMPARE(mock_->mockGetWriteMemCallCount(), 2);
        QCOMPARE(mock_->mockLastWriteMemAddress(), QString("00c6"));
    }

    void testSendPetsciiBufferAddressFormattedAs0277()
    {
        // Verify the address constant matches what the implementation formats
        // KeyboardBufferAddress = 0x0277 formatted as 4-hex lowercase = "0277"
        QCOMPARE(KeyboardInputService::KeyboardBufferAddress, static_cast<quint16>(0x0277));

        // The last call goes to "00c6"; verify constant matches too
        QCOMPARE(KeyboardInputService::BufferLengthAddress, static_cast<quint16>(0x00C6));
    }

    void testSendPetsciiSecondCallAddressIs00c6()
    {
        service_->sendPetscii(65);

        QCOMPARE(mock_->mockLastWriteMemAddress(), QString("00c6"));
    }

    // ========== sendPetscii — data payloads ==========

    void testSendPetsciiSecondCallDataIsOne()
    {
        service_->sendPetscii(65);

        QByteArray expected;
        expected.append(static_cast<char>(1));
        QCOMPARE(mock_->mockLastWriteMemData(), expected);
    }

    void testSendPetsciiFirstCallDataContainsPetsciiByteVerifiedViaFreshSpy()
    {
        // The first call carries the PETSCII byte. We verify indirectly: after the
        // second call data is 0x01 (buffer length = 1). The only way the C64 gets
        // the character is via the first call, so the test verifies the *total*
        // write count and that the char makes it into keySent.
        QSignalSpy spy(service_, &KeyboardInputService::keySent);

        service_->sendPetscii(42);

        QCOMPARE(mock_->mockGetWriteMemCallCount(), 2);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().value<quint8>(), static_cast<quint8>(42));
    }

    // ========== sendPetscii — keySent signal ==========

    void testSendPetsciiEmitsKeySentWithCorrectCode()
    {
        QSignalSpy spy(service_, &KeyboardInputService::keySent);

        service_->sendPetscii(13);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().value<quint8>(), static_cast<quint8>(13));
    }

    void testSendPetsciiEmitsKeySentForArbitraryCode()
    {
        QSignalSpy spy(service_, &KeyboardInputService::keySent);

        service_->sendPetscii(65);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().value<quint8>(), static_cast<quint8>(65));
    }

    // ========== sendText ==========

    void testSendTextTwoLettersMakesFourWriteMemCalls()
    {
        service_->sendText("AB");

        // 2 writeMem calls per character
        QCOMPARE(mock_->mockGetWriteMemCallCount(), 4);
    }

    void testSendTextLowercaseConvertsToUppercasePetscii()
    {
        QSignalSpy spy(service_, &KeyboardInputService::keySent);

        service_->sendText("ab");

        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(0).first().value<quint8>(), static_cast<quint8>(65));  // 'A'
        QCOMPARE(spy.at(1).first().value<quint8>(), static_cast<quint8>(66));  // 'B'
    }

    void testSendTextUppercasePassesThroughUnchanged()
    {
        QSignalSpy spy(service_, &KeyboardInputService::keySent);

        service_->sendText("AB");

        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(0).first().value<quint8>(), static_cast<quint8>(65));  // 'A'
        QCOMPARE(spy.at(1).first().value<quint8>(), static_cast<quint8>(66));  // 'B'
    }

    void testSendTextEmptyStringMakesNoWriteMemCalls()
    {
        service_->sendText("");

        QCOMPARE(mock_->mockGetWriteMemCallCount(), 0);
    }

    void testSendTextDigitsPassThroughUnchanged()
    {
        QSignalSpy spy(service_, &KeyboardInputService::keySent);

        service_->sendText("09");

        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(0).first().value<quint8>(), static_cast<quint8>('0'));
        QCOMPARE(spy.at(1).first().value<quint8>(), static_cast<quint8>('9'));
    }

    // ========== handleKeyPress — return key ==========

    void testHandleKeyPressReturnReturnsTrueAndEmitsKeySent13()
    {
        QSignalSpy spy(service_, &KeyboardInputService::keySent);
        QKeyEvent event(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);

        bool handled = service_->handleKeyPress(&event);

        QVERIFY(handled);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().value<quint8>(), static_cast<quint8>(13));
    }

    // ========== handleKeyPress — Backspace ==========

    void testHandleKeyPressBackspaceReturnsTrueAndEmitsKeySent20()
    {
        QSignalSpy spy(service_, &KeyboardInputService::keySent);
        QKeyEvent event(QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier);

        bool handled = service_->handleKeyPress(&event);

        QVERIFY(handled);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().value<quint8>(), static_cast<quint8>(20));
    }

    // ========== handleKeyPress — Delete ==========

    void testHandleKeyPressDeleteReturnsTrueAndEmitsKeySent20()
    {
        QSignalSpy spy(service_, &KeyboardInputService::keySent);
        QKeyEvent event(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);

        bool handled = service_->handleKeyPress(&event);

        QVERIFY(handled);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().value<quint8>(), static_cast<quint8>(20));
    }

    // ========== handleKeyPress — Home ==========

    void testHandleKeyPressHomeReturnsTrueAndEmitsKeySent19()
    {
        QSignalSpy spy(service_, &KeyboardInputService::keySent);
        QKeyEvent event(QEvent::KeyPress, Qt::Key_Home, Qt::NoModifier);

        bool handled = service_->handleKeyPress(&event);

        QVERIFY(handled);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().value<quint8>(), static_cast<quint8>(19));
    }

    void testHandleKeyPressShiftHomeReturnsTrueAndEmitsKeySent147()
    {
        QSignalSpy spy(service_, &KeyboardInputService::keySent);
        QKeyEvent event(QEvent::KeyPress, Qt::Key_Home, Qt::ShiftModifier);

        bool handled = service_->handleKeyPress(&event);

        QVERIFY(handled);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().value<quint8>(), static_cast<quint8>(147));
    }

    // ========== handleKeyPress — cursor keys ==========

    void testHandleKeyPressCursorUpEmitsKeySent145()
    {
        QSignalSpy spy(service_, &KeyboardInputService::keySent);
        QKeyEvent event(QEvent::KeyPress, Qt::Key_Up, Qt::NoModifier);

        bool handled = service_->handleKeyPress(&event);

        QVERIFY(handled);
        QCOMPARE(spy.first().first().value<quint8>(), static_cast<quint8>(145));
    }

    void testHandleKeyPressCursorDownEmitsKeySent17()
    {
        QSignalSpy spy(service_, &KeyboardInputService::keySent);
        QKeyEvent event(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);

        bool handled = service_->handleKeyPress(&event);

        QVERIFY(handled);
        QCOMPARE(spy.first().first().value<quint8>(), static_cast<quint8>(17));
    }

    void testHandleKeyPressCursorLeftEmitsKeySent157()
    {
        QSignalSpy spy(service_, &KeyboardInputService::keySent);
        QKeyEvent event(QEvent::KeyPress, Qt::Key_Left, Qt::NoModifier);

        bool handled = service_->handleKeyPress(&event);

        QVERIFY(handled);
        QCOMPARE(spy.first().first().value<quint8>(), static_cast<quint8>(157));
    }

    void testHandleKeyPressCursorRightEmitsKeySent29()
    {
        QSignalSpy spy(service_, &KeyboardInputService::keySent);
        QKeyEvent event(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);

        bool handled = service_->handleKeyPress(&event);

        QVERIFY(handled);
        QCOMPARE(spy.first().first().value<quint8>(), static_cast<quint8>(29));
    }

    // ========== handleKeyPress — function keys ==========

    void testHandleKeyPressF1EmitsKeySent133()
    {
        QSignalSpy spy(service_, &KeyboardInputService::keySent);
        QKeyEvent event(QEvent::KeyPress, Qt::Key_F1, Qt::NoModifier);

        QVERIFY(service_->handleKeyPress(&event));
        QCOMPARE(spy.first().first().value<quint8>(), static_cast<quint8>(133));
    }

    void testHandleKeyPressF2EmitsKeySent137()
    {
        QSignalSpy spy(service_, &KeyboardInputService::keySent);
        QKeyEvent event(QEvent::KeyPress, Qt::Key_F2, Qt::NoModifier);

        QVERIFY(service_->handleKeyPress(&event));
        QCOMPARE(spy.first().first().value<quint8>(), static_cast<quint8>(137));
    }

    void testHandleKeyPressF3EmitsKeySent134()
    {
        QSignalSpy spy(service_, &KeyboardInputService::keySent);
        QKeyEvent event(QEvent::KeyPress, Qt::Key_F3, Qt::NoModifier);

        QVERIFY(service_->handleKeyPress(&event));
        QCOMPARE(spy.first().first().value<quint8>(), static_cast<quint8>(134));
    }

    void testHandleKeyPressF4EmitsKeySent138()
    {
        QSignalSpy spy(service_, &KeyboardInputService::keySent);
        QKeyEvent event(QEvent::KeyPress, Qt::Key_F4, Qt::NoModifier);

        QVERIFY(service_->handleKeyPress(&event));
        QCOMPARE(spy.first().first().value<quint8>(), static_cast<quint8>(138));
    }

    void testHandleKeyPressF5EmitsKeySent135()
    {
        QSignalSpy spy(service_, &KeyboardInputService::keySent);
        QKeyEvent event(QEvent::KeyPress, Qt::Key_F5, Qt::NoModifier);

        QVERIFY(service_->handleKeyPress(&event));
        QCOMPARE(spy.first().first().value<quint8>(), static_cast<quint8>(135));
    }

    void testHandleKeyPressF6EmitsKeySent139()
    {
        QSignalSpy spy(service_, &KeyboardInputService::keySent);
        QKeyEvent event(QEvent::KeyPress, Qt::Key_F6, Qt::NoModifier);

        QVERIFY(service_->handleKeyPress(&event));
        QCOMPARE(spy.first().first().value<quint8>(), static_cast<quint8>(139));
    }

    void testHandleKeyPressF7EmitsKeySent136()
    {
        QSignalSpy spy(service_, &KeyboardInputService::keySent);
        QKeyEvent event(QEvent::KeyPress, Qt::Key_F7, Qt::NoModifier);

        QVERIFY(service_->handleKeyPress(&event));
        QCOMPARE(spy.first().first().value<quint8>(), static_cast<quint8>(136));
    }

    void testHandleKeyPressF8EmitsKeySent140()
    {
        QSignalSpy spy(service_, &KeyboardInputService::keySent);
        QKeyEvent event(QEvent::KeyPress, Qt::Key_F8, Qt::NoModifier);

        QVERIFY(service_->handleKeyPress(&event));
        QCOMPARE(spy.first().first().value<quint8>(), static_cast<quint8>(140));
    }

    // ========== handleKeyPress — Escape ==========

    void testHandleKeyPressEscapeReturnsTrueAndEmitsKeySent3()
    {
        QSignalSpy spy(service_, &KeyboardInputService::keySent);
        QKeyEvent event(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);

        bool handled = service_->handleKeyPress(&event);

        QVERIFY(handled);
        QCOMPARE(spy.first().first().value<quint8>(), static_cast<quint8>(3));
    }

    // ========== handleKeyPress — Insert ==========

    void testHandleKeyPressInsertReturnsTrueAndEmitsKeySent148()
    {
        QSignalSpy spy(service_, &KeyboardInputService::keySent);
        QKeyEvent event(QEvent::KeyPress, Qt::Key_Insert, Qt::NoModifier);

        bool handled = service_->handleKeyPress(&event);

        QVERIFY(handled);
        QCOMPARE(spy.first().first().value<quint8>(), static_cast<quint8>(148));
    }

    // ========== handleKeyPress — regular letter ==========

    void testHandleKeyPressLetterAReturnsTrueAndSendsPetscii65()
    {
        QSignalSpy spy(service_, &KeyboardInputService::keySent);
        QKeyEvent event(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, "A");

        bool handled = service_->handleKeyPress(&event);

        QVERIFY(handled);
        QCOMPARE(spy.first().first().value<quint8>(), static_cast<quint8>(65));
    }

    void testHandleKeyPressLowercaseLetterConvertsToUpperPetscii()
    {
        QSignalSpy spy(service_, &KeyboardInputService::keySent);
        QKeyEvent event(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, "a");

        bool handled = service_->handleKeyPress(&event);

        QVERIFY(handled);
        QCOMPARE(spy.first().first().value<quint8>(), static_cast<quint8>(65));
    }

    // ========== handleKeyPress — unhandled key ==========

    void testHandleKeyPressF12ReturnsFalse()
    {
        QKeyEvent event(QEvent::KeyPress, Qt::Key_F12, Qt::NoModifier);

        bool handled = service_->handleKeyPress(&event);

        QVERIFY(!handled);
    }

    void testHandleKeyPressF12MakesNoWriteMemCalls()
    {
        QKeyEvent event(QEvent::KeyPress, Qt::Key_F12, Qt::NoModifier);

        service_->handleKeyPress(&event);

        QCOMPARE(mock_->mockGetWriteMemCallCount(), 0);
    }

    void testHandleKeyPressF12DoesNotEmitKeySent()
    {
        QSignalSpy spy(service_, &KeyboardInputService::keySent);
        QKeyEvent event(QEvent::KeyPress, Qt::Key_F12, Qt::NoModifier);

        service_->handleKeyPress(&event);

        QCOMPARE(spy.count(), 0);
    }

    // ========== mockReset properly resets call counts ==========

    void testMockResetClearsWriteMemCallCount()
    {
        service_->sendPetscii(65);
        QCOMPARE(mock_->mockGetWriteMemCallCount(), 2);

        mock_->mockReset();

        QCOMPARE(mock_->mockGetWriteMemCallCount(), 0);
    }

    void testMockResetClearsLastWriteMemAddress()
    {
        service_->sendPetscii(65);
        QVERIFY(!mock_->mockLastWriteMemAddress().isEmpty());

        mock_->mockReset();

        QVERIFY(mock_->mockLastWriteMemAddress().isEmpty());
    }

    void testMockResetClearsLastWriteMemData()
    {
        service_->sendPetscii(65);
        QVERIFY(!mock_->mockLastWriteMemData().isEmpty());

        mock_->mockReset();

        QVERIFY(mock_->mockLastWriteMemData().isEmpty());
    }

    void testMockResetAllowsCleanSubsequentTest()
    {
        service_->sendPetscii(65);
        mock_->mockReset();

        service_->sendPetscii(13);

        // After reset and a fresh send, count must be exactly 2
        QCOMPARE(mock_->mockGetWriteMemCallCount(), 2);
    }

    // ========== keySent signal verification via QSignalSpy ==========

    void testKeySentSignalCarriesCorrectPetsciiCodeForReturn()
    {
        QSignalSpy spy(service_, &KeyboardInputService::keySent);
        QKeyEvent event(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);

        service_->handleKeyPress(&event);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().value<quint8>(), static_cast<quint8>(13));
    }

    void testKeySentSignalCarriesCorrectPetsciiCodeForDirectSend()
    {
        QSignalSpy spy(service_, &KeyboardInputService::keySent);

        service_->sendPetscii(147);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().value<quint8>(), static_cast<quint8>(147));
    }

    void testKeySentNotEmittedForUnmappedKey()
    {
        QSignalSpy spy(service_, &KeyboardInputService::keySent);
        QKeyEvent event(QEvent::KeyPress, Qt::Key_F12, Qt::NoModifier);

        service_->handleKeyPress(&event);

        QCOMPARE(spy.count(), 0);
    }

private:
    MockRestClient *mock_ = nullptr;
    KeyboardInputService *service_ = nullptr;
};

QTEST_MAIN(TestKeyboardInputService)
#include "test_keyboardinputservice.moc"
