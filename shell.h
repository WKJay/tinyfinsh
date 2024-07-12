/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2011-06-02     Bernard      Add finsh_get_prompt function declaration
 * 2024-07-12     WKJay        抽离 RT-Thread
 */

#ifndef __SHELL_H__
#define __SHELL_H__

#include "finsh.h"

#ifndef FINSH_CONSOLEBUF_SIZE
#define FINSH_CONSOLEBUF_SIZE 128
#endif

#ifndef FINSH_CMD_SIZE
#define FINSH_CMD_SIZE 80
#endif

#ifndef FINSH_USING_USER_LIB_FUNC
#include <stdio.h>
#include <string.h>

#define FINSH_PRINTF(...) printf(__VA_ARGS__)
#define FINSH_MEMSET      memset
#define FINSH_MEMCPY      memcpy
#define FINSH_MEMCMP      memcmp
#define FINSH_STRCPY      strcpy
#define FINSH_STRNCPY     strncpy
#define FINSH_STRCAT      strcat
#define FINSH_STRLEN      strlen
#define FINSH_STRNCMP     strncmp
#define FINSH_MEMMOVE     memmove
#endif

#define FINSH_OPTION_ECHO 0x01

#define FINSH_PROMPT finsh_get_prompt()
const char *finsh_get_prompt(void);
int finsh_set_prompt(const char *prompt);

#ifdef FINSH_USING_HISTORY
#ifndef FINSH_HISTORY_LINES
#define FINSH_HISTORY_LINES 5
#endif
#endif

#ifdef FINSH_USING_AUTH
#ifndef FINSH_PASSWORD_MAX
#define FINSH_PASSWORD_MAX 8
#endif
#ifndef FINSH_PASSWORD_MIN
#define FINSH_PASSWORD_MIN 6
#endif
#ifndef FINSH_DEFAULT_PASSWORD
#define FINSH_DEFAULT_PASSWORD "rtthread"
#endif
#endif /* FINSH_USING_AUTH */

enum input_stat {
    WAIT_NORMAL,
    WAIT_SPEC_KEY,
    WAIT_FUNC_KEY,
};
struct finsh_shell {
    enum input_stat stat;

    uint8_t echo_mode : 1;
    uint8_t prompt_mode : 1;

#ifdef FINSH_USING_HISTORY
    uint16_t current_history;
    uint16_t history_count;

    char cmd_history[FINSH_HISTORY_LINES][FINSH_CMD_SIZE];
#endif

    char line[FINSH_CMD_SIZE + 1];
    uint16_t line_position;
    uint16_t line_curpos;

#ifdef FINSH_USING_AUTH
    char password[FINSH_PASSWORD_MAX];
#endif

    int (*get_char)(void);
};

void finsh_set_echo(uint32_t echo);
uint32_t finsh_get_echo(void);

uint32_t finsh_get_prompt_mode(void);
void finsh_set_prompt_mode(uint32_t prompt_mode);

#ifdef FINSH_USING_AUTH
int finsh_set_password(const char *password);
const char *finsh_get_password(void);
#endif

#endif
