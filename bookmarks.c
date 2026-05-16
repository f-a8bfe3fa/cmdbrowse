#include "bookmarks.h"
#include "utils.h"

void bookmarks_init(BrowserContext* ctx) {
    ctx->bookmark_capacity = 256;
    ctx->bookmarks = (BookmarkEntry*)calloc(ctx->bookmark_capacity, sizeof(BookmarkEntry));
    ctx->bookmark_count = 0;
}

void bookmarks_add(BrowserContext* ctx, const char* title, const char* url, const char* tags, const char* folder) {
    if (!ctx->bookmarks) bookmarks_init(ctx);
    if (ctx->bookmark_count >= ctx->bookmark_capacity) {
        ctx->bookmark_capacity *= 2;
        ctx->bookmarks = (BookmarkEntry*)realloc(ctx->bookmarks, ctx->bookmark_capacity * sizeof(BookmarkEntry));
        if (!ctx->bookmarks) return;
    }
    BookmarkEntry* b = &ctx->bookmarks[ctx->bookmark_count];
    memset(b, 0, sizeof(BookmarkEntry));
    snprintf(b->title, sizeof(b->title), "%s", title);
    snprintf(b->url, sizeof(b->url), "%s", url);
    snprintf(b->tags, sizeof(b->tags), "%s", tags ? tags : "");
    snprintf(b->folder, sizeof(b->folder), "%s", folder ? folder : "default");
    b->timestamp = time(NULL);
    b->visit_count = 1;
    ctx->bookmark_count++;
}

void bookmarks_remove(BrowserContext* ctx, int index) {
    if (!ctx->bookmarks || index < 0 || index >= ctx->bookmark_count) {
        printf("Invalid bookmark index.\n"); return;
    }
    if (index < ctx->bookmark_count - 1) {
        memmove(&ctx->bookmarks[index], &ctx->bookmarks[index + 1],
            (ctx->bookmark_count - index - 1) * sizeof(BookmarkEntry));
    }
    ctx->bookmark_count--;
    printf("Bookmark removed.\n");
}

void bookmarks_list(BrowserContext* ctx, const char* folder_filter, const char* tag_filter) {
    if (!ctx->bookmarks || ctx->bookmark_count == 0) {
        printf(COLOR_DIM "No bookmarks.\n" COLOR_RESET); return;
    }
    printf("\n" COLOR_BOLD "Bookmarks" COLOR_RESET);
    if (folder_filter) printf(" [folder: %s]", folder_filter);
    if (tag_filter) printf(" [tag: %s]", tag_filter);
    printf("\n\n");

    int shown = 0;
    for (int i = 0; i < ctx->bookmark_count; i++) {
        BookmarkEntry* b = &ctx->bookmarks[i];
        if (folder_filter && !str_contains(b->folder, folder_filter)) continue;
        if (tag_filter && !str_contains(b->tags, tag_filter)) continue;
        char tbuf[32];
        get_timestamp_str(b->timestamp, tbuf, sizeof(tbuf));
        printf("  [%3d] " COLOR_BRIGHT_CYAN "%s" COLOR_RESET "\n", shown + 1, b->title);
        printf("        " COLOR_BRIGHT_BLUE "%s" COLOR_RESET "\n", b->url);
        printf("        " COLOR_DIM "%s | %s | visits: %d" COLOR_RESET "\n",
            b->folder, tbuf, b->visit_count);
        if (b->tags[0]) printf("        Tags: %s\n", b->tags);
        shown++;
    }
    printf("\n" COLOR_DIM "Total: %d bookmarks shown" COLOR_RESET "\n\n", shown);
}

void bookmarks_search(BrowserContext* ctx, const char* keyword) {
    if (!ctx->bookmarks || ctx->bookmark_count == 0) {
        printf(COLOR_DIM "No bookmarks.\n" COLOR_RESET); return;
    }
    printf("\n" COLOR_BOLD "Bookmark search: '%s'" COLOR_RESET "\n\n", keyword);
    int found = 0;
    for (int i = 0; i < ctx->bookmark_count; i++) {
        BookmarkEntry* b = &ctx->bookmarks[i];
        if (str_contains(b->title, keyword) || str_contains(b->url, keyword) ||
            str_contains(b->tags, keyword) || str_contains(b->folder, keyword)) {
            printf("  [%d] %s\n      %s\n", found + 1, b->title, b->url);
            found++;
            if (found >= 30) { printf("  ... and more\n"); break; }
        }
    }
    if (found == 0) printf(COLOR_DIM "  No matches.\n" COLOR_RESET);
    printf("\n");
}

void bookmarks_save(BrowserContext* ctx) {
    char filepath[MAX_PATH_LENGTH];
    snprintf(filepath, sizeof(filepath), "%s\\bookmarks.dat", get_config_dir());
    ensure_directory(get_config_dir());
    FILE* f = fopen(filepath, "wb");
    if (!f) return;
    fwrite(&ctx->bookmark_count, sizeof(int), 1, f);
    if (ctx->bookmark_count > 0) {
        fwrite(ctx->bookmarks, sizeof(BookmarkEntry), ctx->bookmark_count, f);
    }
    fclose(f);
}

