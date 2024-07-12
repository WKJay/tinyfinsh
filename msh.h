/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2013-03-30     Bernard      the first verion for FinSH
 * 2024-07-12     WKJay        抽离 RT-Thread
 */

#ifndef __M_SHELL__
#define __M_SHELL__
#include <stdint.h>

int msh_exec(char *cmd, uint32_t length);
void msh_auto_complete(char *prefix);

int msh_exec_module(const char *cmd_line, int size);
int msh_exec_script(const char *cmd_line, int size);

#endif
