#include "engine.h"
#include "utils.h"

static const SearchEngine PRESET_ENGINES[] = {
    {
        .name = "tavily", .display_name = "Tavily Search",
        .search_url = "https://api.tavily.com/search",
        .search_url_encoded = "https://api.tavily.com/search?q={query}&max_results={count}&search_depth=basic&include_answer=false&include_raw_content=false",
        .query_param = "q", .page_param = "", .count_param = "max_results",
        .results_per_page = 10, .supports_suggestions = false, .supports_images = false,
        .supports_video = false, .supports_news = false, .supports_academic = false,
        .requires_api_key = true, .api_key_env = "TAVILY_API_KEY", .tier = ENGINE_TIER_GLOBAL,
        .country_code = "", .language_code = "en", .region_tag = "global",
        .icon = "T", .enabled = true, .priority = 1, .rate_limit_ms = 200
    },
    {
        .name = "bing_api", .display_name = "Bing Search API",
        .search_url = "https://api.bing.microsoft.com/v7.0/search",
        .search_url_encoded = "https://api.bing.microsoft.com/v7.0/search?q={query}&count={count}&offset={offset}&mkt={lang}&safesearch={safe}",
        .query_param = "q", .page_param = "offset", .count_param = "count",
        .results_per_page = 10, .supports_suggestions = false, .supports_images = false,
        .supports_video = false, .supports_news = false, .supports_academic = false,
        .requires_api_key = true, .api_key_env = "BING_API_KEY", .tier = ENGINE_TIER_GLOBAL,
        .country_code = "", .language_code = "en-US", .region_tag = "global",
        .icon = "B", .enabled = true, .priority = 2, .rate_limit_ms = 300
    },
    {
        .name = "google_api", .display_name = "Google Custom Search",
        .search_url = "https://www.googleapis.com/customsearch/v1",
        .search_url_encoded = "https://www.googleapis.com/customsearch/v1?q={query}&num={count}&start={offset}&hl={lang}&safe={safe}&cx={cx}&key={key}",
        .query_param = "q", .page_param = "start", .count_param = "num",
        .results_per_page = 10, .supports_suggestions = false, .supports_images = false,
        .supports_video = false, .supports_news = false, .supports_academic = false,
        .requires_api_key = true, .api_key_env = "GOOGLE_API_KEY", .tier = ENGINE_TIER_GLOBAL,
        .country_code = "", .language_code = "en", .region_tag = "global",
        .icon = "G", .enabled = true, .priority = 3, .rate_limit_ms = 400
    },
};

#define PRESET_COUNT (sizeof(PRESET_ENGINES) / sizeof(PRESET_ENGINES[0]))

static int engine_cmp_by_priority(const void* a, const void* b) {
    const SearchEngine* ea = (const SearchEngine*)a;
    const SearchEngine* eb = (const SearchEngine*)b;
    if (ea->priority != eb->priority) return ea->priority - eb->priority;
    return strcmp(ea->name, eb->name);
}

void engine_registry_init(BrowserContext* ctx) {
    ctx->engine_count = 0;
    for (size_t i = 0; i < PRESET_COUNT && ctx->engine_count < MAX_ENGINES; i++) {
        memcpy(&ctx->engines[ctx->engine_count], &PRESET_ENGINES[i], sizeof(SearchEngine));
        ctx->engine_count++;
    }
    engine_sort_by_priority(ctx);
}

SearchEngine* engine_get_by_name(BrowserContext* ctx, const char* name) {
    for (int i = 0; i < ctx->engine_count; i++) {
        if (strcmp(ctx->engines[i].name, name) == 0)
            return &ctx->engines[i];
    }
    return NULL;
}

SearchEngine* engine_get_by_index(BrowserContext* ctx, int index) {
    if (index < 0 || index >= ctx->engine_count) return NULL;
    return &ctx->engines[index];
}

void engine_set_enabled(BrowserContext* ctx, const char* name, bool enabled) {
    SearchEngine* engine = engine_get_by_name(ctx, name);
    if (engine) engine->enabled = enabled;
}

void engine_set_priority(BrowserContext* ctx, const char* name, int priority) {
    SearchEngine* engine = engine_get_by_name(ctx, name);
    if (engine) { engine->priority = priority; engine_sort_by_priority(ctx); }
}

