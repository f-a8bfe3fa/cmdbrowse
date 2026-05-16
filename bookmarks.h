#ifndef BOOKMARKS_H
#define BOOKMARKS_H
#include "types.h"

void bookmarks_init(BrowserContext* ctx);
void bookmarks_add(BrowserContext* ctx, const char* title, const char* url, const char* tags, const char* folder);
void bookmarks_remove(BrowserContext* ctx, int index);
void bookmarks_list(BrowserContext* ctx, const char* folder_filter, const char* tag_filter);
void bookmarks_search(BrowserContext* ctx, const char* keyword);
void bookmarks_save(BrowserContext* ctx);
bool bookmarks_load(BrowserContext* ctx);
void bookmarks_export(BrowserContext* ctx, const char* filepath, OutputFormat format);
void bookmarks_import_html(BrowserContext* ctx, const char* filepath);
int  bookmarks_count(BrowserContext* ctx);
#endif
