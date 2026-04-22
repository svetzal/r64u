#ifndef STREAMPROTOCOLCORE_H
#define STREAMPROTOCOLCORE_H

#include <QByteArray>
#include <QString>

namespace streamprotocol {

/**
 * @brief Command types for the stream control binary protocol.
 *
 * The device listens on TCP port 64 for these one-byte command codes.
 */
enum class CommandType : quint8 {
    StartVideo = 0x20,  ///< Start video stream
    StartAudio = 0x21,  ///< Start audio stream
    StopVideo = 0x30,   ///< Stop video stream
    StopAudio = 0x31    ///< Stop audio stream
};

// ---------------------------------------------------------------------------
// Protocol serialisation
// ---------------------------------------------------------------------------

/**
 * @brief Builds a start-stream command packet.
 *
 * Wire format:
 * @code
 * [cmd:1] [0xFF:1] [paramLen_lo:1] [paramLen_hi:1]
 * [durationTicks_lo:1] [durationTicks_hi:1] [targetHost:port string]
 * @endcode
 *
 * @param type        StartVideo or StartAudio.
 * @param targetHost  IP address to receive the stream.
 * @param targetPort  UDP port to receive the stream.
 * @param durationTicks Duration in 5 ms ticks; 0 means infinite.
 * @return Fully serialised command packet.
 */
[[nodiscard]] QByteArray buildStartCommand(CommandType type, const QString &targetHost,
                                           quint16 targetPort, quint16 durationTicks);

/**
 * @brief Builds a stop-stream command packet.
 *
 * Wire format:
 * @code
 * [cmd:1] [0xFF:1] [0x00:1] [0x00:1]
 * @endcode
 *
 * @param type StopVideo or StopAudio.
 * @return Fully serialised 4-byte command packet.
 */
[[nodiscard]] QByteArray buildStopCommand(CommandType type);

/**
 * @brief Returns a human-readable name for a command type.
 * @param type The command type.
 * @return "StartVideo", "StartAudio", "StopVideo", "StopAudio", or "Unknown".
 */
[[nodiscard]] QString commandTypeToString(CommandType type);

}  // namespace streamprotocol

#endif  // STREAMPROTOCOLCORE_H
