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

/*
 * CSR test/inspection commands.
 *
 *   csr map                   -- print the top-level memory map
 *   csr <peripheral>          -- read and print every CSR of a uDMA peripheral
 *                                (name, address, value).  Peripherals:
 *                                ctrl uart0 uart1 qspi0 qspi1 i2cm0 i2cm1 sdio cam
 *
 * The per-peripheral register tables below mirror the generated headers in
 * hal/include/hal_udma_*_reg_defs.h.  They are reproduced here rather than
 * #include-d because those headers all define the same REG_* macro names
 * (REG_RX_SADDR, REG_STATUS, ...) and therefore cannot coexist in one
 * translation unit.  The base addresses come from
 * target/core-v-mcu/include/core-v-mcu-config.h (UDMA_CH_ADDR_*), and the
 * top-level memory map matches docs/doc-src/mmap.rst in the CORE-V-MCU manual.
 *
 * NOTE: every CSR is read unconditionally, as requested.  A uDMA channel whose
 * clock is gated off (peripheral never opened) may read back as zeros or stale
 * data, and write-only / read-to-clear CSRs are still read here -- reading them
 * can have side effects on the peripheral.  This is expected for a raw CSR dump.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "semphr.h"

#include "target/core-v-mcu/include/core-v-mcu-config.h"
#include "libs/cli/include/cli.h"
#include "libs/utils/include/dbg_uart.h"

/* One CSR: human-readable name and byte offset from the peripheral base. */
typedef struct {
    const char *name;
    uint16_t    offset;
} csr_reg_t;

/* One peripheral: name, base address and its CSR table. */
typedef struct {
    const char      *name;
    uint32_t         base;
    const csr_reg_t *regs;
    uint16_t         n_regs;
} csr_periph_t;

/* ---- Register tables (mirror hal/include/hal_udma_*_reg_defs.h) ---------- */

/* hal_udma_ctrl_reg_defs.h */
static const csr_reg_t ctrl_regs[] = {
    { "reg_cg",          0x000 },
    { "reg_cfg_evt",     0x004 },
    { "reg_rst",         0x008 },
};

/* hal_udma_uart_reg_defs.h */
static const csr_reg_t uart_regs[] = {
    { "rx_saddr",        0x00 },
    { "rx_size",         0x04 },
    { "rx_cfg",          0x08 },
    { "tx_saddr",        0x10 },
    { "tx_size",         0x14 },
    { "tx_cfg",          0x18 },
    { "status",          0x20 },
    { "uart_setup",      0x24 },
    { "error",           0x28 },
    { "irq_en",          0x2C },
    { "valid",           0x30 },
    { "data",            0x34 },
};

/* hal_udma_qspi_reg_defs.h */
static const csr_reg_t qspi_regs[] = {
    { "rx_saddr",        0x00 },
    { "rx_size",         0x04 },
    { "rx_cfg",          0x08 },
    { "tx_saddr",        0x10 },
    { "tx_size",         0x14 },
    { "tx_cfg",          0x18 },
    { "cmd_saddr",       0x20 },
    { "cmd_size",        0x24 },
    { "cmd_cfg",         0x28 },
    { "status",          0x30 },
};

/* hal_udma_i2cm_reg_defs.h */
static const csr_reg_t i2cm_regs[] = {
    { "rx_saddr",        0x00 },
    { "rx_size",         0x04 },
    { "rx_cfg",          0x08 },
    { "tx_saddr",        0x10 },
    { "tx_size",         0x14 },
    { "tx_cfg",          0x18 },
    { "status",          0x20 },
    { "setup",           0x24 },
};

/* hal_udma_sdio_reg_defs.h */
static const csr_reg_t sdio_regs[] = {
    { "rx_saddr",        0x00 },
    { "rx_size",         0x04 },
    { "rx_cfg",          0x08 },
    { "tx_saddr",        0x10 },
    { "tx_size",         0x14 },
    { "tx_cfg",          0x18 },
    { "cmd_op",          0x20 },
    { "cmd_arg",         0x24 },
    { "data_setup",      0x28 },
    { "start",           0x2C },
    { "rsp0",            0x30 },
    { "rsp1",            0x34 },
    { "rsp2",            0x38 },
    { "rsp3",            0x3C },
    { "clk_div",         0x40 },
    { "status",          0x44 },
};

/* hal_udma_cam_reg_defs.h */
static const csr_reg_t cam_regs[] = {
    { "rx_saddr",        0x00 },
    { "rx_size",         0x04 },
    { "rx_cfg",          0x08 },
    { "cfg_glob",        0x20 },
    { "cfg_ll",          0x24 },
    { "cfg_ur",          0x28 },
    { "cfg_size",        0x2C },
    { "cfg_filter",      0x30 },
    { "vsync_polarity",  0x34 },
};

