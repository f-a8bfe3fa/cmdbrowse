#ifndef ENGINE_H
#define ENGINE_H

#include "types.h"

void engine_registry_init(BrowserContext* ctx);
SearchEngine* engine_get_by_name(BrowserContext* ctx, const char* name);
SearchEngine* engine_get_by_index(BrowserContext* ctx, int index);
void engine_set_enabled(BrowserContext* ctx, const char* name, bool enabled);
void engine_set_priority(BrowserContext* ctx, const char* name, int priority);
void engine_list_all(BrowserContext* ctx);
void engine_list_enabled(BrowserContext* ctx);
int  engine_get_default_page_count(SearchEngine* engine);
char* engine_build_search_url(SearchEngine* engine, SearchQuery* query);
const char* engine_get_tier_name(EngineTier tier);
const char* engine_get_tier_icon(EngineTier tier);
void engine_sort_by_priority(BrowserContext* ctx);
int  engine_get_enabled_count(BrowserContext* ctx);
void engine_add_custom(BrowserContext* ctx, const char* name, const char* url, const char* query_param);
bool engine_remove_custom(BrowserContext* ctx, const char* name);
void engine_export_config(BrowserContext* ctx, const char* filepath);
bool engine_import_config(BrowserContext* ctx, const char* filepath);

#endif
