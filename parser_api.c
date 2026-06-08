#include "parser.h"
#include "utils.h"
#include <ctype.h>

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif

/* Helper: extract a JSON string value after a given key.
   Looks for: "key" : "value"
   Returns a newly allocated string (must be freed), or NULL.
*/
static char* json_extract_string(const char* json, const char* key) {
    if (!json || !key) return NULL;
    size_t key_len = strlen(key);
    const char* p = json;
    while ((p = strstr(p, key)) != NULL) {
        if (p > json) {
            const char* before = p - 1;
            while (before > json && isspace((unsigned char)*before)) before--;
            if (*before != '"') { p += key_len; continue; }
        }
        p += key_len;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p != ':') continue;
        p++;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p != '"') continue;
        p++;
        const char* start = p;
        const char* end = p;
        while (*end) {
            if (*end == '\\' && *(end + 1) == '"') {
                end += 2;
            } else if (*end == '"') {
                break;
            } else {
                end++;
            }
        }
        if (*end != '"') return NULL;
        size_t len = end - start;
        if (len == 0) return NULL;
        char* result = (char*)malloc(len + 1);
        if (!result) return NULL;
        size_t ri = 0;
        for (size_t i = 0; i < len; i++) {
            if (start[i] == '\\' && i + 1 < len) {
                char next = start[i + 1];
                if (next == '"' || next == '\\' || next == '/') {
                    result[ri++] = next; i++;
                } else if (next == 'n') {
                    result[ri++] = '\n'; i++;
                } else if (next == 'r') {
                    result[ri++] = '\r'; i++;
                } else if (next == 't') {
                    result[ri++] = '\t'; i++;
                } else {
                    result[ri++] = start[i];
                }
            } else {
                result[ri++] = start[i];
            }
        }
        result[ri] = '\0';
        return result;
    }
    return NULL;
}

/* Helper: find the matching '}' or ']' for an opening '{' or '[' starting at *start. */
static const char* json_find_matching(const char* start) {
    if (!start) return NULL;
    char open = *start;
    char close = (open == '{') ? '}' : (open == '[') ? ']' : '\0';
    if (!close) return NULL;
    int depth = 1;
    const char* p = start + 1;
    bool in_string = false;
    while (*p && depth > 0) {
        if (!in_string) {
            if (*p == '"') {
                in_string = true;
            } else if (*p == open) {
                depth++;
            } else if (*p == close) {
                depth--;
                if (depth == 0) return p;
            }
        } else {
            if (*p == '\\') {
                p++;
            } else if (*p == '"') {
                in_string = false;
            }
        }
        p++;
    }
    return NULL;
}

static char* json_extract_string_in_block(const char* block, const char* key) {
    return json_extract_string(block, key);
}

/* ===================== Tavily API Parser ===================== */
SearchResponse* parser_parse_tavily_api(const char* json, SearchEngine* engine, SearchQuery* query) {
    if (!json || !engine || !query) return NULL;

    SearchResponse* resp = (SearchResponse*)calloc(1, sizeof(SearchResponse));
    if (!resp) return NULL;
    resp->engine = engine;
    resp->page = query->page;
    resp->timestamp = time(NULL);
    snprintf(resp->query, sizeof(resp->query), "%s", query->query);

    int cap = query->results_per_page > 0 ? query->results_per_page : 10;
    if (cap > MAX_RESULTS_PER_PAGE) cap = MAX_RESULTS_PER_PAGE;
    resp->results = (SearchResult*)calloc(cap, sizeof(SearchResult));
    if (!resp->results) { free(resp); return NULL; }

    const char* results_array = strstr(json, "\"results\"");
    if (!results_array) {
        resp->result_count = 0;
        return resp;
    }

    const char* arr_start = strchr(results_array, '[');
    if (!arr_start) {
        resp->result_count = 0;
        return resp;
    }

    const char* arr_end = json_find_matching(arr_start);
    if (!arr_end) arr_end = arr_start + strlen(arr_start);

    const char* p = arr_start + 1;
    int idx = 0;
    while (idx < cap && p < arr_end) {
        const char* obj_start = strchr(p, '{');
        if (!obj_start || obj_start > arr_end) break;
        const char* obj_end = json_find_matching(obj_start);
        if (!obj_end || obj_end > arr_end) break;

        size_t blen = obj_end - obj_start + 1;
        char* block = (char*)malloc(blen + 1);
        if (!block) break;
        memcpy(block, obj_start, blen);
        block[blen] = '\0';

        char* title = json_extract_string_in_block(block, "\"title\"");
        char* url   = json_extract_string_in_block(block, "\"url\"");
        char* desc  = json_extract_string_in_block(block, "\"content\"");

        SearchResult* r = &resp->results[idx];
        if (title) {
            snprintf(r->title, sizeof(r->title), "%s", title);
            free(title);
        }
        if (url) {
            snprintf(r->url, sizeof(r->url), "%s", url);
            free(url);
        }
        if (desc) {
            snprintf(r->snippet, sizeof(r->snippet), "%s", desc);
            free(desc);
        }
        if (r->title[0] || r->url[0]) {
            snprintf(r->engine, sizeof(r->engine), "%s", engine->name);
            r->timestamp = time(NULL);
            r->relevance_score = 0.95f;
            idx++;
        }

        free(block);
        p = obj_end + 1;
    }

    resp->result_count = idx;
    return resp;
}

