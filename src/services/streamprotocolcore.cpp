/**
 * @file streamprotocolcore.cpp
 * @brief Implementation of pure stream control protocol serialisation functions.
 */

#include "streamprotocolcore.h"

namespace streamprotocol {

QByteArray buildStartCommand(CommandType type, const QString &targetHost, quint16 targetPort,
                             quint16 durationTicks)
{
    // Build the destination string: "IP:PORT"
    QString destination = QString("%1:%2").arg(targetHost).arg(targetPort);
    QByteArray destBytes = destination.toLatin1();

    // Parameter length = 2 (duration) + destination string length
    auto paramLength = static_cast<quint16>(2 + destBytes.size());

    QByteArray command;
    command.reserve(4 + paramLength);

    // Command byte
    command.append(static_cast<char>(type));

    // Command marker
    command.append(static_cast<char>(0xFF));

    // Parameter length (little-endian)
    command.append(static_cast<char>(paramLength & 0xFF));
    command.append(static_cast<char>((paramLength >> 8) & 0xFF));

    // Duration (little-endian, 0 = infinite)
    command.append(static_cast<char>(durationTicks & 0xFF));
    command.append(static_cast<char>((durationTicks >> 8) & 0xFF));

    // Destination string
    command.append(destBytes);

    return command;
}

QByteArray buildStopCommand(CommandType type)
{
    QByteArray command;
    command.reserve(4);

    // Command byte
    command.append(static_cast<char>(type));

    // Command marker
    command.append(static_cast<char>(0xFF));

    // Parameter length = 0 (little-endian)
    command.append(static_cast<char>(0x00));
    command.append(static_cast<char>(0x00));

    return command;
}

QString commandTypeToString(CommandType type)
{
    switch (type) {
    case CommandType::StartVideo:
        return "StartVideo";
    case CommandType::StartAudio:
        return "StartAudio";
    case CommandType::StopVideo:
        return "StopVideo";
    case CommandType::StopAudio:
        return "StopAudio";
    default:
        return "Unknown";
    }
}

}  // namespace streamprotocol
