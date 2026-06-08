#ifndef PARSER_H
#define PARSER_H

#include "types.h"

SearchResponse* parser_parse_response(SearchEngine* engine, SearchQuery* query, HttpResponse* http_resp);
SearchResponse* parser_parse_google(const char* html, SearchEngine* engine, SearchQuery* query);
SearchResponse* parser_parse_bing(const char* html, SearchEngine* engine, SearchQuery* query);
SearchResponse* parser_parse_duckduckgo(const char* html, SearchEngine* engine, SearchQuery* query);
SearchResponse* parser_parse_yahoo(const char* html, SearchEngine* engine, SearchQuery* query);
SearchResponse* parser_parse_generic(const char* html, SearchEngine* engine, SearchQuery* query);
SearchResponse* parser_parse_none(const char* html, SearchEngine* engine, SearchQuery* query);
SearchResponse* parser_parse_brave_api(const char* json, SearchEngine* engine, SearchQuery* query);
SearchResponse* parser_parse_ddg_api(const char* json, SearchEngine* engine, SearchQuery* query);
char* parser_extract_text_between(const char* haystack, const char* after, const char* before);
int parser_dedup_results(SearchResult* results, int count);
void parser_sort_results(SearchResult* results, int count, SortOrder order);
void search_response_free(SearchResponse* resp);
SearchResponse* search_merge_responses(SearchResponse** responses, int count, SortOrder sort_order);

#endif
