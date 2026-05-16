#include "utils.h"
#include <ctype.h>
#include <sys/stat.h>
#include <direct.h>

static bool ansi_enabled = true;

void url_encode(const char* src, char* dst, size_t dst_size) {
    static const char hex[] = "0123456789ABCDEF";
    static const char safe[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~";
    size_t pos = 0;
    for (size_t i = 0; src[i] && pos < dst_size - 1; i++) {
        unsigned char c = (unsigned char)src[i];
        if (strchr(safe, c) != NULL) {
            dst[pos++] = c;
        } else if (pos + 3 < dst_size) {
            dst[pos++] = '%';
            dst[pos++] = hex[c >> 4];
            dst[pos++] = hex[c & 0x0F];
        }
    }
    if (pos < dst_size) dst[pos] = '\0';
    else if (dst_size > 0) dst[dst_size - 1] = '\0';
}

void url_decode(const char* src, char* dst, size_t dst_size) {
    size_t pos = 0;
    for (size_t i = 0; src[i] && pos < dst_size - 1; i++) {
        if (src[i] == '%' && isxdigit((unsigned char)src[i+1]) && isxdigit((unsigned char)src[i+2])) {
            char hex_byte[3] = { src[i+1], src[i+2], '\0' };
            dst[pos++] = (char)strtol(hex_byte, NULL, 16);
            i += 2;
        } else if (src[i] == '+') {
            dst[pos++] = ' ';
        } else {
            dst[pos++] = src[i];
        }
    }
    if (pos < dst_size) dst[pos] = '\0';
    else if (dst_size > 0) dst[dst_size - 1] = '\0';
}

void html_entity_decode(const char* src, char* dst, size_t dst_size) {
    static const struct { const char* entity; const char* replacement; } entities[] = {
        {"&amp;","&"},{"&lt;","<"},{"&gt;",">"},{"&quot;","\""},{"&apos;","'"},
        {"&nbsp;"," "},{"&copy;","(c)"},{"&reg;","(R)"},{"&trade;","(TM)"},
        {"&mdash;","--"},{"&ndash;","-"},{"&lsquo;","'"},{"&rsquo;","'"},
        {"&ldquo;","\""},{"&rdquo;","\""},{"&hellip;","..."},{"&laquo;","<<"},
        {"&raquo;",">>"},{"&deg;","deg"},{"&plusmn;","+-"},{"&times;","x"},
        {"&divide;","/"},{"&euro;","EUR"},{"&pound;","GBP"},{"&yen;","JPY"},
        {"&agrave;","a"},{"&aacute;","a"},{"&eacute;","e"},{"&oacute;","o"},
        {NULL,NULL}
    };
    size_t pos = 0, i = 0;
    while (src[i] && pos < dst_size - 1) {
        if (src[i] == '&') {
            bool found = false;
            for (int e = 0; entities[e].entity; e++) {
                size_t elen = strlen(entities[e].entity);
                if (strncmp(src + i, entities[e].entity, elen) == 0) {
                    size_t rlen = strlen(entities[e].replacement);
                    if (pos + rlen < dst_size) {
                        memcpy(dst + pos, entities[e].replacement, rlen);
                        pos += rlen;
                    }
                    i += elen;
                    found = true;
                    break;
                }
            }
            if (!found) {
                if (src[i+1] == '#' && isdigit((unsigned char)src[i+2])) {
                    int codepoint = 0;
                    i += 2;
                    while (isdigit((unsigned char)src[i])) {
                        codepoint = codepoint * 10 + (src[i] - '0');
                        i++;
                    }
                    if (src[i] == ';') i++;
                    if (codepoint < 128 && pos < dst_size - 1) {
                        dst[pos++] = (char)codepoint;
                    }
                } else {
                    dst[pos++] = src[i++];
                }
            }
        } else {
            dst[pos++] = src[i++];
        }
    }
    if (pos < dst_size) dst[pos] = '\0';
    else if (dst_size > 0) dst[dst_size - 1] = '\0';
}

void strip_html_tags(const char* src, char* dst, size_t dst_size) {
    size_t pos = 0;
    bool in_tag = false;
    for (size_t i = 0; src[i] && pos < dst_size - 1; i++) {
        if (src[i] == '<') { in_tag = true; continue; }
        if (src[i] == '>') { in_tag = false; continue; }
        if (!in_tag) {
            if (src[i] == '\n' || src[i] == '\r') {
                if (pos > 0 && dst[pos-1] != ' ' && pos < dst_size - 1)
                    dst[pos++] = ' ';
            } else {
                dst[pos++] = src[i];
            }
        }
    }
    if (pos < dst_size) dst[pos] = '\0';
    else if (dst_size > 0) dst[dst_size - 1] = '\0';
}

void trim_whitespace(char* str) {
    if (!str || !*str) return;
    char* start = str;
    while (isspace((unsigned char)*start)) start++;
    char* end = str + strlen(str) - 1;
    while (end > start && isspace((unsigned char)*end)) end--;
    size_t len = end - start + 1;
    memmove(str, start, len);
    str[len] = '\0';
}

void str_to_lower(char* str) {
    for (; *str; str++) *str = (char)tolower((unsigned char)*str);
}

void str_to_upper(char* str) {
    for (; *str; str++) *str = (char)toupper((unsigned char)*str);
}

char* str_dup(const char* src) {
    if (!src) return NULL;
    size_t len = strlen(src);
    char* dst = (char*)malloc(len + 1);
    if (dst) { memcpy(dst, src, len); dst[len] = '\0'; }
    return dst;
}

bool str_contains(const char* haystack, const char* needle) {
    return strstr(haystack, needle) != NULL;
}

bool str_starts_with(const char* str, const char* prefix) {
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

bool str_ends_with(const char* str, const char* suffix) {
    size_t slen = strlen(str), plen = strlen(suffix);
    return slen >= plen && strcmp(str + slen - plen, suffix) == 0;
}

char* str_between(const char* haystack, const char* start, const char* end) {
    const char* p1 = strstr(haystack, start);
    if (!p1) return NULL;
    p1 += strlen(start);
    const char* p2 = strstr(p1, end);
    if (!p2) return NULL;
    size_t len = p2 - p1;
    char* result = (char*)malloc(len + 1);
    if (result) { memcpy(result, p1, len); result[len] = '\0'; }
    return result;
}

int str_count(const char* haystack, const char* needle) {
    int count = 0;
    size_t nlen = strlen(needle);
    const char* p = haystack;
    while ((p = strstr(p, needle)) != NULL) { count++; p += nlen; }
    return count;
}

void byte_buffer_init(ByteBuffer* buf) {
    buf->data = NULL;
    buf->length = 0;
    buf->capacity = 0;
}

void byte_buffer_append(ByteBuffer* buf, const void* data, size_t length) {
    if (buf->length + length > buf->capacity) {
        size_t new_cap = buf->capacity ? buf->capacity * 2 : 4096;
        while (new_cap < buf->length + length) new_cap *= 2;
        char* new_data = (char*)realloc(buf->data, new_cap);
        if (!new_data) return;
        buf->data = new_data;
        buf->capacity = new_cap;
    }
    memcpy(buf->data + buf->length, data, length);
    buf->length += length;
}

void byte_buffer_free(ByteBuffer* buf) {
    free(buf->data);
    buf->data = NULL;
    buf->length = 0;
    buf->capacity = 0;
}

void byte_buffer_reset(ByteBuffer* buf) {
    buf->length = 0;
}

char* get_timestamp_str(time_t t, char* buf, size_t size) {
    struct tm* tm_info = localtime(&t);
    strftime(buf, size, "%Y-%m-%d %H:%M:%S", tm_info);
    return buf;
}

char* get_relative_time_str(time_t t, char* buf, size_t size) {
    time_t now = time(NULL);
    int64_t diff = (int64_t)difftime(now, t);
    if (diff < 60) snprintf(buf, size, "%llds ago", (long long)diff);
    else if (diff < 3600) snprintf(buf, size, "%lldm ago", (long long)(diff / 60));
    else if (diff < 86400) snprintf(buf, size, "%lldh ago", (long long)(diff / 3600));
    else if (diff < 604800) snprintf(buf, size, "%lldd ago", (long long)(diff / 86400));
    else if (diff < 2592000) snprintf(buf, size, "%lldw ago", (long long)(diff / 604800));
    else if (diff < 31536000) snprintf(buf, size, "%lldmo ago", (long long)(diff / 2592000));
    else snprintf(buf, size, "%lldy ago", (long long)(diff / 31536000));
    return buf;
}

bool is_valid_url(const char* url) {
    return str_starts_with(url, "http://") || str_starts_with(url, "https://");
}

void extract_domain(const char* url, char* domain, size_t size) {
    const char* p = url;
    if (str_starts_with(p, "https://")) p += 8;
    else if (str_starts_with(p, "http://")) p += 7;
    const char* slash = strchr(p, '/');
    size_t len = slash ? (size_t)(slash - p) : strlen(p);
    if (len >= size) len = size - 1;
    memcpy(domain, p, len);
    domain[len] = '\0';
}

int get_terminal_width(void) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
        return csbi.srWindow.Right - csbi.srWindow.Left + 1;
    char* cols = getenv("COLUMNS");
    return cols ? atoi(cols) : 80;
}

int get_terminal_height(void) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
        return csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    char* rows = getenv("LINES");
    return rows ? atoi(rows) : 24;
}

