#include "ui.h"
#include "utils.h"
#include "engine.h"
#include "bookmarks.h"
#include "cache.h"

static BrowserContext* g_ctx = NULL;

void ui_init(BrowserContext* ctx) {
    g_ctx = ctx;
    ctx->terminal_width = get_terminal_width();
    ctx->terminal_height = get_terminal_height();
    if (ctx->color_enabled) enable_ansi_escapes();
}

void ui_render_header(BrowserContext* ctx) {
    int w = get_terminal_width();
    printf(COLOR_BRIGHT_CYAN COLOR_BOLD);
    printf("  CMDBrowser v2.0");
    printf(COLOR_RESET);

    int enabled = engine_get_enabled_count(ctx);
    char eng_status[64];
    snprintf(eng_status, sizeof(eng_status), "%d engines active", enabled);
    int pos = 18;
    while (pos < w - (int)strlen(eng_status) - 2) { printf(" "); pos++; }
    printf(COLOR_BRIGHT_GREEN "%s" COLOR_RESET, eng_status);

    printf("\n" COLOR_DIM);
    for (int i = 0; i < w; i++) printf("-");
    printf(COLOR_RESET "\n");
}

void ui_render_footer(BrowserContext* ctx) {
    int w = get_terminal_width();
    printf(COLOR_DIM);
    for (int i = 0; i < w; i++) printf("-");
    printf(COLOR_RESET "\n");
    printf(COLOR_BRIGHT_BLACK "  [/help] Commands  [/engines] Engines  [/history] History  [/bkm] Bookmarks  [/quit] Exit");
    printf(COLOR_RESET);

    int bkm_count = bookmarks_count(ctx);
    int hist_count = ctx->history ? ctx->history_count : 0;
    int cache_count_val = cache_count(ctx);
    char stats[128];
    snprintf(stats, sizeof(stats), "H:%d B:%d C:%d", hist_count, bkm_count, cache_count_val);
    int pos = 80;
    for (; pos < w - (int)strlen(stats) - 2; pos++) printf(" ");
    printf(COLOR_DIM "%s" COLOR_RESET, stats);

    printf("\n");
}

void ui_render_results(BrowserContext* ctx, SearchResponse* resp) {
    if (!resp) { printf(COLOR_DIM "No results.\n" COLOR_RESET); return; }

    char num_buf[32];

    printf("\n" COLOR_BRIGHT_WHITE COLOR_BOLD "  Search Results" COLOR_RESET);
    if (resp->engine)
        printf(COLOR_BRIGHT_GREEN " via " COLOR_BRIGHT_CYAN "%s" COLOR_RESET, resp->engine->display_name);

    printf(COLOR_DIM);
    if (resp->engine)
        printf(" [%s]", engine_get_tier_name(resp->engine->tier));
    if (resp->total_count > 0)
        printf(" | %s results", format_number(resp->total_count, num_buf, sizeof(num_buf)));
    if (resp->search_time_seconds > 0)
        printf(" | %.2fs", resp->search_time_seconds);
    printf(COLOR_RESET "\n\n");

    for (int i = 0; i < resp->result_count && i < ctx->max_results; i++) {
        ui_render_result_detail(&resp->results[i], i + 1);
        printf("\n");
    }

    if (resp->result_count == 0) {
        printf(COLOR_DIM "  No results found for this query.\n" COLOR_RESET "\n");
    }

    if (resp->total_pages > 0) {
        printf(COLOR_DIM "  Page %d of %d  |  %d results shown\n" COLOR_RESET,
            resp->page + 1, resp->total_pages,
            resp->result_count > ctx->max_results ? ctx->max_results : resp->result_count);
    }

    printf(COLOR_BRIGHT_BLACK "  Use /page <n> to navigate | /next | /prev | /open <n> | /bookmark <n>\n" COLOR_RESET "\n");
}

