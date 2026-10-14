CC = cc
AR = ar
CURL_CONFIG ?= curl-config
CPPFLAGS += -Iinclude -Isrc $(shell $(CURL_CONFIG) --cflags)
CFLAGS ?= -O2 -g
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic -Werror -pthread
LDLIBS += $(shell $(CURL_CONFIG) --libs) -pthread
BUILD := .build
OBJECTS := $(BUILD)/cpapago.o $(BUILD)/json.o

.PHONY: all test test-live test-sanitize clean
all: $(BUILD)/libcpapago.a $(BUILD)/cpapago $(BUILD)/basic

$(BUILD):
	mkdir -p $@

$(BUILD)/%.o: src/%.c include/cpapago.h src/json.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/libcpapago.a: $(OBJECTS)
	$(AR) rcs $@ $^

$(BUILD)/cpapago: src/main.c $(BUILD)/libcpapago.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILD)/basic: examples/basic.c $(BUILD)/libcpapago.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILD)/unit: tests/unit.c $(BUILD)/libcpapago.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

test: all $(BUILD)/unit
	$(BUILD)/unit
	CPAPAGO_CLI=$(BUILD)/cpapago python3 tests/integration.py

test-live: all
	python3 tests/live.py

# 별도 출력 경로를 사용해 일반 빌드와 메모리 검사 빌드가 섞이지 않도록 합니다.
test-sanitize:
	$(MAKE) BUILD=.build/sanitized CC=clang CFLAGS="-O1 -g -std=c11 -Wall -Wextra -Wpedantic -Werror -pthread -fsanitize=address,undefined" LDFLAGS="-fsanitize=address,undefined" test

# 이 프로젝트에서 생성한 빌드 파일만 삭제합니다.
clean:
	rm -f $(BUILD)/cpapago.o $(BUILD)/json.o $(BUILD)/libcpapago.a $(BUILD)/cpapago $(BUILD)/basic $(BUILD)/unit