void enable_ansi_escapes(void) {
    ansi_enabled = true;
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD mode = 0;
    if (GetConsoleMode(hOut, &mode)) {
        mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, mode);
    }
}

void disable_ansi_escapes(void) {
    ansi_enabled = false;
}

bool file_exists(const char* path) {
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
}

bool dir_exists(const char* path) {
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));
}

int file_size_bytes(const char* path) {
    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return -1;
    DWORD high = 0;
    DWORD low = GetFileSize(hFile, &high);
    CloseHandle(hFile);
    return (int)low;
}

char* file_read_all(const char* path, size_t* out_size) {
    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return NULL;
    DWORD high = 0;
    DWORD low = GetFileSize(hFile, &high);
    size_t size = ((size_t)high << 32) | low;
    char* data = (char*)malloc(size + 1);
    if (!data) { CloseHandle(hFile); return NULL; }
    DWORD bytes_read = 0;
    if (!ReadFile(hFile, data, (DWORD)size, &bytes_read, NULL) || bytes_read != size) {
        free(data); CloseHandle(hFile); return NULL;
    }
    data[size] = '\0';
    CloseHandle(hFile);
    if (out_size) *out_size = size;
    return data;
}

bool file_write_all(const char* path, const void* data, size_t size) {
    HANDLE hFile = CreateFileA(path, GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    bool ok = WriteFile(hFile, data, (DWORD)size, &written, NULL) && written == size;
    CloseHandle(hFile);
    return ok;
}

bool file_append_line(const char* path, const char* line) {
    HANDLE hFile = CreateFileA(path, FILE_APPEND_DATA, 0, NULL,
        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    SetFilePointer(hFile, 0, NULL, FILE_END);
    char nl[2] = { '\n', '\0' };
    DWORD written = 0;
    WriteFile(hFile, line, (DWORD)strlen(line), &written, NULL);
    WriteFile(hFile, nl, 1, &written, NULL);
    CloseHandle(hFile);
    return true;
}

char** file_read_lines(const char* path, int* out_count) {
    size_t size = 0;
    char* data = file_read_all(path, &size);
    if (!data) { *out_count = 0; return NULL; }
    int capacity = 128, count = 0;
    char** lines = (char**)malloc(capacity * sizeof(char*));
    char* line_start = data;
    for (size_t i = 0; i <= size; i++) {
        if (data[i] == '\n' || data[i] == '\r' || data[i] == '\0') {
            if (i > (size_t)(line_start - data)) {
                size_t len = &data[i] - line_start;
                if (count >= capacity) {
                    capacity *= 2;
                    lines = (char**)realloc(lines, capacity * sizeof(char*));
                }
                lines[count] = (char*)malloc(len + 1);
                memcpy(lines[count], line_start, len);
                lines[count][len] = '\0';
                count++;
            }
            if (data[i] == '\r' && data[i+1] == '\n') i++;
            line_start = &data[i + 1];
        }
    }
    free(data);
    *out_count = count;
    return lines;
}

void free_lines(char** lines, int count) {
    for (int i = 0; i < count; i++) free(lines[i]);
    free(lines);
}

char* get_config_dir(void) {
    static char path[MAX_PATH_LENGTH];
    const char* appdata = getenv("APPDATA");
    if (appdata) snprintf(path, sizeof(path), "%s\\cmdbrowser", appdata);
    else snprintf(path, sizeof(path), "%s\\.cmdbrowser", getenv("USERPROFILE") ? getenv("USERPROFILE") : "C:\\Users\\Public");
    return path;
}

char* get_cache_dir(void) {
    static char path[MAX_PATH_LENGTH];
    snprintf(path, sizeof(path), "%s\\cache", get_config_dir());
    return path;
}

bool ensure_directory(const char* path) {
    char tmp[MAX_PATH_LENGTH];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char* p = tmp; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char sep = *p; *p = '\0';
            if (strlen(tmp) > 2) CreateDirectoryA(tmp, NULL);
            *p = sep;
        }
    }
    CreateDirectoryA(tmp, NULL);
    return dir_exists(path) || CreateDirectoryA(path, NULL) != 0;
}

