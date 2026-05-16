#include "http.h"
#include "utils.h"

static char g_user_agent[256] = "CMDBrowser/2.0 (Windows; CLI Search Client)";
static HINTERNET g_hSession = NULL;

bool http_init(void) {
    wchar_t* wua = utf8_to_wchar(g_user_agent);
    if (!wua) return false;
    g_hSession = WinHttpOpen(
        wua,
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    if (!g_hSession && GetLastError() == ERROR_WINHTTP_SECURE_FAILURE) {
        g_hSession = WinHttpOpen(wua,
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS,
            WINHTTP_FLAG_SECURE_PROTOCOL_ALL);
    }
    free(wua);
    return g_hSession != NULL;
}

void http_cleanup(void) {
    if (g_hSession) { WinHttpCloseHandle(g_hSession); g_hSession = NULL; }
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

static bool http_winhttp_execute(HttpRequest* req, HttpResponse* resp) {
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);

    wchar_t* whost = utf8_to_wchar(req->host);
    wchar_t* wpath = utf8_to_wchar(req->path);
    if (!whost || !wpath) { free(whost); free(wpath); return false; }

    HINTERNET hConnect = WinHttpConnect(g_hSession, whost,
        (INTERNET_PORT)(req->port > 0 ? req->port : (req->use_ssl ? 443 : 80)), 0);
    free(whost);
    if (!hConnect) { free(wpath); return false; }

    DWORD flags = req->use_ssl ? WINHTTP_FLAG_SECURE : 0;
    static const wchar_t* methods[] = { L"GET", L"POST", L"HEAD", L"PUT", L"DELETE" };
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, methods[req->method], wpath,
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    free(wpath);
    if (!hRequest) { WinHttpCloseHandle(hConnect); return false; }

    if (req->use_ssl) {
        DWORD sec_flags = 0;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &sec_flags, sizeof(sec_flags));
    }

    WinHttpSetTimeouts(hRequest, req->timeout_ms, req->timeout_ms, req->timeout_ms, req->timeout_ms);

    if (!req->follow_redirects) {
        DWORD disable_redirects = WINHTTP_DISABLE_REDIRECTS;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_DISABLE_FEATURE, &disable_redirects, sizeof(disable_redirects));
    }

    if (strlen(req->proxy) > 0) {
        wchar_t* wproxy = utf8_to_wchar(req->proxy);
        if (wproxy) {
            WINHTTP_PROXY_INFO proxyInfo = { WINHTTP_ACCESS_TYPE_NAMED_PROXY, wproxy, NULL };
            WinHttpSetOption(hRequest, WINHTTP_OPTION_PROXY, &proxyInfo, sizeof(proxyInfo));
            free(wproxy);
        }
    }

    if (req->header_count > 0) {
        char headers_buf[8192] = {0};
        int pos = 0;
        for (int i = 0; i < req->header_count; i++) {
            pos += snprintf(headers_buf + pos, sizeof(headers_buf) - pos,
                "%s: %s\r\n", req->headers[i].key, req->headers[i].value);
        }
        wchar_t* wheaders = utf8_to_wchar(headers_buf);
        if (wheaders) {
            WinHttpAddRequestHeaders(hRequest, wheaders, (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);
            free(wheaders);
        }
    }

    bool send_ok = false;
    if (req->body_length > 0) {
        send_ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            (LPVOID)req->body, (DWORD)req->body_length, (DWORD)req->body_length, 0);
    } else {
        send_ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    }

    if (!send_ok) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); return false; }

    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); return false;
    }

    DWORD status_code = 0, status_code_size = sizeof(status_code);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &status_code_size, WINHTTP_NO_HEADER_INDEX);
    resp->status_code = (int)status_code;

    wchar_t wstatus_text[128];
    DWORD st_size = sizeof(wstatus_text);
    if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_TEXT,
        WINHTTP_HEADER_NAME_BY_INDEX, wstatus_text, &st_size, WINHTTP_NO_HEADER_INDEX)) {
        char* st = wchar_to_utf8(wstatus_text);
        if (st) { snprintf(resp->status_text, sizeof(resp->status_text), "%s", st); free(st); }
    }

    wchar_t* raw_headers = NULL;
    DWORD raw_header_size = 0;
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF,
        WINHTTP_HEADER_NAME_BY_INDEX, NULL, &raw_header_size, WINHTTP_NO_HEADER_INDEX);
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && raw_header_size > 0) {
        raw_headers = (wchar_t*)malloc(raw_header_size);
        if (raw_headers && WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF,
            WINHTTP_HEADER_NAME_BY_INDEX, raw_headers, &raw_header_size, WINHTTP_NO_HEADER_INDEX)) {
            char* hr = wchar_to_utf8(raw_headers);
            if (hr) {
                char* line = strtok(hr, "\r\n");
                int h_idx = 0;
                while (line && h_idx < MAX_HEADERS) {
                    if (strchr(line, ':')) {
                        char* colon = strchr(line, ':');
                        size_t klen = (size_t)(colon - line);
                        if (klen < sizeof(resp->headers[0].key)) {
                            memcpy(resp->headers[h_idx].key, line, klen);
                            resp->headers[h_idx].key[klen] = '\0';
                        }
                        const char* val = colon + 1;
                        while (*val == ' ') val++;
                        snprintf(resp->headers[h_idx].value, sizeof(resp->headers[0].value), "%s", val);
                        h_idx++;
                    }
                    line = strtok(NULL, "\r\n");
                }
                resp->header_count = h_idx;
                free(hr);
            }
        }
        free(raw_headers);
    }

    DWORD available = 0;
    ByteBuffer body_buf;
    byte_buffer_init(&body_buf);

    while (WinHttpQueryDataAvailable(hRequest, &available) && available > 0) {
        if (available > HTTP_BUFFER_SIZE) available = HTTP_BUFFER_SIZE;
        char* chunk = (char*)malloc(available + 1);
        if (!chunk) break;
        DWORD bytes_read = 0;
        if (WinHttpReadData(hRequest, chunk, available, &bytes_read)) {
            if (bytes_read > 0) bytes_read = min(bytes_read, available);
            byte_buffer_append(&body_buf, chunk, bytes_read);
        }
        free(chunk);
        if (bytes_read == 0) break;
        available = 0;
    }

    resp->body = body_buf.data;
    resp->body_length = body_buf.length;
    resp->body_capacity = body_buf.capacity;

    QueryPerformanceCounter(&end);
    resp->elapsed_seconds = (double)(end.QuadPart - start.QuadPart) / freq.QuadPart;

    int scheme_len = snprintf(resp->effective_url, sizeof(resp->effective_url),
        "%s://", req->use_ssl ? "https" : "http");
    if (scheme_len < 0) scheme_len = 0;
    if (scheme_len < (int)sizeof(resp->effective_url)) {
        snprintf(resp->effective_url + scheme_len, sizeof(resp->effective_url) - scheme_len,
            "%s%s", req->host, req->path);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    return resp->status_code > 0;
}

bool http_execute(HttpRequest* req, HttpResponse* resp) {
    if (!g_hSession) {
        if (!http_init()) return false;
    }
    if (!http_parse_url(req->url, req->host, sizeof(req->host), &req->port, req->path, sizeof(req->path), &req->use_ssl)) {
        if (!req->port) req->port = 80;
    }
    return http_winhttp_execute(req, resp);
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