void ui_render_merged_results(BrowserContext* ctx, SearchResponse* resp) {
    if (!resp) { printf(COLOR_DIM "No results.\n" COLOR_RESET); return; }
    char num_buf[32];

    printf("\n" COLOR_BRIGHT_WHITE COLOR_BOLD "  Aggregated Results" COLOR_RESET);
    printf(COLOR_BRIGHT_MAGENTA " from %d engines" COLOR_RESET, engine_get_enabled_count(ctx));
    if (resp->search_time_seconds > 0)
        printf(COLOR_DIM " | %.2fs" COLOR_RESET, resp->search_time_seconds);
    printf("\n\n");

    for (int i = 0; i < resp->result_count && i < ctx->max_results; i++) {
        SearchResult* r = &resp->results[i];
        printf("  " COLOR_BRIGHT_YELLOW "[%3d]" COLOR_RESET " ", i + 1);

        if (ctx->show_line_numbers) {
            printf(COLOR_BRIGHT_BLACK "%-2d " COLOR_RESET, i + 1);
        }

        printf(COLOR_BRIGHT_CYAN COLOR_BOLD "%s" COLOR_RESET "\n", r->title);
        printf("       " COLOR_BRIGHT_BLUE "%s" COLOR_RESET "\n", r->url);
        printf("       " COLOR_DIM "%s" COLOR_RESET, r->snippet);

        if (ctx->color_enabled) {
            printf("  " COLOR_BRIGHT_GREEN "[%s]" COLOR_RESET, r->engine);
        } else {
            printf("  [%s]", r->engine);
        }

        if (r->is_advertisement)
            printf(COLOR_BRIGHT_YELLOW " [AD]" COLOR_RESET);

        printf("\n\n");
    }

    if (resp->result_count == 0) {
        printf(COLOR_DIM "  No results found.\n" COLOR_RESET "\n");
    }

    printf(COLOR_BRIGHT_BLACK "  %s results aggregated | Use /open <n> to visit | /bookmark <n> to save\n" COLOR_RESET "\n",
        format_number(resp->result_count, num_buf, sizeof(num_buf)));
}

void ui_render_result_detail(SearchResult* result, int index) {
    int w = get_terminal_width();
    int num_width = index < 10 ? 1 : (index < 100 ? 2 : 3);

    printf(COLOR_BRIGHT_YELLOW "  %*d. " COLOR_RESET, num_width, index);
    printf(COLOR_BRIGHT_CYAN COLOR_BOLD "%-*s" COLOR_RESET "\n",
        w - 7 - num_width, result->title);

    if (result->url[0]) {
        printf(COLOR_BRIGHT_BLUE "       %-*s" COLOR_RESET "\n",
            w - 7, result->url);
    }

    if (result->snippet[0]) {
        printf(COLOR_BRIGHT_WHITE "       %-*s" COLOR_RESET, w - 7, result->snippet);
        if (result->is_advertisement)
            printf(COLOR_BRIGHT_YELLOW "  [Ad]" COLOR_RESET);
        printf("\n");
    }

    if (result->display_url[0] && strcmp(result->display_url, result->url) != 0) {
        printf(COLOR_DIM "       <%s>" COLOR_RESET "\n", result->display_url);
    }

    printf(COLOR_BRIGHT_BLACK "       [%s] ", result->engine);
    if (result->relevance_score > 0) printf("score:%.2f ", result->relevance_score);
    printf(COLOR_RESET);

    char rel_buf[32];
    printf(COLOR_DIM "%s" COLOR_RESET "\n", get_relative_time_str(result->timestamp, rel_buf, sizeof(rel_buf)));
}

void ui_render_engine_list(BrowserContext* ctx) {
    printf("\n" COLOR_BOLD "Available Search Engines" COLOR_RESET "\n\n");
    printf(COLOR_BRIGHT_BLACK);
    printf("  %-3s %-20s %-12s %-10s %-7s %s\n",
        "#", "Name", "Tier", "Region", "Active", "Base URL");
    for (int i = 0; i < 80; i++) printf("-");
    printf(COLOR_RESET "\n");

    for (int i = 0; i < ctx->engine_count; i++) {
        SearchEngine* e = &ctx->engines[i];
        const char* icon = engine_get_tier_icon(e->tier);
        printf("  %-3d " COLOR_BRIGHT_CYAN "%-20s" COLOR_RESET " ", i + 1, e->display_name);
        printf(COLOR_DIM "[%s] %-12s" COLOR_RESET " ", icon, engine_get_tier_name(e->tier));
        printf("%-10s ", e->region_tag[0] ? e->region_tag : "global");
        printf("%-7s ", e->enabled ? COLOR_GREEN "ON" COLOR_RESET : COLOR_DIM "OFF" COLOR_RESET);
        printf(COLOR_BRIGHT_BLACK "%s" COLOR_RESET "\n", e->search_url);
    }
    printf("\n");
}

