

#pragma once

typedef void (*worker_routine)(void* context);

//
// schedules work item to run once at absolute time due. System worker thread will
// call workitem(context) no earlier than due and will make a best effort to call it
// as soon as possible after due.
// if due time is earlier than now(), best effort will be made to call it as soon
// as possible.
//
// Returns a referenced handle that can be used for cancellation. Never returns zero.
// Never fails.
//
int schedule(__int64 due, worker_routine* workitem, void *context);

//
// Attempts to cancel a work item that has been scheduled previously. Has no effect on
// a work item that is already running or has completed.
//
// If the work item was successfully cancelled, returns true, otherwise, that is, when the
// work item was already running or has already completed, returns false.
//
bool cancel_scheduled(int id);

//
// releases a handle to work item. Has no effect on a work item in regards to scheduling
// and cancellation.
//
// It is a programming error to release a work item handle more than once
//
void release_workitem_handle(int id);

//
// crash
//
void crash(void *);