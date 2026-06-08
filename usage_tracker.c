#include "usage_tracker.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#define MAX_TRACKED_ENGINES 16
#define USAGE_FILE "usage.dat"

static EngineUsage g_usage[MAX_TRACKED_ENGINES];
static EngineQuota g_quota[MAX_TRACKED_ENGINES];
static int g_engine_count = 0;
static bool g_initialized = false;

static char* get_usage_file_path(void) {
    static char path[MAX_PATH_LENGTH];
    snprintf(path, sizeof(path), "%s/%s", get_config_dir(), USAGE_FILE);
    return path;
}

static int find_engine_index(const char* engine_name) {
    for (int i = 0; i < g_engine_count; i++) {
        if (strcmp(g_usage[i].engine_name, engine_name) == 0) return i;
    }
    return -1;
}

static int add_engine(const char* engine_name) {
    if (g_engine_count >= MAX_TRACKED_ENGINES) return -1;
    int idx = g_engine_count++;
    memset(&g_usage[idx], 0, sizeof(EngineUsage));
    memset(&g_quota[idx], 0, sizeof(EngineQuota));
    snprintf(g_usage[idx].engine_name, sizeof(g_usage[idx].engine_name), "%s", engine_name);
    snprintf(g_quota[idx].engine_name, sizeof(g_quota[idx].engine_name), "%s", engine_name);
    return idx;
}

static bool is_same_day(time_t a, time_t b) {
    struct tm ta = *localtime(&a);
    struct tm tb = *localtime(&b);
    return ta.tm_year == tb.tm_year && ta.tm_yday == tb.tm_yday;
}

static bool is_same_month(time_t a, time_t b) {
    struct tm ta = *localtime(&a);
    struct tm tb = *localtime(&b);
    return ta.tm_year == tb.tm_year && ta.tm_mon == tb.tm_mon;
}

void usage_tracker_init(void) {
    if (g_initialized) return;
    memset(g_usage, 0, sizeof(g_usage));
    memset(g_quota, 0, sizeof(g_quota));
    g_engine_count = 0;
    g_initialized = true;

    /* Default quotas matching user's requirements */
    /* Tavily: monthly 1000 (no daily limit), also track daily for visibility */
    usage_tracker_set_quota("tavily", 0, 1000);
    /* Bing API: monthly 1000 (no daily limit), also track daily for visibility */
    usage_tracker_set_quota("bing_api", 0, 1000);
    /* Google Custom Search: daily 100 (no monthly limit) */
    usage_tracker_set_quota("google_api", 100, 0);

    usage_tracker_load();
    usage_tracker_check_resets();
}

void usage_tracker_shutdown(void) {
    if (!g_initialized) return;
    usage_tracker_save();
    g_initialized = false;
}

void usage_tracker_set_quota(const char* engine_name, int daily_limit, int monthly_limit) {
    int idx = find_engine_index(engine_name);
    if (idx < 0) idx = add_engine(engine_name);
    if (idx < 0) return;
    g_quota[idx].daily_limit = daily_limit;
    g_quota[idx].monthly_limit = monthly_limit;
    g_quota[idx].reset_daily = (daily_limit > 0);
    g_quota[idx].reset_monthly = (monthly_limit > 0);
}

void usage_tracker_check_resets(void) {
    time_t now = time(NULL);
    for (int i = 0; i < g_engine_count; i++) {
        /* Daily reset */
        if (g_quota[i].reset_daily) {
            if (g_usage[i].last_reset_day == 0 || !is_same_day(now, g_usage[i].last_reset_day)) {
                g_usage[i].today_count = 0;
                g_usage[i].last_reset_day = now;
            }
        }
        /* Monthly reset */
        if (g_quota[i].reset_monthly) {
            if (g_usage[i].last_reset_month == 0 || !is_same_month(now, g_usage[i].last_reset_month)) {
                g_usage[i].month_count = 0;
                g_usage[i].last_reset_month = now;
            }
        }
    }
}

