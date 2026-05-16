#ifndef HISTORY_H
#define HISTORY_H

#include "types.h"

void history_init(BrowserContext* ctx);
void history_add(BrowserContext* ctx, const char* query, const char* engine, int result_count, bool success);
void history_show(BrowserContext* ctx, int limit);
void history_search(BrowserContext* ctx, const char* keyword);
void history_clear(BrowserContext* ctx);
bool history_save(BrowserContext* ctx);
bool history_load(BrowserContext* ctx);
void history_stats(BrowserContext* ctx);
void history_export(BrowserContext* ctx, const char* filepath, OutputFormat format);
void history_remove_entry(BrowserContext* ctx, int index);
#endif
