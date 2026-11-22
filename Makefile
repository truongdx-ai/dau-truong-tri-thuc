CC = gcc
CPPFLAGS += -Iinclude -D_POSIX_C_SOURCE=200809L
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic -Werror -g

BUILD_DIR ?= build
UNITY_DIR := third_party/unity
COMMON_SOURCES := $(wildcard src/common/*.c)
SERVER_SOURCES := $(wildcard src/server/*.c) $(COMMON_SOURCES)
CLIENT_SOURCES := $(wildcard src/client/*.c) $(COMMON_SOURCES)
TEST_SOURCES := $(wildcard tests/unit/test_*.c tests/integration/test_*.c)
TEST_BINS := $(patsubst %.c,$(BUILD_DIR)/%,$(TEST_SOURCES))
TEST_SUPPORT := $(filter-out %/main.c,$(sort $(SERVER_SOURCES) $(CLIENT_SOURCES)))
FORMAT_SOURCES := $(shell find include src tests -type f \( -name '*.c' -o -name '*.h' \))
COVERAGE_MIN ?= 90

.PHONY: all test coverage valgrind format format-check clean

all: $(BUILD_DIR)/server $(BUILD_DIR)/client

$(BUILD_DIR)/server: $(SERVER_SOURCES:%.c=$(BUILD_DIR)/%.o)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -lcrypto -o $@

$(BUILD_DIR)/client: $(CLIENT_SOURCES:%.c=$(BUILD_DIR)/%.o)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/tests/%.o: CPPFLAGS += -I$(UNITY_DIR)

$(TEST_BINS): %: %.o $(TEST_SUPPORT:%.c=$(BUILD_DIR)/%.o) $(BUILD_DIR)/$(UNITY_DIR)/unity.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -lcrypto -o $@

test: all $(TEST_BINS)
	@set -e; for test in $(TEST_BINS); do "$$test"; done
	python3 tests/integration/test_week2.py $(BUILD_DIR)

coverage:
	@if test -d build/coverage; then find build/coverage -type f -name '*.gcda' -delete; fi
	$(MAKE) BUILD_DIR=build/coverage CFLAGS='$(CFLAGS) --coverage' LDFLAGS='$(LDFLAGS) --coverage' test
	@mkdir -p coverage
	gcovr -r . --filter 'src/' --exclude 'src/.*/main\.c$$' \
		--fail-under-line $(COVERAGE_MIN) --html-details coverage/index.html \
		--print-summary build/coverage

valgrind: $(TEST_BINS)
	@set -e; for test in $(TEST_BINS); do \
		valgrind --leak-check=full --error-exitcode=1 "$$test"; \
	done

format:
	clang-format -i $(FORMAT_SOURCES)

format-check:
	clang-format --dry-run --Werror $(FORMAT_SOURCES)

clean:
	rm -rf -- build coverage

OBJECT_SOURCES := $(sort $(SERVER_SOURCES) $(CLIENT_SOURCES) $(TEST_SOURCES) $(UNITY_DIR)/unity.c)
-include $(OBJECT_SOURCES:%.c=$(BUILD_DIR)/%.d)
