#include "cache.h"
#include "utils.h"
#include "parser.h"

typedef struct {
    char key[256];
    char query[MAX_QUERY_LENGTH];
    char engine[MAX_ENGINE_NAME];
    int  page;
    SearchResponse resp;
    time_t created_at;
    time_t expires_at;
    bool  valid;
} CacheEntry;

static CacheEntry* g_cache = NULL;
static int g_cache_count = 0;
static int g_cache_capacity = 0;

char* cache_build_key(const char* query, const char* engine, int page) {
    static char key[256];
    char lower_q[256] = {0};
    char lower_e[64] = {0};
    if (query && query[0]) {
        size_t qlen = strlen(query);
        if (qlen >= sizeof(lower_q)) qlen = sizeof(lower_q) - 1;
        memcpy(lower_q, query, qlen);
    }
    if (engine && engine[0]) {
        size_t elen = strlen(engine);
        if (elen >= sizeof(lower_e)) elen = sizeof(lower_e) - 1;
        memcpy(lower_e, engine, elen);
    }
    str_to_lower(lower_q);
    str_to_lower(lower_e);
    snprintf(key, sizeof(key), "%s|%s|%d", lower_q, lower_e, page);
    return key;
}

void cache_init(BrowserContext* ctx) {
    (void)ctx;
    g_cache_capacity = 256;
    g_cache = (CacheEntry*)calloc(g_cache_capacity, sizeof(CacheEntry));
    g_cache_count = 0;
}

bool cache_lookup(BrowserContext* ctx, const char* query, const char* engine, int page, SearchResponse** out_resp) {
    (void)ctx;
    if (!g_cache) return false;
    char* key = cache_build_key(query, engine, page);
    time_t now = time(NULL);
    for (int i = 0; i < g_cache_count; i++) {
        if (g_cache[i].valid && strcmp(g_cache[i].key, key) == 0) {
            if (g_cache[i].expires_at > now) {
                SearchResponse* copy = (SearchResponse*)malloc(sizeof(SearchResponse));
                if (copy) {
                    memcpy(copy, &g_cache[i].resp, sizeof(SearchResponse));
                    if (copy->result_count > 0 && copy->results) {
                        SearchResult* result_copy = (SearchResult*)malloc(
                            copy->result_count * sizeof(SearchResult));
                        if (result_copy) {
                            memcpy(result_copy, copy->results,
                                copy->result_count * sizeof(SearchResult));
                            copy->results = result_copy;
                        }
                    } else {
                        copy->results = NULL;
                    }
                    *out_resp = copy;
                    return true;
                }
            } else {
                g_cache[i].valid = false;
            }
        }
    }
    return false;
}

bool cache_store(BrowserContext* ctx, const char* query, const char* engine, int page, SearchResponse* resp) {
    (void)ctx;
    if (!g_cache) cache_init(ctx);
    if (!resp || resp->result_count <= 0) return false;

    char* key = cache_build_key(query, engine, page);

    for (int i = 0; i < g_cache_count; i++) {
        if (!g_cache[i].valid && strcmp(g_cache[i].key, key) == 0) {
            search_response_free(&g_cache[i].resp);
            memcpy(&g_cache[i].resp, resp, sizeof(SearchResponse));
            g_cache[i].resp.results = (SearchResult*)malloc(resp->result_count * sizeof(SearchResult));
            if (g_cache[i].resp.results) {
                memcpy(g_cache[i].resp.results, resp->results,
                    resp->result_count * sizeof(SearchResult));
            }
            g_cache[i].created_at = time(NULL);
            g_cache[i].expires_at = time(NULL) + 3600;
            g_cache[i].valid = true;
            return true;
        }
    }

    if (g_cache_count >= g_cache_capacity) {
        int oldest_idx = 0;
        time_t oldest_time = g_cache[0].created_at;
        for (int i = 1; i < g_cache_count; i++) {
            if (g_cache[i].created_at < oldest_time) {
                oldest_time = g_cache[i].created_at;
                oldest_idx = i;
            }
        }
        search_response_free(&g_cache[oldest_idx].resp);
        memmove(&g_cache[oldest_idx], &g_cache[oldest_idx + 1],
            (g_cache_count - oldest_idx - 1) * sizeof(CacheEntry));
        g_cache_count--;
    }

    CacheEntry* entry = &g_cache[g_cache_count];
    memset(entry, 0, sizeof(CacheEntry));
    snprintf(entry->key, sizeof(entry->key), "%s", key);
    snprintf(entry->query, sizeof(entry->query), "%s", query);
    snprintf(entry->engine, sizeof(entry->engine), "%s", engine);
    entry->page = page;
    memcpy(&entry->resp, resp, sizeof(SearchResponse));
    entry->resp.results = (SearchResult*)malloc(resp->result_count * sizeof(SearchResult));
    if (entry->resp.results) {
        memcpy(entry->resp.results, resp->results,
            resp->result_count * sizeof(SearchResult));
    }
    entry->created_at = time(NULL);
    entry->expires_at = time(NULL) + 3600;
    entry->valid = true;
    g_cache_count++;
    return true;
}

void cache_clear(BrowserContext* ctx) {
    (void)ctx;
    if (g_cache) {
        for (int i = 0; i < g_cache_count; i++) {
            search_response_free(&g_cache[i].resp);
        }
        free(g_cache);
        g_cache = NULL;
        g_cache_count = 0;
        g_cache_capacity = 0;
    }
    printf("Cache cleared.\n");
}

void cache_stats(BrowserContext* ctx) {
    (void)ctx;
    if (!g_cache || g_cache_count == 0) {
        printf(COLOR_DIM "Cache is empty.\n" COLOR_RESET); return;
    }
    int valid = 0, expired = 0;
    int total_results = 0;
    time_t now = time(NULL);
    for (int i = 0; i < g_cache_count; i++) {
        if (g_cache[i].valid && g_cache[i].expires_at > now) { valid++; total_results += g_cache[i].resp.result_count; }
        else expired++;
    }
    printf("\n" COLOR_BOLD "Cache Statistics:" COLOR_RESET "\n\n");
    printf("  Total entries:   %d\n", g_cache_count);
    printf("  Valid entries:   %d\n", valid);
    printf("  Expired entries: %d\n", expired);
    printf("  Cached results:  %d\n", total_results);
    printf("  Capacity:        %d\n", g_cache_capacity);
    printf("\n");
}

void cache_cleanup_expired(BrowserContext* ctx, time_t max_age_seconds) {
    (void)ctx;
    if (!g_cache) return;
    time_t now = time(NULL);
    int cleaned = 0;
    for (int i = 0; i < g_cache_count; i++) {
        if (g_cache[i].valid && (now - g_cache[i].created_at) > max_age_seconds) {
            search_response_free(&g_cache[i].resp);
            g_cache[i].valid = false;
            cleaned++;
        }
    }
    if (cleaned > 0) printf("Cleaned %d expired cache entries.\n", cleaned);
}

int cache_count(BrowserContext* ctx) {
    (void)ctx;
    return g_cache ? g_cache_count : 0;
}
