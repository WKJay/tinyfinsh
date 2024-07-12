/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2006-04-30     Bernard      the first version for FinSH
 * 2006-05-08     Bernard      change finsh thread stack to 2048
 * 2006-06-03     Bernard      add support for skyeye
 * 2006-09-24     Bernard      remove the code related with hardware
 * 2010-01-18     Bernard      fix down then up key bug.
 * 2010-03-19     Bernard      fix backspace issue and fix device read in shell.
 * 2010-04-01     Bernard      add prompt output when start and remove the empty history
 * 2011-02-23     Bernard      fix variable section end issue of finsh shell
 *                             initialization when use GNU GCC compiler.
 * 2016-11-26     armink       add password authentication
 * 2018-07-02     aozima       add custom prompt support.
 * 2024-07-12     WKJay        抽离 RT-Thread
 */

#include <string.h>
#include <stdio.h>

#include "shell.h"
#include "msh.h"

struct finsh_syscall *_syscall_table_begin = NULL;
struct finsh_syscall *_syscall_table_end = NULL;

struct finsh_shell g_shell;
struct finsh_shell *shell;
static char *finsh_prompt_custom = NULL;

#if defined(_MSC_VER) || (defined(__GNUC__) && defined(__x86_64__))
struct finsh_syscall *finsh_syscall_next(struct finsh_syscall *call) {
    unsigned int *ptr;
    ptr = (unsigned int *)(call + 1);
    while ((*ptr == 0) && ((unsigned int *)ptr < (unsigned int *)_syscall_table_end)) ptr++;

    return (struct finsh_syscall *)ptr;
}

#endif /* defined(_MSC_VER) || (defined(__GNUC__) && defined(__x86_64__)) */

#define _MSH_PROMPT "msh "

const char *finsh_get_prompt(void) {
    static char finsh_prompt[FINSH_CONSOLEBUF_SIZE + 1] = {0};

    /* check prompt mode */
    if (!shell->prompt_mode) {
        finsh_prompt[0] = '\0';
        return finsh_prompt;
    }

    if (finsh_prompt_custom) {
        FINSH_STRNCPY(finsh_prompt, finsh_prompt_custom, sizeof(finsh_prompt) - 1);
        return finsh_prompt;
    }
    FINSH_STRCPY(finsh_prompt, _MSH_PROMPT);

    FINSH_STRCAT(finsh_prompt, ">");

    return finsh_prompt;
}

/**
 * @ingroup finsh
 *
 * This function get the prompt mode of finsh shell.
 *
 * @return prompt the prompt mode, 0 disable prompt mode, other values enable prompt mode.
 */
uint32_t finsh_get_prompt_mode(void) {
    if (shell == NULL) {
        FINSH_PRINTF("shell is NULL\r\n");
        return 0;
    }
    return shell->prompt_mode;
}

/**
 * @ingroup finsh
 *
 * This function set the prompt mode of finsh shell.
 *
 * The parameter 0 disable prompt mode, other values enable prompt mode.
 *
 * @param prompt the prompt mode
 */
void finsh_set_prompt_mode(uint32_t prompt_mode) {
    if (shell == NULL) {
        FINSH_PRINTF("shell is NULL\r\n");
        return;
    }
    shell->prompt_mode = prompt_mode;
}

/**
 * @ingroup finsh
 *
 * This function set the echo mode of finsh shell.
 *
 * FINSH_OPTION_ECHO=0x01 is echo mode, other values are none-echo mode.
 *
 * @param echo the echo mode
 */
void finsh_set_echo(uint32_t echo) {
    if (shell == NULL) {
        FINSH_PRINTF("shell is NULL\r\n");
        return;
    }
    shell->echo_mode = (uint8_t)echo;
}

/**
 * @ingroup finsh
 *
 * This function gets the echo mode of finsh shell.
 *
 * @return the echo mode
 */
uint32_t finsh_get_echo() {
    if (shell == NULL) {
        FINSH_PRINTF("shell is NULL\r\n");
        return 0;
    }

    return shell->echo_mode;
}

#ifdef FINSH_USING_AUTH
/**
 * set a new password for finsh
 *
 * @param password new password
 *
 * @return result, 0 on OK, -1 on the new password length is less than
 *  FINSH_PASSWORD_MIN or greater than FINSH_PASSWORD_MAX
 */