void engine_list_all(BrowserContext* ctx) {
    printf("\n" COLOR_BOLD "%-5s %-20s %-10s %-12s %-8s %s\n" COLOR_RESET,
        "#", "NAME", "TIER", "REGION", "ENABLED", "URL");
    printf(COLOR_DIM);
    for (int i = 0; i < 80; i++) putchar('-');
    printf(COLOR_RESET "\n");
    for (int i = 0; i < ctx->engine_count; i++) {
        SearchEngine* e = &ctx->engines[i];
        printf("%-5d %-20s %-10s %-12s %-8s %s\n",
            i + 1, e->name,
            engine_get_tier_name(e->tier),
            e->region_tag[0] ? e->region_tag : "global",
            e->enabled ? COLOR_GREEN "YES" COLOR_RESET : COLOR_DIM "NO" COLOR_RESET,
            e->search_url);
    }
    printf("\nTotal: %d engines | Enabled: %d | %s\n",
        ctx->engine_count, engine_get_enabled_count(ctx),
        PRESET_COUNT == (size_t)ctx->engine_count ? "all presets loaded" : "custom engines added");
}

void engine_list_enabled(BrowserContext* ctx) {
    printf("\n" COLOR_BOLD COLOR_BRIGHT_GREEN "Active Search Engines:" COLOR_RESET "\n\n");
    for (int i = 0; i < ctx->engine_count; i++) {
        if (ctx->engines[i].enabled) {
            printf("  [%2d] " COLOR_BRIGHT_CYAN "%-20s" COLOR_RESET " %s %s %s\n",
                ctx->engines[i].priority,
                ctx->engines[i].display_name,
                COLOR_DIM "|", engine_get_tier_name(ctx->engines[i].tier),
                COLOR_RESET);
        }
    }
}

int engine_get_default_page_count(SearchEngine* engine) {
    return engine->results_per_page > 0 ? engine->results_per_page : 10;
}

char* engine_build_search_url(SearchEngine* engine, SearchQuery* query) {
    static char url[MAX_URL_LENGTH];
    char encoded_query[MAX_QUERY_LENGTH * 3];
    url_encode(query->query, encoded_query, sizeof(encoded_query));

    char lang[8] = "en";
    if (query->language[0]) snprintf(lang, sizeof(lang), "%s", query->language);

    int offset = (query->page) * query->results_per_page;

    char* url_pattern = engine->search_url_encoded;
    if (!url_pattern[0]) {
        int written = snprintf(url, sizeof(url), "%s?%s=%s",
            engine->search_url, engine->query_param, encoded_query);
        if (written < 0) written = 0;
        if (written >= (int)sizeof(url)) written = (int)sizeof(url) - 1;
        if (engine->page_param[0]) {
            snprintf(url + written, sizeof(url) - written, "&%s=%d", engine->page_param, offset);
        }
        return url;
    }

    size_t pos = 0;
    const char* p = url_pattern;
    while (*p && pos < sizeof(url) - 1) {
        if (*p == '{') {
            const char* end = strchr(p, '}');
            if (!end) { url[pos++] = *p++; continue; }
            size_t key_len = end - p - 1;
            char key[32];
            memcpy(key, p + 1, key_len);
            key[key_len] = '\0';

            char val_buf[32];
            if (strcmp(key, "query") == 0) {
                pos += snprintf(url + pos, sizeof(url) - pos, "%s", encoded_query);
            } else if (strcmp(key, "offset") == 0 || strcmp(key, "page") == 0) {
                pos += snprintf(url + pos, sizeof(url) - pos, "%d", offset);
            } else if (strcmp(key, "count") == 0 || strcmp(key, "num") == 0) {
                pos += snprintf(url + pos, sizeof(url) - pos, "%d", query->results_per_page);
            } else if (strcmp(key, "lang") == 0) {
                pos += snprintf(url + pos, sizeof(url) - pos, "%s", lang);
            } else if (strcmp(key, "safe") == 0) {
                pos += snprintf(url + pos, sizeof(url) - pos, "%s",
                    query->safe_search == SAFE_STRICT ? "active" : query->safe_search == SAFE_MODERATE ? "moderate" : "off");
            } else if (strcmp(key, "country") == 0) {
                pos += snprintf(url + pos, sizeof(url) - pos, "%s", query->country[0] ? query->country : "us");
            } else if (strcmp(key, "key") == 0) {
                const char* api_key = getenv(engine->api_key_env);
                pos += snprintf(url + pos, sizeof(url) - pos, "%s", api_key ? api_key : "");
            } else if (strcmp(key, "cx") == 0) {
                const char* cx = getenv("GOOGLE_SEARCH_CX");
                pos += snprintf(url + pos, sizeof(url) - pos, "%s", cx ? cx : "");
            } else {
                pos += snprintf(url + pos, sizeof(url) - pos, "%s", val_buf);
            }
            p = end + 1;
        } else {
            url[pos++] = *p++;
        }
    }
    url[pos] = '\0';
    return url;
}

