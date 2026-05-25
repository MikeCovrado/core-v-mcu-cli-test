# Standalone Makefile for core-v-mcu cli-test
#
# Targets:
#   make all         - Build cli_test ELF + binary artifacts
#   make sim         - Build, update Verilator mem_init, run simulation
#   make fpga-gdb    - Build, load onto FPGA via OpenOCD/GDB (fast iteration)
#   make fpga-flash  - Build, program QSPI flash via spi_load.py
#   make tools       - Build native host utility tools
#   make clean       - Remove build artifacts and compiled tools
#
# Overridable variables:
#   SIM_CYCLES=N     - Number of simulation cycles (default 2000)
#   CROSS=prefix-    - Cross-compiler prefix (default riscv32-corev-elf-)

CROSS   ?= riscv32-corev-elf-
CC       = $(CROSS)gcc
OBJCOPY  = $(CROSS)objcopy

BUILD_DIR  = build
SIM_DIR    = ../../../core-v-mcu/claude/sim
SIM_CYCLES ?= 2000

# ── Source files ─────────────────────────────────────────────────────────────

APP_SRCS = \
	app/main.c \
	app/cli.c \
	app/cli_gpio_tests.c \
	app/cli_i2c_tests.c \
	app/cli_i2cs_tests.c \
	app/cli_interrupt_tests.c \
	app/cli_adv_timer_unit_tests.c \
	app/cli_efpga_tests.c \
	app/cli_efpgaio_tests.c \
	app/cli_fcb_tests.c \
	app/qspi_tests.c \
	app/sdio_tests.c \
	app/barrMemTest.c \
	app/gpio_map.c \
	app/i2c_task.c \
	app/programFPGA.c \
	app/cam_tests.c

HAL_SRCS = \
	hal/source/hal_gpio.c \
	hal/source/hal_gpio_pulp.c \
	hal/source/hal_fll.c \
	hal/source/hal_fll_pi.c \
	hal/source/hal_irq.c \
	hal/source/hal_timer_irq.c \
	hal/source/hal_fc_event.c \
	hal/source/hal_apb_i2cs.c \
	hal/source/hal_pinmux.c \
	hal/source/hal_pinmux1.c \
	hal/source/hal_soc_eu.c

DRV_SRCS = \
	drivers/source/udma_uart_driver.c \
	drivers/source/udma_i2cm_driver.c \
	drivers/source/udma_qspi_driver.c \
	drivers/source/udma_sdio_driver.c \
	drivers/source/udma_cam_driver.c

KERNEL_SRCS = \
	kernel/croutine.c \
	kernel/event_groups.c \
	kernel/list.c \
	kernel/queue.c \
	kernel/stream_buffer.c \
	kernel/tasks.c \
	kernel/timers.c \
	kernel/portable/GCC/RISC-V/port.c \
	kernel/portable/MemMang/heap_4.c

CLI_SRCS = \
	libs/cli/source/cli_args.c \
	libs/cli/source/cli_filesystem.c \
	libs/cli/source/cli_hexdump.c \
	libs/cli/source/cli_io.c \
	libs/cli/source/cli_loop.c \
	libs/cli/source/cli_misc.c \
	libs/cli/source/cli_platform.c \
	libs/cli/source/cli_stdcmds.c

UTIL_SRCS = \
	libs/utils/source/dbg_uart.c

TARGET_SRCS = \
	target/core-v-mcu/core-v-mcu.c

C_SRCS = $(APP_SRCS) $(HAL_SRCS) $(DRV_SRCS) $(KERNEL_SRCS) $(CLI_SRCS) $(UTIL_SRCS) $(TARGET_SRCS)

ASM_SRCS = \
	target/crt0.S \
	target/vectors.S \
	kernel/portable/GCC/RISC-V/portASM.S

# ── Flags ────────────────────────────────────────────────────────────────────

