#include "config.h"
#include "utils.h"

static ConfigSection* config_find_section(ConfigFile* cfg, const char* name) {
    for (int i = 0; i < cfg->count; i++) {
        if (strcmp(cfg->sections[i].section, name) == 0)
            return &cfg->sections[i];
    }
    return NULL;
}

static ConfigSection* config_add_section(ConfigFile* cfg, const char* name) {
    ConfigSection* existing = config_find_section(cfg, name);
    if (existing) return existing;
    if (cfg->count >= cfg->capacity) {
        cfg->capacity = cfg->capacity ? cfg->capacity * 2 : 16;
        cfg->sections = (ConfigSection*)realloc(cfg->sections, cfg->capacity * sizeof(ConfigSection));
        if (!cfg->sections) return NULL;
    }
    ConfigSection* sec = &cfg->sections[cfg->count];
    memset(sec, 0, sizeof(ConfigSection));
    snprintf(sec->section, sizeof(sec->section), "%s", name);
    cfg->count++;
    return sec;
}

static ConfigPair* config_find_pair(ConfigSection* section, const char* key) {
    for (int i = 0; i < section->count; i++) {
        if (strcmp(section->pairs[i].key, key) == 0)
            return &section->pairs[i];
    }
    return NULL;
}

static ConfigPair* config_add_pair(ConfigSection* section, const char* key) {
    ConfigPair* existing = config_find_pair(section, key);
    if (existing) return existing;
    if (section->count >= section->capacity) {
        section->capacity = section->capacity ? section->capacity * 2 : 32;
        section->pairs = (ConfigPair*)realloc(section->pairs, section->capacity * sizeof(ConfigPair));
        if (!section->pairs) return NULL;
    }
    ConfigPair* pair = &section->pairs[section->count];
    memset(pair, 0, sizeof(ConfigPair));
    snprintf(pair->key, sizeof(pair->key), "%s", key);
    section->count++;
    return pair;
}

bool config_load(BrowserContext* ctx, const char* filepath) {
    ConfigFile* cfg = &ctx->config;
    snprintf(cfg->filepath, sizeof(cfg->filepath), "%s", filepath);

    if (!file_exists(filepath)) return false;

    int line_count = 0;
    char** lines = file_read_lines(filepath, &line_count);
    if (!lines) return false;

    ConfigSection* current_section = NULL;

    for (int i = 0; i < line_count; i++) {
        char* line = str_dup(lines[i]);
        trim_whitespace(line);

        if (line[0] == '#' || line[0] == ';' || line[0] == '\0') {
            free(line); continue;
        }

        if (line[0] == '[') {
            char* end = strchr(line, ']');
            if (end) {
                *end = '\0';
                current_section = config_add_section(cfg, line + 1);
            }
        } else if (current_section) {
            char* eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char* key = line;
                char* value = eq + 1;
                trim_whitespace(key);
                trim_whitespace(value);

                if (value[0] == '"' && value[strlen(value) - 1] == '"') {
                    value++;
                    value[strlen(value) - 1] = '\0';
                }

                ConfigPair* pair = config_add_pair(current_section, key);
                if (pair) snprintf(pair->value, sizeof(pair->value), "%s", value);
            }
        }
        free(line);
    }
    free_lines(lines, line_count);
    return true;
}

bool config_save(BrowserContext* ctx, const char* filepath) {
    ConfigFile* cfg = &ctx->config;
    const char* path = filepath ? filepath : cfg->filepath;
    if (!path[0]) return false;

    FILE* f = fopen(path, "w");
    if (!f) return false;

    fprintf(f, "# CMDBrowser Configuration File\n");
    fprintf(f, "# Auto-generated on %s\n\n", get_timestamp_str(time(NULL), (char[64]){0}, 64));

    for (int i = 0; i < cfg->count; i++) {
        ConfigSection* sec = &cfg->sections[i];
        fprintf(f, "[%s]\n", sec->section);
        for (int j = 0; j < sec->count; j++) {
            if (strchr(sec->pairs[j].value, ' ') || sec->pairs[j].value[0] == '\0')
                fprintf(f, "%s=\"%s\"\n", sec->pairs[j].key, sec->pairs[j].value);
            else
                fprintf(f, "%s=%s\n", sec->pairs[j].key, sec->pairs[j].value);
        }
        fprintf(f, "\n");
    }
    fclose(f);
    return true;
}

char* config_get(BrowserContext* ctx, const char* section, const char* key, const char* default_val) {
    ConfigSection* sec = config_find_section(&ctx->config, section);
    if (!sec) return (char*)default_val;
    ConfigPair* pair = config_find_pair(sec, key);
    return pair ? pair->value : (char*)default_val;
}

bool config_set(BrowserContext* ctx, const char* section, const char* key, const char* value) {
    ConfigSection* sec = config_add_section(&ctx->config, section);
    if (!sec) return false;
    ConfigPair* pair = config_add_pair(sec, key);
    if (!pair) return false;
    snprintf(pair->value, sizeof(pair->value), "%s", value);
    return true;
}

int config_get_int(BrowserContext* ctx, const char* section, const char* key, int default_val) {
    char* val = config_get(ctx, section, key, NULL);
    return val ? atoi(val) : default_val;
}

bool config_get_bool(BrowserContext* ctx, const char* section, const char* key, bool default_val) {
    char* val = config_get(ctx, section, key, NULL);
    if (!val) return default_val;
    str_to_lower(val);
    return strcmp(val, "true") == 0 || strcmp(val, "1") == 0 || strcmp(val, "yes") == 0;
}

bool config_remove_section(BrowserContext* ctx, const char* section) {
    ConfigSection* sec = config_find_section(&ctx->config, section);
    if (!sec) return false;
    int idx = (int)(sec - ctx->config.sections);
    free(sec->pairs);
    if (idx < ctx->config.count - 1) {
        memmove(&ctx->config.sections[idx], &ctx->config.sections[idx + 1],
            (ctx->config.count - idx - 1) * sizeof(ConfigSection));
    }
    ctx->config.count--;
    return true;
}

bool config_remove_key(BrowserContext* ctx, const char* section, const char* key) {
    ConfigSection* sec = config_find_section(&ctx->config, section);
    if (!sec) return false;
    ConfigPair* pair = config_find_pair(sec, key);
    if (!pair) return false;
    int idx = (int)(pair - sec->pairs);
    if (idx < sec->count - 1) {
        memmove(&sec->pairs[idx], &sec->pairs[idx + 1],
            (sec->count - idx - 1) * sizeof(ConfigPair));
    }
    sec->count--;
    return true;
}

void config_list_sections(BrowserContext* ctx) {
    ConfigFile* cfg = &ctx->config;
    printf("\n" COLOR_BOLD "Configuration Sections:" COLOR_RESET "\n\n");
    for (int i = 0; i < cfg->count; i++) {
        printf("  [%s] (%d keys)\n", cfg->sections[i].section, cfg->sections[i].count);
    }
    printf("\n");
}

void config_list_keys(BrowserContext* ctx, const char* section) {
    ConfigSection* sec = config_find_section(&ctx->config, section);
    if (!sec) { printf("Section [%s] not found.\n", section); return; }
    printf("\n" COLOR_BOLD "[%s] Keys:" COLOR_RESET "\n\n", section);
    for (int i = 0; i < sec->count; i++) {
        printf("  %-30s = %s\n", sec->pairs[i].key, sec->pairs[i].value);
    }
    printf("\n");
}