const char* engine_get_tier_name(EngineTier tier) {
    switch (tier) {
        case ENGINE_TIER_GLOBAL:   return "Global";
        case ENGINE_TIER_REGIONAL: return "Regional";
        case ENGINE_TIER_PRIVACY:  return "Privacy";
        case ENGINE_TIER_ACADEMIC: return "Academic";
        case ENGINE_TIER_META:     return "Meta";
        case ENGINE_TIER_LEGACY:   return "Legacy";
        case ENGINE_TIER_CUSTOM:   return "Custom";
        default:                   return "Unknown";
    }
}

const char* engine_get_tier_icon(EngineTier tier) {
    switch (tier) {
        case ENGINE_TIER_GLOBAL:   return "G";
        case ENGINE_TIER_REGIONAL: return "R";
        case ENGINE_TIER_PRIVACY:  return "P";
        case ENGINE_TIER_ACADEMIC: return "A";
        case ENGINE_TIER_META:     return "M";
        case ENGINE_TIER_LEGACY:   return "L";
        case ENGINE_TIER_CUSTOM:   return "C";
        default:                   return "?";
    }
}

void engine_sort_by_priority(BrowserContext* ctx) {
    qsort(ctx->engines, ctx->engine_count, sizeof(SearchEngine), engine_cmp_by_priority);
}

int engine_get_enabled_count(BrowserContext* ctx) {
    int count = 0;
    for (int i = 0; i < ctx->engine_count; i++)
        if (ctx->engines[i].enabled) count++;
    return count;
}

void engine_add_custom(BrowserContext* ctx, const char* name, const char* url, const char* query_param) {
    if (ctx->engine_count >= MAX_ENGINES) { printf("Engine registry full.\n"); return; }
    SearchEngine* e = &ctx->engines[ctx->engine_count];
    memset(e, 0, sizeof(SearchEngine));
    snprintf(e->name, sizeof(e->name), "%s", name);
    snprintf(e->display_name, sizeof(e->display_name), "%s", name);
    snprintf(e->search_url, sizeof(e->search_url), "%s", url);
    snprintf(e->query_param, sizeof(e->query_param), "%s", query_param);
    e->results_per_page = 10;
    e->enabled = true;
    e->priority = 50;
    e->tier = ENGINE_TIER_CUSTOM;
    e->icon[0] = 'C';
    ctx->engine_count++;
    engine_sort_by_priority(ctx);
}

bool engine_remove_custom(BrowserContext* ctx, const char* name) {
    for (int i = 0; i < ctx->engine_count; i++) {
        if (strcmp(ctx->engines[i].name, name) == 0) {
            memmove(&ctx->engines[i], &ctx->engines[i + 1],
                (ctx->engine_count - i - 1) * sizeof(SearchEngine));
            ctx->engine_count--;
            return true;
        }
    }
    return false;
}

void engine_export_config(BrowserContext* ctx, const char* filepath) {
    FILE* f = fopen(filepath, "w");
    if (!f) { printf("Cannot write to %s\n", filepath); return; }
    fprintf(f, "# CMDBrowser Search Engine Configuration\n");
    fprintf(f, "# Exported: %s\n\n", get_timestamp_str(time(NULL), (char[64]){0}, 64));
    for (int i = 0; i < ctx->engine_count; i++) {
        SearchEngine* e = &ctx->engines[i];
        if (e->tier == ENGINE_TIER_CUSTOM) {
            fprintf(f, "[engine:%s]\n", e->name);
            fprintf(f, "url=%s\n", e->search_url);
            fprintf(f, "query_param=%s\n", e->query_param);
            fprintf(f, "enabled=%s\n", e->enabled ? "true" : "false");
            fprintf(f, "priority=%d\n", e->priority);
            fprintf(f, "\n");
        }
    }
    fclose(f);
    printf("Engine config exported to %s\n", filepath);
}

bool engine_import_config(BrowserContext* ctx, const char* filepath) {
    int count = 0;
    char** lines = file_read_lines(filepath, &count);
    if (!lines) return false;
    char name[128] = {0}, url[256] = {0}, qparam[32] = {0};
    for (int i = 0; i < count; i++) {
        char* line = str_dup(lines[i]);
        trim_whitespace(line);
        if (str_starts_with(line, "[engine:")) {
            const char* e = strchr(line, ':');
            const char* close = strchr(e + 1, ']');
            size_t len = close ? (size_t)(close - e - 1) : strlen(e + 1);
            if (len < sizeof(name)) { memcpy(name, e + 1, len); name[len] = '\0'; }
        } else if (str_starts_with(line, "url=")) {
            snprintf(url, sizeof(url), "%s", line + 4);
        } else if (str_starts_with(line, "query_param=")) {
            snprintf(qparam, sizeof(qparam), "%s", line + 12);
        } else if (name[0] && url[0] && qparam[0]) {
            engine_add_custom(ctx, name, url, qparam);
            name[0] = url[0] = qparam[0] = '\0';
        }
        free(line);
    }
    free_lines(lines, count);
    return true;
}
