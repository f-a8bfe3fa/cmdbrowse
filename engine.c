#include "engine.h"
#include "utils.h"

static const SearchEngine PRESET_ENGINES[] = {
    {
        .name = "google", .display_name = "Google",
        .search_url = "https://www.google.com/search",
        .search_url_encoded = "https://www.google.com/search?q={query}&start={offset}&num={count}&hl={lang}&safe={safe}",
        .query_param = "q", .page_param = "start", .count_param = "num",
        .results_per_page = 10, .supports_suggestions = true, .supports_images = true,
        .supports_video = true, .supports_news = true, .supports_academic = true,
        .requires_api_key = false, .tier = ENGINE_TIER_GLOBAL,
        .country_code = "", .language_code = "en", .region_tag = "global",
        .icon = "G", .enabled = true, .priority = 1, .rate_limit_ms = 500
    },
    {
        .name = "bing", .display_name = "Microsoft Bing",
        .search_url = "https://www.bing.com/search",
        .search_url_encoded = "https://www.bing.com/search?q={query}&first={offset}&count={count}&setlang={lang}",
        .query_param = "q", .page_param = "first", .count_param = "count",
        .results_per_page = 15, .supports_suggestions = true, .supports_images = true,
        .supports_video = true, .supports_news = true, .supports_academic = false,
        .requires_api_key = false, .tier = ENGINE_TIER_GLOBAL,
        .country_code = "", .language_code = "en", .region_tag = "global",
        .icon = "B", .enabled = true, .priority = 2, .rate_limit_ms = 500
    },
    {
        .name = "duckduckgo", .display_name = "DuckDuckGo",
        .search_url = "https://html.duckduckgo.com/html/",
        .search_url_encoded = "https://html.duckduckgo.com/html/?q={query}&s={offset}&dc={count}&kl={lang}",
        .query_param = "q", .page_param = "s", .count_param = "dc",
        .results_per_page = 25, .supports_suggestions = false, .supports_images = true,
        .supports_video = false, .supports_news = true, .supports_academic = false,
        .requires_api_key = false, .tier = ENGINE_TIER_PRIVACY,
        .country_code = "", .language_code = "en", .region_tag = "privacy",
        .icon = "D", .enabled = true, .priority = 3, .rate_limit_ms = 1000
    },
    {
        .name = "yahoo", .display_name = "Yahoo! Search",
        .search_url = "https://search.yahoo.com/search",
        .search_url_encoded = "https://search.yahoo.com/search?p={query}&b={offset}&n={count}&vl={lang}",
        .query_param = "p", .page_param = "b", .count_param = "n",
        .results_per_page = 15, .supports_suggestions = true, .supports_images = true,
        .supports_video = true, .supports_news = true, .supports_academic = false,
        .requires_api_key = false, .tier = ENGINE_TIER_GLOBAL,
        .country_code = "", .language_code = "en", .region_tag = "americas",
        .icon = "Y", .enabled = true, .priority = 5, .rate_limit_ms = 500
    },
    {
        .name = "baidu", .display_name = "Baidu",
        .search_url = "https://www.baidu.com/s",
        .search_url_encoded = "https://www.baidu.com/s?wd={query}&pn={offset}&rn={count}",
        .query_param = "wd", .page_param = "pn", .count_param = "rn",
        .results_per_page = 10, .supports_suggestions = true, .supports_images = true,
        .supports_video = true, .supports_news = true, .supports_academic = true,
        .requires_api_key = false, .tier = ENGINE_TIER_REGIONAL,
        .country_code = "cn", .language_code = "zh", .region_tag = "china",
        .icon = "B", .enabled = false, .priority = 40, .rate_limit_ms = 1000
    },
    {
        .name = "yandex", .display_name = "Yandex",
        .search_url = "https://yandex.com/search/",
        .search_url_encoded = "https://yandex.com/search/?text={query}&p={page}&numdoc={count}&lang={lang}",
        .query_param = "text", .page_param = "p", .count_param = "numdoc",
        .results_per_page = 15, .supports_suggestions = true, .supports_images = true,
        .supports_video = true, .supports_news = true, .supports_academic = false,
        .requires_api_key = false, .tier = ENGINE_TIER_REGIONAL,
        .country_code = "ru", .language_code = "en", .region_tag = "russia",
        .icon = "Y", .enabled = false, .priority = 30, .rate_limit_ms = 1000
    },
    {
        .name = "ecosia", .display_name = "Ecosia",
        .search_url = "https://www.ecosia.org/search",
        .search_url_encoded = "https://www.ecosia.org/search?q={query}&p={page}",
        .query_param = "q", .page_param = "p", .count_param = "",
        .results_per_page = 10, .supports_suggestions = true, .supports_images = true,
        .supports_video = true, .supports_news = false, .supports_academic = false,
        .requires_api_key = false, .tier = ENGINE_TIER_PRIVACY,
        .country_code = "", .language_code = "en", .region_tag = "eco",
        .icon = "E", .enabled = false, .priority = 20, .rate_limit_ms = 1000
    },
    {
        .name = "startpage", .display_name = "Startpage",
        .search_url = "https://www.startpage.com/sp/search",
        .search_url_encoded = "https://www.startpage.com/sp/search?query={query}&page={page}&num={count}&language={lang}",
        .query_param = "query", .page_param = "page", .count_param = "num",
        .results_per_page = 10, .supports_suggestions = false, .supports_images = true,
        .supports_video = true, .supports_news = false, .supports_academic = false,
        .requires_api_key = false, .tier = ENGINE_TIER_PRIVACY,
        .country_code = "", .language_code = "en", .region_tag = "privacy",
        .icon = "S", .enabled = false, .priority = 15, .rate_limit_ms = 1500
    },
    {
        .name = "brave", .display_name = "Brave Search",
        .search_url = "https://search.brave.com/search",
        .search_url_encoded = "https://search.brave.com/search?q={query}&offset={offset}&count={count}",
        .query_param = "q", .page_param = "offset", .count_param = "count",
        .results_per_page = 20, .supports_suggestions = true, .supports_images = true,
        .supports_video = true, .supports_news = true, .supports_academic = false,
        .requires_api_key = false, .tier = ENGINE_TIER_PRIVACY,
        .country_code = "", .language_code = "en", .region_tag = "privacy",
        .icon = "B", .enabled = false, .priority = 10, .rate_limit_ms = 1000
    },
    {
        .name = "searx", .display_name = "SearXNG (Meta)",
        .search_url = "https://searx.be/search",
        .search_url_encoded = "https://searx.be/search?q={query}&pageno={page}&categories=general&language={lang}",
        .query_param = "q", .page_param = "pageno", .count_param = "",
        .results_per_page = 15, .supports_suggestions = true, .supports_images = true,
        .supports_video = true, .supports_news = true, .supports_academic = true,
        .requires_api_key = false, .tier = ENGINE_TIER_META,
        .country_code = "", .language_code = "en", .region_tag = "meta",
        .icon = "S", .enabled = false, .priority = 6, .rate_limit_ms = 1500
    },
    {
        .name = "swisscows", .display_name = "Swisscows",
        .search_url = "https://swisscows.com/web",
        .search_url_encoded = "https://swisscows.com/web?query={query}&offset={offset}&items={count}",
        .query_param = "query", .page_param = "offset", .count_param = "items",
        .results_per_page = 10, .supports_suggestions = true, .supports_images = true,
        .supports_video = false, .supports_news = false, .supports_academic = false,
        .requires_api_key = false, .tier = ENGINE_TIER_PRIVACY,
        .country_code = "", .language_code = "en", .region_tag = "privacy",
        .icon = "S", .enabled = false, .priority = 35, .rate_limit_ms = 1000
    },
    {
        .name = "mojeek", .display_name = "Mojeek",
        .search_url = "https://www.mojeek.com/search",
        .search_url_encoded = "https://www.mojeek.com/search?q={query}&s={offset}&sc={count}&l={lang}",
        .query_param = "q", .page_param = "s", .count_param = "sc",
        .results_per_page = 20, .supports_suggestions = false, .supports_images = false,
        .supports_video = false, .supports_news = false, .supports_academic = false,
        .requires_api_key = false, .tier = ENGINE_TIER_PRIVACY,
        .country_code = "", .language_code = "en", .region_tag = "independent",
        .icon = "M", .enabled = false, .priority = 25, .rate_limit_ms = 1500
    },
    {
        .name = "ask", .display_name = "Ask.com",
        .search_url = "https://www.ask.com/web",
        .search_url_encoded = "https://www.ask.com/web?q={query}&page={page}&qsrc=0",
        .query_param = "q", .page_param = "page", .count_param = "",
        .results_per_page = 10, .supports_suggestions = true, .supports_images = false,
        .supports_video = false, .supports_news = false, .supports_academic = false,
        .requires_api_key = false, .tier = ENGINE_TIER_LEGACY,
        .country_code = "", .language_code = "en", .region_tag = "legacy",
        .icon = "A", .enabled = false, .priority = 50, .rate_limit_ms = 1000
    },
    {
        .name = "aol", .display_name = "AOL Search",
        .search_url = "https://search.aol.com/aol/search",
        .search_url_encoded = "https://search.aol.com/aol/search?q={query}&b={offset}&pz={count}",
        .query_param = "q", .page_param = "b", .count_param = "pz",
        .results_per_page = 15, .supports_suggestions = false, .supports_images = false,
        .supports_video = false, .supports_news = false, .supports_academic = false,
        .requires_api_key = false, .tier = ENGINE_TIER_LEGACY,
        .country_code = "", .language_code = "en", .region_tag = "legacy",
        .icon = "A", .enabled = false, .priority = 55, .rate_limit_ms = 1000
    },
    {
        .name = "lycos", .display_name = "Lycos",
        .search_url = "https://search.lycos.com/web/",
        .search_url_encoded = "https://search.lycos.com/web/?q={query}&keyvol=4&page={page}",
        .query_param = "q", .page_param = "page", .count_param = "",
        .results_per_page = 10, .supports_suggestions = false, .supports_images = false,
        .supports_video = false, .supports_news = false, .supports_academic = false,
        .requires_api_key = false, .tier = ENGINE_TIER_LEGACY,
        .country_code = "", .language_code = "en", .region_tag = "legacy",
        .icon = "L", .enabled = false, .priority = 60, .rate_limit_ms = 1000
    },
    {
        .name = "gibiru", .display_name = "Gibiru",
        .search_url = "https://gibiru.com/results.html",
        .search_url_encoded = "https://gibiru.com/results.html?q={query}",
        .query_param = "q", .page_param = "", .count_param = "",
        .results_per_page = 10, .supports_suggestions = false, .supports_images = false,
        .supports_video = false, .supports_news = false, .supports_academic = false,
        .requires_api_key = false, .tier = ENGINE_TIER_PRIVACY,
        .country_code = "", .language_code = "en", .region_tag = "uncensored",
        .icon = "G", .enabled = false, .priority = 45, .rate_limit_ms = 1500
    },
    {
        .name = "qwant", .display_name = "Qwant",
        .search_url = "https://www.qwant.com/",
        .search_url_encoded = "https://www.qwant.com/?q={query}&t=web&p={offset}&language={lang}",
        .query_param = "q", .page_param = "p", .count_param = "",
        .results_per_page = 10, .supports_suggestions = true, .supports_images = true,
        .supports_video = true, .supports_news = true, .supports_academic = false,
        .requires_api_key = false, .tier = ENGINE_TIER_PRIVACY,
        .country_code = "fr", .language_code = "en", .region_tag = "privacy",
        .icon = "Q", .enabled = false, .priority = 15, .rate_limit_ms = 1000
    },
    {
        .name = "scholar", .display_name = "Google Scholar",
        .search_url = "https://scholar.google.com/scholar",
        .search_url_encoded = "https://scholar.google.com/scholar?q={query}&start={offset}&num={count}&hl={lang}",
        .query_param = "q", .page_param = "start", .count_param = "num",
        .results_per_page = 10, .supports_suggestions = false, .supports_images = false,
        .supports_video = false, .supports_news = false, .supports_academic = true,
        .requires_api_key = false, .tier = ENGINE_TIER_ACADEMIC,
        .country_code = "", .language_code = "en", .region_tag = "scholarly",
        .icon = "S", .enabled = false, .priority = 12, .rate_limit_ms = 2000
    },
    {
        .name = "naver", .display_name = "Naver (Korea)",
        .search_url = "https://search.naver.com/search.naver",
        .search_url_encoded = "https://search.naver.com/search.naver?query={query}&where=web&start={offset}",
        .query_param = "query", .page_param = "start", .count_param = "",
        .results_per_page = 10, .supports_suggestions = true, .supports_images = true,
        .supports_video = true, .supports_news = true, .supports_academic = false,
        .requires_api_key = false, .tier = ENGINE_TIER_REGIONAL,
        .country_code = "kr", .language_code = "ko", .region_tag = "korea",
        .icon = "N", .enabled = false, .priority = 35, .rate_limit_ms = 1000
    },
    {
        .name = "seznam", .display_name = "Seznam (Czech)",
        .search_url = "https://search.seznam.cz/",
        .search_url_encoded = "https://search.seznam.cz/?q={query}&count={count}&from={offset}",
        .query_param = "q", .page_param = "from", .count_param = "count",
        .results_per_page = 10, .supports_suggestions = true, .supports_images = true,
        .supports_video = false, .supports_news = false, .supports_academic = false,
        .requires_api_key = false, .tier = ENGINE_TIER_REGIONAL,
        .country_code = "cz", .language_code = "cs", .region_tag = "czech",
        .icon = "S", .enabled = false, .priority = 40, .rate_limit_ms = 1000
    },
    {
        .name = "sogou", .display_name = "Sogou (China)",
        .search_url = "https://www.sogou.com/web",
        .search_url_encoded = "https://www.sogou.com/web?query={query}&page={page}&num={count}",
        .query_param = "query", .page_param = "page", .count_param = "num",
        .results_per_page = 10, .supports_suggestions = true, .supports_images = true,
        .supports_video = false, .supports_news = false, .supports_academic = false,
        .requires_api_key = false, .tier = ENGINE_TIER_REGIONAL,
        .country_code = "cn", .language_code = "zh", .region_tag = "china",
        .icon = "S", .enabled = false, .priority = 45, .rate_limit_ms = 1000
    },
    {
        .name = "mailru", .display_name = "Mail.ru (Russia)",
        .search_url = "https://go.mail.ru/search",
        .search_url_encoded = "https://go.mail.ru/search?q={query}&offset={offset}&count={count}",
        .query_param = "q", .page_param = "offset", .count_param = "count",
        .results_per_page = 10, .supports_suggestions = true, .supports_images = true,
        .supports_video = true, .supports_news = true, .supports_academic = false,
        .requires_api_key = false, .tier = ENGINE_TIER_REGIONAL,
        .country_code = "ru", .language_code = "ru", .region_tag = "russia",
        .icon = "M", .enabled = false, .priority = 40, .rate_limit_ms = 1000
    },
    {
        .name = "dogpile", .display_name = "Dogpile (Meta)",
        .search_url = "https://www.dogpile.com/serp",
        .search_url_encoded = "https://www.dogpile.com/serp?q={query}&page={page}",
        .query_param = "q", .page_param = "page", .count_param = "",
        .results_per_page = 10, .supports_suggestions = false, .supports_images = false,
        .supports_video = false, .supports_news = false, .supports_academic = false,
        .requires_api_key = false, .tier = ENGINE_TIER_META,
        .country_code = "", .language_code = "en", .region_tag = "meta",
        .icon = "D", .enabled = false, .priority = 50, .rate_limit_ms = 1500
    },
    {
        .name = "webcrawler", .display_name = "WebCrawler (Meta)",
        .search_url = "https://www.webcrawler.com/serp",
        .search_url_encoded = "https://www.webcrawler.com/serp?q={query}&page={page}",
        .query_param = "q", .page_param = "page", .count_param = "",
        .results_per_page = 10, .supports_suggestions = false, .supports_images = false,
        .supports_video = false, .supports_news = false, .supports_academic = false,
        .requires_api_key = false, .tier = ENGINE_TIER_META,
        .country_code = "", .language_code = "en", .region_tag = "meta",
        .icon = "W", .enabled = false, .priority = 55, .rate_limit_ms = 1500
    },
    {
        .name = "excite", .display_name = "Excite",
        .search_url = "https://results.excite.com/serp",
        .search_url_encoded = "https://results.excite.com/serp?q={query}&page={page}",
        .query_param = "q", .page_param = "page", .count_param = "",
        .results_per_page = 10, .supports_suggestions = false, .supports_images = false,
        .supports_video = false, .supports_news = false, .supports_academic = false,
        .requires_api_key = false, .tier = ENGINE_TIER_LEGACY,
        .country_code = "", .language_code = "en", .region_tag = "legacy",
        .icon = "E", .enabled = false, .priority = 60, .rate_limit_ms = 1000
    },
    {
        .name = "metacrawler", .display_name = "MetaCrawler (Meta)",
        .search_url = "https://www.metacrawler.com/serp",
        .search_url_encoded = "https://www.metacrawler.com/serp?q={query}&page={page}",
        .query_param = "q", .page_param = "page", .count_param = "",
        .results_per_page = 10, .supports_suggestions = false, .supports_images = false,
        .supports_video = false, .supports_news = false, .supports_academic = false,
        .requires_api_key = false, .tier = ENGINE_TIER_META,
        .country_code = "", .language_code = "en", .region_tag = "meta",
        .icon = "M", .enabled = false, .priority = 55, .rate_limit_ms = 1500
    },
    {
        .name = "givewater", .display_name = "GiveWater",
        .search_url = "https://search.givewater.com/serp",
        .search_url_encoded = "https://search.givewater.com/serp?q={query}&page={page}",
        .query_param = "q", .page_param = "page", .count_param = "",
        .results_per_page = 10, .supports_suggestions = false, .supports_images = false,
        .supports_video = false, .supports_news = false, .supports_academic = false,
        .requires_api_key = false, .tier = ENGINE_TIER_PRIVACY,
        .country_code = "", .language_code = "en", .region_tag = "charity",
        .icon = "W", .enabled = false, .priority = 45, .rate_limit_ms = 1000
    },
    {
        .name = "searchalot", .display_name = "Searchalot",
        .search_url = "https://www.searchalot.com/results.aspx",
        .search_url_encoded = "https://www.searchalot.com/results.aspx?q={query}&page={page}",
        .query_param = "q", .page_param = "page", .count_param = "",
        .results_per_page = 10, .supports_suggestions = false, .supports_images = false,
        .supports_video = false, .supports_news = false, .supports_academic = false,
        .requires_api_key = false, .tier = ENGINE_TIER_LEGACY,
        .country_code = "", .language_code = "en", .region_tag = "legacy",
        .icon = "S", .enabled = false, .priority = 60, .rate_limit_ms = 1000
    },
    {
        .name = "brave_api", .display_name = "Brave Search API",
        .search_url = "https://api.search.brave.com/res/v1/web/search",
        .search_url_encoded = "https://api.search.brave.com/res/v1/web/search?q={query}&count={count}&offset={offset}&search_lang={lang}&safesearch={safe}",
        .query_param = "q", .page_param = "offset", .count_param = "count",
        .results_per_page = 20, .supports_suggestions = false, .supports_images = false,
        .supports_video = false, .supports_news = false, .supports_academic = false,
        .requires_api_key = true, .api_key_env = "BRAVE_API_KEY", .tier = ENGINE_TIER_GLOBAL,
        .country_code = "", .language_code = "en", .region_tag = "global",
        .icon = "B", .enabled = true, .priority = 1, .rate_limit_ms = 200
    },
    {
        .name = "ddg_api", .display_name = "DuckDuckGo API",
        .search_url = "https://api.duckduckgo.com/",
        .search_url_encoded = "https://api.duckduckgo.com/?q={query}&format=json",
        .query_param = "q", .page_param = "", .count_param = "",
        .results_per_page = 10, .supports_suggestions = false, .supports_images = false,
        .supports_video = false, .supports_news = false, .supports_academic = false,
        .requires_api_key = false, .tier = ENGINE_TIER_PRIVACY,
        .country_code = "", .language_code = "en", .region_tag = "privacy",
        .icon = "D", .enabled = true, .priority = 2, .rate_limit_ms = 500
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
