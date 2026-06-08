#ifndef UTILS_H
#define UTILS_H

#include "types.h"

void url_encode(const char* src, char* dst, size_t dst_size);
void url_decode(const char* src, char* dst, size_t dst_size);
void html_entity_decode(const char* src, char* dst, size_t dst_size);
void strip_html_tags(const char* src, char* dst, size_t dst_size);
void trim_whitespace(char* str);
void str_to_lower(char* str);
void str_to_upper(char* str);
char* str_dup(const char* src);
bool str_contains(const char* haystack, const char* needle);
bool str_starts_with(const char* str, const char* prefix);
bool str_ends_with(const char* str, const char* suffix);
char* str_between(const char* haystack, const char* start, const char* end);
int  str_count(const char* haystack, const char* needle);
void byte_buffer_init(ByteBuffer* buf);
void byte_buffer_append(ByteBuffer* buf, const void* data, size_t length);
void byte_buffer_free(ByteBuffer* buf);
void byte_buffer_reset(ByteBuffer* buf);
char* get_timestamp_str(time_t t, char* buf, size_t size);
char* get_relative_time_str(time_t t, char* buf, size_t size);
bool is_valid_url(const char* url);
void extract_domain(const char* url, char* domain, size_t size);
int  get_terminal_width(void);
int  get_terminal_height(void);
void enable_ansi_escapes(void);
void disable_ansi_escapes(void);
bool file_exists(const char* path);
bool dir_exists(const char* path);
int  file_size_bytes(const char* path);
char* file_read_all(const char* path, size_t* out_size);
bool file_write_all(const char* path, const void* data, size_t size);
bool file_append_line(const char* path, const char* line);
char** file_read_lines(const char* path, int* out_count);
void free_lines(char** lines, int count);
char* get_config_dir(void);
char* get_cache_dir(void);
bool ensure_directory(const char* path);
char* path_join(const char* base, const char* name);
int  random_int(int min, int max);
void sleep_ms(int milliseconds);
char* wchar_to_utf8(const wchar_t* wstr);
wchar_t* utf8_to_wchar(const char* str);
char* json_escape(const char* str);
char* csv_escape(const char* str);
void progress_bar(int current, int total, int width);
void print_centered(const char* str, int width);
void print_banner(void);
char* format_number(int64_t num, char* buf, size_t size);
int  levenshtein_distance(const char* s1, const char* s2);
char* base64_encode(const uint8_t* data, size_t length);
uint8_t* base64_decode(const char* str, size_t* out_length);
void xor_cipher(const uint8_t* input, uint8_t* output, size_t length, const char* key);
bool load_env_file(const char* path);

#endif
