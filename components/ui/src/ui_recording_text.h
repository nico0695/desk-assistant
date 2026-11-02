#pragma once

#define UI_TEXT_RECORDING_SEND "Send"
#define UI_TEXT_RECORDING_CANCEL "Cancel"
#define UI_TEXT_RECORDING_CLOSE "Close"
/* Elapsed and maximum duration, as minutes:seconds. */
#define UI_TEXT_RECORDING_DURATION_FORMAT "%u:%02u / %u:%02u"

#define UI_TEXT_RECORDING_TITLE_RECORDING "Recording"
#define UI_TEXT_RECORDING_TITLE_FINALIZING "Finishing"
#define UI_TEXT_RECORDING_TITLE_READY "Ready to send"
#define UI_TEXT_RECORDING_TITLE_CANCELLING "Clearing audio"
#define UI_TEXT_RECORDING_MESSAGE_RECORDING "Tap Send when you are done."
#define UI_TEXT_RECORDING_MESSAGE_FINALIZING "Saving the recording..."
#define UI_TEXT_RECORDING_MESSAGE_READY "Recording saved. Sending is not available yet."
#define UI_TEXT_RECORDING_MESSAGE_CANCELLING "One moment..."

#define UI_TEXT_RECORDING_TITLE_START_FAILED "Recording unavailable"
#define UI_TEXT_RECORDING_MESSAGE_START_FAILED "The microphone could not start. Try again."
#define UI_TEXT_RECORDING_TITLE_TOO_SHORT "Too short"
#define UI_TEXT_RECORDING_MESSAGE_TOO_SHORT "Record at least half a second."
#define UI_TEXT_RECORDING_TITLE_INTERRUPTED "Recording interrupted"
#define UI_TEXT_RECORDING_MESSAGE_INTERRUPTED "Some audio was lost. Record again."
#define UI_TEXT_RECORDING_TITLE_STORAGE_FAILED "Recording not saved"
#define UI_TEXT_RECORDING_MESSAGE_STORAGE_FAILED "The audio could not be stored. Record again."
#define UI_TEXT_RECORDING_TITLE_CLEANUP_FAILED "Audio not cleared"
#define UI_TEXT_RECORDING_MESSAGE_CLEANUP_FAILED "Recording stays blocked until the audio is cleared. Close to try again."
