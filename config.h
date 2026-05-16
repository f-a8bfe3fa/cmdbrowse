#ifndef CONFIG_H
#define CONFIG_H

#include "types.h"

bool config_load(BrowserContext* ctx, const char* filepath);
bool config_save(BrowserContext* ctx, const char* filepath);
char* config_get(BrowserContext* ctx, const char* section, const char* key, const char* default_val);
bool config_set(BrowserContext* ctx, const char* section, const char* key, const char* value);
int  config_get_int(BrowserContext* ctx, const char* section, const char* key, int default_val);
bool config_get_bool(BrowserContext* ctx, const char* section, const char* key, bool default_val);
bool config_remove_section(BrowserContext* ctx, const char* section);
bool config_remove_key(BrowserContext* ctx, const char* section, const char* key);
void config_list_sections(BrowserContext* ctx);
void config_list_keys(BrowserContext* ctx, const char* section);

#endif
