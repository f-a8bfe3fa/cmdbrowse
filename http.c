#include "http.h"
#include "utils.h"
#include <curl/curl.h>
#include <sys/time.h>

static char g_user_agent[256] = "CMDBrowser/2.0 (Windows; CLI Search Client)";
static bool g_curl_initialized = false;

bool http_init(void) {
    CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT);
    g_curl_initialized = (res == CURLE_OK);
    return g_curl_initialized;
}

void http_cleanup(void) {
    if (g_curl_initialized) {
        curl_global_cleanup();
        g_curl_initialized = false;
    }
}

void http_set_user_agent(const char* ua) {
    if (ua && strlen(ua) < sizeof(g_user_agent)) {
        snprintf(g_user_agent, sizeof(g_user_agent), "%s", ua);
    }
}

const char* http_get_default_user_agent(void) {
    return g_user_agent;
}

bool http_parse_url(const char* url_str, char* host, size_t host_size, int* port, char* path, size_t path_size, bool* use_ssl) {
    if (!url_str) return false;
    const char* p = url_str;
    *use_ssl = str_starts_with(p, "https://");
    if (*use_ssl) p += 8;
    else if (str_starts_with(p, "http://")) p += 7;
    else return false;

    if (*port == 0) *port = *use_ssl ? 443 : 80;

    const char* slash = strchr(p, '/');
    const char* colon = strchr(p, ':');
    const char* host_end = slash;

    if (colon && (!slash || colon < slash)) {
        size_t host_len = (size_t)(colon - p);
        if (host_len >= host_size) host_len = host_size - 1;
        memcpy(host, p, host_len);
        host[host_len] = '\0';
        *port = atoi(colon + 1);
    } else {
        size_t host_len = host_end ? (size_t)(host_end - p) : strlen(p);
        if (host_len >= host_size) host_len = host_size - 1;
        memcpy(host, p, host_len);
        host[host_len] = '\0';
    }

    if (slash) {
        snprintf(path, path_size, "%s", slash);
    } else {
        snprintf(path, path_size, "/");
    }
    return true;
}

void http_request_init(HttpRequest* req) {
    memset(req, 0, sizeof(HttpRequest));
    req->method = HTTP_GET;
    req->use_ssl = false;
    req->port = 0;
    req->follow_redirects = true;
    req->timeout_ms = HTTP_TIMEOUT_MS;
}

void http_response_init(HttpResponse* resp) {
    memset(resp, 0, sizeof(HttpResponse));
}

bool http_add_header(HttpRequest* req, const char* key, const char* value) {
    if (req->header_count >= MAX_HEADERS) return false;
    snprintf(req->headers[req->header_count].key, sizeof(req->headers[0].key), "%s", key);
    snprintf(req->headers[req->header_count].value, sizeof(req->headers[0].value), "%s", value);
    req->header_count++;
    return true;
}

char* http_build_request_string(HttpRequest* req) {
    static const char* method_strs[] = { "GET", "POST", "HEAD", "PUT", "DELETE" };
    static char buf[65536];
    int pos = snprintf(buf, sizeof(buf), "%s %s HTTP/1.1\r\nHost: %s\r\n",
        method_strs[req->method], req->path[0] ? req->path : "/", req->host);
    for (int i = 0; i < req->header_count; i++) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%s: %s\r\n",
            req->headers[i].key, req->headers[i].value);
    }
    if (req->body_length > 0) {
        pos += snprintf(buf + pos, sizeof(buf) - pos,
            "Content-Length: %d\r\n", req->body_length);
    }
    pos += snprintf(buf + pos, sizeof(buf) - pos, "Connection: close\r\n\r\n");
    if (req->body_length > 0 && pos + req->body_length < (int)sizeof(buf)) {
        memcpy(buf + pos, req->body, req->body_length);
        pos += req->body_length;
    }
    buf[pos] = '\0';
    return buf;
}

