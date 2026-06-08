#include "types.h"
#include "utils.h"
#include "http.h"
#include "engine.h"
#include "parser.h"
#include "config.h"
#include "history.h"
#include "bookmarks.h"
#include "cache.h"
#include "ui.h"

static BrowserContext g_browser;

static void browser_init(BrowserContext* ctx) {
    memset(ctx, 0, sizeof(BrowserContext));
    ctx->mode = UI_MODE_INTERACTIVE;
    ctx->color_enabled = true;
    ctx->show_line_numbers = false;
    ctx->auto_dedup = true;
    ctx->json_pretty = true;
    ctx->max_results = 50;
    ctx->concurrent_limit = 4;
    ctx->output_format = OUTPUT_FORMAT_RICH;
    ctx->default_safe_search = SAFE_MODERATE;
    snprintf(ctx->user_agent, sizeof(ctx->user_agent), "%s", http_get_default_user_agent());

    engine_registry_init(ctx);
    ui_init(ctx);

    char config_path[MAX_PATH_LENGTH];
    snprintf(config_path, sizeof(config_path), "%s\\config.ini", get_config_dir());
    ensure_directory(get_config_dir());

    if (file_exists(config_path)) {
        config_load(ctx, config_path);
    }

    ctx->color_enabled = config_get_bool(ctx, "display", "color", true);
    ctx->max_results = config_get_int(ctx, "display", "max_results", 50);
    ctx->concurrent_limit = config_get_int(ctx, "network", "concurrent_limit", 4);

    char* proxy_val = config_get(ctx, "network", "proxy", "");
    if (proxy_val[0]) snprintf(ctx->proxy, sizeof(ctx->proxy), "%s", proxy_val);

    char* format_val = config_get(ctx, "display", "output_format", "rich");
    if (strcmp(format_val, "json") == 0) ctx->output_format = OUTPUT_FORMAT_JSON;
    else if (strcmp(format_val, "csv") == 0) ctx->output_format = OUTPUT_FORMAT_CSV;
    else if (strcmp(format_val, "markdown") == 0) ctx->output_format = OUTPUT_FORMAT_MARKDOWN;
    else if (strcmp(format_val, "html") == 0) ctx->output_format = OUTPUT_FORMAT_HTML;
    else if (strcmp(format_val, "rich") == 0) ctx->output_format = OUTPUT_FORMAT_RICH;
    else ctx->output_format = OUTPUT_FORMAT_PLAIN;

    history_load(ctx);
    bookmarks_load(ctx);
}

static void browser_save_all(BrowserContext* ctx) {
    char config_path[MAX_PATH_LENGTH];
    snprintf(config_path, sizeof(config_path), "%s\\config.ini", get_config_dir());
    ensure_directory(get_config_dir());

    config_set(ctx, "display", "color", ctx->color_enabled ? "true" : "false");
    config_set(ctx, "display", "max_results", NULL);
    char mr_buf[16]; snprintf(mr_buf, sizeof(mr_buf), "%d", ctx->max_results);
    config_set(ctx, "display", "max_results", mr_buf);
    config_set(ctx, "network", "concurrent_limit", NULL);
    char cl_buf[16]; snprintf(cl_buf, sizeof(cl_buf), "%d", ctx->concurrent_limit);
    config_set(ctx, "network", "concurrent_limit", cl_buf);
    config_set(ctx, "network", "proxy", ctx->proxy);

    const char* fmt_names[] = {"plain","json","csv","markdown","html","xml","rich"};
    config_set(ctx, "display", "output_format", fmt_names[ctx->output_format]);

    config_save(ctx, config_path);
    history_save(ctx);
    bookmarks_save(ctx);

    ui_render_success("All data saved.");
}

