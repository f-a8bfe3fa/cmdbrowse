#include "parser.h"
#include "utils.h"
#include <ctype.h>

/* Helper: extract a JSON string value after a given key.
   Looks for: "key" : "value"
   Returns a newly allocated string (must be freed), or NULL.
*/
static char* json_extract_string(const char* json, const char* key) {
    if (!json || !key) return NULL;
    size_t key_len = strlen(key);
    const char* p = json;
    while ((p = strstr(p, key)) != NULL) {
        /* Ensure this looks like a key (preceded by quote or whitespace) */
        if (p > json) {
            const char* before = p - 1;
            while (before > json && isspace((unsigned char)*before)) before--;
            if (*before != '"') { p += key_len; continue; }
        }
        p += key_len;
        /* Skip whitespace and colon */
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p != ':') continue;
        p++;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p != '"') {
            /* Try to capture unquoted numbers/booleans if needed, but for strings we need quotes */
            continue;
        }
        p++; /* skip opening quote */
        const char* start = p;
        /* Find closing quote, handling escaped quotes */
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

/* Helper: find the matching '}' or ']' for an opening '{' or '[' starting at *start.
   Returns pointer to the matching char, or NULL.
*/
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
                p++; /* skip escaped char */
            } else if (*p == '"') {
                in_string = false;
            }
        }
        p++;
    }
    return NULL;
}

/* Helper: extract a field from a JSON object block (e.g. a result item).
   We search for key inside block, but ensure we don't cross into a sibling object.
*/
static char* json_extract_string_in_block(const char* block, const char* key) {
    return json_extract_string(block, key);
}

SearchResponse* parser_parse_brave_api(const char* json, SearchEngine* engine, SearchQuery* query) {
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
        /* Try alternate location under "web" */
        const char* web = strstr(json, "\"web\"");
        if (web) results_array = strstr(web, "\"results\"");
    }
    if (!results_array) {
        resp->result_count = 0;
        return resp;
    }

    /* Move to '[' after "results" */
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
        char* desc  = json_extract_string_in_block(block, "\"description\"");

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
            r->relevance_score = 0.9f;
            idx++;
        }

        free(block);
        p = obj_end + 1;
    }

    resp->result_count = idx;
    return resp;
}

/* Helper to add a DDG result */
static void add_ddg_result(SearchResponse* resp, int* idx, int cap,
                           const char* text, const char* first_url,
                           SearchEngine* engine) {
    if (*idx >= cap) return;
    if ((!text || !text[0]) && (!first_url || !first_url[0])) return;
    SearchResult* r = &resp->results[*idx];
    if (text) snprintf(r->title, sizeof(r->title), "%s", text);
    if (first_url) snprintf(r->url, sizeof(r->url), "%s", first_url);
    if (text && !r->snippet[0]) {
        snprintf(r->snippet, sizeof(r->snippet), "%s", text);
    }
    snprintf(r->engine, sizeof(r->engine), "%s", engine->name);
    r->timestamp = time(NULL);
    r->relevance_score = 0.7f;
    (*idx)++;
}

/* Parse a RelatedTopics array, handling both direct items and nested Topics. */
static void parse_related_topics(const char* json_start, const char* json_end,
                                 SearchResponse* resp, int* idx, int cap,
                                 SearchEngine* engine) {
    const char* p = json_start;
    while (*idx < cap && p < json_end) {
        const char* obj_start = strchr(p, '{');
        if (!obj_start || obj_start > json_end) break;
        const char* obj_end = json_find_matching(obj_start);
        if (!obj_end || obj_end > json_end) break;

        size_t blen = obj_end - obj_start + 1;
        char* block = (char*)malloc(blen + 1);
        if (!block) break;
        memcpy(block, obj_start, blen);
        block[blen] = '\0';

        /* Check for nested "Topics" array first */
        const char* nested_topics = strstr(block, "\"Topics\"");
        if (nested_topics) {
            const char* nested_arr = strchr(nested_topics, '[');
            if (nested_arr) {
                const char* nested_arr_end = json_find_matching(nested_arr);
                if (nested_arr_end && nested_arr_end < block + blen) {
                    parse_related_topics(nested_arr + 1, nested_arr_end, resp, idx, cap, engine);
                }
            }
        } else {
            char* text = json_extract_string_in_block(block, "\"Text\"");
            char* first_url = json_extract_string_in_block(block, "\"FirstURL\"");
            add_ddg_result(resp, idx, cap, text, first_url, engine);
            if (text) free(text);
            if (first_url) free(first_url);
        }

        free(block);
        p = obj_end + 1;
    }
}

SearchResponse* parser_parse_ddg_api(const char* json, SearchEngine* engine, SearchQuery* query) {
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

    int idx = 0;

    /* Use AbstractText/AbstractURL as first result if available */
    char* abstract_text = json_extract_string(json, "\"AbstractText\"");
    char* abstract_url  = json_extract_string(json, "\"AbstractURL\"");
    if (abstract_text && abstract_text[0]) {
        add_ddg_result(resp, &idx, cap, abstract_text, abstract_url, engine);
    }
    if (abstract_text) free(abstract_text);
    if (abstract_url) free(abstract_url);

    /* Parse RelatedTopics array */
    const char* related = strstr(json, "\"RelatedTopics\"");
    if (related) {
        const char* arr_start = strchr(related, '[');
        if (arr_start) {
            const char* arr_end = json_find_matching(arr_start);
            if (arr_end) {
                parse_related_topics(arr_start + 1, arr_end, resp, &idx, cap, engine);
            }
        }
    }

    resp->result_count = idx;
    return resp;
}