INCLUDES = \
	-I. \
	-Ikernel/include \
	-Ikernel/portable/GCC/RISC-V \
	-Ikernel/portable/GCC/RISC-V/chip_specific_extensions/PULPissimo \
	-Ihal/include \
	-Idrivers/include \
	-Iapp/include \
	-Ilibs/cli/include \
	-Ilibs/utils/include \
	-Itarget/core-v-mcu/include

SIM_BUILD  ?= 0

CFLAGS = \
	-march=rv32imc_zicsr -mabi=ilp32 -msmall-data-limit=8 -mno-save-restore \
	-fsigned-char -ffunction-sections -fdata-sections \
	-std=gnu11 \
	-Wall -Wextra -Wshadow -Wformat=2 -Wundef \
	-Wno-unused-parameter -Wno-unused-variable -Wconversion \
	-Og -g3 \
	-DFEATURE_CLUSTER=0 -D__PULP__=1 -DASYNC=0 -DTRACE_UART \
	-DPI_LOG_LOCAL_LEVEL=5 -DDEBUG \
	-DportasmHANDLE_INTERRUPT=vSystemIrqHandler \
	-DSTREAM_CHECK_ITERATIONS=2 \
	-DmainCREATE_SIMPLE_BLINKY_DEMO_ONLY=0 \
	$(INCLUDES)

ifeq ($(SIM_BUILD),1)
CFLAGS += -DSIM_BUILD
endif

ASFLAGS = \
	-march=rv32imc_zicsr -mabi=ilp32 \
	-DportasmHANDLE_INTERRUPT=vSystemIrqHandler \
	-DportasmSKIP_ADDITIONAL_REGISTERS \
	-Ikernel/portable/GCC/RISC-V/chip_specific_extensions/PULPissimo

LDFLAGS = \
	-T target/core-v-mcu/core-v-mcu.ld \
	-nostartfiles \
	-Wl,--gc-sections \
	-lgcc -lc -lm

# ── Object file lists ─────────────────────────────────────────────────────────

C_OBJS   = $(addprefix $(BUILD_DIR)/,$(C_SRCS:.c=.o))
ASM_OBJS = $(addprefix $(BUILD_DIR)/,$(ASM_SRCS:.S=.o))

# N25Q driver lives in a directory with a space — handled via explicit rules below
N25Q_OBJS = $(BUILD_DIR)/app/N25Q.o $(BUILD_DIR)/app/Serialize.o

ALL_OBJS = $(C_OBJS) $(ASM_OBJS) $(N25Q_OBJS)

# ── N25Q driver (space in directory name — use escaped paths and quoted shell) ─

N25Q_DIR = app/N25Q_16Mb-1Gb_Device_Driver\ V2.1

$(BUILD_DIR)/app/N25Q.o: $(N25Q_DIR)/N25Q.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c "app/N25Q_16Mb-1Gb_Device_Driver V2.1/N25Q.c" -o "$@"

$(BUILD_DIR)/app/Serialize.o: $(N25Q_DIR)/Serialize.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c "app/N25Q_16Mb-1Gb_Device_Driver V2.1/Serialize.c" -o "$@"

# ── Native host tools ─────────────────────────────────────────────────────────

INTERLEAVED = utils/InterleavedRAMFileGen/InterleavedRAMFileGen
BIN2TXT     = utils/bin2txt/bin2txt

# ── Primary targets ───────────────────────────────────────────────────────────

.PHONY: all sim fpga-gdb fpga-flash tools clean

all: $(BUILD_DIR)/cli_test $(BUILD_DIR)/cli_test.bin $(BUILD_DIR)/cli.bin $(BUILD_DIR)/cli_sim.bin

# ── Link ─────────────────────────────────────────────────────────────────────

$(BUILD_DIR)/cli_test: $(ALL_OBJS)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@
	$(CROSS)size $@

# ── Binary artifacts ──────────────────────────────────────────────────────────

$(BUILD_DIR)/cli_test.bin: $(BUILD_DIR)/cli_test
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/cli.bin: $(BUILD_DIR)/cli_test.bin header.bin
	cat header.bin $< > $@