static SearchResponse* do_search(BrowserContext* ctx, const char* query, const char* engine_name, int page) {
    if (!query || !query[0]) return NULL;

    SearchEngine* engine = engine_get_by_name(ctx, engine_name);
    if (!engine) {
        ui_render_error("Engine not found.");
        return NULL;
    }
    if (!engine->enabled && engine->tier != ENGINE_TIER_CUSTOM) {
        ui_render_warning("Engine is disabled. Use /engines on to enable it.");
        return NULL;
    }

    SearchResponse* cached = NULL;
    if (cache_lookup(ctx, query, engine_name, page, &cached)) {
        return cached;
    }

    SearchQuery sq;
    memset(&sq, 0, sizeof(sq));
    snprintf(sq.query, sizeof(sq.query), "%s", query);
    sq.engines = &engine;
    sq.engine_count = 1;
    sq.page = page;
    sq.results_per_page = engine_get_default_page_count(engine);
    sq.safe_search = ctx->default_safe_search;
    sq.deduplicate = ctx->auto_dedup;

    char* search_url = engine_build_search_url(engine, &sq);
    if (!search_url) return NULL;

    HttpRequest req;
    http_request_init(&req);
    snprintf(req.url, sizeof(req.url), "%s", search_url);
    if (ctx->proxy[0]) snprintf(req.proxy, sizeof(req.proxy), "%s", ctx->proxy);

    http_add_header(&req, "User-Agent", ctx->user_agent);
    http_add_header(&req, "Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
    http_add_header(&req, "Accept-Language", "en-US,en;q=0.9");

    if (strcmp(engine_name, "duckduckgo") == 0) {
        snprintf(req.url, sizeof(req.url), "%s", search_url);
    }

    HttpResponse http_resp;
    http_response_init(&http_resp);

    bool http_ok = false;
    if (engine->requires_api_key && engine->api_key_env[0]) {
        const char* api_key = getenv(engine->api_key_env);
        if (api_key && api_key[0]) {
            const char* header_name = "X-Subscription-Token";
            if (strcmp(engine->name, "bing_api") == 0) {
                header_name = "Ocp-Apim-Subscription-Key";
                http_ok = http_execute_with_api_key(&req, &http_resp, header_name, api_key);
            } else if (strcmp(engine->name, "tavily") == 0) {
                header_name = "Authorization";
                char bearer[1024];
                snprintf(bearer, sizeof(bearer), "Bearer %s", api_key);
                http_ok = http_execute_with_api_key(&req, &http_resp, header_name, bearer);
            } else {
                http_ok = http_execute_with_api_key(&req, &http_resp, header_name, api_key);
            }
        } else {
            http_response_free(&http_resp);
            SearchResponse* err = (SearchResponse*)calloc(1, sizeof(SearchResponse));
            if (err) { snprintf(err->error_message, sizeof(err->error_message),
                "API key not set. Set %s environment variable.", engine->api_key_env); err->status_code = -1; }
            return err;
        }
    } else {
        http_ok = http_execute(&req, &http_resp);
    }

    if (!http_ok) {
        http_response_free(&http_resp);
        SearchResponse* err = (SearchResponse*)calloc(1, sizeof(SearchResponse));
        if (err) { snprintf(err->error_message, sizeof(err->error_message), "Network error"); err->status_code = -1; }
        return err;
    }

    if (http_resp.status_code == 403 || http_resp.status_code == 429) {
        sleep_ms(engine->rate_limit_ms * 2);
    }

    SearchResponse* resp = parser_parse_response(engine, &sq, &http_resp);
    http_response_free(&http_resp);

    if (resp && resp->result_count > 0) {
        if (sq.deduplicate) {
            resp->result_count = parser_dedup_results(resp->results, resp->result_count);
        }
        cache_store(ctx, query, engine_name, page, resp);
    }

    history_add(ctx, query, engine_name, resp ? resp->result_count : 0, resp && resp->status_code >= 200 && resp->status_code < 400);
    return resp;
}

static SearchResponse* do_multi_search(BrowserContext* ctx, const char* query, int page) {
    if (!query || !query[0]) return NULL;

    int enabled_count = engine_get_enabled_count(ctx);
    if (enabled_count <= 0) {
        ui_render_error("No search engines enabled. Use /engines on <name> to enable.");
        return NULL;
    }

    SearchResponse** responses = (SearchResponse**)calloc(enabled_count, sizeof(SearchResponse*));
    if (!responses) return NULL;

    int completed = 0, failed = 0;

    int r_idx = 0;
    for (int i = 0; i < ctx->engine_count && r_idx < enabled_count; i++) {
        if (ctx->engines[i].enabled) {
            ui_render_info("Searching engine...");
            SearchResponse* r = do_search(ctx, query, ctx->engines[i].name, page);
            if (r && r->result_count > 0) {
                responses[r_idx] = r;
                completed++;
            } else {
                if (r) {
                    if (r->status_code > 0 && r->status_code < 500) {
                        responses[r_idx] = r;
                        completed++;
                    } else {
                        search_response_free(r);
                        failed++;
                    }
                } else {
                    failed++;
                }
            }
            r_idx++;
        }
    }

    SearchResponse* merged = search_merge_responses(responses, enabled_count, SORT_RELEVANCE);
    if (merged) {
        merged->engine = NULL;
        merged->search_time_seconds = 0;
        merged->total_count = merged->result_count;
        for (int i = 0; i < enabled_count; i++) {
            if (responses[i]) {
                merged->search_time_seconds += responses[i]->search_time_seconds;
                merged->total_count += responses[i]->total_count;
                search_response_free(responses[i]);
            }
        }
    }
    free(responses);

    if (merged && merged->result_count <= 0 && merged->error_message[0]) {
        ui_render_error(merged->error_message);
    }

    history_add(ctx, query, "all", merged ? merged->result_count : 0, merged && merged->result_count > 0);
    return merged;
}

static bool parse_bool(const char* str) {
    if (!str) return false;
    char lower[16]; snprintf(lower, sizeof(lower), "%s", str); str_to_lower(lower);
    return strcmp(lower, "on") == 0 || strcmp(lower, "true") == 0 || strcmp(lower, "1") == 0 || strcmp(lower, "yes") == 0;
}

static void handle_command(BrowserContext* ctx, char* input) {
    trim_whitespace(input);
    if (!input[0]) return;

    char* cmd = str_dup(input);
    char* argc[16] = {0};
    int argn = 0;

    char* token = strtok(cmd, " \t");
    while (token && argn < 15) {
        argc[argn++] = token;
        token = strtok(NULL, " \t");
    }

    if (argn == 0) { free(cmd); return; }

    char* command = argc[0];
    str_to_lower(command);

    if (strcmp(command, "/quit") == 0 || strcmp(command, "/exit") == 0 || strcmp(command, "/q") == 0) {
        free(cmd);
        browser_save_all(ctx);
        exit(0);
    }
    else if (strcmp(command, "/help") == 0 || strcmp(command, "/?") == 0 || strcmp(command, "help") == 0) {
        ui_render_help();
    }
    else if (strcmp(command, "/about") == 0) {
        print_banner();
        printf(COLOR_DIM "  CMDBrowser v2.0 - Multi-Engine CLI Web Search Tool\n");
        printf("  Built with C11 + WinHTTP | %d search engines | Color terminal UI\n", ctx->engine_count);
        printf("  Platform: Windows x64 | Compiler: MSVC/GCC\n" COLOR_RESET "\n");
    }
    else if (strcmp(command, "/clear") == 0 || strcmp(command, "cls") == 0) {
        ui_clear_screen();
    }
    else if (strcmp(command, "/save") == 0) {
        browser_save_all(ctx);
    }
    else if (strcmp(command, "/search") == 0 || strcmp(command, "/s") == 0 || strcmp(command, "?") == 0) {
        if (argn < 2) { ui_render_error("Usage: /search <query>"); free(cmd); return; }
        char query[MAX_QUERY_LENGTH] = {0};
        for (int i = 1; i < argn; i++) {
            if (i > 1) strcat(query, " ");
            strcat(query, argc[i]);
        }
        ui_render_info("Searching...");
        SearchResponse* resp = do_multi_search(ctx, query, 0);
        ui_render_output_formatted(ctx, resp);
        search_response_free(resp);
    }
    else if (strcmp(command, "/mega") == 0 || strcmp(command, "/all") == 0) {
        if (argn < 2) { ui_render_error("Usage: /mega <query>"); free(cmd); return; }
        int orig_concurrent = ctx->concurrent_limit;
        ctx->concurrent_limit = MAX_CONCURRENT_SEARCHES;
        int orig_enabled_count = 0;
        for (int i = 0; i < ctx->engine_count; i++) {
            if (ctx->engines[i].enabled) orig_enabled_count++;
            ctx->engines[i].enabled = true;
        }
        char query[MAX_QUERY_LENGTH] = {0};
        for (int i = 1; i < argn; i++) {
            if (i > 1) strcat(query, " ");
            strcat(query, argc[i]);
        }
        ui_render_info("MEGA search across ALL engines...");
        SearchResponse* resp = do_multi_search(ctx, query, 0);
        ui_render_output_formatted(ctx, resp);
        search_response_free(resp);
        ctx->concurrent_limit = orig_concurrent;
    }
    else if (strcmp(command, "/google") == 0 || strcmp(command, "/g") == 0) {
        if (argn < 2) { ui_render_error("Usage: /google <query>"); free(cmd); return; }
        char query[MAX_QUERY_LENGTH] = {0};
        for (int i = 1; i < argn; i++) { if (i > 1) strcat(query, " "); strcat(query, argc[i]); }
        SearchResponse* resp = do_search(ctx, query, "google", 0);
        ui_render_output_formatted(ctx, resp);
        search_response_free(resp);
    }
    else if (strcmp(command, "/bing") == 0 || strcmp(command, "/b") == 0) {
        if (argn < 2) { ui_render_error("Usage: /bing <query>"); free(cmd); return; }
        char query[MAX_QUERY_LENGTH] = {0};
        for (int i = 1; i < argn; i++) { if (i > 1) strcat(query, " "); strcat(query, argc[i]); }
        SearchResponse* resp = do_search(ctx, query, "bing", 0);
        ui_render_output_formatted(ctx, resp);
        search_response_free(resp);
    }
    else if (strcmp(command, "/ddg") == 0) {
        if (argn < 2) { ui_render_error("Usage: /ddg <query>"); free(cmd); return; }
        char query[MAX_QUERY_LENGTH] = {0};
        for (int i = 1; i < argn; i++) { if (i > 1) strcat(query, " "); strcat(query, argc[i]); }
        SearchResponse* resp = do_search(ctx, query, "duckduckgo", 0);
        ui_render_output_formatted(ctx, resp);
        search_response_free(resp);
    }
    else if (strcmp(command, "/yahoo") == 0) {
        if (argn < 2) { ui_render_error("Usage: /yahoo <query>"); free(cmd); return; }
        char query[MAX_QUERY_LENGTH] = {0};
        for (int i = 1; i < argn; i++) { if (i > 1) strcat(query, " "); strcat(query, argc[i]); }
        SearchResponse* resp = do_search(ctx, query, "yahoo", 0);
        ui_render_output_formatted(ctx, resp);
        search_response_free(resp);
    }
    else if (strcmp(command, "/engine") == 0 || strcmp(command, "/e") == 0) {
        if (argn < 3) { ui_render_error("Usage: /engine <name> <query>"); free(cmd); return; }
        char* eng_name = argc[1];
        char query[MAX_QUERY_LENGTH] = {0};
        for (int i = 2; i < argn; i++) { if (i > 2) strcat(query, " "); strcat(query, argc[i]); }
        SearchResponse* resp = do_search(ctx, query, eng_name, 0);
        ui_render_output_formatted(ctx, resp);
        search_response_free(resp);
    }
    else if (strcmp(command, "/engines") == 0) {
        if (argn < 2) {
            ui_render_engine_list(ctx);
        } else if (strcmp(argc[1], "on") == 0 && argn >= 3) {
            engine_set_enabled(ctx, argc[2], true);
            ui_render_success("Engine enabled.");
        } else if (strcmp(argc[1], "off") == 0 && argn >= 3) {
            engine_set_enabled(ctx, argc[2], false);
            ui_render_success("Engine disabled.");
        } else if (strcmp(argc[1], "add") == 0 && argn >= 5) {
            engine_add_custom(ctx, argc[2], argc[3], argc[4]);
            ui_render_success("Custom engine added.");
        } else if (strcmp(argc[1], "remove") == 0 && argn >= 3) {
            if (engine_remove_custom(ctx, argc[2]))
                ui_render_success("Engine removed.");
            else
                ui_render_error("Engine not found or cannot remove preset engine.");
        } else {
            ui_render_engine_list(ctx);
        }
    }
    else if (strcmp(command, "/history") == 0 || strcmp(command, "/h") == 0) {
        if (argn < 2) {
            history_show(ctx, 20);
        } else if (strcmp(argc[1], "search") == 0 && argn >= 3) {
            history_search(ctx, argc[2]);
        } else if (strcmp(argc[1], "clear") == 0) {
            history_clear(ctx);
        } else if (strcmp(argc[1], "stats") == 0) {
            history_stats(ctx);
        } else if (strcmp(argc[1], "export") == 0 && argn >= 3) {
            history_export(ctx, argc[2], OUTPUT_FORMAT_CSV);
        } else {
            int limit = atoi(argc[1]);
            history_show(ctx, limit > 0 ? limit : 20);
        }
    }
    else if (strcmp(command, "/bookmark") == 0 || strcmp(command, "/bkm") == 0) {
        if (argn < 2) {
            bookmarks_list(ctx, NULL, NULL);
        } else if (strcmp(argc[1], "add") == 0 && argn >= 4) {
            bookmarks_add(ctx, argc[2], argc[3], argn >= 5 ? argc[4] : "", "general");
            ui_render_success("Bookmark added.");
        } else if (strcmp(argc[1], "remove") == 0 && argn >= 3) {
            bookmarks_remove(ctx, atoi(argc[2]) - 1);
        } else if (strcmp(argc[1], "list") == 0) {
            bookmarks_list(ctx, argn >= 3 ? argc[2] : NULL, argn >= 4 ? argc[3] : NULL);
        } else if (strcmp(argc[1], "search") == 0 && argn >= 3) {
            bookmarks_search(ctx, argc[2]);
        } else if (strcmp(argc[1], "export") == 0 && argn >= 3) {
            bookmarks_export(ctx, argc[2], OUTPUT_FORMAT_HTML);
        } else if (strcmp(argc[1], "import") == 0 && argn >= 3) {
            bookmarks_import_html(ctx, argc[2]);
        } else {
            bookmarks_list(ctx, NULL, NULL);
        }
    }
    else if (strcmp(command, "/cache") == 0) {
        if (argn < 2) {
            cache_stats(ctx);
        } else if (strcmp(argc[1], "stats") == 0) {
            cache_stats(ctx);
        } else if (strcmp(argc[1], "clear") == 0) {
            cache_clear(ctx);
        } else if (strcmp(argc[1], "cleanup") == 0) {
            cache_cleanup_expired(ctx, 3600);
        }
    }
    else if (strcmp(command, "/format") == 0) {
        if (argn < 2) {
            const char* fmt_names[] = {"plain","json","csv","markdown","html","xml","rich"};
            printf("Current format: %s\nAvailable: plain, json, csv, markdown, html, rich\n", fmt_names[ctx->output_format]);
        } else {
            if (strcmp(argc[1], "json") == 0) ctx->output_format = OUTPUT_FORMAT_JSON;
            else if (strcmp(argc[1], "csv") == 0) ctx->output_format = OUTPUT_FORMAT_CSV;
            else if (strcmp(argc[1], "markdown") == 0 || strcmp(argc[1], "md") == 0) ctx->output_format = OUTPUT_FORMAT_MARKDOWN;
            else if (strcmp(argc[1], "html") == 0) ctx->output_format = OUTPUT_FORMAT_HTML;
            else if (strcmp(argc[1], "rich") == 0) ctx->output_format = OUTPUT_FORMAT_RICH;
            else if (strcmp(argc[1], "plain") == 0) ctx->output_format = OUTPUT_FORMAT_PLAIN;
            ui_render_success("Output format set.");
        }
    }
    else if (strcmp(command, "/proxy") == 0) {
        if (argn < 2) {
            memset(ctx->proxy, 0, sizeof(ctx->proxy));
            ui_render_success("Proxy cleared.");
        } else {
            snprintf(ctx->proxy, sizeof(ctx->proxy), "%s", argc[1]);
            ui_render_success("Proxy set.");
        }
    }
    else if (strcmp(command, "/lang") == 0) {
        if (argn < 2) {
            ui_render_info("Usage: /lang <code> (e.g., en, zh, fr, de, ja, ko, ru)");
        } else {
            ui_render_success("Language set.");
        }
    }
    else if (strcmp(command, "/safe") == 0) {
        if (argn < 2) {
            const char* levels[] = {"off","moderate","strict"};
            printf("Current safe search: %s\n", levels[ctx->default_safe_search]);
        } else if (strcmp(argc[1], "off") == 0) { ctx->default_safe_search = SAFE_OFF; ui_render_success("Safe search: OFF"); }
        else if (strcmp(argc[1], "mod") == 0) { ctx->default_safe_search = SAFE_MODERATE; ui_render_success("Safe search: MODERATE"); }
        else if (strcmp(argc[1], "strict") == 0) { ctx->default_safe_search = SAFE_STRICT; ui_render_success("Safe search: STRICT"); }
    }
    else if (strcmp(command, "/color") == 0) {
        if (argn < 2) {
            ctx->color_enabled = !ctx->color_enabled;
        } else {
            ctx->color_enabled = parse_bool(argc[1]);
        }
        if (ctx->color_enabled) enable_ansi_escapes(); else disable_ansi_escapes();
        printf("Color output: %s\n", ctx->color_enabled ? "ON" : "OFF");
    }
    else if (strcmp(command, "/lines") == 0) {
        ctx->show_line_numbers = argn >= 2 ? parse_bool(argc[1]) : !ctx->show_line_numbers;
        printf("Line numbers: %s\n", ctx->show_line_numbers ? "ON" : "OFF");
    }
    else if (strcmp(command, "/max") == 0) {
        if (argn < 2) {
            printf("Max results: %d\n", ctx->max_results);
        } else {
            ctx->max_results = atoi(argc[1]);
            if (ctx->max_results < 1) ctx->max_results = 1;
            if (ctx->max_results > 200) ctx->max_results = 200;
            ui_render_success("Max results set.");
        }
    }
    else if (strcmp(command, "/config") == 0) {
        if (argn < 2) {
            config_list_sections(ctx);
        } else if (strcmp(argc[1], "get") == 0 && argn >= 4) {
            char* val = config_get(ctx, argc[2], argc[3], "");
            printf("%s\n", val);
        } else if (strcmp(argc[1], "set") == 0 && argn >= 5) {
            config_set(ctx, argc[2], argc[3], argc[4]);
            ui_render_success("Config value set.");
        } else if (strcmp(argc[1], "list") == 0) {
            config_list_sections(ctx);
        } else if (strcmp(argc[1], "keys") == 0 && argn >= 3) {
            config_list_keys(ctx, argc[2]);
        }
    }
    else if (strcmp(command, "/export") == 0) {
        if (argn < 2) {
            ui_render_error("Usage: /export <filepath>");
        } else {
            ui_render_info("Export not implemented in this context. Use /format + redirect.");
        }
    }
    else if (strcmp(command, "/open") == 0) {
        if (argn < 2) {
            ui_render_error("Usage: /open <url>");
        } else {
            char cmd_open[MAX_URL_LENGTH + 16];
            snprintf(cmd_open, sizeof(cmd_open), "start \"\" \"%s\"", argc[1]);
            system(cmd_open);
            ui_render_success("Opening in browser...");
        }
    }
    else if (strcmp(command, "/pager") == 0 || strcmp(command, "/page") == 0) {
        ui_render_info("Interactive pager available after search results.");
    }
    else {
        if (input[0] == '/' || input[0] == '?' || input[0] == '.') {
            ui_render_error("Unknown command. Type /help for available commands.");
        } else {
            ui_render_info("Searching...");
            SearchResponse* resp = do_multi_search(ctx, input, 0);
            ui_render_output_formatted(ctx, resp);
            search_response_free(resp);
        }
    }

    free(cmd);
}

static void interactive_mode(BrowserContext* ctx) {
    ui_clear_screen();
    ui_render_welcome();
    ui_render_header(ctx);

    char input[4096];

    for (;;) {
        ui_render_footer(ctx);
        printf(COLOR_BRIGHT_GREEN "  >>> " COLOR_RESET);

        if (!fgets(input, sizeof(input), stdin)) break;
        trim_whitespace(input);

        if (strlen(input) == 0) continue;

        handle_command(ctx, input);
    }
}

static void command_mode(BrowserContext* ctx, const char* query) {
    if (query && query[0]) {
        if (query[0] == '/') {
            handle_command(ctx, (char*)query);
        } else {
            ui_render_info("Searching...");
            SearchResponse* resp = do_multi_search(ctx, query, 0);
            ui_render_output_formatted(ctx, resp);
            search_response_free(resp);
        }
    }
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    memset(&g_browser, 0, sizeof(g_browser));
    browser_init(&g_browser);

    if (!http_init()) {
        fprintf(stderr, "ERROR: Failed to initialize HTTP subsystem.\n");
        fprintf(stderr, "Please check your network connection.\n");
        return 1;
    }

    if (argc > 1) {
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            printf("Usage: cmdbrowser [query] or [command]\n");
            printf("  cmdbrowser                  Interactive shell mode\n");
            printf("  cmdbrowser \"search query\"   Search and display results\n");
            printf("  cmdbrowser /search query    Run a command\n");
            printf("  cmdbrowser --help           This help\n");
            http_cleanup();
            return 0;
        }
        g_browser.mode = UI_MODE_COMMAND;

        char query[4096] = {0};
        for (int i = 1; i < argc; i++) {
            if (i > 1) strcat(query, " ");
            strcat(query, argv[i]);
        }
        command_mode(&g_browser, query);
    } else {
        g_browser.mode = UI_MODE_INTERACTIVE;
        interactive_mode(&g_browser);
    }

    browser_save_all(&g_browser);
    http_cleanup();

    free(g_browser.config.sections);
    free(g_browser.history);
    free(g_browser.bookmarks);

    return 0;
}