int finsh_set_password(const char *password) {
    // rt_ubase_t level;
    uint32_t pw_len = FINSH_STRLEN(password);

    if (pw_len < FINSH_PASSWORD_MIN || pw_len > FINSH_PASSWORD_MAX) return -1;

    // level = rt_hw_interrupt_disable();
    FINSH_STRNCPY(shell->password, password, FINSH_PASSWORD_MAX);
    // rt_hw_interrupt_enable(level);

    return 0;
}

/**
 * get the finsh password
 *
 * @return password
 */
const char *finsh_get_password(void) { return shell->password; }

static void finsh_wait_auth(void) {
    int ch;
    uint8_t input_finish = 0;
    char password[FINSH_PASSWORD_MAX] = {0};
    uint32_t cur_pos = 0;
    /* password not set */
    if (FINSH_STRLEN(finsh_get_password()) == 0) return;

    while (1) {
        FINSH_PRINTF("Password for login: ");
        while (!input_finish) {
            while (1) {
                /* read one character from device */
                ch = (int)shell->get_char();
                if (ch < 0) {
                    continue;
                }

                if (ch >= ' ' && ch <= '~' && cur_pos < FINSH_PASSWORD_MAX) {
                    /* change the printable characters to '*' */
                    FINSH_PRINTF("*");
                    password[cur_pos++] = ch;
                } else if (ch == '\b' && cur_pos > 0) {
                    /* backspace */
                    cur_pos--;
                    password[cur_pos] = '\0';
                    FINSH_PRINTF("\b \b");
                } else if (ch == '\r' || ch == '\n') {
                    FINSH_PRINTF("\r\n");
                    input_finish = 1;
                    break;
                }
            }
        }
        if (!FINSH_STRNCMP(shell->password, password, FINSH_PASSWORD_MAX))
            return;
        else {
            FINSH_PRINTF("Sorry, try again.\r\n");
            cur_pos = 0;
            input_finish = 0;
            FINSH_MEMSET(password, '\0', FINSH_PASSWORD_MAX);
        }
    }
}
#endif /* FINSH_USING_AUTH */

static void shell_auto_complete(char *prefix) {
    FINSH_PRINTF("\r\n");
    msh_auto_complete(prefix);

    FINSH_PRINTF("%s%s", FINSH_PROMPT, prefix);
}

#ifdef FINSH_USING_HISTORY
static uint8_t shell_handle_history(struct finsh_shell *shell) {
#if defined(_WIN32)
    int i;
    FINSH_PRINTF("\r");

    for (i = 0; i <= 60; i++) putchar(' ');
    FINSH_PRINTF("\r");

#else
    FINSH_PRINTF("\033[2K\r");
#endif
    FINSH_PRINTF("%s%s", FINSH_PROMPT, shell->line);
    return 0;
}

static void shell_push_history(struct finsh_shell *shell) {
    if (shell->line_position != 0) {
        /* push history */
        if (shell->history_count >= FINSH_HISTORY_LINES) {
            /* if current cmd is same as last cmd, don't push */
            if (FINSH_MEMCMP(&shell->cmd_history[FINSH_HISTORY_LINES - 1], shell->line, FINSH_CMD_SIZE)) {
                /* move history */
                int index;
                for (index = 0; index < FINSH_HISTORY_LINES - 1; index++) {
                    FINSH_MEMCPY(&shell->cmd_history[index][0], &shell->cmd_history[index + 1][0], FINSH_CMD_SIZE);
                }
                FINSH_MEMSET(&shell->cmd_history[index][0], 0, FINSH_CMD_SIZE);
                FINSH_MEMCPY(&shell->cmd_history[index][0], shell->line, shell->line_position);

                /* it's the maximum history */
                shell->history_count = FINSH_HISTORY_LINES;
            }
        } else {
            /* if current cmd is same as last cmd, don't push */
            if (shell->history_count == 0 || FINSH_MEMCMP(&shell->cmd_history[shell->history_count - 1], shell->line, FINSH_CMD_SIZE)) {
                shell->current_history = shell->history_count;
                FINSH_MEMSET(&shell->cmd_history[shell->history_count][0], 0, FINSH_CMD_SIZE);
                FINSH_MEMCPY(&shell->cmd_history[shell->history_count][0], shell->line, shell->line_position);

                /* increase count and set current history position */
                shell->history_count++;
            }
        }
    }
    shell->current_history = shell->history_count;
}
#endif

