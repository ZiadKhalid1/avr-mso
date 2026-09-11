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

.PHONY: all lib clean size test

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

$(TEST_ELF): $(LIB_FILE) $(TEST_SRC)
	@mkdir -p $(dir $(TEST_ELF))
	$(CC) $(CFLAGS) $(TEST_SRC) $(LIB_FILE) -o $@

test: $(TEST_ELF)
	@rm -f $(TEST_LOG)
	@echo "== DIO driver test =="
	-@timeout $(TEST_TIMEOUT) $(SIMAVR) -m $(MCU) $(TEST_ELF) 2>&1 | sed -u 's/\x1b\[[0-9;]*m//g' > $(TEST_LOG)
	@cat $(TEST_LOG)
	@echo "---"
	@grep -a -q 'DIO_RESULT PASS' $(TEST_LOG) && echo "RESULT: PASS" && exit 0 \
		|| (echo "RESULT: FAIL"; exit 1)

clean:
	rm -rf $(BUILD_DIR)