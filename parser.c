#include "parser.h"
#include "utils.h"

char* parser_extract_text_between(const char* haystack, const char* after, const char* before) {
    const char* s = after ? strstr(haystack, after) : haystack;
    if (!s) return NULL;
    s += after ? strlen(after) : 0;
    const char* e = before ? strstr(s, before) : (s + strlen(s));
    if (!e) e = s + strlen(s);
    size_t len = e - s;
    if (len < 1) return NULL;
    if (len > 10000) len = 10000;
    char* result = (char*)malloc(len + 1);
    if (result) { memcpy(result, s, len); result[len] = '\0'; }
    return result;
}

static SearchResult parse_single_result(const char* block, SearchEngine* engine) {
    SearchResult r; memset(&r, 0, sizeof(r));
    r.timestamp = time(NULL);

    char* title = parser_extract_text_between(block, "\">", "</a>");
    if (!title) title = parser_extract_text_between(block, "title=\"", "\"");
    if (!title) title = parser_extract_text_between(block, "<h3", "</h3>");
    if (!title) title = parser_extract_text_between(block, "aria-label=\"", "\"");
    if (title) {
        strip_html_tags(title, r.title, sizeof(r.title));
        html_entity_decode(r.title, r.title, sizeof(r.title));
        trim_whitespace(r.title);
        free(title);
    } else {
        snprintf(r.title, sizeof(r.title), "Untitled Result");
    }

    char* url = parser_extract_text_between(block, "href=\"", "\"");
    if (!url) url = parser_extract_text_between(block, "href='", "'");
    if (!url) url = parser_extract_text_between(block, "data-url=\"", "\"");
    if (url) {
        snprintf(r.url, sizeof(r.url), "%s", url);
        free(url);
    } else {
        snprintf(r.url, sizeof(r.url), "about:blank");
    }

    const char* snippet_patterns[] = {
        "class=\"result__snippet\"", "class=\"s\"", "class=\"st\"", "class=\"description\"",
        "\"snippet\":\"", "class=\"b_caption\"", "class=\"b_snippet\"", "id=\"snippet",
        "class=\"d-snippet\"", "class=\"web-result__description\"",
        "<p>", "class=\"result-snippet\"", "class=\"search-snippet\"",
        "class=\"S13443\"", "class=\"spnsr\"", NULL
    };
    bool found_snippet = false;
    for (int pi = 0; snippet_patterns[pi]; pi++) {
        const char* s = strstr(block, snippet_patterns[pi]);
        if (s) {
            const char* start = s + strlen(snippet_patterns[pi]);
            const char* end = strstr(start, "</");
            if (!end) end = strstr(start, "<");
            if (!end) end = start + strlen(start);
            if (end > start) {
                size_t slen = end - start;
                if (slen >= sizeof(r.snippet)) slen = sizeof(r.snippet) - 1;
                memcpy(r.snippet, start, slen);
                r.snippet[slen] = '\0';
                strip_html_tags(r.snippet, r.snippet, sizeof(r.snippet));
                html_entity_decode(r.snippet, r.snippet, sizeof(r.snippet));
                trim_whitespace(r.snippet);
                found_snippet = true;
                break;
            }
        }
    }
    if (!found_snippet) {
        char* all_text = str_dup(block);
        if (all_text) {
            strip_html_tags(all_text, r.snippet, sizeof(r.snippet));
            trim_whitespace(r.snippet);
            if (strlen(r.snippet) > 300) r.snippet[300] = '\0';
            free(all_text);
        }
    }

    snprintf(r.engine, sizeof(r.engine), "%s", engine->name);
    r.is_advertisement = strstr(block, "ad") || strstr(block, "sponsored") || strstr(block, "advertisement");
    r.relevance_score = r.is_advertisement ? 0.1f : (float)(1.0 - (rand() % 100) / 1000.0);
    str_to_lower(r.snippet);
    r.snippet[0] = (char)toupper((unsigned char)r.snippet[0]);

    if (strlen(r.title) < 2 && strlen(r.url) > 10) {
        extract_domain(r.url, r.title, sizeof(r.title));
    }

    extract_domain(r.url, r.display_url, sizeof(r.display_url));
    return r;
}

