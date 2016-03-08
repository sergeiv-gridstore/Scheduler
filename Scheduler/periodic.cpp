// 
//

#include "stdafx.h"
#include "periodic.h"


#ifdef _MSC_VER
#define time _time32
#endif


void schedule_periodic_workitem(void* context);

struct PERIODIC_CONTEXT {
    CRITICAL_SECTION    lock;
    worker_routine*     user_workitem;
    void*               user_context;
    time_t              period;
    int                 scheduler_id;

    PERIODIC_CONTEXT() {
        InitializeCriticalSection(&lock);
        user_workitem = NULL;
        user_context = NULL;
        period = 0;
        scheduler_id = 0;
    };
    ~PERIODIC_CONTEXT() {
        DeleteCriticalSection(&lock);
    }

    void schedule_next(__int64 interval, void* context) {
        EnterCriticalSection(&lock);

        // Clean up the scheduler stuff before assigning a new one.
        release_workitem_handle(scheduler_id);

        // Schedule next execution
        // Rom> VS 2015 is reporting some type of casting error of its own.  I tried changing 
        //      the default calling convention of the whole project to no avail.  At this point,
        //      I normally monkey with the calling convention of the function/type def prototype 
        //      itself.  Except, in this case, I was already told I had to use the prototypes 
        //      that were in place.  I figured the theoretical stuff was the actual problem to 
        //      solve and worked around the issue by casting the pointer.  I've reverted the 
        //      code back to original behaviour.
        scheduler_id = schedule(interval, schedule_periodic_workitem, context);

        LeaveCriticalSection(&lock);
    }

    int get_scheduler_id() {
        int retval = 0;
        EnterCriticalSection(&lock);
        retval = scheduler_id;
        LeaveCriticalSection(&lock);
        return retval;
    }
};

// the high level design is good. You have a chain of schedule_periodic_workitem() that
// schedule themselves.
void schedule_periodic_workitem(void* context) {
    PERIODIC_CONTEXT* periodic_context = (PERIODIC_CONTEXT*)context;
    time_t timer_start = 0;
    time_t timer_end = 0;
    time_t timer_elapsed = 0;
    time_t next_execution = 0;

    // Time the execution of the workitem
    timer_start = time(NULL);
    (*(periodic_context->user_workitem))(periodic_context->user_context);
    timer_end = time(NULL);
    timer_elapsed = timer_end - timer_start;

    // How long until the next execution of the workitem
    if (timer_elapsed <= periodic_context->period) {
        next_execution = periodic_context->period - timer_elapsed;
    }
    else {
        next_execution = periodic_context->period - timer_start % periodic_context->period;
    }

    // Schedule next execution
    periodic_context->schedule_next(next_execution, periodic_context);
}


//
// schedules periodic work item.
//
// Work becomes due at now() + period (in same units as first parameter of schedule()
// function declared above), now + 2 * period, and so on. Worker routine runs when it
// becomes due on best effort basis. If by any chance previous occurrence of worker
// routine is still running when next becomes due, next occurrence
// is skipped:
//
//                                      | skipped occurrence because       | work
//                                      | work is still running            | started
// <-----period----->                   V                                  V again(5).
// *-----------------*-----------------*-----------------*-----------------*------...
// ...................-------------------->...............------->.........------->...
// ^ work            ^ work started    ^ work not        ^ work started again...
// | scheduled (1)   | (2)             | started (3)     | (4)
//
//
// user called schedule_periodic(period, work, ctx)
// at (1) work(ctx) was called 
// at (2) == (1) + period
// at (3) == (1) + period + period
//   work happened to be still running, so we didn’t do anything
// somewhere between (3) and (4) work has completed, so at (4) work was started again
// it completed between (4) and (5) so at (5) it was started again
//
// Work should run as described indefinitely until it is cancelled.
//
// Returns a handle that can be used to cancel timer. You decide what the exact
// type of handle is.
//
periodic_handle schedule_periodic(int period, worker_routine* workitem, void* context) {
    PERIODIC_CONTEXT* periodic_context = NULL;

    // validate parameters
    if (!period) return NULL;
    if (!workitem) return NULL;

    // allocate new context to execute
    periodic_context = new PERIODIC_CONTEXT();
    if (!periodic_context) return NULL;

    // populate periodic workitem
    periodic_context->user_workitem = workitem;
    periodic_context->user_context = context;
    periodic_context->period = period;

    // schedule for execution
    // wy do you need a cast?
    periodic_context->schedule_next(time(NULL) + period, periodic_context);

    // return handle to caller
    return periodic_context;
}


//
// cancels periodic work item previously set up by schedule_periodic();
// Guarantees that no work item code would be executing when after cancel_periodic()
// returns.
//
void cancel_periodic(periodic_handle id) {
    PERIODIC_CONTEXT* periodic_context = (PERIODIC_CONTEXT*)id;

    // Cancel the scheduled job.
    // Keep looping until the cancel_scheduled() returns success.  This might
    // be after a couple of scheduler_id changes depending on the interval and/or
    // how long it takes the processor to update the cache with the new value.
    while (!cancel_scheduled(periodic_context->get_scheduler_id())) {
    }

    // Clean up the scheduler stuff since the caller doesn't know anything about it
    release_workitem_handle(periodic_context->get_scheduler_id());

    // Cleanup our own stuff
    delete periodic_context;
}

