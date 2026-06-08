#ifndef TYPES_H
#define TYPES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <wchar.h>

#ifdef _WIN32
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0A00
    #endif
    #include <winsock2.h>
    #include <windows.h>
    #include <winhttp.h>
    #ifdef _MSC_VER
        #pragma comment(lib, "winhttp.lib")
        #pragma comment(lib, "ws2_32.lib")
    #endif
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <pthread.h>
    #define SOCKET int
    #define INVALID_SOCKET (-1)
    #define SOCKET_ERROR (-1)
    #define closesocket close
#endif

#define MAX_URL_LENGTH           2048
#define MAX_QUERY_LENGTH         1024
#define MAX_RESULT_TITLE         512
#define MAX_RESULT_URL           2048
#define MAX_RESULT_SNIPPET       2048
#define MAX_ENGINE_NAME          64
#define MAX_ENGINE_URL           256
#define MAX_PROXY_ADDR           256
#define MAX_CONFIG_LINE          1024
#define MAX_PATH_LENGTH          1024
#define MAX_HISTORY_ENTRIES      10000
#define MAX_BOOKMARK_ENTRIES     5000
#define MAX_CACHE_ENTRIES        1000
#define MAX_ENGINES              64
#define MAX_RESULTS_PER_PAGE     50
#define MAX_HEADERS              64
#define MAX_TABS                  16
#define HTTP_TIMEOUT_MS          30000
#define HTTP_BUFFER_SIZE         65536
#define MAX_CONCURRENT_SEARCHES   8

#define COLOR_RESET          "\033[0m"
#define COLOR_BOLD           "\033[1m"
#define COLOR_DIM            "\033[2m"
#define COLOR_ITALIC         "\033[3m"
#define COLOR_UNDERLINE      "\033[4m"
#define COLOR_BLINK          "\033[5m"
#define COLOR_REVERSE        "\033[7m"
#define COLOR_BLACK          "\033[30m"
#define COLOR_RED            "\033[31m"
#define COLOR_GREEN          "\033[32m"
#define COLOR_YELLOW         "\033[33m"
#define COLOR_BLUE           "\033[34m"
#define COLOR_MAGENTA        "\033[35m"
#define COLOR_CYAN           "\033[36m"
#define COLOR_WHITE          "\033[37m"
#define COLOR_BRIGHT_BLACK   "\033[90m"
#define COLOR_BRIGHT_RED     "\033[91m"
#define COLOR_BRIGHT_GREEN   "\033[92m"
#define COLOR_BRIGHT_YELLOW  "\033[93m"
#define COLOR_BRIGHT_BLUE    "\033[94m"
#define COLOR_BRIGHT_MAGENTA "\033[95m"
#define COLOR_BRIGHT_CYAN    "\033[96m"
#define COLOR_BRIGHT_WHITE   "\033[97m"
#define BG_BLACK             "\033[40m"
#define BG_RED               "\033[41m"
#define BG_GREEN             "\033[42m"
#define BG_YELLOW            "\033[43m"
#define BG_BLUE              "\033[44m"
#define BG_MAGENTA           "\033[45m"
#define BG_CYAN              "\033[46m"
#define BG_WHITE             "\033[47m"

typedef enum {
    ENGINE_TIER_GLOBAL,
    ENGINE_TIER_REGIONAL,
    ENGINE_TIER_PRIVACY,
    ENGINE_TIER_ACADEMIC,
    ENGINE_TIER_META,
    ENGINE_TIER_LEGACY,
    ENGINE_TIER_CUSTOM
} EngineTier;

typedef enum {
    RESULT_TYPE_WEB,
    RESULT_TYPE_IMAGE,
    RESULT_TYPE_VIDEO,
    RESULT_TYPE_NEWS,
    RESULT_TYPE_ACADEMIC,
    RESULT_TYPE_SHOPPING,
    RESULT_TYPE_MAP,
    RESULT_TYPE_CODE,
    RESULT_TYPE_SOCIAL,
    RESULT_TYPE_UNKNOWN
} ResultType;

typedef enum {
    OUTPUT_FORMAT_PLAIN,
    OUTPUT_FORMAT_JSON,
    OUTPUT_FORMAT_CSV,
    OUTPUT_FORMAT_MARKDOWN,
    OUTPUT_FORMAT_HTML,
    OUTPUT_FORMAT_XML,
    OUTPUT_FORMAT_RICH
} OutputFormat;

typedef enum {
    SORT_RELEVANCE,
    SORT_DATE,
    SORT_TITLE,
    SORT_URL,
    SORT_ENGINE
} SortOrder;

typedef enum {
    SAFE_OFF,
    SAFE_MODERATE,
    SAFE_STRICT
} SafeSearchMode;

typedef enum {
    UI_MODE_INTERACTIVE,
    UI_MODE_COMMAND,
    UI_MODE_PIPE,
    UI_MODE_SCRIPT
} UIMode;

typedef enum {
    HTTP_GET,
    HTTP_POST,
    HTTP_HEAD,
    HTTP_PUT,
    HTTP_DELETE
} HttpMethod;

typedef struct {
    char key[256];
    char value[1024];
} HttpHeader;