void ui_render_help(void) {
    printf("\n" COLOR_BOLD COLOR_BRIGHT_CYAN "  CMDBrowser v2.0 - Help & Commands" COLOR_RESET "\n\n");

    printf(COLOR_BOLD "  Navigation Commands:" COLOR_RESET "\n");
    printf("  " COLOR_BRIGHT_GREEN "/search <query>" COLOR_RESET "     or " COLOR_BRIGHT_GREEN "?<query>" COLOR_RESET "         - Search all active engines\n");
    printf("  " COLOR_BRIGHT_GREEN "/s <query>" COLOR_RESET "              or " COLOR_BRIGHT_GREEN ".s <query>" COLOR_RESET "        - Quick search\n");
    printf("  " COLOR_BRIGHT_GREEN "/google <query>" COLOR_RESET "          - Search only Google\n");
    printf("  " COLOR_BRIGHT_GREEN "/bing <query>" COLOR_RESET "            - Search only Bing\n");
    printf("  " COLOR_BRIGHT_GREEN "/ddg <query>" COLOR_RESET "             - Search only DuckDuckGo\n");
    printf("  " COLOR_BRIGHT_GREEN "/engine <name> <query>" COLOR_RESET "   - Search with specific engine\n");
    printf("  " COLOR_BRIGHT_GREEN "/mega <query>" COLOR_RESET "           - Search ALL engines in parallel\n");
    printf("  " COLOR_BRIGHT_GREEN "/page <n>" COLOR_RESET "               - Go to page N\n");
    printf("  " COLOR_BRIGHT_GREEN "/next" COLOR_RESET "                   - Next page of results\n");
    printf("  " COLOR_BRIGHT_GREEN "/prev" COLOR_RESET "                   - Previous page of results\n\n");

    printf(COLOR_BOLD "  Action Commands:" COLOR_RESET "\n");
    printf("  " COLOR_BRIGHT_GREEN "/open <n>" COLOR_RESET "              - Open result #N in browser\n");
    printf("  " COLOR_BRIGHT_GREEN "/bookmark <n>" COLOR_RESET "         - Bookmark result #N\n");
    printf("  " COLOR_BRIGHT_GREEN "/bkm add <title> <url>" COLOR_RESET "  - Add a bookmark\n");
    printf("  " COLOR_BRIGHT_GREEN "/bkm list" COLOR_RESET "              - List all bookmarks\n");
    printf("  " COLOR_BRIGHT_GREEN "/bkm search <keyword>" COLOR_RESET "  - Search bookmarks\n");
    printf("  " COLOR_BRIGHT_GREEN "/bkm export <file>" COLOR_RESET "     - Export bookmarks\n");
    printf("  " COLOR_BRIGHT_GREEN "/bkm import <file.html>" COLOR_RESET " - Import bookmarks from HTML\n");
    printf("  " COLOR_BRIGHT_GREEN "/history" COLOR_RESET "               - Show search history\n");
    printf("  " COLOR_BRIGHT_GREEN "/history search <kwd>" COLOR_RESET "  - Search in history\n");
    printf("  " COLOR_BRIGHT_GREEN "/history stats" COLOR_RESET "         - History statistics\n");
    printf("  " COLOR_BRIGHT_GREEN "/history clear" COLOR_RESET "         - Clear all history\n");
    printf("  " COLOR_BRIGHT_GREEN "/cache stats" COLOR_RESET "           - Show cache statistics\n");
    printf("  " COLOR_BRIGHT_GREEN "/cache clear" COLOR_RESET "           - Clear the result cache\n\n");

    printf(COLOR_BOLD "  Configuration Commands:" COLOR_RESET "\n");
    printf("  " COLOR_BRIGHT_GREEN "/engines" COLOR_RESET "               - List all search engines\n");
    printf("  " COLOR_BRIGHT_GREEN "/engines on <name>" COLOR_RESET "     - Enable an engine\n");
    printf("  " COLOR_BRIGHT_GREEN "/engines off <name>" COLOR_RESET "    - Disable an engine\n");
    printf("  " COLOR_BRIGHT_GREEN "/engines add <name> <url> <param>" COLOR_RESET " - Add custom engine\n");
    printf("  " COLOR_BRIGHT_GREEN "/engines remove <name>" COLOR_RESET " - Remove custom engine\n");
    printf("  " COLOR_BRIGHT_GREEN "/config get <sec> <key>" COLOR_RESET " - Get config value\n");
    printf("  " COLOR_BRIGHT_GREEN "/config set <sec> <key> <val>" COLOR_RESET " - Set config value\n");
    printf("  " COLOR_BRIGHT_GREEN "/config list" COLOR_RESET "           - List all config sections\n");
    printf("  " COLOR_BRIGHT_GREEN "/format <type>" COLOR_RESET "         - Set output format\n");
    printf("  " COLOR_BRIGHT_GREEN "/proxy <addr>" COLOR_RESET "          - Set proxy server\n");
    printf("  " COLOR_BRIGHT_GREEN "/lang <code>" COLOR_RESET "           - Set language (en, zh, fr, de...)\n");
    printf("  " COLOR_BRIGHT_GREEN "/safe <off|mod|strict>" COLOR_RESET "  - Set safe search level\n\n");

    printf(COLOR_BOLD "  Output & Export:" COLOR_RESET "\n");
    printf("  " COLOR_BRIGHT_GREEN "/export <filepath>" COLOR_RESET "     - Export last results\n");
    printf("  " COLOR_BRIGHT_GREEN "/format json|csv|md|html|xml" COLOR_RESET " - Set default output format\n");
    printf("  " COLOR_BRIGHT_GREEN "/color on|off" COLOR_RESET "         - Toggle colored output\n");
    printf("  " COLOR_BRIGHT_GREEN "/lines on|off" COLOR_RESET "         - Toggle line numbers\n\n");

    printf(COLOR_BOLD "  System Commands:" COLOR_RESET "\n");
    printf("  " COLOR_BRIGHT_GREEN "/help" COLOR_RESET "                  - Show this help screen\n");
    printf("  " COLOR_BRIGHT_GREEN "/about" COLOR_RESET "                - Show version info\n");
    printf("  " COLOR_BRIGHT_GREEN "/clear" COLOR_RESET "               - Clear the screen\n");
    printf("  " COLOR_BRIGHT_GREEN "/quit" COLOR_RESET "                - Exit CMDBrowser\n");
    printf("  " COLOR_BRIGHT_GREEN "/exit" COLOR_RESET "                - Exit CMDBrowser\n");
    printf("  " COLOR_BRIGHT_GREEN "/save" COLOR_RESET "                - Save all data (history, bookmarks, config)\n\n");

    printf(COLOR_DIM "  Tip: You can use short command aliases:\n");
    printf("  /s = /search, /g = /google, /b = /bing, /e = /engines, /h = /history, /q = /quit\n");
    printf(COLOR_RESET "\n");
}

