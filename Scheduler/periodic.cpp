// 
//

#include "stdafx.h"
#include "periodic.h"


#ifdef _MSC_VER
#define time _time32
#endif


struct PERIODIC_CONTEXT {
    CRITICAL_SECTION    lock;
    worker_routine*     user_workitem;
    void*               user_context;
    time_t              period;
    __int64             scheduler_id;

    PERIODIC_CONTEXT() {
        InitializeCriticalSection(&lock);
        // come on, initialize other crap in here if you are cpp guy
    };
    ~PERIODIC_CONTEXT() {
        DeleteCriticalSection(&lock);
    }

    __int64 get_scheduler_id() {
        int retval = 0;
        // this critical_section has no effect except for read-write barrier.
        EnterCriticalSection(&lock);
        retval = scheduler_id;
        LeaveCriticalSection(&lock);
        return retval;
    }

    void set_scheduler_id(__int64 id) {
        // this critical_section has no effect except for read-write barrier.
        EnterCriticalSection(&lock);
        scheduler_id = id;
        LeaveCriticalSection(&lock);
    };
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

    // Clean up the scheduler stuff before assigning a new one.
    release_workitem_handle(periodic_context->get_scheduler_id());
    // and now you want to cancel it, on another thread. You will use the old ID
    

    // Schedule next execution
    periodic_context->set_scheduler_id(schedule(next_execution, (worker_routine*)&schedule_periodic_workitem, periodic_context));
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
    periodic_context->set_scheduler_id(schedule(time(NULL) + period, (worker_routine*)&schedule_periodic_workitem, periodic_context));

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

