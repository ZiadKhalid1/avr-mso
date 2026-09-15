# AVR MSO - Makefile for ATmega328P
# Build all sources in src/ into a static library (libmso.a)

MCU      = atmega328p
CC       = avr-gcc
AR       = avr-ar
RANLIB   = avr-ranlib
OBJCOPY  = avr-objcopy
SIZE     = avr-size

F_CPU    = 16000000UL

WARN     = -Wall -Wextra
OPT      = -Os
CFLAGS   = -mmcu=$(MCU) -DF_CPU=$(F_CPU) $(WARN) $(OPT) -I src

BUILD_DIR = build
LIB_NAME  = libmso
LIB_FILE  = $(BUILD_DIR)/$(LIB_NAME).a

SRCS   := $(shell find src -name '*.c')
OBJS   := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))

HEX_FILE = $(BUILD_DIR)/$(LIB_NAME).hex

SIMAVR   = simavr
TEST_SRC = tests/dio_test.c
TEST_ELF = $(BUILD_DIR)/tests/dio_test.elf
TEST_LOG = $(BUILD_DIR)/tests/simout.txt
TEST_TIMEOUT := 10

TEST_ADC_SRC = tests/adc_test.c
TEST_ADC_ELF = $(BUILD_DIR)/tests/adc_test.elf
TEST_ADC_LOG = $(BUILD_DIR)/tests/adcsimout.txt

TEST_AC_SRC = tests/ac_test.c
TEST_AC_ELF = $(BUILD_DIR)/tests/ac_test.elf
TEST_AC_LOG = $(BUILD_DIR)/tests/acsimout.txt

TEST_SCOPE_SRC = tests/scope_test.c
TEST_SCOPE_ELF = $(BUILD_DIR)/tests/scope_test.elf
TEST_SCOPE_LOG = $(BUILD_DIR)/tests/scopesimout.txt

TEST_UART_SRC = tests/uart_test.c
TEST_UART_ELF = $(BUILD_DIR)/tests/uart_test.elf
TEST_UART_LOG = $(BUILD_DIR)/tests/uartsimout.txt

TEST_FRAME_SRC = tests/controller_test.c
TEST_FRAME_ELF = $(BUILD_DIR)/tests/controller_test.elf
TEST_FRAME_LOG = $(BUILD_DIR)/tests/controller_test_simout.txt

.PHONY: all lib clean size test test-adc test-ac test-scope test-uart test-ctrl run-dio run-adc run-ac run-scope run-uart run-ctrl firmware flash run-gui

all: lib size

lib: $(LIB_FILE)

$(LIB_FILE): $(OBJS)
	$(AR) rcs $@ $^
	$(RANLIB) $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

size: $(LIB_FILE)
	$(SIZE) $(LIB_FILE)

# ---------- Firmware image (main.c + libmso.a) for real hardware ----------
PORT      = /dev/ttyUSB0
AVRDUDE   = avrdude

MAIN_SRC  = main.c
MAIN_ELF  = $(BUILD_DIR)/main.elf
MAIN_HEX  = $(BUILD_DIR)/main.hex

firmware: $(MAIN_HEX)

$(MAIN_ELF): $(LIB_FILE) $(MAIN_SRC)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(MAIN_SRC) $(LIB_FILE) -o $@

$(MAIN_HEX): $(MAIN_ELF)
	$(OBJCOPY) -O ihex -R .eeprom $< $@

flash: firmware
	$(AVRDUDE) -p $(MCU) -c arduino -P $(PORT) -b 115200 -U flash:w:$(MAIN_HEX):i

GUI ?= $(PORT)
run-gui:
	python3 tools/gui.py $(GUI)

$(TEST_ELF): $(LIB_FILE) $(TEST_SRC)
	@mkdir -p $(dir $(TEST_ELF))
	$(CC) $(CFLAGS) $(TEST_SRC) $(LIB_FILE) -o $@

$(TEST_ADC_ELF): $(LIB_FILE) $(TEST_ADC_SRC)
	@mkdir -p $(dir $(TEST_ADC_ELF))
	$(CC) $(CFLAGS) $(TEST_ADC_SRC) $(LIB_FILE) -o $@

$(TEST_AC_ELF): $(LIB_FILE) $(TEST_AC_SRC)
	@mkdir -p $(dir $(TEST_AC_ELF))
	$(CC) $(CFLAGS) $(TEST_AC_SRC) $(LIB_FILE) -o $@

$(TEST_SCOPE_ELF): $(LIB_FILE) $(TEST_SCOPE_SRC)
	@mkdir -p $(dir $(TEST_SCOPE_ELF))
	$(CC) $(CFLAGS) $(TEST_SCOPE_SRC) $(LIB_FILE) -o $@

$(TEST_UART_ELF): $(LIB_FILE) $(TEST_UART_SRC)
	@mkdir -p $(dir $(TEST_UART_ELF))
	$(CC) $(CFLAGS) $(TEST_UART_SRC) $(LIB_FILE) -o $@

