#ifndef VIC2FRAMECONVERTER_H
#define VIC2FRAMECONVERTER_H

#include "services/ivideostreamreceiver.h"

#include <QByteArray>
#include <QImage>

namespace Vic2 {
QImage convertFrame(const QByteArray &frameData, IVideoStreamReceiver::VideoFormat format);
}

#endif  // VIC2FRAMECONVERTER_H
