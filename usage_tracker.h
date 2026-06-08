#ifndef USAGE_TRACKER_H
#define USAGE_TRACKER_H

#include "types.h"

/* Engine usage quota configuration */
typedef struct {
    char engine_name[MAX_ENGINE_NAME];
    int daily_limit;      /* 0 = unlimited */
    int monthly_limit;    /* 0 = unlimited */
    bool reset_daily;     /* true = reset at midnight local time */
    bool reset_monthly;   /* true = reset on 1st of month */
} EngineQuota;

/* Engine usage statistics for current period */
typedef struct {
    char engine_name[MAX_ENGINE_NAME];
    int today_count;
    int month_count;
    time_t last_call_time;
    time_t last_reset_day;
    time_t last_reset_month;
} EngineUsage;

void usage_tracker_init(void);
void usage_tracker_shutdown(void);

/* Record a call to an engine. Returns false if quota exceeded. */
bool usage_tracker_record_call(const char* engine_name);

/* Check if engine has remaining quota (does not increment). */
bool usage_tracker_can_use(const char* engine_name);

/* Get usage stats for an engine. */
EngineUsage usage_tracker_get(const char* engine_name);

/* Set quota for an engine. */
void usage_tracker_set_quota(const char* engine_name, int daily_limit, int monthly_limit);

/* Reset daily/monthly counters if needed (call before each search session). */
void usage_tracker_check_resets(void);

/* Save/load usage data from disk. */
void usage_tracker_save(void);
void usage_tracker_load(void);

/* Print usage stats for all tracked engines. */
void usage_tracker_print_stats(void);

#endif