typedef struct {
    HttpMethod method;
    char url[MAX_URL_LENGTH];
    char host[256];
    int  port;
    char path[MAX_URL_LENGTH];
    bool use_ssl;
    HttpHeader headers[MAX_HEADERS];
    int  header_count;
    char body[HTTP_BUFFER_SIZE];
    int  body_length;
    char proxy[MAX_PROXY_ADDR];
    bool follow_redirects;
    int  timeout_ms;
} HttpRequest;

typedef struct {
    int  status_code;
    char status_text[128];
    HttpHeader headers[MAX_HEADERS];
    int  header_count;
    char* body;
    size_t body_length;
    size_t body_capacity;
    double elapsed_seconds;
    char effective_url[MAX_URL_LENGTH];
} HttpResponse;

typedef struct {
    char title[MAX_RESULT_TITLE];
    char url[MAX_RESULT_URL];
    char snippet[MAX_RESULT_SNIPPET];
    char engine[MAX_ENGINE_NAME];
    ResultType type;
    time_t timestamp;
    float  relevance_score;
    bool   is_advertisement;
    char   thumbnail_url[MAX_URL_LENGTH];
    char   display_url[MAX_URL_LENGTH];
    int64_t content_size;
} SearchResult;

typedef struct {
    char name[MAX_ENGINE_NAME];
    char display_name[MAX_ENGINE_NAME];
    char search_url[MAX_ENGINE_URL];
    char suggest_url[MAX_ENGINE_URL];
    char search_url_encoded[MAX_ENGINE_URL];
    char query_param[32];
    char page_param[32];
    char count_param[32];
    int  results_per_page;
    bool supports_suggestions;
    bool supports_images;
    bool supports_video;
    bool supports_news;
    bool supports_academic;
    bool requires_api_key;
    char api_key_env[64];
    EngineTier tier;
    char country_code[8];
    char language_code[8];
    char region_tag[32];
    char icon[8];
    bool enabled;
    int  priority;
    int  rate_limit_ms;
    SafeSearchMode safe_search_compat;
} SearchEngine;

typedef struct {
    char query[MAX_QUERY_LENGTH];
    SearchEngine** engines;
    int  engine_count;
    int  page;
    int  results_per_page;
    SafeSearchMode safe_search;
    char language[8];
    char country[8];
    char date_range[32];
    ResultType filter_type;
    SortOrder sort;
    time_t max_age_seconds;
    bool exact_phrase;
    char site_filter[MAX_URL_LENGTH];
    char filetype_filter[16];
    bool deduplicate;
} SearchQuery;

typedef struct {
    SearchResult* results;
    int  total_count;
    int  result_count;
    int  page;
    int  total_pages;
    double search_time_seconds;
    SearchEngine* engine;
    char query[MAX_QUERY_LENGTH];
    time_t timestamp;
    int  status_code;
    char error_message[256];
} SearchResponse;

typedef struct {
    char title[MAX_RESULT_TITLE];
    char url[MAX_RESULT_URL];
    time_t timestamp;
    char tags[256];
    int  visit_count;
    char folder[128];
} BookmarkEntry;

typedef struct {
    char query[MAX_QUERY_LENGTH];
    char engine[MAX_ENGINE_NAME];
    time_t timestamp;
    int  result_count;
    bool success;
} HistoryEntry;

typedef struct {
    char key[256];
    char value[1024];
} ConfigPair;

typedef struct {
    ConfigPair* pairs;
    int count;
    int capacity;
    char section[128];
} ConfigSection;

typedef struct {
    ConfigSection* sections;
    int count;
    int capacity;
    char filepath[MAX_PATH_LENGTH];
} ConfigFile;

typedef struct {
    char id[32];
    char query[MAX_QUERY_LENGTH];
    int  page;
    SearchResult* results;
    int  result_count;
    int  total_results;
    int  scroll_offset;
    int  selected_index;
    char active_engine[MAX_ENGINE_NAME];
    time_t created_at;
} BrowserTab;

typedef struct {
    SearchEngine engines[MAX_ENGINES];
    int  engine_count;
    BrowserTab tabs[MAX_TABS];
    int  tab_count;
    int  active_tab;
    ConfigFile config;
    HistoryEntry* history;
    int  history_count;
    int  history_capacity;
    BookmarkEntry* bookmarks;
    int  bookmark_count;
    int  bookmark_capacity;
    UIMode mode;
    char proxy[MAX_PROXY_ADDR];
    char user_agent[256];
    OutputFormat output_format;
    SafeSearchMode default_safe_search;
    bool color_enabled;
    bool show_line_numbers;
    bool auto_dedup;
    bool json_pretty;
    int  max_results;
    int  concurrent_limit;
    int  terminal_width;
    int  terminal_height;
    char export_dir[MAX_PATH_LENGTH];
} BrowserContext;

typedef struct {
    char* data;
    size_t length;
    size_t capacity;
} ByteBuffer;

typedef struct {
    int engines_completed;
    int engines_total;
    int engines_failed;
    SearchResponse** responses;
    pthread_mutex_t lock;
} SearchProgress;

#endif