static size_t http_curl_write_callback(void* ptr, size_t size, size_t nmemb, void* userdata) {
    size_t total = size * nmemb;
    ByteBuffer* buf = (ByteBuffer*)userdata;
    byte_buffer_append(buf, ptr, total);
    return total;
}

static size_t http_curl_header_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    size_t total = size * nmemb;
    HttpResponse* resp = (HttpResponse*)userdata;
    if (!resp || total < 2) return total;

    char* line = (char*)malloc(total + 1);
    if (!line) return total;
    memcpy(line, ptr, total);
    line[total] = '\0';

    char* crlf = strstr(line, "\r\n");
    if (crlf) *crlf = '\0';

    if (str_starts_with(line, "HTTP/")) {
        char* space = strchr(line, ' ');
        if (space) {
            int code = atoi(space + 1);
            if (code > 0) resp->status_code = code;
            char* next_space = strchr(space + 1, ' ');
            if (next_space) {
                snprintf(resp->status_text, sizeof(resp->status_text), "%s", next_space + 1);
            }
        }
    } else if (strchr(line, ':')) {
        if (resp->header_count < MAX_HEADERS) {
            char* colon = strchr(line, ':');
            size_t klen = (size_t)(colon - line);
            if (klen < sizeof(resp->headers[0].key)) {
                memcpy(resp->headers[resp->header_count].key, line, klen);
                resp->headers[resp->header_count].key[klen] = '\0';
            }
            const char* val = colon + 1;
            while (*val == ' ' || *val == '\t') val++;
            snprintf(resp->headers[resp->header_count].value, sizeof(resp->headers[0].value), "%s", val);
            resp->header_count++;
        }
    }

    free(line);
    return total;
}

static bool http_curl_execute(HttpRequest* req, HttpResponse* resp) {
    struct timeval tv_start, tv_end;
    gettimeofday(&tv_start, NULL);

    CURL* curl = curl_easy_init();
    if (!curl) return false;

    char full_url[MAX_URL_LENGTH];
    int scheme_len = snprintf(full_url, sizeof(full_url), "%s://", req->use_ssl ? "https" : "http");
    if (scheme_len < 0) scheme_len = 0;
    if (scheme_len < (int)sizeof(full_url)) {
        char host_port[512];
        if (req->port > 0 && ((req->use_ssl && req->port != 443) || (!req->use_ssl && req->port != 80))) {
            snprintf(host_port, sizeof(host_port), "%s:%d", req->host, req->port);
        } else {
            snprintf(host_port, sizeof(host_port), "%s", req->host);
        }
        snprintf(full_url + scheme_len, sizeof(full_url) - scheme_len, "%s%s", host_port, req->path);
    }

    curl_easy_setopt(curl, CURLOPT_URL, full_url);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, g_user_agent);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, req->follow_redirects ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)req->timeout_ms);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, (long)req->timeout_ms);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");

    if (strlen(req->proxy) > 0) {
        curl_easy_setopt(curl, CURLOPT_PROXY, req->proxy);
    }

    static const char* method_strs[] = { "GET", "POST", "HEAD", "PUT", "DELETE" };
    const char* method = method_strs[req->method];
    if (req->method == HTTP_POST) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
    } else if (req->method == HTTP_PUT) {
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 0L);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    } else if (req->method == HTTP_DELETE) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    } else if (req->method == HTTP_HEAD) {
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    } else {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    }

    struct curl_slist* header_list = NULL;
    if (req->header_count > 0) {
        for (int i = 0; i < req->header_count; i++) {
            char header_line[1280];
            snprintf(header_line, sizeof(header_line), "%s: %s", req->headers[i].key, req->headers[i].value);
            header_list = curl_slist_append(header_list, header_line);
        }
    }
    if (header_list) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
    }

    if (req->body_length > 0) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req->body);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)req->body_length);
    }

    ByteBuffer body_buf;
    byte_buffer_init(&body_buf);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_curl_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body_buf);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, http_curl_header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, resp);

    CURLcode res = curl_easy_perform(curl);

    if (res == CURLE_OK) {
        long status_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
        if (resp->status_code == 0) resp->status_code = (int)status_code;

        char* effective_url = NULL;
        curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_url);
        if (effective_url) {
            snprintf(resp->effective_url, sizeof(resp->effective_url), "%s", effective_url);
        }

        double total_time = 0.0;
        curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &total_time);
        resp->elapsed_seconds = total_time;
    }

    if (header_list) curl_slist_free_all(header_list);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        byte_buffer_free(&body_buf);
        return false;
    }

    resp->body = body_buf.data;
    resp->body_length = body_buf.length;
    resp->body_capacity = body_buf.capacity;

    gettimeofday(&tv_end, NULL);
    if (resp->elapsed_seconds == 0.0) {
        resp->elapsed_seconds = (double)(tv_end.tv_sec - tv_start.tv_sec) + (double)(tv_end.tv_usec - tv_start.tv_usec) / 1000000.0;
    }

    if (resp->effective_url[0] == '\0') {
        int scheme_len2 = snprintf(resp->effective_url, sizeof(resp->effective_url), "%s://", req->use_ssl ? "https" : "http");
        if (scheme_len2 < 0) scheme_len2 = 0;
        if (scheme_len2 < (int)sizeof(resp->effective_url)) {
            snprintf(resp->effective_url + scheme_len2, sizeof(resp->effective_url) - scheme_len2, "%s%s", req->host, req->path);
        }
    }

    return resp->status_code > 0;
}