char* path_join(const char* base, const char* name) {
    static char result[MAX_PATH_LENGTH];
    snprintf(result, sizeof(result), "%s\\%s", base, name);
    return result;
}

int random_int(int min, int max) {
    static bool seeded = false;
    if (!seeded) { srand((unsigned)time(NULL)); seeded = true; }
    return min + rand() % (max - min + 1);
}

void sleep_ms(int milliseconds) {
    Sleep((DWORD)milliseconds);
}

char* wchar_to_utf8(const wchar_t* wstr) {
    if (!wstr) return NULL;
    int needed = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    char* buf = (char*)malloc(needed);
    if (buf) WideCharToMultiByte(CP_UTF8, 0, wstr, -1, buf, needed, NULL, NULL);
    return buf;
}

wchar_t* utf8_to_wchar(const char* str) {
    if (!str) return NULL;
    int needed = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
    wchar_t* buf = (wchar_t*)malloc(needed * sizeof(wchar_t));
    if (buf) MultiByteToWideChar(CP_UTF8, 0, str, -1, buf, needed);
    return buf;
}

char* json_escape(const char* str) {
    static char buf[8192];
    size_t pos = 0;
    for (const char* p = str; *p && pos < sizeof(buf) - 2; p++) {
        switch (*p) {
            case '"':  buf[pos++]='\\'; buf[pos++]='"'; break;
            case '\\': buf[pos++]='\\'; buf[pos++]='\\'; break;
            case '\n': buf[pos++]='\\'; buf[pos++]='n'; break;
            case '\r': buf[pos++]='\\'; buf[pos++]='r'; break;
            case '\t': buf[pos++]='\\'; buf[pos++]='t'; break;
            default:   buf[pos++] = *p; break;
        }
    }
    buf[pos] = '\0';
    return buf;
}