/* ---- Peripheral table --------------------------------------------------- */

#define N_REGS(t)   ((uint16_t)(sizeof(t) / sizeof((t)[0])))

static const csr_periph_t csr_periphs[] = {
    { "ctrl",  UDMA_CH_ADDR_CTRL,   ctrl_regs, N_REGS(ctrl_regs) },
    { "uart0", UDMA_CH_ADDR_UART0,  uart_regs, N_REGS(uart_regs) },
    { "uart1", UDMA_CH_ADDR_UART1,  uart_regs, N_REGS(uart_regs) },
    { "qspi0", UDMA_CH_ADDR_QSPIM0, qspi_regs, N_REGS(qspi_regs) },
    { "qspi1", UDMA_CH_ADDR_QSPIM1, qspi_regs, N_REGS(qspi_regs) },
    { "i2cm0", UDMA_CH_ADDR_I2CM0,  i2cm_regs, N_REGS(i2cm_regs) },
    { "i2cm1", UDMA_CH_ADDR_I2CM1,  i2cm_regs, N_REGS(i2cm_regs) },
    { "sdio",  UDMA_CH_ADDR_SDIO0,  sdio_regs, N_REGS(sdio_regs) },
    { "cam",   UDMA_CH_ADDR_CAM0,   cam_regs,  N_REGS(cam_regs)  },
};
#define N_PERIPHS   (sizeof(csr_periphs) / sizeof(csr_periphs[0]))

/* ---- Top-level memory map (matches docs/doc-src/mmap.rst) ---------------- */

typedef struct {
    const char *name;
    uint32_t    start;
    uint32_t    end;
} mmap_entry_t;

static const mmap_entry_t mmap_regions[] = {
    { "Boot ROM",                    0x1A000000, 0x1A03FFFF },
    { "Peripheral Domain",           0x1A100000, 0x1A2FFFFF },
    { "eFPGA Domain",                0x1A300000, 0x1A3FFFFF },
    { "Non-Interleaved Mem Bank 0",  0x1C000000, 0x1C007FFF },
    { "Non-Interleaved Mem Bank 1",  0x1C008000, 0x1C00FFFF },
    { "Interleaved Memory",          0x1C010000, 0x1C07FFFF },
};
#define N_MMAP_REGIONS  (sizeof(mmap_regions) / sizeof(mmap_regions[0]))

static const mmap_entry_t mmap_periphs[] = {
    { "APB Frequency-locked loop", 0x1A100000, 0x1A100FFC },
    { "APB GPIOs",                 0x1A101000, 0x1A101FFC },
    { "uDMA",                      0x1A102000, 0x1A103FFC },
    { "uDMA UART0",                0x1A102080, 0x1A1020FC },
    { "uDMA UART1",                0x1A102100, 0x1A10217C },
    { "uDMA QSPI0",                0x1A102180, 0x1A1021FC },
    { "uDMA QSPI1",                0x1A102200, 0x1A10227C },
    { "uDMA I2CM0",                0x1A102280, 0x1A1022FC },
    { "uDMA I2CM1",                0x1A102300, 0x1A10237C },
    { "uDMA SDIO",                 0x1A102380, 0x1A1023FC },
    { "uDMA CAMERA",               0x1A102400, 0x1A10247C },
    { "APB SoC Controller",        0x1A104000, 0x1A104FFC },
    { "APB Advanced Timer",        0x1A105000, 0x1A105FFC },
    { "APB SoC Event Controller",  0x1A106000, 0x1A106FFC },
    { "APB I2CS",                  0x1A107000, 0x1A107FFC },
    { "APB Timer",                 0x1A10B000, 0x1A10BFFC },
    { "stdout emulator",           0x1A10F000, 0x1A10FFFC },
    { "Debug",                     0x1A110000, 0x1A11FFFC },
    { "eFPGA configuration",       0x1A200000, 0x1A2F0000 },
};
#define N_MMAP_PERIPHS  (sizeof(mmap_periphs) / sizeof(mmap_periphs[0]))

/* ---- Handlers ----------------------------------------------------------- */

static void csr_help(const struct cli_cmd_entry *pEntry);
static void csr_map(const struct cli_cmd_entry *pEntry);
static void csr_dump_cmd(const struct cli_cmd_entry *pEntry);

/*
 * Each peripheral command carries a pointer to its csr_periph_t descriptor in
 * the menu-entry cookie, so a single handler (csr_dump_cmd) serves them all.
 */
