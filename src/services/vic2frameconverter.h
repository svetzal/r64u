#ifndef VIC2FRAMECONVERTER_H
#define VIC2FRAMECONVERTER_H

#include "services/ivideostreamreceiverservice.h"

#include <QByteArray>
#include <QImage>

namespace Vic2 {
QImage convertFrame(const QByteArray &frameData, IVideoStreamReceiverService::VideoFormat format);
}

#endif  // VIC2FRAMECONVERTER_H
