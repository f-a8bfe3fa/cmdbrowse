#include "history.h"
#include "utils.h"

void history_init(BrowserContext* ctx) {
    ctx->history_capacity = 1024;
    ctx->history = (HistoryEntry*)calloc(ctx->history_capacity, sizeof(HistoryEntry));
    ctx->history_count = 0;
}

void history_add(BrowserContext* ctx, const char* query, const char* engine, int result_count, bool success) {
    if (!ctx->history) history_init(ctx);
    if (ctx->history_count >= ctx->history_capacity) {
        ctx->history_capacity *= 2;
        ctx->history = (HistoryEntry*)realloc(ctx->history, ctx->history_capacity * sizeof(HistoryEntry));
        if (!ctx->history) return;
    }
    HistoryEntry* entry = &ctx->history[ctx->history_count];
    snprintf(entry->query, sizeof(entry->query), "%s", query);
    snprintf(entry->engine, sizeof(entry->engine), "%s", engine);
    entry->timestamp = time(NULL);
    entry->result_count = result_count;
    entry->success = success;
    ctx->history_count++;
}

void history_show(BrowserContext* ctx, int limit) {
    if (!ctx->history || ctx->history_count == 0) {
        printf(COLOR_DIM "No search history.\n" COLOR_RESET);
        return;
    }
    if (limit <= 0 || limit > ctx->history_count) limit = ctx->history_count;
    int start = ctx->history_count - limit;
    printf("\n" COLOR_BOLD "Search History" COLOR_RESET);
    printf(COLOR_DIM " (showing last %d of %d)" COLOR_RESET "\n\n", limit, ctx->history_count);
    printf(COLOR_DIM "  #  Time                  Engine         Results  Query\n");
    for (int i = 0; i < 78; i++) putchar('-');
    printf(COLOR_RESET "\n");

    for (int i = start; i < ctx->history_count; i++) {
        HistoryEntry* e = &ctx->history[i];
        char tbuf[32];
        get_timestamp_str(e->timestamp, tbuf, sizeof(tbuf));
        const char* status = e->success ? COLOR_GREEN "OK" COLOR_RESET : COLOR_RED "FAIL" COLOR_RESET;
        printf(" %3d  %s  %-14s %s %4d  %s\n",
            i - start + 1, tbuf, e->engine, status, e->result_count, e->query);
    }
    printf("\n");
}

void history_search(BrowserContext* ctx, const char* keyword) {
    if (!ctx->history || ctx->history_count == 0) {
        printf(COLOR_DIM "No search history.\n" COLOR_RESET);
        return;
    }
    printf("\n" COLOR_BOLD "History matches for '%s':" COLOR_RESET "\n\n", keyword);
    int found = 0;
    for (int i = ctx->history_count - 1; i >= 0; i--) {
        if (str_contains(ctx->history[i].query, keyword) || str_contains(ctx->history[i].engine, keyword)) {
            char tbuf[32];
            get_timestamp_str(ctx->history[i].timestamp, tbuf, sizeof(tbuf));
            printf("  [%d] %s | %s | %d results | %s\n",
                ctx->history_count - i, tbuf, ctx->history[i].engine,
                ctx->history[i].result_count, ctx->history[i].query);
            found++;
            if (found >= 20) { printf("  ... and more\n"); break; }
        }
    }
    if (found == 0) printf(COLOR_DIM "  No matches found.\n" COLOR_RESET);
    printf("\n");
}

void history_clear(BrowserContext* ctx) {
    free(ctx->history);
    ctx->history = NULL;
    ctx->history_count = 0;
    ctx->history_capacity = 0;
    printf("History cleared.\n");
}

bool history_save(BrowserContext* ctx) {
    char filepath[MAX_PATH_LENGTH];
    snprintf(filepath, sizeof(filepath), "%s\\history.dat", get_config_dir());
    ensure_directory(get_config_dir());

    FILE* f = fopen(filepath, "wb");
    if (!f) return false;

    fwrite(&ctx->history_count, sizeof(int), 1, f);
    if (ctx->history_count > 0) {
        fwrite(ctx->history, sizeof(HistoryEntry), ctx->history_count, f);
    }
    fclose(f);
    return true;
}

