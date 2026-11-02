#pragma once

/* Title of a list page: module name and page number. */
#define UI_TEXT_MODULES_PAGE_TITLE_FORMAT "%s \xC2\xB7 %u"

#define UI_TEXT_MODULES_LOADING "Loading..."
#define UI_TEXT_MODULES_EMPTY "No items"
#define UI_TEXT_MODULES_NOT_AVAILABLE "Not available"
#define UI_TEXT_MODULES_REFRESH_HINT "Tap refresh to try again."

#define UI_TEXT_MODULES_FAILED_LIST "Could not load this page"
#define UI_TEXT_MODULES_FAILED_DETAIL "Could not load this item"
#define UI_TEXT_MODULES_BUSY "Busy"

#define UI_TEXT_MODULES_ERROR_BUSY_AUDIO "A recording is in progress."
#define UI_TEXT_MODULES_ERROR_BUSY_BRIDGE "The bridge is busy with another request."
#define UI_TEXT_MODULES_ERROR_NETWORK "No connection to the bridge."
#define UI_TEXT_MODULES_ERROR_UNAUTHORIZED "The bridge rejected this device."
#define UI_TEXT_MODULES_ERROR_DENIED "The bridge denied access to your data."
#define UI_TEXT_MODULES_ERROR_UNAVAILABLE "The bridge could not read your data."
#define UI_TEXT_MODULES_ERROR_REJECTED "The bridge rejected the request."
#define UI_TEXT_MODULES_ERROR_INVALID "The reply was not valid."
#define UI_TEXT_MODULES_ERROR_TOO_LARGE "The reply is too large to show."
#define UI_TEXT_MODULES_ERROR_NOT_FOUND "This item is not available. It may have changed."
#define UI_TEXT_MODULES_ERROR_NO_MEMORY "Not enough memory."

#define UI_TEXT_MODULES_NOTE_UNSUPPORTED "Some characters cannot be shown."

/* Dates arrive already formatted; the first argument of each is that text. */
#define UI_TEXT_MODULES_ROW_CREATED "Created %s"
#define UI_TEXT_MODULES_ROW_ALERT "Alert %s"
#define UI_TEXT_MODULES_ROW_SCHEDULED "Scheduled %s"

#define UI_TEXT_MODULES_STATUS_PENDING "Pending"
#define UI_TEXT_MODULES_STATUS_IN_PROGRESS "In progress"
#define UI_TEXT_MODULES_STATUS_COMPLETED "Completed"
#define UI_TEXT_MODULES_STATUS_CANCELED "Canceled"
#define UI_TEXT_MODULES_SENT_NO "No"

#define UI_TEXT_MODULES_DETAIL_STATUS "Status: %s"
#define UI_TEXT_MODULES_DETAIL_ALERT "Alert: %s"
#define UI_TEXT_MODULES_DETAIL_SCHEDULED "Scheduled: %s"
#define UI_TEXT_MODULES_DETAIL_SENT "Sent: %s"
#define UI_TEXT_MODULES_DETAIL_CREATED "Created: %s"
#define UI_TEXT_MODULES_DETAIL_UPDATED "Updated: %s"
#define UI_TEXT_MODULES_DETAIL_TAG "Tag: %s"
#define UI_TEXT_MODULES_DETAIL_ID "ID: %s"

#define UI_TEXT_MODULES_DELETE_TITLE_FORMAT "Delete this %s?"
#define UI_TEXT_MODULES_ITEM_NOTE "note"
#define UI_TEXT_MODULES_ITEM_TASK "task"
#define UI_TEXT_MODULES_ITEM_ALERT "alert"
#define UI_TEXT_MODULES_DELETE "Delete"
#define UI_TEXT_MODULES_CANCEL "Cancel"
#define UI_TEXT_MODULES_SNOOZE_TITLE "Snooze this alert"
#define UI_TEXT_MODULES_SNOOZE_FIVE_MINUTES "5 minutes"
#define UI_TEXT_MODULES_SNOOZE_ONE_HOUR "1 hour"
#define UI_TEXT_MODULES_SNOOZE_TOMORROW "Tomorrow"

/* Result line of the last action; it stays until the next navigation or action. */
#define UI_TEXT_MODULES_NOTICE_DELETING "Deleting..."
#define UI_TEXT_MODULES_NOTICE_COMPLETING "Completing..."
#define UI_TEXT_MODULES_NOTICE_SNOOZING "Snoozing..."
#define UI_TEXT_MODULES_NOTICE_DELETED "Deleted."
#define UI_TEXT_MODULES_NOTICE_COMPLETED "Task completed."
/* The confirmed time arrives already formatted. */
#define UI_TEXT_MODULES_NOTICE_SNOOZED_FORMAT "Snoozed until %s."
#define UI_TEXT_MODULES_NOTICE_BUSY "Not sent: the device is busy. Try again."
#define UI_TEXT_MODULES_NOTICE_NOT_SENT "Not sent. Check the connection and try again."
#define UI_TEXT_MODULES_NOTICE_REJECTED "The bridge rejected the action."
#define UI_TEXT_MODULES_NOTICE_NOT_FOUND "This item is not available. Refresh it first."
#define UI_TEXT_MODULES_NOTICE_CHANGED "This item changed. Refresh it first."
#define UI_TEXT_MODULES_NOTICE_UNCONFIRMED "It may have been applied. Refresh it before acting again."
