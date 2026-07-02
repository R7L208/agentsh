# agentsh - Makefile
#
# Targets:
#   make            build agentsh + broker
#   make test       build, then run the VFS/policy unit tests
#   make demo       run the end-to-end two-agent sandbox demo
#   make clean      remove build artifacts and runtime FIFOs/logs

CC      := cc
CFLAGS  := -std=c11 -Wall -Wextra -O2
LDFLAGS := -pthread

SRC_DIR := src
BUILD   := .

AGENTSH_OBJS := $(SRC_DIR)/agentsh.o \
                $(SRC_DIR)/policy.o \
                $(SRC_DIR)/virtual_filesystem.o \
                $(SRC_DIR)/message_bus.o \
                $(SRC_DIR)/audit.o

BROKER_OBJS  := $(SRC_DIR)/broker.o

.PHONY: all test demo clean

all: agentsh broker

agentsh: $(AGENTSH_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

broker: $(BROKER_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

test: all
	./tests/run_tests.sh

demo: all
	./tests/demo.sh

clean:
	rm -f agentsh broker $(SRC_DIR)/*.o
	rm -f serverFIFO agent* user* 2>/dev/null || true
	rm -rf audit queryResults
