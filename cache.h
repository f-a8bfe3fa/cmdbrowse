#ifndef CACHE_H
#define CACHE_H

#include "types.h"

void cache_init(BrowserContext* ctx);
bool cache_lookup(BrowserContext* ctx, const char* query, const char* engine, int page, SearchResponse** out_resp);
bool cache_store(BrowserContext* ctx, const char* query, const char* engine, int page, SearchResponse* resp);
void cache_clear(BrowserContext* ctx);
void cache_stats(BrowserContext* ctx);
void cache_cleanup_expired(BrowserContext* ctx, time_t max_age_seconds);
int  cache_count(BrowserContext* ctx);
char* cache_build_key(const char* query, const char* engine, int page);
#endif