SearchResponse* parser_parse_google(const char* html, SearchEngine* engine, SearchQuery* query) {
    SearchResponse* resp = (SearchResponse*)calloc(1, sizeof(SearchResponse));
    if (!resp) return NULL;
    resp->engine = engine;
    resp->page = query->page;
    resp->timestamp = time(NULL);
    snprintf(resp->query, sizeof(resp->query), "%s", query->query);

    int cap = query->results_per_page > 0 ? query->results_per_page : 10;
    resp->results = (SearchResult*)malloc(cap * sizeof(SearchResult));
    if (!resp->results) { free(resp); return NULL; }

    const char* result_patterns[] = { "<div class=\"g\"", "<div class=\"Gx5Zad", "<div data-hveid", NULL };
    const char* p = html;
    int idx = 0;
    bool first = true;

    while (idx < cap && (p = strstr(p, first ? result_patterns[0] : "<div class=\"g\"")) != NULL) {
        const char* next = strstr(p + 1, "<div class=\"g\"");
        size_t block_len = next ? (size_t)(next - p) : min(strlen(p), 30000);
        char* block = (char*)malloc(block_len + 1);
        if (block) {
            memcpy(block, p, block_len);
            block[block_len] = '\0';
            SearchResult r = parse_single_result(block, engine);
            if (strlen(r.title) > 2 || strlen(r.url) > 10) {
                if (idx < cap) memcpy(&resp->results[idx], &r, sizeof(SearchResult));
                idx++;
            }
            free(block);
        }
        p += 1;
    }

    if (idx == 0) {
        SearchResponse* fallback = parser_parse_generic(html, engine, query);
        free(resp->results);
        free(resp);
        return fallback;
    }

    if (strcmp(engine->name, "google") == 0) {
        char* total = str_between(html, "id=\"result-stats\">", "<");
        if (!total) total = str_between(html, "About ", " results");
        if (total) {
            const char* num_start = total;
            while (*num_start && !isdigit((unsigned char)*num_start) && *num_start != ',') num_start++;
            int num = 0;
            for (const char* d = num_start; *d; d++) {
                if (isdigit((unsigned char)*d)) num = num * 10 + (*d - '0');
                else if (*d == ',') continue;
                else break;
            }
            resp->total_count = num;
            if (resp->total_count > 0)
                resp->total_pages = (resp->total_count + query->results_per_page - 1) / query->results_per_page;
            free(total);
        }
    }

    resp->result_count = idx;
    if (idx > 1) resp->total_count = max(resp->total_count, idx);
    return resp;
}

SearchResponse* parser_parse_bing(const char* html, SearchEngine* engine, SearchQuery* query) {
    SearchResponse* resp = (SearchResponse*)calloc(1, sizeof(SearchResponse));
    if (!resp) return NULL;
    resp->engine = engine;
    resp->page = query->page;
    resp->timestamp = time(NULL);
    snprintf(resp->query, sizeof(resp->query), "%s", query->query);

    int cap = 15;
    resp->results = (SearchResult*)malloc(cap * sizeof(SearchResult));
    if (!resp->results) { free(resp); return NULL; }

    int idx = 0;
    const char* patterns[] = { "<li class=\"b_algo\"", "<li class=\"b_ans\"", NULL };

    for (int pi = 0; patterns[pi] && idx < cap; pi++) {
        const char* p = html;
        while (idx < cap && (p = strstr(p, patterns[pi])) != NULL) {
            const char* end_marker = strstr(p + 1, "<li class=\"b_algo\"");
            if (!end_marker) end_marker = p + min(strlen(p), 20000);
            size_t blen = end_marker - p;
            char* block = (char*)malloc(blen + 1);
            if (block) {
                memcpy(block, p, blen);
                block[blen] = '\0';
                SearchResult r = parse_single_result(block, engine);
                if (strlen(r.title) > 2 && strlen(r.url) > 5) {
                    if (idx < cap) memcpy(&resp->results[idx], &r, sizeof(SearchResult));
                    idx++;
                }
                free(block);
            }
            p += 10;
        }
    }

    if (idx == 0) {
        SearchResponse* fallback = parser_parse_generic(html, engine, query);
        free(resp->results);
        free(resp);
        return fallback;
    }

    resp->result_count = idx;
    return resp;
}