bool usage_tracker_record_call(const char* engine_name) {
    if (!g_initialized) usage_tracker_init();
    usage_tracker_check_resets();

    int idx = find_engine_index(engine_name);
    if (idx < 0) idx = add_engine(engine_name);
    if (idx < 0) return false;

    /* Check quota before incrementing */
    if (g_quota[idx].daily_limit > 0 && g_usage[idx].today_count >= g_quota[idx].daily_limit) {
        return false;
    }
    if (g_quota[idx].monthly_limit > 0 && g_usage[idx].month_count >= g_quota[idx].monthly_limit) {
        return false;
    }

    g_usage[idx].today_count++;
    g_usage[idx].month_count++;
    g_usage[idx].last_call_time = time(NULL);
    return true;
}

bool usage_tracker_can_use(const char* engine_name) {
    if (!g_initialized) usage_tracker_init();
    usage_tracker_check_resets();

    int idx = find_engine_index(engine_name);
    if (idx < 0) return true; /* No quota set = unlimited */

    if (g_quota[idx].daily_limit > 0 && g_usage[idx].today_count >= g_quota[idx].daily_limit) {
        return false;
    }
    if (g_quota[idx].monthly_limit > 0 && g_usage[idx].month_count >= g_quota[idx].monthly_limit) {
        return false;
    }
    return true;
}

EngineUsage usage_tracker_get(const char* engine_name) {
    EngineUsage empty = {0};
    if (!g_initialized) usage_tracker_init();
    int idx = find_engine_index(engine_name);
    if (idx < 0) return empty;
    return g_usage[idx];
}

void usage_tracker_save(void) {
    if (!g_initialized) return;
    ensure_directory(get_config_dir());
    FILE* f = fopen(get_usage_file_path(), "wb");
    if (!f) return;

    fwrite(&g_engine_count, sizeof(int), 1, f);
    for (int i = 0; i < g_engine_count; i++) {
        fwrite(&g_usage[i], sizeof(EngineUsage), 1, f);
        fwrite(&g_quota[i], sizeof(EngineQuota), 1, f);
    }
    fclose(f);
}

void usage_tracker_load(void) {
    FILE* f = fopen(get_usage_file_path(), "rb");
    if (!f) return;

    int count = 0;
    if (fread(&count, sizeof(int), 1, f) != 1) { fclose(f); return; }
    if (count < 0 || count > MAX_TRACKED_ENGINES) { fclose(f); return; }

    g_engine_count = count;
    for (int i = 0; i < count; i++) {
        if (fread(&g_usage[i], sizeof(EngineUsage), 1, f) != 1) break;
        if (fread(&g_quota[i], sizeof(EngineQuota), 1, f) != 1) break;
    }
    fclose(f);
}

void usage_tracker_print_stats(void) {
    if (!g_initialized) usage_tracker_init();
    usage_tracker_check_resets();

    printf("\n" COLOR_BOLD "%-20s %-10s %-10s %-10s %-10s %-10s\n" COLOR_RESET,
        "Engine", "Today", "DailyLim", "Month", "MonthLim", "Status");
    printf(COLOR_DIM);
    for (int i = 0; i < 72; i++) putchar('-');
    printf(COLOR_RESET "\n");

    for (int i = 0; i < g_engine_count; i++) {
        bool ok = true;
        if (g_quota[i].daily_limit > 0 && g_usage[i].today_count >= g_quota[i].daily_limit) ok = false;
        if (g_quota[i].monthly_limit > 0 && g_usage[i].month_count >= g_quota[i].monthly_limit) ok = false;

        printf("%-20s %-10d %-10d %-10d %-10d %s\n",
            g_usage[i].engine_name,
            g_usage[i].today_count,
            g_quota[i].daily_limit > 0 ? g_quota[i].daily_limit : 0,
            g_usage[i].month_count,
            g_quota[i].monthly_limit > 0 ? g_quota[i].monthly_limit : 0,
            ok ? COLOR_GREEN "OK" COLOR_RESET : COLOR_RED "LIMITED" COLOR_RESET);
    }
    printf("\n");
}