/* ===================== Bing API Parser ===================== */
SearchResponse* parser_parse_bing_api(const char* json, SearchEngine* engine, SearchQuery* query) {
    if (!json || !engine || !query) return NULL;

    SearchResponse* resp = (SearchResponse*)calloc(1, sizeof(SearchResponse));
    if (!resp) return NULL;
    resp->engine = engine;
    resp->page = query->page;
    resp->timestamp = time(NULL);
    snprintf(resp->query, sizeof(resp->query), "%s", query->query);

    int cap = query->results_per_page > 0 ? query->results_per_page : 10;
    if (cap > MAX_RESULTS_PER_PAGE) cap = MAX_RESULTS_PER_PAGE;
    resp->results = (SearchResult*)calloc(cap, sizeof(SearchResult));
    if (!resp->results) { free(resp); return NULL; }

    /* Bing v7 returns results under webPages -> value array */
    const char* web_pages = strstr(json, "\"webPages\"");
    const char* results_array = web_pages ? strstr(web_pages, "\"value\"") : NULL;
    if (!results_array) {
        resp->result_count = 0;
        return resp;
    }

    const char* arr_start = strchr(results_array, '[');
    if (!arr_start) {
        resp->result_count = 0;
        return resp;
    }

    const char* arr_end = json_find_matching(arr_start);
    if (!arr_end) arr_end = arr_start + strlen(arr_start);

    const char* p = arr_start + 1;
    int idx = 0;
    while (idx < cap && p < arr_end) {
        const char* obj_start = strchr(p, '{');
        if (!obj_start || obj_start > arr_end) break;
        const char* obj_end = json_find_matching(obj_start);
        if (!obj_end || obj_end > arr_end) break;

        size_t blen = obj_end - obj_start + 1;
        char* block = (char*)malloc(blen + 1);
        if (!block) break;
        memcpy(block, obj_start, blen);
        block[blen] = '\0';

        char* title = json_extract_string_in_block(block, "\"name\"");
        char* url   = json_extract_string_in_block(block, "\"url\"");
        char* desc  = json_extract_string_in_block(block, "\"snippet\"");

        SearchResult* r = &resp->results[idx];
        if (title) {
            snprintf(r->title, sizeof(r->title), "%s", title);
            free(title);
        }
        if (url) {
            snprintf(r->url, sizeof(r->url), "%s", url);
            free(url);
        }
        if (desc) {
            snprintf(r->snippet, sizeof(r->snippet), "%s", desc);
            free(desc);
        }
        if (r->title[0] || r->url[0]) {
            snprintf(r->engine, sizeof(r->engine), "%s", engine->name);
            r->timestamp = time(NULL);
            r->relevance_score = 0.85f;
            idx++;
        }

        free(block);
        p = obj_end + 1;
    }

    resp->result_count = idx;
    return resp;
}

/* ===================== Google Custom Search API Parser ===================== */
SearchResponse* parser_parse_google_api(const char* json, SearchEngine* engine, SearchQuery* query) {
    if (!json || !engine || !query) return NULL;

    SearchResponse* resp = (SearchResponse*)calloc(1, sizeof(SearchResponse));
    if (!resp) return NULL;
    resp->engine = engine;
    resp->page = query->page;
    resp->timestamp = time(NULL);
    snprintf(resp->query, sizeof(resp->query), "%s", query->query);

    int cap = query->results_per_page > 0 ? query->results_per_page : 10;
    if (cap > MAX_RESULTS_PER_PAGE) cap = MAX_RESULTS_PER_PAGE;
    resp->results = (SearchResult*)calloc(cap, sizeof(SearchResult));
    if (!resp->results) { free(resp); return NULL; }

    /* Google CSE returns results under items array */
    const char* results_array = strstr(json, "\"items\"");
    if (!results_array) {
        resp->result_count = 0;
        return resp;
    }

    const char* arr_start = strchr(results_array, '[');
    if (!arr_start) {
        resp->result_count = 0;
        return resp;
    }

    const char* arr_end = json_find_matching(arr_start);
    if (!arr_end) arr_end = arr_start + strlen(arr_start);

    const char* p = arr_start + 1;
    int idx = 0;
    while (idx < cap && p < arr_end) {
        const char* obj_start = strchr(p, '{');
        if (!obj_start || obj_start > arr_end) break;
        const char* obj_end = json_find_matching(obj_start);
        if (!obj_end || obj_end > arr_end) break;

        size_t blen = obj_end - obj_start + 1;
        char* block = (char*)malloc(blen + 1);
        if (!block) break;
        memcpy(block, obj_start, blen);
        block[blen] = '\0';

        char* title = json_extract_string_in_block(block, "\"title\"");
        char* url   = json_extract_string_in_block(block, "\"link\"");
        char* desc  = json_extract_string_in_block(block, "\"snippet\"");

        SearchResult* r = &resp->results[idx];
        if (title) {
            snprintf(r->title, sizeof(r->title), "%s", title);
            free(title);
        }
        if (url) {
            snprintf(r->url, sizeof(r->url), "%s", url);
            free(url);
        }
        if (desc) {
            snprintf(r->snippet, sizeof(r->snippet), "%s", desc);
            free(desc);
        }
        if (r->title[0] || r->url[0]) {
            snprintf(r->engine, sizeof(r->engine), "%s", engine->name);
            r->timestamp = time(NULL);
            r->relevance_score = 0.8f;
            idx++;
        }

        free(block);
        p = obj_end + 1;
    }

    resp->result_count = idx;
    return resp;
}