SearchResponse* parser_parse_duckduckgo(const char* html, SearchEngine* engine, SearchQuery* query) {
    SearchResponse* resp = (SearchResponse*)calloc(1, sizeof(SearchResponse));
    if (!resp) return NULL;
    resp->engine = engine;
    resp->page = query->page;
    resp->timestamp = time(NULL);
    snprintf(resp->query, sizeof(resp->query), "%s", query->query);

    int cap = 30;
    resp->results = (SearchResult*)malloc(cap * sizeof(SearchResult));
    if (!resp->results) { free(resp); return NULL; }

    int idx = 0;
    const char* p = html;
    while (idx < cap && (p = strstr(p, "<a rel=\"nofollow\" class=\"result-link\"")) != NULL) {
        const char* end = strstr(p + 1, "</a>");
        if (!end) end = p + 5000;
        size_t blen = end - p;
        char* block = (char*)malloc(blen + 1);
        if (block) {
            memcpy(block, p, blen); block[blen] = '\0';
            char* link = str_between(block, "href=\"", "\"");
            if (link) {
                url_decode(link, link, strlen(link) + 1);
                snprintf(resp->results[idx].url, sizeof(resp->results[idx].url), "%s", link);
                free(link);
            }
            char* title = str_between(block, "class=\"result-link\">", "<");
            if (title) {
                strip_html_tags(title, resp->results[idx].title, sizeof(resp->results[idx].title));
                html_entity_decode(resp->results[idx].title, resp->results[idx].title,
                    sizeof(resp->results[idx].title));
                trim_whitespace(resp->results[idx].title);
                free(title);
            }
            const char* snippet_start = strstr(p, "<td class=\"result-snippet\"");
            if (!snippet_start) snippet_start = strstr(p, "class=\"snippet\"");
            if (snippet_start) {
                const char* sq = strstr(snippet_start, ">");
                if (sq) {
                    const char* se = strstr(sq, "</td>");
                    if (se) {
                        size_t slen = se - sq - 1;
                        if (slen >= sizeof(resp->results[idx].snippet)) slen = sizeof(resp->results[idx].snippet) - 1;
                        memcpy(resp->results[idx].snippet, sq + 1, slen);
                        resp->results[idx].snippet[slen] = '\0';
                    }
                }
            }
            resp->results[idx].timestamp = time(NULL);
            resp->results[idx].relevance_score = 0.8f;
            snprintf(resp->results[idx].engine, sizeof(resp->results[idx].engine), "%s", engine->name);
            idx++;
            free(block);
        }
        p += 1;
    }

    if (idx == 0) {
        SearchResponse* fallback = parser_parse_generic(html, engine, query);
        free(resp->results);
        free(resp);
        return fallback;
    }

    resp->result_count = idx;
    return resp;
}

SearchResponse* parser_parse_yahoo(const char* html, SearchEngine* engine, SearchQuery* query) {
    SearchResponse* resp = (SearchResponse*)calloc(1, sizeof(SearchResponse));
    if (!resp) return NULL;
    resp->engine = engine;
    resp->page = query->page;
    resp->timestamp = time(NULL);
    snprintf(resp->query, sizeof(resp->query), "%s", query->query);
    int cap = 20;
    resp->results = (SearchResult*)malloc(cap * sizeof(SearchResult));
    if (!resp->results) { free(resp); return NULL; }
    int idx = 0;
    const char* p = html;
    while (idx < cap && (p = strstr(p, "<div class=\"Sr\"")) != NULL) {
        const char* next_div = strstr(p + 1, "<div class=\"Sr\"");
        size_t blen = next_div ? (size_t)(next_div - p) : min(strlen(p), 20000);
        char* block = (char*)malloc(blen + 1);
        if (block) {
            memcpy(block, p, blen); block[blen] = '\0';
            SearchResult r = parse_single_result(block, engine);
            if (strlen(r.title) > 2 && strlen(r.url) > 5) {
                if (idx < cap) memcpy(&resp->results[idx], &r, sizeof(SearchResult));
                idx++;
            }
            free(block);
        }
        p += 1;
    }
    if (idx == 0) {
        SearchResponse* fallback = parser_parse_generic(html, engine, query);
        free(resp->results);
        free(resp);
        return fallback;
    }
    resp->result_count = idx;
    return resp;
}