$(TEST_FRAME_ELF): $(LIB_FILE) $(TEST_FRAME_SRC)
	@mkdir -p $(dir $(TEST_FRAME_ELF))
	$(CC) $(CFLAGS) $(TEST_FRAME_SRC) $(LIB_FILE) -o $@

run-dio: $(TEST_ELF)
	@mkdir -p build/tests
	@rm -f $(TEST_LOG)
	@echo "== DIO driver test =="
	-@timeout $(TEST_TIMEOUT) $(SIMAVR) -m $(MCU) $(TEST_ELF) 2>&1 | sed -u 's/\x1b\[[0-9;]*m//g' > $(TEST_LOG)
	@cat $(TEST_LOG)
	@echo "---"

run-adc: $(TEST_ADC_ELF)
	@mkdir -p build/tests
	@rm -f $(TEST_ADC_LOG)
	@echo "== ADC driver test =="
	-@timeout $(TEST_TIMEOUT) $(SIMAVR) -m $(MCU) $(TEST_ADC_ELF) 2>&1 | sed -u 's/\x1b\[[0-9;]*m//g' > $(TEST_ADC_LOG)
	@cat $(TEST_ADC_LOG)
	@echo "---"

run-ac: $(TEST_AC_ELF)
	@mkdir -p build/tests
	@rm -f $(TEST_AC_LOG)
	@echo "== AC driver test =="
	-@timeout $(TEST_TIMEOUT) $(SIMAVR) -m $(MCU) $(TEST_AC_ELF) 2>&1 | sed -u 's/\x1b\[[0-9;]*m//g' > $(TEST_AC_LOG)
	@cat $(TEST_AC_LOG)
	@echo "---"

run-scope: $(TEST_SCOPE_ELF)
	@mkdir -p build/tests
	@rm -f $(TEST_SCOPE_LOG)
	@echo "== Scope pipeline test =="
	-@timeout $(TEST_TIMEOUT) $(SIMAVR) -m $(MCU) $(TEST_SCOPE_ELF) 2>&1 | sed -u 's/\x1b\[[0-9;]*m//g' > $(TEST_SCOPE_LOG)
	@cat $(TEST_SCOPE_LOG)
	@echo "---"

run-uart: $(TEST_UART_ELF)
	@mkdir -p build/tests
	@rm -f $(TEST_UART_LOG)
	@echo "== UART driver test =="
	-@timeout $(TEST_TIMEOUT) $(SIMAVR) -m $(MCU) $(TEST_UART_ELF) 2>&1 | sed -u 's/\x1b\[[0-9;]*m//g' > $(TEST_UART_LOG)
	@cat $(TEST_UART_LOG)
	@echo "---"

run-ctrl: $(TEST_FRAME_ELF)
	@mkdir -p build/tests
	@rm -f $(TEST_FRAME_LOG)
	@echo "== Controller integration test =="
	-@timeout $(TEST_TIMEOUT) $(SIMAVR) -m $(MCU) $(TEST_FRAME_ELF) 2>&1 | sed -u 's/\x1b\[[0-9;]*m//g' > $(TEST_FRAME_LOG)
	@cat $(TEST_FRAME_LOG)
	@echo "---"

test-adc: run-adc
	@grep -a -q 'ADC_RESULT PASS' $(TEST_ADC_LOG) && echo "RESULT: PASS" && exit 0 \
		|| (echo "RESULT: FAIL"; exit 1)

test-ac: run-ac
	@grep -a -q 'AC_RESULT PASS' $(TEST_AC_LOG) && echo "RESULT: PASS" && exit 0 \
		|| (echo "RESULT: FAIL"; exit 1)

test-scope: run-scope
	@grep -a -q 'SCOPE_RESULT PASS' $(TEST_SCOPE_LOG) && echo "RESULT: PASS" && exit 0 \
		|| (echo "RESULT: FAIL"; exit 1)

test-uart: run-uart
	@grep -a -q 'UART_RESULT PASS' $(TEST_UART_LOG) && echo "RESULT: PASS" && exit 0 \
		|| (echo "RESULT: FAIL"; exit 1)

test-ctrl: run-ctrl
	@grep -a -q 'CTRL_RESULT PASS' $(TEST_FRAME_LOG) && echo "RESULT: PASS" && exit 0 \
		|| (echo "RESULT: FAIL"; exit 1)

test: run-dio run-adc run-ac run-scope run-uart run-ctrl
	@grep -a -q 'DIO_RESULT PASS' $(TEST_LOG) && grep -a -q 'ADC_RESULT PASS' $(TEST_ADC_LOG) \
		&& grep -a -q 'AC_RESULT PASS' $(TEST_AC_LOG) \
		&& grep -a -q 'SCOPE_RESULT PASS' $(TEST_SCOPE_LOG) \
		&& grep -a -q 'UART_RESULT PASS' $(TEST_UART_LOG) \
		&& grep -a -q 'CTRL_RESULT PASS' $(TEST_FRAME_LOG) \
		&& echo "RESULT: PASS" && exit 0 \
		|| (echo "RESULT: FAIL"; exit 1)

clean:
	rm -rf $(BUILD_DIR)