bool history_load(BrowserContext* ctx) {
    char filepath[MAX_PATH_LENGTH];
    snprintf(filepath, sizeof(filepath), "%s\\history.dat", get_config_dir());
    if (!file_exists(filepath)) return false;

    FILE* f = fopen(filepath, "rb");
    if (!f) return false;

    int count = 0;
    if (fread(&count, sizeof(int), 1, f) != 1 || count <= 0) { fclose(f); return false; }

    history_init(ctx);
    ctx->history_count = count;
    ctx->history_capacity = count > 1024 ? count * 2 : 1024;
    ctx->history = (HistoryEntry*)calloc(ctx->history_capacity, sizeof(HistoryEntry));
    if (!ctx->history) { fclose(f); return false; }

    fread(ctx->history, sizeof(HistoryEntry), count, f);
    fclose(f);
    return true;
}

void history_stats(BrowserContext* ctx) {
    if (!ctx->history || ctx->history_count == 0) {
        printf(COLOR_DIM "No history data.\n" COLOR_RESET);
        return;
    }
    int success_count = 0, fail_count = 0;
    int engine_usage[MAX_ENGINES][2] = {{0}};
    int engine_count = 0;
    time_t earliest = ctx->history[0].timestamp, latest = ctx->history[0].timestamp;
    int64_t total_results = 0;

    for (int i = 0; i < ctx->history_count; i++) {
        if (ctx->history[i].success) success_count++; else fail_count++;
        if (ctx->history[i].timestamp < earliest) earliest = ctx->history[i].timestamp;
        if (ctx->history[i].timestamp > latest) latest = ctx->history[i].timestamp;
        total_results += ctx->history[i].result_count;

        int eidx = -1;
        for (int j = 0; j < engine_count; j++) {
            if (strcmp((char*)(&engine_usage[j]), ctx->history[i].engine) == 0) { eidx = j; break; }
        }
        if (eidx < 0 && engine_count < MAX_ENGINES) {
            eidx = engine_count++;
        }
        if (eidx >= 0) engine_usage[eidx][1]++;
    }

    char t1[32], t2[32];
    printf("\n" COLOR_BOLD "History Statistics:" COLOR_RESET "\n\n");
    printf("  Total searches:     %d\n", ctx->history_count);
    printf("  Successful:         %d (%.1f%%)\n", success_count, 100.0 * success_count / ctx->history_count);
    printf("  Failed:             %d (%.1f%%)\n", fail_count, 100.0 * fail_count / ctx->history_count);
    printf("  Total results seen: %lld\n", (long long)total_results);
    printf("  First search:       %s\n", get_timestamp_str(earliest, t1, sizeof(t1)));
    printf("  Last search:        %s\n", get_timestamp_str(latest, t2, sizeof(t2)));
    printf("  Engine breakdown:\n");
    for (int i = 0; i < engine_count; i++) {
        printf("    %-20s %d searches\n", (char*)(&engine_usage[i]), engine_usage[i][1]);
    }
    printf("\n");
}

void history_export(BrowserContext* ctx, const char* filepath, OutputFormat format) {
    if (!ctx->history || ctx->history_count == 0) {
        printf("No history to export.\n"); return;
    }
    FILE* f = fopen(filepath, "w");
    if (!f) { printf("Cannot write to %s\n", filepath); return; }
    for (int i = 0; i < ctx->history_count; i++) {
        HistoryEntry* e = &ctx->history[i];
        char tbuf[32];
        get_timestamp_str(e->timestamp, tbuf, sizeof(tbuf));
        switch (format) {
            case OUTPUT_FORMAT_JSON: break;
            case OUTPUT_FORMAT_CSV:
                fprintf(f, "\"%s\",\"%s\",\"%s\",%d,%s\n", tbuf, csv_escape(e->query),
                    e->engine, e->result_count, e->success ? "true" : "false");
                break;
            default:
                fprintf(f, "%s | %s | %s | %d results | %s\n",
                    tbuf, e->engine, e->query, e->result_count,
                    e->success ? "OK" : "FAIL");
                break;
        }
    }
    fclose(f);
    printf("History exported to %s\n", filepath);
}

void history_remove_entry(BrowserContext* ctx, int index) {
    if (!ctx->history || index < 0 || index >= ctx->history_count) {
        printf("Invalid history index.\n"); return;
    }
    if (index < ctx->history_count - 1) {
        memmove(&ctx->history[index], &ctx->history[index + 1],
            (ctx->history_count - index - 1) * sizeof(HistoryEntry));
    }
    ctx->history_count--;
    printf("History entry %d removed.\n", index + 1);
}