SearchResponse* parser_parse_generic(const char* html, SearchEngine* engine, SearchQuery* query) {
    SearchResponse* resp = (SearchResponse*)calloc(1, sizeof(SearchResponse));
    if (!resp) return NULL;
    resp->engine = engine;
    resp->page = query->page;
    resp->timestamp = time(NULL);
    snprintf(resp->query, sizeof(resp->query), "%s", query->query);
    int cap = query->results_per_page > 0 ? query->results_per_page : 10;
    resp->results = (SearchResult*)malloc(cap * sizeof(SearchResult));
    if (!resp->results) { free(resp); return NULL; }

    int count = 0;
    const char* p = html;
    while (count < cap && (p = strstr(p, "<a ")) != NULL) {
        const char* href = strstr(p, "href=\"");
        if (!href) { p++; continue; }
        href += 6;
        const char* url_end = strchr(href, '"');
        if (!url_end) { p++; continue; }
        size_t ulen = url_end - href;
        if (ulen >= 5 && ulen < MAX_RESULT_URL) {
            char temp_url[MAX_RESULT_URL];
            memcpy(temp_url, href, ulen); temp_url[ulen] = '\0';
            if (str_starts_with(temp_url, "http://") || str_starts_with(temp_url, "https://")) {
                if (!str_contains(temp_url, "google") && !str_contains(temp_url, "bing") &&
                    !str_contains(temp_url, "yahoo") && !str_contains(temp_url, "javascript")) {
                    snprintf(resp->results[count].url, sizeof(resp->results[count].url), "%s", temp_url);
                    const char* close_a = strstr(url_end, "</a>");
                    if (close_a) {
                        const char* title_text = url_end + 1;
                        size_t tlen = close_a - title_text;
                        if (tlen > 2 && tlen < MAX_RESULT_TITLE) {
                            char raw_title[MAX_RESULT_TITLE];
                            memcpy(raw_title, title_text, min(tlen, MAX_RESULT_TITLE - 1));
                            raw_title[min(tlen, MAX_RESULT_TITLE - 1)] = '\0';
                            strip_html_tags(raw_title, resp->results[count].title, sizeof(resp->results[count].title));
                            trim_whitespace(resp->results[count].title);
                        }
                    }
                    if (!resp->results[count].title[0]) {
                        extract_domain(temp_url, resp->results[count].title, sizeof(resp->results[count].title));
                    }
                    const char* snippet_start = close_a ? close_a + 4 : url_end + 1;
                    const char* next_link = strstr(snippet_start, "<a ");
                    size_t slen = next_link ? (size_t)(next_link - snippet_start) : min(strlen(snippet_start), 600);
                    if (slen > 5 && slen < MAX_RESULT_SNIPPET) {
                        char raw_snippet[MAX_RESULT_SNIPPET];
                        memcpy(raw_snippet, snippet_start, min(slen, MAX_RESULT_SNIPPET - 1));
                        raw_snippet[min(slen, MAX_RESULT_SNIPPET - 1)] = '\0';
                        strip_html_tags(raw_snippet, resp->results[count].snippet, sizeof(resp->results[count].snippet));
                        trim_whitespace(resp->results[count].snippet);
                    }
                    resp->results[count].timestamp = time(NULL);
                    resp->results[count].relevance_score = 0.5f;
                    snprintf(resp->results[count].engine, sizeof(resp->results[count].engine), "%s", engine->name);
                    extract_domain(temp_url, resp->results[count].display_url, sizeof(resp->results[count].display_url));
                    count++;
                }
            }
        }
        p = url_end ? url_end + 1 : p + 1;
    }
    resp->result_count = count;
    return resp;
}

SearchResponse* parser_parse_none(const char* html, SearchEngine* engine, SearchQuery* query) {
    SearchResponse* resp = (SearchResponse*)calloc(1, sizeof(SearchResponse));
    if (resp) {
        resp->engine = engine;
        resp->page = query->page;
        resp->timestamp = time(NULL);
        snprintf(resp->query, sizeof(resp->query), "%s", query->query);
        snprintf(resp->error_message, sizeof(resp->error_message), "Engine not supported for parsing");
    }
    return resp;
}

typedef SearchResponse* (*ParserFunc)(const char*, SearchEngine*, SearchQuery*);

SearchResponse* parser_parse_response(SearchEngine* engine, SearchQuery* query, HttpResponse* http_resp) {
    if (!http_resp || !http_resp->body || http_resp->status_code < 200 || http_resp->status_code >= 400) {
        SearchResponse* resp = (SearchResponse*)calloc(1, sizeof(SearchResponse));
        if (resp) {
            resp->engine = engine;
            resp->timestamp = time(NULL);
            resp->status_code = http_resp ? http_resp->status_code : -1;
            snprintf(resp->query, sizeof(resp->query), "%s", query->query);
            snprintf(resp->error_message, sizeof(resp->error_message),
                "HTTP %d - %s", resp->status_code,
                http_resp ? http_resp->status_text : "Network Error");
        }
        return resp;
    }

    ParserFunc parser = NULL;
    if (strcmp(engine->name, "google") == 0) parser = parser_parse_google;
    else if (strcmp(engine->name, "bing") == 0) parser = parser_parse_bing;
    else if (strcmp(engine->name, "duckduckgo") == 0) parser = parser_parse_duckduckgo;
    else if (strcmp(engine->name, "yahoo") == 0) parser = parser_parse_yahoo;
    else parser = parser_parse_generic;

    SearchResponse* resp = parser(http_resp->body, engine, query);
    if (resp) {
        resp->status_code = http_resp->status_code;
        resp->search_time_seconds = http_resp->elapsed_seconds;
    }
    return resp;
}

