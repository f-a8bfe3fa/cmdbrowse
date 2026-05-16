#ifndef UI_H
#define UI_H

#include "types.h"

void ui_init(BrowserContext* ctx);
void ui_render_header(BrowserContext* ctx);
void ui_render_footer(BrowserContext* ctx);
void ui_render_results(BrowserContext* ctx, SearchResponse* resp);
void ui_render_merged_results(BrowserContext* ctx, SearchResponse* resp);
void ui_render_result_detail(SearchResult* result, int index);
void ui_render_engine_list(BrowserContext* ctx);
void ui_render_help(void);
void ui_render_welcome(void);
void ui_render_status_line(const char* status, const char* color);
void ui_render_progress(int current, int total, const char* message);
void ui_render_error(const char* message);
void ui_render_warning(const char* message);
void ui_render_info(const char* message);
void ui_render_success(const char* message);
void ui_clear_screen(void);
void ui_render_output_formatted(BrowserContext* ctx, SearchResponse* resp);
void ui_render_json_output(BrowserContext* ctx, SearchResponse* resp);
void ui_render_csv_output(BrowserContext* ctx, SearchResponse* resp);
void ui_render_markdown_output(BrowserContext* ctx, SearchResponse* resp);
void ui_render_html_output(BrowserContext* ctx, SearchResponse* resp);
void ui_render_compact_list(BrowserContext* ctx, SearchResponse* resp);
void ui_render_pager(BrowserContext* ctx, SearchResponse* resp);

#endif
