#include "audiostreamreceiverservice.h"
#include "errorhandler.h"
#include "ierroremitter.h"
#include "istreamcontrolservice.h"
#include "streamcontrolservice.h"
#include "streamingservice.h"
#include "videostreamreceiverservice.h"

void ErrorHandler::connectStreamingServiceSources(StreamingService *ss)
{
    // StreamingService and IStreamControlService now inherit IErrorEmitter
    registerSource(ss);
    if (ss->streamControl()) {
        registerSource(ss->streamControl());
    }

    // AudioStreamReceiverService and VideoStreamReceiverService do NOT inherit
    // IErrorEmitter (their interfaces have no error signals), so use legacy path
    if (ss->videoReceiver()) {
        connect(ss->videoReceiver(), &VideoStreamReceiverService::socketError, this,
                [this](const QString &message) {
                    handleError(ErrorCategory::System, ErrorSeverity::Warning,
                                tr("Streaming Error"), message);
                });
    }
    if (ss->audioReceiver()) {
        connect(ss->audioReceiver(), &AudioStreamReceiverService::socketError, this,
                [this](const QString &message) {
                    handleError(ErrorCategory::System, ErrorSeverity::Warning,
                                tr("Streaming Error"), message);
                });
    }
}