void ui_render_welcome(void) {
    print_banner();
    int w = get_terminal_width();
    printf(COLOR_BRIGHT_BLACK);
    print_centered("Type /help for all commands, or just type a search query to begin.", w);
    print_centered("Example: /search how to write C programs", w);
    printf(COLOR_RESET "\n");
}

void ui_render_status_line(const char* status, const char* color) {
    printf("%s%s%s\n", color ? color : COLOR_BRIGHT_BLACK, status, COLOR_RESET);
}

void ui_render_progress(int current, int total, const char* message) {
    int w = get_terminal_width();
    if (message) {
        printf("\r" COLOR_BRIGHT_CYAN "  %-20s" COLOR_RESET " ", message);
    }
    progress_bar(current, total, w > 40 ? w - 30 : 30);
}

void ui_render_error(const char* message) {
    printf(COLOR_BRIGHT_RED "  [ERROR] %s" COLOR_RESET "\n", message);
}

void ui_render_warning(const char* message) {
    printf(COLOR_BRIGHT_YELLOW "  [WARN] %s" COLOR_RESET "\n", message);
}

void ui_render_info(const char* message) {
    printf(COLOR_BRIGHT_CYAN "  [INFO] %s" COLOR_RESET "\n", message);
}

void ui_render_success(const char* message) {
    printf(COLOR_BRIGHT_GREEN "  [OK] %s" COLOR_RESET "\n", message);
}

void ui_clear_screen(void) {
    printf("\033[2J\033[H");
}