static int result_cmp_relevance(const void* a, const void* b) {
    const SearchResult* ra = (const SearchResult*)a;
    const SearchResult* rb = (const SearchResult*)b;
    if (ra->relevance_score < rb->relevance_score) return 1;
    if (ra->relevance_score > rb->relevance_score) return -1;
    return 0;
}

static int result_cmp_title(const void* a, const void* b) {
    return strcmp(((const SearchResult*)a)->title, ((const SearchResult*)b)->title);
}

static int result_cmp_date(const void* a, const void* b) {
    if (((const SearchResult*)a)->timestamp < ((const SearchResult*)b)->timestamp) return 1;
    if (((const SearchResult*)a)->timestamp > ((const SearchResult*)b)->timestamp) return -1;
    return 0;
}

static int result_cmp_engine(const void* a, const void* b) {
    return strcmp(((const SearchResult*)a)->engine, ((const SearchResult*)b)->engine);
}

void parser_sort_results(SearchResult* results, int count, SortOrder order) {
    switch (order) {
        case SORT_RELEVANCE: qsort(results, count, sizeof(SearchResult), result_cmp_relevance); break;
        case SORT_TITLE:     qsort(results, count, sizeof(SearchResult), result_cmp_title); break;
        case SORT_DATE:      qsort(results, count, sizeof(SearchResult), result_cmp_date); break;
        case SORT_ENGINE:    qsort(results, count, sizeof(SearchResult), result_cmp_engine); break;
        default: break;
    }
}

int parser_dedup_results(SearchResult* results, int count) {
    if (count <= 1) return count;
    int write_idx = 0;
    for (int i = 0; i < count; i++) {
        bool dup = false;
        for (int j = 0; j < write_idx; j++) {
            if (strcmp(results[i].url, results[j].url) == 0 ||
                levenshtein_distance(results[i].title, results[j].title) < 5) {
                dup = true;
                break;
            }
        }
        if (!dup) {
            if (write_idx != i) memcpy(&results[write_idx], &results[i], sizeof(SearchResult));
            write_idx++;
        }
    }
    return write_idx;
}

void search_response_free(SearchResponse* resp) {
    if (resp) {
        free(resp->results);
        free(resp);
    }
}

SearchResponse* search_merge_responses(SearchResponse** responses, int count, SortOrder sort_order) {
    if (count <= 0) return NULL;
    if (count == 1) {
        SearchResponse* resp = (SearchResponse*)malloc(sizeof(SearchResponse));
        if (resp) memcpy(resp, responses[0], sizeof(SearchResponse));
        return resp;
    }

    int total = 0;
    for (int i = 0; i < count; i++) total += responses[i] ? responses[i]->result_count : 0;

    SearchResponse* merged = (SearchResponse*)calloc(1, sizeof(SearchResponse));
    if (!merged) return NULL;
    merged->results = (SearchResult*)malloc(total * sizeof(SearchResult));
    if (!merged->results) { free(merged); return NULL; }

    int pos = 0;
    for (int i = 0; i < count; i++) {
        if (responses[i] && responses[i]->results) {
            memcpy(&merged->results[pos], responses[i]->results,
                responses[i]->result_count * sizeof(SearchResult));
            pos += responses[i]->result_count;
        }
    }
    merged->result_count = pos;

    if (sort_order == SORT_RELEVANCE) {
        for (int i = 0; i < merged->result_count; i++) {
            merged->results[i].relevance_score *= 0.9f;
        }
    }

    parser_sort_results(merged->results, merged->result_count, sort_order);
    merged->page = 1;
    merged->timestamp = time(NULL);
    snprintf(merged->query, sizeof(merged->query), "%s", responses[0] ? responses[0]->query : "");

    return merged;
}
