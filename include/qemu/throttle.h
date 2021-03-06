/*
 * QEMU throttling infrastructure
 *
 * Copyright (C) Nodalink, EURL. 2013-2014
 * Copyright (C) Igalia, S.L. 2015
 *
 * Authors:
 *   Benoît Canet <benoit.canet@nodalink.com>
 *   Alberto Garcia <berto@igalia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 or
 * (at your option) version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef THROTTLE_H
#define THROTTLE_H

#include <stdint.h>
#include "qemu-common.h"
#include "qemu/timer.h"

typedef enum {
    THROTTLE_BPS_TOTAL,
    THROTTLE_BPS_READ,
    THROTTLE_BPS_WRITE,
    THROTTLE_OPS_TOTAL,
    THROTTLE_OPS_READ,
    THROTTLE_OPS_WRITE,
    BUCKETS_COUNT,
} BucketType;

/*
 * The max parameter of the leaky bucket throttling algorithm can be used to
 * allow the guest to do bursts.
 * The max value is a pool of I/O that the guest can use without being throttled
 * at all. Throttling is triggered once this pool is empty.
 */

typedef struct LeakyBucket {
    double  avg;              /* average goal in units per second */
    double  max;              /* leaky bucket max burst in units */
    double  level;            /* bucket level in units */
} LeakyBucket;

/* The following structure is used to configure a ThrottleState
 * It contains a bit of state: the bucket field of the LeakyBucket structure.
 * However it allows to keep the code clean and the bucket field is reset to
 * zero at the right time.
 */
typedef struct ThrottleConfig {
    LeakyBucket buckets[BUCKETS_COUNT]; /* leaky buckets */
    uint64_t op_size;         /* size of an operation in bytes */
} ThrottleConfig;

typedef struct ThrottleState {
    ThrottleConfig cfg;       /* configuration */
    int64_t previous_leak;    /* timestamp of the last leak done */
} ThrottleState;

typedef struct ThrottleTimers {
    QEMUTimer *timers[2];     /* timers used to do the throttling */
    QEMUClockType clock_type; /* the clock used */

    /* Callbacks */
    QEMUTimerCB *read_timer_cb;
    QEMUTimerCB *write_timer_cb;
    void *timer_opaque;
} ThrottleTimers;

/* operations on single leaky buckets */
void throttle_leak_bucket(LeakyBucket *bkt, int64_t delta);

int64_t throttle_compute_wait(LeakyBucket *bkt);

/* expose timer computation function for unit tests */
bool throttle_compute_timer(ThrottleState *ts,
                            bool is_write,
                            int64_t now,
                            int64_t *next_timestamp);

/* init/destroy cycle */
void throttle_init(ThrottleState *ts);

void throttle_timers_init(ThrottleTimers *tt,
                          AioContext *aio_context,
                          QEMUClockType clock_type,
                          QEMUTimerCB *read_timer_cb,
                          QEMUTimerCB *write_timer_cb,
                          void *timer_opaque);

void throttle_timers_destroy(ThrottleTimers *tt);

void throttle_timers_detach_aio_context(ThrottleTimers *tt);

void throttle_timers_attach_aio_context(ThrottleTimers *tt,
                                        AioContext *new_context);

bool throttle_timers_are_initialized(ThrottleTimers *tt);

/* configuration */
bool throttle_enabled(ThrottleConfig *cfg);

bool throttle_conflicting(ThrottleConfig *cfg);

bool throttle_is_valid(ThrottleConfig *cfg);

bool throttle_max_is_missing_limit(ThrottleConfig *cfg);

void throttle_config(ThrottleState *ts,
                     ThrottleTimers *tt,
                     ThrottleConfig *cfg);

void throttle_get_config(ThrottleState *ts, ThrottleConfig *cfg);

/* usage */
bool throttle_schedule_timer(ThrottleState *ts,
                             ThrottleTimers *tt,
                             bool is_write);

void throttle_account(ThrottleState *ts, bool is_write, uint64_t size);

#endif
