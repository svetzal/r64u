/**
 * @file keyboardinputservice.h
 * @brief Service for sending keyboard input to the C64 via memory writes.
 *
 * Converts Qt key events to PETSCII codes and writes them to the
 * C64 keyboard buffer at $0277-$0280.
 */

#ifndef KEYBOARDINPUTSERVICE_H
#define KEYBOARDINPUTSERVICE_H

#include <QObject>
#include <QKeyEvent>

class C64URestClient;

/**
 * @brief Sends keyboard input to the C64 via the REST API.
 *
 * This service converts Qt key events to PETSCII codes and writes them
 * to the C64 keyboard buffer. It works with BASIC and KERNAL-based
 * programs but not games that read the CIA matrix directly.
 *
 * The C64 keyboard buffer is at $0277-$0280 (10 bytes max).
 * The buffer length is at $00C6.
 */
class KeyboardInputService : public QObject
{
    Q_OBJECT

public:
    /// C64 keyboard buffer address
    static constexpr quint16 KeyboardBufferAddress = 0x0277;

    /// C64 keyboard buffer length address
    static constexpr quint16 BufferLengthAddress = 0x00C6;

    /// Maximum characters in keyboard buffer
    static constexpr int MaxBufferSize = 10;

    /**
     * @brief Constructs the keyboard input service.
     * @param restClient REST client for sending commands.
     * @param parent Optional parent QObject.
     */
    explicit KeyboardInputService(C64URestClient *restClient, QObject *parent = nullptr);

    /**
     * @brief Handles a Qt key press event.
     * @param event The key event to process.
     * @return True if the key was handled, false otherwise.
     */
    bool handleKeyPress(QKeyEvent *event);

    /**
     * @brief Sends a single PETSCII character to the C64.
     * @param petscii The PETSCII code to send.
     */
    void sendPetscii(quint8 petscii);

    /**
     * @brief Sends a string of text to the C64.
     * @param text ASCII text to convert and send.
     */
    void sendText(const QString &text);

signals:
    /**
     * @brief Emitted when a key is successfully sent.
     * @param petscii The PETSCII code that was sent.
     */
    void keySent(quint8 petscii);

    /**
     * @brief Emitted when an error occurs.
     * @param error Error description.
     */
    void errorOccurred(const QString &error);

private:
    /**
     * @brief Converts a Qt key event to PETSCII code.
     * @param event The key event.
     * @return PETSCII code, or 0 if the key cannot be mapped.
     */
    [[nodiscard]] quint8 keyToPetscii(QKeyEvent *event) const;

    /**
     * @brief Converts an ASCII character to PETSCII.
     * @param ascii The ASCII character.
     * @return PETSCII code.
     */
    [[nodiscard]] static quint8 asciiToPetscii(char ascii);

    C64URestClient *restClient_;
};

#endif // KEYBOARDINPUTSERVICE_H