const struct cli_cmd_entry csr_functions[] =
{
    CLI_CMD_SIMPLE(   "map",   csr_map,      "print the top-level memory map" ),
    CLI_CMD_WITH_ARG( "ctrl",  csr_dump_cmd, (intptr_t)&csr_periphs[0], "dump uDMA control CSRs" ),
    CLI_CMD_WITH_ARG( "uart0", csr_dump_cmd, (intptr_t)&csr_periphs[1], "dump uDMA UART0 CSRs" ),
    CLI_CMD_WITH_ARG( "uart1", csr_dump_cmd, (intptr_t)&csr_periphs[2], "dump uDMA UART1 CSRs" ),
    CLI_CMD_WITH_ARG( "qspi0", csr_dump_cmd, (intptr_t)&csr_periphs[3], "dump uDMA QSPI0 CSRs" ),
    CLI_CMD_WITH_ARG( "qspi1", csr_dump_cmd, (intptr_t)&csr_periphs[4], "dump uDMA QSPI1 CSRs" ),
    CLI_CMD_WITH_ARG( "i2cm0", csr_dump_cmd, (intptr_t)&csr_periphs[5], "dump uDMA I2CM0 CSRs" ),
    CLI_CMD_WITH_ARG( "i2cm1", csr_dump_cmd, (intptr_t)&csr_periphs[6], "dump uDMA I2CM1 CSRs" ),
    CLI_CMD_WITH_ARG( "sdio",  csr_dump_cmd, (intptr_t)&csr_periphs[7], "dump uDMA SDIO CSRs" ),
    CLI_CMD_WITH_ARG( "cam",   csr_dump_cmd, (intptr_t)&csr_periphs[8], "dump uDMA CAMERA CSRs" ),
    CLI_CMD_SIMPLE(   "help",  csr_help,     "print this help message" ),
    CLI_CMD_SIMPLE(   "?",     csr_help,     "print this help message" ),
    CLI_CMD_TERMINATE()
};

static void csr_help(const struct cli_cmd_entry *pEntry)
{
    (void)pEntry;
    CLI_printf("CSR commands:\n");
    CLI_printf("  map      -- print the top-level memory map\n");
    CLI_printf("  ctrl     -- dump uDMA control CSRs\n");
    CLI_printf("  uart0    -- dump uDMA UART0 CSRs\n");
    CLI_printf("  uart1    -- dump uDMA UART1 CSRs\n");
    CLI_printf("  qspi0    -- dump uDMA QSPI0 CSRs\n");
    CLI_printf("  qspi1    -- dump uDMA QSPI1 CSRs\n");
    CLI_printf("  i2cm0    -- dump uDMA I2CM0 CSRs\n");
    CLI_printf("  i2cm1    -- dump uDMA I2CM1 CSRs\n");
    CLI_printf("  sdio     -- dump uDMA SDIO CSRs\n");
    CLI_printf("  cam      -- dump uDMA CAMERA CSRs\n");
    CLI_printf("  help, ?  -- print this help message\n");
    dbg_str("<<DONE>>\r\n");
}

static void csr_map(const struct cli_cmd_entry *pEntry)
{
    (void)pEntry;
    uint32_t i;

    CLI_printf("Top-level memory map:\n");
    CLI_printf("  %-28s %-12s %s\n", "REGION", "START", "END");
    for (i = 0; i < N_MMAP_REGIONS; i++) {
        CLI_printf("  %-28s 0x%08x  0x%08x\n",
                   mmap_regions[i].name, mmap_regions[i].start, mmap_regions[i].end);
    }

    CLI_printf("\nPeripheral domain:\n");
    CLI_printf("  %-28s %-12s %s\n", "IP BLOCK", "START", "END");
    for (i = 0; i < N_MMAP_PERIPHS; i++) {
        CLI_printf("  %-28s 0x%08x  0x%08x\n",
                   mmap_periphs[i].name, mmap_periphs[i].start, mmap_periphs[i].end);
    }
    dbg_str("<<DONE>>\r\n");
}

/* Read and print every CSR of one peripheral. */
static void csr_dump_periph(const csr_periph_t *p)
{
    uint16_t i;

    CLI_printf("%s CSRs @ 0x%08x\n", p->name, p->base);
    CLI_printf("  %-20s %-12s %s\n", "NAME", "ADDRESS", "VALUE");
    for (i = 0; i < p->n_regs; i++) {
        uint32_t addr  = p->base + p->regs[i].offset;
        uint32_t value = *(volatile uint32_t *)addr;
        CLI_printf("  %-20s 0x%08x  0x%08x\n", p->regs[i].name, addr, value);
    }
    dbg_str("<<DONE>>\r\n");
}

static void csr_dump_cmd(const struct cli_cmd_entry *pEntry)
{
    csr_dump_periph((const csr_periph_t *)pEntry->cookie);
}
