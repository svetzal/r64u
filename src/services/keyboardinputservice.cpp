/**
 * @file keyboardinputservice.cpp
 * @brief Implementation of the keyboard input service.
 */

#include "keyboardinputservice.h"
#include "c64urestclient.h"
#include "utils/logging.h"

KeyboardInputService::KeyboardInputService(C64URestClient *restClient, QObject *parent)
    : QObject(parent)
    , restClient_(restClient)
{
}

bool KeyboardInputService::handleKeyPress(QKeyEvent *event)
{
    quint8 petscii = keyToPetscii(event);
    if (petscii != 0) {
        sendPetscii(petscii);
        return true;
    }
    return false;
}

void KeyboardInputService::sendPetscii(quint8 petscii)
{
    if (!restClient_) {
        emit errorOccurred("No REST client configured");
        return;
    }

    LOG_VERBOSE() << "KeyboardInputService: Sending PETSCII" << petscii
                  << "(" << QChar(petscii > 31 && petscii < 127 ? petscii : '?') << ")";

    // Write the character to the keyboard buffer
    // We write to position 0 and set buffer length to 1
    // The KERNAL will consume it on the next scan
    QByteArray bufferData;
    bufferData.append(static_cast<char>(petscii));

    // Write character to buffer at $0277
    restClient_->writeMem(QString("%1").arg(KeyboardBufferAddress, 4, 16, QChar('0')),
                          bufferData);

    // Set buffer length to 1 at $00C6
    QByteArray lengthData;
    lengthData.append(static_cast<char>(1));
    restClient_->writeMem(QString("%1").arg(BufferLengthAddress, 4, 16, QChar('0')),
                          lengthData);

    emit keySent(petscii);
}

void KeyboardInputService::sendText(const QString &text)
{
    for (const QChar &ch : text) {
        char ascii = ch.toLatin1();
        quint8 petscii = asciiToPetscii(ascii);
        if (petscii != 0) {
            sendPetscii(petscii);
        }
    }
}

quint8 KeyboardInputService::keyToPetscii(QKeyEvent *event) const
{
    int key = event->key();
    Qt::KeyboardModifiers mods = event->modifiers();

    // Handle special keys first
    switch (key) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return 13;  // RETURN

    case Qt::Key_Backspace:
        return 20;  // DEL (delete/backspace in PETSCII)

    case Qt::Key_Delete:
        return 20;  // DEL

    case Qt::Key_Home:
        if (mods & Qt::ShiftModifier) {
            return 147; // CLR (clear screen)
        }
        return 19;  // HOME

    case Qt::Key_Up:
        return 145; // Cursor up

    case Qt::Key_Down:
        return 17;  // Cursor down

    case Qt::Key_Left:
        return 157; // Cursor left

    case Qt::Key_Right:
        return 29;  // Cursor right

    case Qt::Key_F1:
        return 133; // F1

    case Qt::Key_F2:
        return 137; // F2

    case Qt::Key_F3:
        return 134; // F3

    case Qt::Key_F4:
        return 138; // F4

    case Qt::Key_F5:
        return 135; // F5

    case Qt::Key_F6:
        return 139; // F6

    case Qt::Key_F7:
        return 136; // F7

    case Qt::Key_F8:
        return 140; // F8

    case Qt::Key_Escape:
        return 3;   // RUN/STOP (may not work remotely)

    case Qt::Key_Insert:
        return 148; // INSERT

    default:
        break;
    }

    // For regular characters, use the text from the event
    QString text = event->text();
    if (!text.isEmpty()) {
        char ascii = text.at(0).toLatin1();
        return asciiToPetscii(ascii);
    }

    return 0; // Key not handled
}

quint8 KeyboardInputService::asciiToPetscii(char ascii)
{
    // Space
    if (ascii == ' ') {
        return 32;
    }

    // Numbers 0-9 (same in ASCII and PETSCII)
    if (ascii >= '0' && ascii <= '9') {
        return static_cast<quint8>(ascii);
    }

    // Uppercase letters A-Z (same in ASCII and PETSCII)
    if (ascii >= 'A' && ascii <= 'Z') {
        return static_cast<quint8>(ascii);
    }

    // Lowercase letters a-z -> convert to uppercase for BASIC
    if (ascii >= 'a' && ascii <= 'z') {
        return static_cast<quint8>(ascii - 32); // Convert to uppercase
    }

    // Common punctuation (mostly same as ASCII)
    switch (ascii) {
    case '!': return 33;
    case '"': return 34;
    case '#': return 35;
    case '$': return 36;
    case '%': return 37;
    case '&': return 38;
    case '\'': return 39;
    case '(': return 40;
    case ')': return 41;
    case '*': return 42;
    case '+': return 43;
    case ',': return 44;
    case '-': return 45;
    case '.': return 46;
    case '/': return 47;
    case ':': return 58;
    case ';': return 59;
    case '<': return 60;
    case '=': return 61;
    case '>': return 62;
    case '?': return 63;
    case '@': return 64;
    case '[': return 91;
    case '\\': return 92;
    case ']': return 93;
    case '^': return 94;  // Up arrow in PETSCII
    case '_': return 95;  // Left arrow in PETSCII
    default:
        break;
    }

    return 0; // Character not supported
}
