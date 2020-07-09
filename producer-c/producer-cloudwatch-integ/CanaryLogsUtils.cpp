/**
 * Kinesis Video Producer cloudwatch logging
 */
#define LOG_CLASS "CanaryStreamLogs"
#include "CanaryStreamUtils.h"

PCloudwatchLogsObject gCloudwatchLogsObject = NULL;

STATUS initializeCloudwatchLogger(PCloudwatchLogsObject pCloudwatchLogsObject) {
    STATUS retStatus = STATUS_SUCCESS;
    CHK(pCloudwatchLogsObject != NULL, STATUS_NULL_ARG);
    pCloudwatchLogsObject->canaryLogGroupRequest.SetLogGroupName(pCloudwatchLogsObject->logGroupName);
    pCloudwatchLogsObject->pCwl->CreateLogGroup(pCloudwatchLogsObject->canaryLogGroupRequest);
    pCloudwatchLogsObject->canaryLogStreamRequest.SetLogStreamName(pCloudwatchLogsObject->logStreamName);
    pCloudwatchLogsObject->canaryLogStreamRequest.SetLogGroupName(pCloudwatchLogsObject->logGroupName);
    pCloudwatchLogsObject->pCwl->CreateLogStream(pCloudwatchLogsObject->canaryLogStreamRequest);
    gCloudwatchLogsObject = pCloudwatchLogsObject;
CleanUp:
    return retStatus;
}

VOID setUpLogEventVector(PCHAR logString) {
    Aws::String awsCwString((Aws::String)logString);
    auto logEvent = Aws::CloudWatchLogs::Model::InputLogEvent()
                   .WithMessage(awsCwString)
                   .WithTimestamp(GETTIME() / HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    gCloudwatchLogsObject->canaryInputLogEventVec.push_back(logEvent);
}

VOID onPutLogEventResponseReceivedHandler(const Aws::CloudWatchLogs::CloudWatchLogsClient* cwClientLog,
                                          const Aws::CloudWatchLogs::Model::PutLogEventsRequest& request,
                                          const Aws::CloudWatchLogs::Model::PutLogEventsOutcome& outcome,
                                          const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context) {
    if (!outcome.IsSuccess()) {
        DLOGE("Failed to push logs: %s", outcome.GetError().GetMessage().c_str());
    }
    else {
        DLOGS("Successfully pushed logs to cloudwatch");
        gCloudwatchLogsObject->token = outcome.GetResult().GetNextSequenceToken();
    }
}

VOID canaryStreamSendLogs(PCloudwatchLogsObject pCloudwatchLogsObject) {
    Aws::CloudWatchLogs::Model::PutLogEventsOutcome outcome;
    auto request = Aws::CloudWatchLogs::Model::PutLogEventsRequest()
                   .WithLogGroupName(pCloudwatchLogsObject->logGroupName)
                   .WithLogStreamName(pCloudwatchLogsObject->logStreamName)
                   .WithLogEvents(pCloudwatchLogsObject->canaryInputLogEventVec);
    if (pCloudwatchLogsObject->token != "") {
        request.SetSequenceToken(pCloudwatchLogsObject->token);
    }
    pCloudwatchLogsObject->pCwl->PutLogEventsAsync(request, onPutLogEventResponseReceivedHandler);
    pCloudwatchLogsObject->canaryInputLogEventVec.clear();
}

VOID canaryStreamSendLogSync(PCloudwatchLogsObject pCloudwatchLogsObject) {
    auto request = Aws::CloudWatchLogs::Model::PutLogEventsRequest()
                   .WithLogGroupName(pCloudwatchLogsObject->logGroupName)
                   .WithLogStreamName(pCloudwatchLogsObject->logStreamName)
                   .WithLogEvents(pCloudwatchLogsObject->canaryInputLogEventVec);
    if (pCloudwatchLogsObject->token != "") {
       request.SetSequenceToken(pCloudwatchLogsObject->token);
    }
    auto outcome = pCloudwatchLogsObject->pCwl->PutLogEvents(request);
    if (!outcome.IsSuccess()) {
            DLOGE("Failed to push logs: %s", outcome.GetError().GetMessage().c_str());
        }
        else {
            DLOGS("Successfully pushed logs to cloudwatch");
        }
    pCloudwatchLogsObject->canaryInputLogEventVec.clear();
}


VOID cloudWatchLogger(UINT32 level, PCHAR tag, PCHAR fmt, ...)
{
    CHAR logFmtString[MAX_LOG_FORMAT_LENGTH + 1];
    CHAR cwLogFmtString[MAX_LOG_FORMAT_LENGTH + 1];
    UINT32 logLevel = GET_LOGGER_LOG_LEVEL();
    UNUSED_PARAM(tag);

    if (level >= logLevel) {
        addLogMetadata(logFmtString, (UINT32) ARRAY_SIZE(logFmtString), fmt, level);

        // Creating a copy to store the logFmtString for cloudwatch logging purpose
        va_list valist;
        va_start(valist, fmt);
        vsprintf(cwLogFmtString, logFmtString, valist);
        va_end(valist);
        va_start(valist, fmt);
        vprintf(logFmtString, valist);
        va_end(valist);
        setUpLogEventVector(cwLogFmtString);
    }
}
