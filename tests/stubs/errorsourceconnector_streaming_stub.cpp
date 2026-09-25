/**
 * @file errorsourceconnector_streaming_stub.cpp
 * @brief Link-time stub for tests that use the real ErrorHandler::connectSources().
 *
 * connectSources() hands a StreamingService to connectStreamingServiceSources(),
 * whose real implementation pulls in the whole streaming stack. Tests that pass
 * no StreamingService never reach it.
 */

#include "services/errorhandler.h"

class StreamingService;

void ErrorHandler::connectStreamingServiceSources(StreamingService * /*ss*/) {}