bool bookmarks_load(BrowserContext* ctx) {
    char filepath[MAX_PATH_LENGTH];
    snprintf(filepath, sizeof(filepath), "%s\\bookmarks.dat", get_config_dir());
    if (!file_exists(filepath)) return false;
    FILE* f = fopen(filepath, "rb");
    if (!f) return false;
    int count = 0;
    if (fread(&count, sizeof(int), 1, f) != 1 || count <= 0) { fclose(f); return false; }
    bookmarks_init(ctx);
    ctx->bookmark_count = count;
    ctx->bookmark_capacity = count > 256 ? count * 2 : 256;
    ctx->bookmarks = (BookmarkEntry*)calloc(ctx->bookmark_capacity, sizeof(BookmarkEntry));
    if (!ctx->bookmarks) { fclose(f); return false; }
    fread(ctx->bookmarks, sizeof(BookmarkEntry), count, f);
    fclose(f);
    return true;
}

void bookmarks_export(BrowserContext* ctx, const char* filepath, OutputFormat format) {
    if (!ctx->bookmarks || ctx->bookmark_count == 0) {
        printf("No bookmarks to export.\n"); return;
    }
    FILE* f = fopen(filepath, "w");
    if (!f) { printf("Cannot write to %s\n", filepath); return; }
    if (format == OUTPUT_FORMAT_HTML) {
        fprintf(f, "<!DOCTYPE NETSCAPE-Bookmark-file-1>\n<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=UTF-8\">\n<TITLE>Bookmarks</TITLE>\n<H1>Bookmarks</H1>\n<DL><p>\n");
        for (int i = 0; i < ctx->bookmark_count; i++) {
            char tbuf[32]; get_timestamp_str(ctx->bookmarks[i].timestamp, tbuf, sizeof(tbuf));
            fprintf(f, "    <DT><A HREF=\"%s\" ADD_DATE=\"%lld\">%s</A>\n",
                ctx->bookmarks[i].url, (long long)ctx->bookmarks[i].timestamp,
                ctx->bookmarks[i].title);
        }
        fprintf(f, "</DL><p>\n");
    } else if (format == OUTPUT_FORMAT_CSV) {
        fprintf(f, "Title,URL,Folder,Tags,Timestamp\n");
        for (int i = 0; i < ctx->bookmark_count; i++) {
            char tbuf[32]; get_timestamp_str(ctx->bookmarks[i].timestamp, tbuf, sizeof(tbuf));
            fprintf(f, "%s,%s,%s,%s,%s\n", csv_escape(ctx->bookmarks[i].title),
                csv_escape(ctx->bookmarks[i].url), ctx->bookmarks[i].folder,
                ctx->bookmarks[i].tags, tbuf);
        }
    } else {
        for (int i = 0; i < ctx->bookmark_count; i++) {
            fprintf(f, "%s\n%s\n%s\n\n", ctx->bookmarks[i].title,
                ctx->bookmarks[i].url, ctx->bookmarks[i].folder);
        }
    }
    fclose(f);
    printf("Bookmarks exported to %s\n", filepath);
}

void bookmarks_import_html(BrowserContext* ctx, const char* filepath) {
    size_t size = 0;
    char* data = file_read_all(filepath, &size);
    if (!data) { printf("Cannot read %s\n", filepath); return; }
    int imported = 0;
    const char* p = data;
    while (1) {
        const char* found = strstr(p, "<A HREF=");
        if (!found) found = strstr(p, "<a href=");
        if (!found) break;
        p = found;
        const char* href = strchr(p, '"');
        if (!href) { p++; continue; }
        href++;
        const char* url_end = strchr(href, '"');
        if (!url_end) { p++; continue; }
        size_t ulen = url_end - href;
        char url[MAX_RESULT_URL];
        if (ulen >= sizeof(url)) ulen = sizeof(url) - 1;
        memcpy(url, href, ulen);
        url[ulen] = '\0';

        const char* close_a = strstr(url_end, "</A>");
        if (!close_a) close_a = strstr(url_end, "</a>");
        if (!close_a) { p = url_end + 1; continue; }

        const char* title_start = strchr(url_end, '>');
        if (!title_start) { p = url_end + 1; continue; }
        title_start++;
        size_t tlen = close_a - title_start;
        char title[MAX_RESULT_TITLE];
        if (tlen >= sizeof(title)) tlen = sizeof(title) - 1;
        memcpy(title, title_start, tlen);
        title[tlen] = '\0';
        html_entity_decode(title, title, sizeof(title));

        bookmarks_add(ctx, title, url, "imported", "imported");
        imported++;
        p = close_a + 4;
    }
    free(data);
    printf("Imported %d bookmarks from %s\n", imported, filepath);
}

int bookmarks_count(BrowserContext* ctx) {
    return ctx->bookmarks ? ctx->bookmark_count : 0;
}