bool http_execute(HttpRequest* req, HttpResponse* resp) {
    if (!g_curl_initialized) {
        if (!http_init()) return false;
    }
    if (!http_parse_url(req->url, req->host, sizeof(req->host), &req->port, req->path, sizeof(req->path), &req->use_ssl)) {
        if (!req->port) req->port = 80;
    }
    return http_curl_execute(req, resp);
}

void http_response_free(HttpResponse* resp) {
    if (resp) {
        free(resp->body);
        resp->body = NULL;
        resp->body_length = 0;
        resp->body_capacity = 0;
    }
}

bool http_download_file(const char* url, const char* output_path, const char* proxy, int timeout_ms) {
    HttpRequest req;
    http_request_init(&req);
    snprintf(req.url, sizeof(req.url), "%s", url);
    req.timeout_ms = timeout_ms;
    if (proxy && strlen(proxy) > 0) snprintf(req.proxy, sizeof(req.proxy), "%s", proxy);

    HttpResponse resp;
    http_response_init(&resp);

    if (!http_execute(&req, &resp) || resp.status_code != 200) {
        http_response_free(&resp);
        return false;
    }

    bool ok = file_write_all(output_path, resp.body, resp.body_length);
    http_response_free(&resp);
    return ok;
}

char* http_quick_get(const char* url, const char* proxy, int timeout_ms) {
    HttpRequest req;
    http_request_init(&req);
    snprintf(req.url, sizeof(req.url), "%s", url);
    req.timeout_ms = timeout_ms > 0 ? timeout_ms : HTTP_TIMEOUT_MS;
    if (proxy && strlen(proxy) > 0) snprintf(req.proxy, sizeof(req.proxy), "%s", proxy);

    HttpResponse resp;
    http_response_init(&resp);

    if (!http_execute(&req, &resp) || resp.status_code < 200 || resp.status_code >= 400) {
        http_response_free(&resp);
        return NULL;
    }

    char* result = (char*)malloc(resp.body_length + 1);
    if (result) {
        memcpy(result, resp.body, resp.body_length);
        result[resp.body_length] = '\0';
    }
    http_response_free(&resp);
    return result;
}

bool http_check_connectivity(const char* test_url) {
    HttpRequest req;
    http_request_init(&req);
    snprintf(req.url, sizeof(req.url), "%s", test_url ? test_url : "https://www.google.com");
    req.timeout_ms = 5000;

    HttpResponse resp;
    http_response_init(&resp);
    bool ok = http_execute(&req, &resp);
    if (ok) http_response_free(&resp);
    return ok;
}
