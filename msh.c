/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2013-03-30     Bernard      the first verion for finsh
 * 2014-01-03     Bernard      msh can execute module.
 * 2017-07-19     Aubr.Cool    limit argc to RT_FINSH_ARG_MAX
 * 2024-07-12     WKJay        抽离 RT-Thread
 */
#include <string.h>

#ifndef FINSH_ARG_MAX
#define FINSH_ARG_MAX 8
#endif /* FINSH_ARG_MAX */

#include "msh.h"
#include "shell.h"

typedef int (*cmd_function_t)(int argc, char **argv);

int msh_help(int argc, char **argv) {
    FINSH_PRINTF("Finsh shell commands:\r\n");
    {
        struct finsh_syscall *index;

        for (index = _syscall_table_begin; index < _syscall_table_end; FINSH_NEXT_SYSCALL(index)) {
#if defined(FINSH_USING_DESCRIPTION)
            FINSH_PRINTF("%-16s - %s\r\n", index->name, index->desc);
#else
            FINSH_PRINTF("%s ", index->name);
#endif
        }
    }
    FINSH_PRINTF("\r\n");

    return 0;
}
MSH_CMD_EXPORT_ALIAS(msh_help, help, Finsh shell help.);

static int msh_split(char *cmd, uint32_t length, char *argv[FINSH_ARG_MAX]) {
    char *ptr;
    uint32_t position;
    uint32_t argc;
    uint32_t i;

    ptr = cmd;
    position = 0;
    argc = 0;

    while (position < length) {
        /* strip bank and tab */
        while ((*ptr == ' ' || *ptr == '\t') && position < length) {
            *ptr = '\0';
            ptr++;
            position++;
        }

        if (argc >= FINSH_ARG_MAX) {
            FINSH_PRINTF("Too many args ! We only Use:\r\n");
            for (i = 0; i < argc; i++) {
                FINSH_PRINTF("%s ", argv[i]);
            }
            FINSH_PRINTF("\r\n");
            break;
        }

        if (position >= length) break;

        /* handle string */
        if (*ptr == '"') {
            ptr++;
            position++;
            argv[argc] = ptr;
            argc++;

            /* skip this string */
            while (*ptr != '"' && position < length) {
                if (*ptr == '\\') {
                    if (*(ptr + 1) == '"') {
                        ptr++;
                        position++;
                    }
                }
                ptr++;
                position++;
            }
            if (position >= length) break;

            /* skip '"' */
            *ptr = '\0';
            ptr++;
            position++;
        } else {
            argv[argc] = ptr;
            argc++;
            while ((*ptr != ' ' && *ptr != '\t') && position < length) {
                ptr++;
                position++;
            }
            if (position >= length) break;
        }
    }

    return argc;
}

static cmd_function_t msh_get_cmd(char *cmd, int size) {
    struct finsh_syscall *index;
    cmd_function_t cmd_func = NULL;

    for (index = _syscall_table_begin; index < _syscall_table_end; FINSH_NEXT_SYSCALL(index)) {
        if (FINSH_STRNCMP(index->name, cmd, size) == 0 && index->name[size] == '\0') {
            cmd_func = (cmd_function_t)index->func;
            break;
        }
    }

    return cmd_func;
}

static int _msh_exec_cmd(char *cmd, uint32_t length, int *retp) {
    int argc;
    uint32_t cmd0_size = 0;
    cmd_function_t cmd_func;
    char *argv[FINSH_ARG_MAX];

    if (cmd == NULL || retp == NULL) return -1;

    /* find the size of first command */
    while ((cmd[cmd0_size] != ' ' && cmd[cmd0_size] != '\t') && cmd0_size < length) cmd0_size++;
    if (cmd0_size == 0) return -1;

    cmd_func = msh_get_cmd(cmd, cmd0_size);
    if (cmd_func == NULL) return -1;

    /* split arguments */
    FINSH_MEMSET(argv, 0x00, sizeof(argv));
    argc = msh_split(cmd, length, argv);
    if (argc == 0) return -1;

    /* exec this command */
    *retp = cmd_func(argc, argv);
    return 0;
}

int msh_exec(char *cmd, uint32_t length) {
    int cmd_ret;

    /* strim the beginning of command */
    while ((length > 0) && (*cmd == ' ' || *cmd == '\t')) {
        cmd++;
        length--;
    }

    if (length == 0) return 0;

    /* Exec sequence:
     * 1. built-in command
     * 2. module(if enabled)
     */
    if (_msh_exec_cmd(cmd, length, &cmd_ret) == 0) {
        return cmd_ret;
    }

    /* truncate the cmd at the first space. */
    {
        char *tcmd;
        tcmd = cmd;
        while (*tcmd != ' ' && *tcmd != '\0') {
            tcmd++;
        }
        *tcmd = '\0';
    }
    FINSH_PRINTF("%s: command not found.\r\n", cmd);
    return -1;
}

static int str_common(const char *str1, const char *str2) {
    const char *str = str1;

    while ((*str != 0) && (*str2 != 0) && (*str == *str2)) {
        str++;
        str2++;
    }

    return (str - str1);
}

void msh_auto_complete(char *prefix) {
    int length, min_length;
    const char *name_ptr, *cmd_name;
    struct finsh_syscall *index;

    min_length = 0;
    name_ptr = NULL;

    if (*prefix == '\0') {
        msh_help(0, NULL);
        return;
    }

    /* checks in internal command */
    {
        for (index = _syscall_table_begin; index < _syscall_table_end; FINSH_NEXT_SYSCALL(index)) {
            /* skip finsh shell function */
            cmd_name = (const char *)index->name;
            if (FINSH_STRNCMP(prefix, cmd_name, FINSH_STRLEN(prefix)) == 0) {
                if (min_length == 0) {
                    /* set name_ptr */
                    name_ptr = cmd_name;
                    /* set initial length */
                    min_length = FINSH_STRLEN(name_ptr);
                }

                length = str_common(name_ptr, cmd_name);
                if (length < min_length) min_length = length;

                FINSH_PRINTF("%s\r\n", cmd_name);
            }
        }
    }

    /* auto complete string */
    if (name_ptr != NULL) {
        FINSH_STRNCPY(prefix, name_ptr, min_length);
    }

    return;
}