$(BUILD_DIR)/cli_sim.bin: $(BUILD_DIR)/cli_test.bin header_sim.bin
	cat header_sim.bin $< > $@

# ── Compile C sources ─────────────────────────────────────────────────────────

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# ── Assemble .S sources ───────────────────────────────────────────────────────

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

# ── Native host tools ─────────────────────────────────────────────────────────

tools: $(INTERLEAVED) $(BIN2TXT)

$(INTERLEAVED): utils/InterleavedRAMFileGen/InterleavedRAMFileGen.c
	gcc -o $@ $<

$(BIN2TXT): utils/bin2txt/main.c
	gcc -o $@ $<

# ── Verilator simulation ──────────────────────────────────────────────────────
#
# Builds cli_test, generates Verilator mem_init files from the binary,
# copies them into the sim/mem_init/ directory, then runs the simulation.

MEM_INIT = $(BUILD_DIR)/memoryInitFiles
SIM_MEMINIT = $(SIM_DIR)/mem_init

sim:
	$(MAKE) SIM_BUILD=1 all tools
	@echo "--- Generating interleaved RAM init files ---"
	cd $(BUILD_DIR) && ../$(INTERLEAVED) -i cli_test.bin -o 0x800
	@echo "--- Generating cli_sim.txt ---"
	python3 -c "\
import sys; \
data = open('$(BUILD_DIR)/cli_sim.bin','rb').read(); \
[print(f'{b:02x} ') for b in data]" \
	> $(BUILD_DIR)/cli_sim.txt
	@echo "--- Updating sim/mem_init ---"
	@mkdir -p $(SIM_MEMINIT)
	cp "$(MEM_INIT)/col0.mem" "$(SIM_MEMINIT)/TOP.core_v_mcu.i_soc_domain.l2_ram_i.CUTS[0].bank_i.u0.mem"
	cp "$(MEM_INIT)/col1.mem" "$(SIM_MEMINIT)/TOP.core_v_mcu.i_soc_domain.l2_ram_i.CUTS[1].bank_i.u0.mem"
	cp "$(MEM_INIT)/col2.mem" "$(SIM_MEMINIT)/TOP.core_v_mcu.i_soc_domain.l2_ram_i.CUTS[2].bank_i.u0.mem"
	cp "$(MEM_INIT)/col3.mem" "$(SIM_MEMINIT)/TOP.core_v_mcu.i_soc_domain.l2_ram_i.CUTS[3].bank_i.u0.mem"
	cp "$(MEM_INIT)/privateBank0.mem" "$(SIM_MEMINIT)/TOP.core_v_mcu.i_soc_domain.l2_ram_i.bank_sram_pri0_i.u0.mem"
	cp "$(MEM_INIT)/privateBank1.mem" "$(SIM_MEMINIT)/TOP.core_v_mcu.i_soc_domain.l2_ram_i.bank_sram_pri1_i.u0.mem"
	cp "$(BUILD_DIR)/cli_sim.txt" "$(SIM_MEMINIT)/cli.txt"
	@echo "--- Running Verilator simulation ($(SIM_CYCLES) cycles) ---"
	cd $(SIM_DIR) && ./sim_core_v_mcu +cycles=$(SIM_CYCLES)

# ── FPGA: load via OpenOCD/GDB ────────────────────────────────────────────────
#
# Requires: JTAG cable connected, FPGA programmed with core-v-mcu bitstream,
#           openocd-gdbpipe-hs2.cfg present in this directory.

fpga-gdb: all
	riscv32-corev-elf-gdb $(BUILD_DIR)/cli_test -x gdb_run

# ── FPGA: program QSPI flash ──────────────────────────────────────────────────
#
# Requires: FPGA running, serial console connected.

fpga-flash: all
	python3 spi_load.py $(BUILD_DIR)/cli.bin

# ── Clean ─────────────────────────────────────────────────────────────────────

clean:
	rm -rf $(BUILD_DIR)
	rm -f $(INTERLEAVED) $(BIN2TXT)
