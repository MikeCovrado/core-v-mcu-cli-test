/*==========================================================
 * Copyright 2021 QuickLogic Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *==========================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "FreeRTOS.h"
#include "semphr.h"

#include "target/core-v-mcu/include/core-v-mcu-config.h"
#include "libs/cli/include/cli.h"
#include "libs/utils/include/dbg_uart.h"

static void csr_help(const struct cli_cmd_entry *pEntry);
static void csr_dump(const struct cli_cmd_entry *pEntry);

const struct cli_cmd_entry csr_functions[] =
{
    CLI_CMD_SIMPLE( "dump",  csr_dump,  "dump all CSR values" ),
    CLI_CMD_SIMPLE( "help",  csr_help,  "print this help message" ),
    CLI_CMD_SIMPLE( "?",     csr_help,  "print this help message" ),
    CLI_CMD_TERMINATE()
};

static void csr_help(const struct cli_cmd_entry *pEntry)
{
    (void)pEntry;
    CLI_printf("CSR commands:\n");
    CLI_printf("  dump  -- dump all CSR values\n");
    CLI_printf("  help  -- print this help message\n");
    CLI_printf("  ?     -- print this help message\n");
    dbg_str("<<DONE>>\r\n");
}

static void csr_dump(const struct cli_cmd_entry *pEntry)
{
    (void)pEntry;
    CLI_printf("not implemented yet\n");
    dbg_str("<<DONE>>\r\n");
}
