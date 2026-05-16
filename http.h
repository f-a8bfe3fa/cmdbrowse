#ifndef HTTP_H
#define HTTP_H

#include "types.h"

bool http_init(void);
void http_cleanup(void);
bool http_execute(HttpRequest* req, HttpResponse* resp);
void http_response_free(HttpResponse* resp);
bool http_download_file(const char* url, const char* output_path, const char* proxy, int timeout_ms);
char* http_quick_get(const char* url, const char* proxy, int timeout_ms);
bool http_check_connectivity(const char* test_url);
void http_set_user_agent(const char* ua);
const char* http_get_default_user_agent(void);
bool http_parse_url(const char* url_str, char* host, size_t host_size, int* port, char* path, size_t path_size, bool* use_ssl);
char* http_build_request_string(HttpRequest* req);
bool http_add_header(HttpRequest* req, const char* key, const char* value);
void http_request_init(HttpRequest* req);
void http_response_init(HttpResponse* resp);

#endif