void finsh_run(void) {
    int ch;

    /* normal is echo mode */
#ifndef FINSH_ECHO_DISABLE_DEFAULT
    shell->echo_mode = 1;
#else
    shell->echo_mode = 0;
#endif

#ifdef FINSH_USING_AUTH
    /* set the default password when the password isn't setting */
    if (FINSH_STRLEN(finsh_get_password()) == 0) {
        if (finsh_set_password(FINSH_DEFAULT_PASSWORD) != 0) {
            FINSH_PRINTF("Finsh password set failed.\r\n");
        }
    }
    /* waiting authenticate success */
    finsh_wait_auth();
#endif
    FINSH_PRINTF("\r\n");
    FINSH_PRINTF(FINSH_PROMPT);

    while (1) {
        ch = (int)shell->get_char();
        if (ch < 0) {
            continue;
        }

        /*
         * handle control key
         * up key  : 0x1b 0x5b 0x41
         * down key: 0x1b 0x5b 0x42
         * right key:0x1b 0x5b 0x43
         * left key: 0x1b 0x5b 0x44
         */
        if (ch == 0x1b) {
            shell->stat = WAIT_SPEC_KEY;
            continue;
        } else if (shell->stat == WAIT_SPEC_KEY) {
            if (ch == 0x5b) {
                shell->stat = WAIT_FUNC_KEY;
                continue;
            }

            shell->stat = WAIT_NORMAL;
        } else if (shell->stat == WAIT_FUNC_KEY) {
            shell->stat = WAIT_NORMAL;

            if (ch == 0x41) /* up key */
            {
#ifdef FINSH_USING_HISTORY
                /* prev history */
                if (shell->current_history > 0)
                    shell->current_history--;
                else {
                    shell->current_history = 0;
                    continue;
                }

                /* copy the history command */
                FINSH_MEMCPY(shell->line, &shell->cmd_history[shell->current_history][0], FINSH_CMD_SIZE);
                shell->line_curpos = shell->line_position = FINSH_STRLEN(shell->line);
                shell_handle_history(shell);
#endif
                continue;
            } else if (ch == 0x42) /* down key */
            {
#ifdef FINSH_USING_HISTORY
                /* next history */
                if (shell->current_history < shell->history_count - 1)
                    shell->current_history++;
                else {
                    /* set to the end of history */
                    if (shell->history_count != 0)
                        shell->current_history = shell->history_count - 1;
                    else
                        continue;
                }

                FINSH_MEMCPY(shell->line, &shell->cmd_history[shell->current_history][0], FINSH_CMD_SIZE);
                shell->line_curpos = shell->line_position = FINSH_STRLEN(shell->line);
                shell_handle_history(shell);
#endif
                continue;
            } else if (ch == 0x44) /* left key */
            {
                if (shell->line_curpos) {
                    FINSH_PRINTF("\b");
                    shell->line_curpos--;
                }

                continue;
            } else if (ch == 0x43) /* right key */
            {
                if (shell->line_curpos < shell->line_position) {
                    FINSH_PRINTF("%c", shell->line[shell->line_curpos]);
                    shell->line_curpos++;
                }

                continue;
            }
        }

        /* received null or error */
        if (ch == '\0' || ch == 0xFF) continue;
        /* handle tab key */
        else if (ch == '\t') {
            int i;
            /* move the cursor to the beginning of line */
            for (i = 0; i < shell->line_curpos; i++) FINSH_PRINTF("\b");

            /* auto complete */
            shell_auto_complete(&shell->line[0]);
            /* re-calculate position */
            shell->line_curpos = shell->line_position = FINSH_STRLEN(shell->line);

            continue;
        }
        /* handle backspace key */
        else if (ch == 0x7f || ch == 0x08) {
            /* note that shell->line_curpos >= 0 */
            if (shell->line_curpos == 0) continue;

            shell->line_position--;
            shell->line_curpos--;

            if (shell->line_position > shell->line_curpos) {
                int i;

                FINSH_MEMMOVE(&shell->line[shell->line_curpos], &shell->line[shell->line_curpos + 1], shell->line_position - shell->line_curpos);
                shell->line[shell->line_position] = 0;

                FINSH_PRINTF("\b%s  \b", &shell->line[shell->line_curpos]);

                /* move the cursor to the origin position */
                for (i = shell->line_curpos; i <= shell->line_position; i++) FINSH_PRINTF("\b");
            } else {
                FINSH_PRINTF("\b \b");
                shell->line[shell->line_position] = 0;
            }

            continue;
        }

        /* handle end of line, break */
        if (ch == '\r' || ch == '\n') {
#ifdef FINSH_USING_HISTORY
            shell_push_history(shell);
#endif
            if (shell->echo_mode) FINSH_PRINTF("\r\n");
            msh_exec(shell->line, shell->line_position);

            FINSH_PRINTF(FINSH_PROMPT);
            FINSH_MEMSET(shell->line, 0, sizeof(shell->line));
            shell->line_curpos = shell->line_position = 0;
            continue;
        }

        /* it's a large line, discard it */
        if (shell->line_position >= FINSH_CMD_SIZE) shell->line_position = 0;

        /* normal character */
        if (shell->line_curpos < shell->line_position) {
            int i;

            FINSH_MEMMOVE(&shell->line[shell->line_curpos + 1], &shell->line[shell->line_curpos], shell->line_position - shell->line_curpos);
            shell->line[shell->line_curpos] = ch;
            if (shell->echo_mode) FINSH_PRINTF("%s", &shell->line[shell->line_curpos]);

            /* move the cursor to new position */
            for (i = shell->line_curpos; i < shell->line_position; i++) FINSH_PRINTF("\b");
        } else {
            shell->line[shell->line_position] = ch;
            if (shell->echo_mode) FINSH_PRINTF("%c", ch);
        }

        ch = 0;
        shell->line_position++;
        shell->line_curpos++;
        if (shell->line_position >= FINSH_CMD_SIZE) {
            /* clear command line */
            shell->line_position = 0;
            shell->line_curpos = 0;
        }
    } /* end of device read */
}