char* csv_escape(const char* str) {
    static char buf[8192];
    bool need_quotes = strchr(str, ',') || strchr(str, '"') || strchr(str, '\n');
    if (!need_quotes) { snprintf(buf, sizeof(buf), "%s", str); return buf; }
    size_t pos = 0;
    buf[pos++] = '"';
    for (const char* p = str; *p && pos < sizeof(buf) - 3; p++) {
        if (*p == '"') buf[pos++] = '"';
        buf[pos++] = *p;
    }
    buf[pos++] = '"';
    buf[pos] = '\0';
    return buf;
}

void progress_bar(int current, int total, int width) {
    if (total <= 0) return;
    int bar_width = width - 20;
    if (bar_width < 10) bar_width = 10;
    float ratio = (float)current / total;
    int filled = (int)(ratio * bar_width);
    printf("\r[");
    for (int i = 0; i < bar_width; i++)
        putchar(i < filled ? '=' : (i == filled ? '>' : ' '));
    printf("] %3.0f%% %d/%d", ratio * 100, current, total);
    fflush(stdout);
}

void print_centered(const char* str, int width) {
    int len = (int)strlen(str);
    int pad = (width - len) / 2;
    if (pad < 0) pad = 0;
    printf("%*s%s\n", pad, "", str);
}

void print_banner(void) {
    const int w = get_terminal_width();
    printf(COLOR_BRIGHT_CYAN);
    printf("\n");
    print_centered("   ___ __  __ ___  ___                             ", w);
    print_centered("  / __|  \\/  |   \\| _ ) ___ _ _ ___ ___  ___ _ _  ", w);
    print_centered(" | (__| |\\/| | |) | _ \\/ _ \\ '_|_ / _ \\/ -_) '_| ", w);
    print_centered("  \\___|_|  |_|___/|___/\\___/_| /__\\___/\\___|_|   ", w);
    print_centered("                                                   ", w);
    print_centered("  Command-Line Multi-Engine Web Search Browser v2.0 ", w);
    printf(COLOR_RESET "\n");
}

