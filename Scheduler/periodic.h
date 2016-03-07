

#pragma once

#include "scheduler.h"

typedef void* periodic_handle;

periodic_handle schedule_periodic(int period, worker_routine* workitem, void* context);

void cancel_periodic(periodic_handle id);