void finsh_system_function_init(const void *begin, const void *end) {
    _syscall_table_begin = (struct finsh_syscall *)begin;
    _syscall_table_end = (struct finsh_syscall *)end;
}

#if defined(__ICCARM__) || defined(__ICCRX__) /* for IAR compiler */
#ifdef FINSH_USING_SYMTAB
#pragma section = "FSymTab"
#endif
#elif defined(__ADSPBLACKFIN__) /* for VisaulDSP++ Compiler*/
#ifdef FINSH_USING_SYMTAB
extern "asm" int __fsymtab_start;
extern "asm" int __fsymtab_end;
#endif
#elif defined(_MSC_VER)
#pragma section("FSymTab$a", read)
const char __fsym_begin_name[] = "__start";
const char __fsym_begin_desc[] = "begin of finsh";
__declspec(allocate("FSymTab$a")) const struct finsh_syscall __fsym_begin = {__fsym_begin_name, __fsym_begin_desc, NULL};

#pragma section("FSymTab$z", read)
const char __fsym_end_name[] = "__end";
const char __fsym_end_desc[] = "end of finsh";
__declspec(allocate("FSymTab$z")) const struct finsh_syscall __fsym_end = {__fsym_end_name, __fsym_end_desc, NULL};
#endif

/*
 * @ingroup finsh
 *
 * This function will initialize finsh shell
 */
int finsh_system_init(finsh_shell_cfg_t *cfg) {
    if (cfg == NULL) {
        return -1;
    }

    shell = &g_shell;
    FINSH_MEMSET(shell, 0, sizeof(struct finsh_shell));
    shell->echo_mode = cfg->echo_mode;
    shell->prompt_mode = cfg->prompt_mode;
    shell->get_char = cfg->get_char;

#ifdef __ARMCC_VERSION /* ARM C Compiler */
    extern const int FSymTab$$Base;
    extern const int FSymTab$$Limit;
    finsh_system_function_init(&FSymTab$$Base, &FSymTab$$Limit);
#elif defined(__ICCARM__) || defined(__ICCRX__) /* for IAR Compiler */
    finsh_system_function_init(__section_begin("FSymTab"), __section_end("FSymTab"));
#elif defined(__GNUC__) || defined(__TI_COMPILER_VERSION__) || defined(__TASKING__)
    /* GNU GCC Compiler and TI CCS */
    extern const int __fsymtab_start;
    extern const int __fsymtab_end;
    finsh_system_function_init(&__fsymtab_start, &__fsymtab_end);
#elif defined(__ADSPBLACKFIN__) /* for VisualDSP++ Compiler */
    finsh_system_function_init(&__fsymtab_start, &__fsymtab_end);
#elif defined(_MSC_VER)
    unsigned int *ptr_begin, *ptr_end;

    if (shell) {
        FINSH_PRINTF("finsh shell already init.\r\n");
        return 0;
    }

    ptr_begin = (unsigned int *)&__fsym_begin;
    ptr_begin += (sizeof(struct finsh_syscall) / sizeof(unsigned int));
    while (*ptr_begin == 0) ptr_begin++;

    ptr_end = (unsigned int *)&__fsym_end;
    ptr_end--;
    while (*ptr_end == 0) ptr_end--;

    finsh_system_function_init(ptr_begin, ptr_end);
#endif

    return 0;
}