char* format_number(int64_t num, char* buf, size_t size) {
    if (num >= 1000000000) snprintf(buf, size, "%.1fB", num / 1000000000.0);
    else if (num >= 1000000) snprintf(buf, size, "%.1fM", num / 1000000.0);
    else if (num >= 1000) snprintf(buf, size, "%.1fK", num / 1000.0);
    else snprintf(buf, size, "%lld", (long long)num);
    return buf;
}

int levenshtein_distance(const char* s1, const char* s2) {
    size_t l1 = strlen(s1), l2 = strlen(s2);
    size_t* v0 = (size_t*)calloc(l2 + 1, sizeof(size_t));
    size_t* v1 = (size_t*)calloc(l2 + 1, sizeof(size_t));
    if (!v0 || !v1) { free(v0); free(v1); return -1; }
    for (size_t i = 0; i <= l2; i++) v0[i] = i;
    for (size_t i = 0; i < l1; i++) {
        v1[0] = i + 1;
        for (size_t j = 0; j < l2; j++) {
            size_t cost = (s1[i] == s2[j]) ? 0 : 1;
            size_t a = v1[j] + 1;
            size_t b = v0[j + 1] + 1;
            size_t c = v0[j] + cost;
            v1[j + 1] = (a < b ? (a < c ? a : c) : (b < c ? b : c));
        }
        size_t* tmp = v0; v0 = v1; v1 = tmp;
    }
    int result = (int)v0[l2];
    free(v0); free(v1);
    return result;
}

char* base64_encode(const uint8_t* data, size_t length) {
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t out_len = 4 * ((length + 2) / 3);
    char* out = (char*)malloc(out_len + 1);
    if (!out) return NULL;
    size_t i, j;
    for (i = 0, j = 0; i < length; i += 3, j += 4) {
        uint32_t octet_a = i < length ? data[i] : 0;
        uint32_t octet_b = i + 1 < length ? data[i + 1] : 0;
        uint32_t octet_c = i + 2 < length ? data[i + 2] : 0;
        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;
        out[j] = table[(triple >> 18) & 0x3F];
        out[j+1] = table[(triple >> 12) & 0x3F];
        out[j+2] = table[(triple >> 6) & 0x3F];
        out[j+3] = table[triple & 0x3F];
    }
    int mod = (int)(length % 3);
    if (mod == 1) { out[out_len - 2] = '='; out[out_len - 1] = '='; }
    else if (mod == 2) { out[out_len - 1] = '='; }
    out[out_len] = '\0';
    return out;
}

uint8_t* base64_decode(const char* str, size_t* out_length) {
    static const int table[128] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
    };
    size_t len = strlen(str);
    if (len % 4 != 0) return NULL;
    size_t padding = (str[len-1] == '=') + (str[len-2] == '=');
    size_t out_len = (len / 4) * 3 - padding;
    uint8_t* out = (uint8_t*)malloc(out_len);
    if (!out) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < len; i += 4) {
        int a = table[(int)str[i]], b = table[(int)str[i+1]];
        int c = table[(int)str[i+2]], d = table[(int)str[i+3]];
        if (a < 0 || b < 0) { free(out); return NULL; }
        uint32_t triple = (uint32_t)(a << 18) | (uint32_t)(b << 12);
        if (c >= 0) triple |= (uint32_t)(c << 6);
        if (d >= 0) triple |= (uint32_t)d;
        out[j++] = (triple >> 16) & 0xFF;
        if (c >= 0 && j < out_len) out[j++] = (triple >> 8) & 0xFF;
        if (d >= 0 && j < out_len) out[j++] = triple & 0xFF;
    }
    if (out_length) *out_length = out_len;
    return out;
}

void xor_cipher(const uint8_t* input, uint8_t* output, size_t length, const char* key) {
    size_t klen = strlen(key);
    for (size_t i = 0; i < length; i++) output[i] = input[i] ^ key[i % klen];
}
