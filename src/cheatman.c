/*
 * Manage cheat codes
 *
 * Copyright (C) 2009-2010 Mathias Lafeldt <misfire@debugon.org>
 * Copyright (C) 2014 doctorxyz
 *
 * This file is part of PS2rd, the PS2 remote debugger.
 *
 * PS2rd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PS2rd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with PS2rd.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "include/opl.h"
#include "include/cheatman.h"
#include "include/util.h"
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

cheat_entry_t gCheats[MAX_CODES];
static u32 gCheatList[MAX_CHEATLIST];
static int gEnableCheat = 0;
static int gCheatMode = 0;

int is_master_cheat(int index)
{
    if (index < 0 || index >= MAX_CODES)
        return 0;
    if (gCheats[index].name[0] == '\0')
        return 0;
    if (strncasecmp(gCheats[index].name, "[Enable]", 8) == 0 ||
        strncasecmp(gCheats[index].name, "Mastercode", 10) == 0)
        return 1;
    return 0;
}

int GetCheatsCount(void)
{
    int i;
    for (i = 0; i < MAX_CODES; i++) {
        if (gCheats[i].name[0] == '\0')
            break;
    }
    return i;
}

int GetCheatsEnabled(void)
{
    return gEnableCheat;
}

int GetCheatMode(void)
{
    return gCheatMode;
}

const u32 *GetCheatsList(void)
{
    return gCheatList;
}

void load_cheats_config(config_set_t *configSet)
{
    char *val = NULL;
    int i;

    for (i = 0; i < MAX_CODES; i++)
        gCheats[i].enabled = 1;

    if (!configSet)
        return;

    if (configGetStr(configSet, "$CheatDisabled", &val) && val) {
        int len = strlen(val);
        for (i = 0; i < MAX_CODES && i < len * 4; i++) {
            char c = val[i / 4];
            int hex = 0;
            if (c >= '0' && c <= '9') hex = c - '0';
            else if (c >= 'a' && c <= 'f') hex = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') hex = c - 'A' + 10;
            else break;

            if ((hex >> (i % 4)) & 1)
                gCheats[i].enabled = 0;
            else
                gCheats[i].enabled = 1;
        }
    }
}

void InitCheatsConfig(config_set_t *configSet)
{
    int cheatSource = SETTINGS_GLOBAL;
    gEnableCheat = 0;
    gCheatMode = 0;

    if (configSet) {
        if (configGetInt(configSet, "$CheatsSource", &cheatSource) && cheatSource == SETTINGS_PERGAME) {
            configGetInt(configSet, "$EnableCheat", &gEnableCheat);
            configGetInt(configSet, "$CheatMode", &gCheatMode);
            load_cheats_config(configSet);
            return;
        }
    }

    config_set_t *globalConfig = configGetByType(CONFIG_OPL);
    if (globalConfig) {
        configGetInt(globalConfig, "$EnableCheat", &gEnableCheat);
        configGetInt(globalConfig, "$CheatMode", &gCheatMode);
    }
    load_cheats_config(configSet);
}

void save_cheats(config_set_t *configSet)
{
    int count = GetCheatsCount();
    if (count == 0 || !configSet)
        return;

    int hex_len = (count + 3) / 4;
    char hex_str[65];
    if (hex_len > 64) hex_len = 64;
    memset(hex_str, '0', hex_len);
    hex_str[hex_len] = '\0';

    int any_disabled = 0;
    int i;
    for (i = 0; i < count; i++) {
        if (!gCheats[i].enabled && !is_master_cheat(i)) {
            any_disabled = 1;
            int digit = i / 4;
            int bit = i % 4;
            if (digit < hex_len) {
                char c = hex_str[digit];
                int val = (c >= '0' && c <= '9') ? (c - '0') : ((c >= 'a' && c <= 'f') ? (c - 'a' + 10) : (c - 'A' + 10));
                val |= (1 << bit);
                hex_str[digit] = "0123456789abcdef"[val & 0xF];
            }
        }
    }

    if (any_disabled) {
        configSetStr(configSet, "$CheatDisabled", hex_str);
    } else {
        configRemoveKey(configSet, "$CheatDisabled");
    }
}

void set_cheats_list(void)
{
    int i, j;
    int cheat_count = 0;
    int list_index = 0;

    for (i = 0; i < MAX_CODES; i++) {
        if (gCheats[i].name[0] != '\0')
            cheat_count++;
        else
            break;
    }

    memset(gCheatList, 0, sizeof(gCheatList));

    if (cheat_count == 0) {
        gCheatList[0] = 0;
        gCheatList[1] = 0;
        return;
    }

    for (i = 0; i < cheat_count; i++) {
        if (!gCheats[i].enabled && !is_master_cheat(i))
            continue;

        for (j = 0; j < MAX_CHEATLIST; j++) {
            if (gCheats[i].codes[j].addr == 0)
                break;

            if (list_index < MAX_CHEATLIST - 2) {
                gCheatList[list_index++] = gCheats[i].codes[j].addr;
                gCheatList[list_index++] = gCheats[i].codes[j].val;
            }
        }
    }

    gCheatList[list_index] = 0;
    if (list_index + 1 < MAX_CHEATLIST)
        gCheatList[list_index + 1] = 0;
}

int load_cheats(const char *cheatfile)
{
    int fd;
    int size;
    char *buf;
    char *p, *line, *next;
    int cheat_idx = -1;
    int code_idx = 0;
    int i;

    memset(gCheats, 0, sizeof(gCheats));

    fd = openFile((char *)cheatfile, O_RDONLY);
    if (fd < 0)
        return -1;

    size = lseek(fd, 0, SEEK_END);
    if (size <= 0) {
        close(fd);
        return -1;
    }
    lseek(fd, 0, SEEK_SET);

    buf = (char *)malloc(size + 1);
    if (!buf) {
        close(fd);
        return -1;
    }

    if (read(fd, buf, size) != size) {
        free(buf);
        close(fd);
        return -1;
    }
    close(fd);
    buf[size] = '\0';

    p = buf;
    while (p && *p) {
        next = strchr(p, '\n');
        if (next) {
            *next = '\0';
            line = p;
            p = next + 1;
        } else {
            line = p;
            p = NULL;
        }

        // Strip comments
        char *comment = strstr(line, "//");
        if (comment) *comment = '\0';
        comment = strchr(line, '#');
        if (comment) *comment = '\0';

        // Strip carriage return
        char *cr = strchr(line, '\r');
        if (cr) *cr = '\0';

        // Trim leading whitespace
        while (*line && isspace((unsigned char)*line)) line++;

        // Trim trailing whitespace
        int len = strlen(line);
        while (len > 0 && isspace((unsigned char)line[len - 1])) {
            line[--len] = '\0';
        }

        if (len == 0) continue;

        // Check if line is code (16 hex digits with optional space)
        int hex_count = 0;
        int is_code = 1;
        char hex_buf[32];

        for (i = 0; i < len; i++) {
            if (isxdigit((unsigned char)line[i])) {
                if (hex_count < 16)
                    hex_buf[hex_count++] = line[i];
            } else if (isspace((unsigned char)line[i])) {
                continue;
            } else {
                is_code = 0;
                break;
            }
        }

        if (is_code && hex_count == 16) {
            hex_buf[16] = '\0';
            char addr_str[9], val_str[9];
            memcpy(addr_str, hex_buf, 8);
            addr_str[8] = '\0';
            memcpy(val_str, hex_buf + 8, 8);
            val_str[8] = '\0';

            u32 addr = (u32)strtoul(addr_str, NULL, 16);
            u32 val = (u32)strtoul(val_str, NULL, 16);

            if (cheat_idx >= 0 && code_idx < MAX_CHEATLIST) {
                gCheats[cheat_idx].codes[code_idx].addr = addr;
                gCheats[cheat_idx].codes[code_idx].val = val;
                code_idx++;
            }
        } else {
            // New cheat title
            if (cheat_idx + 1 < MAX_CODES) {
                cheat_idx++;
                strncpy(gCheats[cheat_idx].name, line, CHEAT_NAME_MAX);
                gCheats[cheat_idx].name[CHEAT_NAME_MAX] = '\0';
                gCheats[cheat_idx].enabled = 1;
                code_idx = 0;
            }
        }
    }

    free(buf);

    int total_loaded = cheat_idx + 1;
    if (total_loaded > 0) {
        for (i = 0; i < total_loaded; i++) {
            if (is_master_cheat(i))
                gCheats[i].enabled = 1;
        }
        return (gCheatMode > 0) ? 1 : 0;
    }

    return -1;
}