void ui_render_output_formatted(BrowserContext* ctx, SearchResponse* resp) {
    switch (ctx->output_format) {
        case OUTPUT_FORMAT_JSON:     ui_render_json_output(ctx, resp); break;
        case OUTPUT_FORMAT_CSV:      ui_render_csv_output(ctx, resp); break;
        case OUTPUT_FORMAT_MARKDOWN: ui_render_markdown_output(ctx, resp); break;
        case OUTPUT_FORMAT_HTML:     ui_render_html_output(ctx, resp); break;
        case OUTPUT_FORMAT_RICH:     ui_render_merged_results(ctx, resp); break;
        default:                     ui_render_results(ctx, resp); break;
    }
}

void ui_render_json_output(BrowserContext* ctx, SearchResponse* resp) {
    if (!resp) { printf("{\"error\":\"no results\"}\n"); return; }
    printf("{\n");
    printf("  \"query\": \"%s\",\n", json_escape(resp->query));
    printf("  \"total_count\": %d,\n", resp->total_count);
    printf("  \"page\": %d,\n", resp->page + 1);
    printf("  \"search_time\": %.3f,\n", resp->search_time_seconds);
    if (resp->engine) printf("  \"engine\": \"%s\",\n", resp->engine->name);
    printf("  \"results\": [\n");
    for (int i = 0; i < resp->result_count && i < ctx->max_results; i++) {
        SearchResult* r = &resp->results[i];
        printf("    {\n");
        printf("      \"index\": %d,\n", i + 1);
        printf("      \"title\": \"%s\",\n", json_escape(r->title));
        printf("      \"url\": \"%s\",\n", json_escape(r->url));
        printf("      \"snippet\": \"%s\",\n", json_escape(r->snippet));
        printf("      \"engine\": \"%s\",\n", r->engine);
        printf("      \"relevance\": %.2f,\n", r->relevance_score);
        printf("      \"is_ad\": %s\n", r->is_advertisement ? "true" : "false");
        printf("    }%s\n", i < resp->result_count - 1 ? "," : "");
    }
    printf("  ]\n}\n");
}

void ui_render_csv_output(BrowserContext* ctx, SearchResponse* resp) {
    if (!resp) { printf("Error,no results\n"); return; }
    printf("Rank,Title,URL,Snippet,Engine,Relevance,IsAd\n");
    for (int i = 0; i < resp->result_count && i < ctx->max_results; i++) {
        SearchResult* r = &resp->results[i];
        printf("%d,%s,%s,%s,%s,%.2f,%s\n",
            i + 1, csv_escape(r->title), csv_escape(r->url), csv_escape(r->snippet),
            r->engine, r->relevance_score, r->is_advertisement ? "Yes" : "No");
    }
}

void ui_render_markdown_output(BrowserContext* ctx, SearchResponse* resp) {
    if (!resp) { printf("*No results found.*\n"); return; }
    printf("# Search Results: %s\n\n", resp->query);
    if (resp->engine) printf("**Engine:** %s | **Time:** %.2fs | **Results:** %d\n\n---\n\n",
        resp->engine->display_name, resp->search_time_seconds, resp->total_count);
    for (int i = 0; i < resp->result_count && i < ctx->max_results; i++) {
        SearchResult* r = &resp->results[i];
        printf("### %d. [%s](%s)\n", i + 1, r->title, r->url);
        printf("%s  \n", r->snippet);
        printf("*%s* | relevance: %.2f\n\n", r->engine, r->relevance_score);
    }
}

void ui_render_html_output(BrowserContext* ctx, SearchResponse* resp) {
    if (!resp) { printf("<p>No results</p>\n"); return; }
    printf("<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>%s</title>", resp->query);
    printf("<style>body{font-family:sans-serif;max-width:800px;margin:0 auto;padding:20px;background:#111;color:#eee}");
    printf(".result{margin:20px 0;padding:15px;background:#222;border-radius:8px}");
    printf(".title{color:#0af;font-size:18px;text-decoration:none}.url{color:#0a0;font-size:13px}.snippet{color:#ccc;margin:5px 0}.meta{color:#888;font-size:12px}");
    printf("</style></head><body><h1>Results: %s</h1>", resp->query);
    if (resp->engine) printf("<p>Engine: %s | Time: %.2fs | %d results</p>",
        resp->engine->display_name, resp->search_time_seconds, resp->total_count);
    for (int i = 0; i < resp->result_count && i < ctx->max_results; i++) {
        SearchResult* r = &resp->results[i];
        printf("<div class=\"result\"><a class=\"title\" href=\"%s\">%d. %s</a>",
            r->url, i + 1, r->title);
        printf("<div class=\"url\">%s</div>", r->url);
        printf("<div class=\"snippet\">%s</div>", r->snippet);
        printf("<div class=\"meta\">%s | relevance: %.2f</div></div>", r->engine, r->relevance_score);
    }
    printf("</body></html>\n");
}

void ui_render_compact_list(BrowserContext* ctx, SearchResponse* resp) {
    if (!resp) return;
    printf("\n");
    for (int i = 0; i < resp->result_count && i < ctx->max_results; i++) {
        SearchResult* r = &resp->results[i];
        printf(COLOR_BRIGHT_YELLOW "[%3d]" COLOR_RESET " " COLOR_BRIGHT_CYAN "%s" COLOR_RESET "\n", i + 1, r->title);
        printf("      " COLOR_BRIGHT_BLUE "%s" COLOR_RESET "\n", r->url);
        if (r->snippet[0]) printf("      " COLOR_DIM "%s" COLOR_RESET "\n", r->snippet);
        printf("      " COLOR_BRIGHT_BLACK "[%s]" COLOR_RESET "\n\n", r->engine);
    }
}

void ui_render_pager(BrowserContext* ctx, SearchResponse* resp) {
    if (!resp || resp->result_count == 0) {
        ui_render_error("No results to display.");
        return;
    }

    int page_size = ctx->terminal_height - 10;
    if (page_size < 5) page_size = 5;

    int total_pages = (resp->result_count + page_size - 1) / page_size;
    int current_page = 0;

    for (;;) {
        ui_clear_screen();
        ui_render_header(ctx);

        int start = current_page * page_size;
        int end = (start + page_size) < resp->result_count ? (start + page_size) : resp->result_count;

        printf("\n" COLOR_BOLD "  Page %d/%d" COLOR_RESET, current_page + 1, total_pages);
        printf(COLOR_DIM " | Showing %d-%d of %d results" COLOR_RESET "\n\n", start + 1, end, resp->result_count);

        for (int i = start; i < end; i++) {
            ui_render_result_detail(&resp->results[i], i + 1);
            printf("\n");
        }

        printf(COLOR_DIM "\n  [n]ext  [p]rev  [q]uit pager  [o]pen <num>  [b]ookmark <num>\n" COLOR_RESET);

        printf("  >>> ");
        char cmd[256];
        if (!fgets(cmd, sizeof(cmd), stdin)) break;
        trim_whitespace(cmd);

        if (strcmp(cmd, "n") == 0 || strcmp(cmd, "next") == 0) {
            if (current_page < total_pages - 1) current_page++;
        } else if (strcmp(cmd, "p") == 0 || strcmp(cmd, "prev") == 0) {
            if (current_page > 0) current_page--;
        } else if (strcmp(cmd, "q") == 0 || strcmp(cmd, "quit") == 0) {
            break;
        } else if (strncmp(cmd, "o ", 2) == 0 || strncmp(cmd, "open ", 5) == 0) {
            int idx = atoi(cmd + (cmd[0] == 'o' ? 5 : 2)) - 1;
            if (idx >= 0 && idx < resp->result_count) {
                printf(COLOR_BRIGHT_GREEN "  Opening: %s" COLOR_RESET "\n", resp->results[idx].url);
                char cmd_open[MAX_URL_LENGTH + 16];
                snprintf(cmd_open, sizeof(cmd_open), "start \"\" \"%s\"", resp->results[idx].url);
                system(cmd_open);
            } else {
                ui_render_error("Invalid result index.");
            }
        } else if (strncmp(cmd, "b ", 2) == 0 || strncmp(cmd, "bookmark ", 9) == 0) {
            int idx = atoi(cmd + (cmd[0] == 'b' ? 9 : 2)) - 1;
            if (idx >= 0 && idx < resp->result_count) {
                bookmarks_add(ctx, resp->results[idx].title, resp->results[idx].url,
                    resp->results[idx].engine, "search");
                ui_render_success("Bookmarked.");
            } else {
                ui_render_error("Invalid result index.");
            }
        } else if (strlen(cmd) > 0) {
            ui_render_error("Unknown pager command.");
        }
    }
}
