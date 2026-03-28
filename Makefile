# 仅当命令行传入 CROSS_COMPILE 时才启用交叉编译
# 用法: make                                     (PC 编译)
#       make CROSS_COMPILE=arm-linux-gnueabihf-   (交叉编译)
ifeq ($(origin CROSS_COMPILE),command line)
CC		:= $(CROSS_COMPILE)gcc
CXX		:= $(CROSS_COMPILE)g++
LWS_CFLAGS	?=
LWS_LDFLAGS	?= -lwebsockets
else
CC		:= gcc
CXX		:= g++
LWS_CFLAGS	:= $(shell pkg-config --cflags libwebsockets 2>/dev/null)
LWS_LDFLAGS	:= $(shell pkg-config --libs libwebsockets 2>/dev/null || echo "-lwebsockets")
endif

OPUS_CFLAGS	:= $(shell pkg-config --cflags opus 2>/dev/null)
OPUS_LDFLAGS	:= $(shell pkg-config --libs opus 2>/dev/null)
ALSA_CFLAGS	:= $(shell pkg-config --cflags alsa 2>/dev/null)
ALSA_LDFLAGS	:= $(shell pkg-config --libs alsa 2>/dev/null)

DEBUG		?= 0
DEBUG_FLAGS	:=
ifeq ($(DEBUG),1)
DEBUG_FLAGS	:= -g -O0
endif

COMMON_CFLAGS	:= -Wall -Wextra -std=c11 -D_GNU_SOURCE -Isrc $(LWS_CFLAGS) $(OPUS_CFLAGS) $(ALSA_CFLAGS) $(DEBUG_FLAGS)
CFLAGS		:= $(COMMON_CFLAGS)
CXXFLAGS	:= -Wall -Wextra -std=c++11 -D_GNU_SOURCE -Isrc $(LWS_CFLAGS) $(OPUS_CFLAGS) $(ALSA_CFLAGS) $(DEBUG_FLAGS)
LDFLAGS		:= $(LWS_LDFLAGS) $(OPUS_LDFLAGS) $(ALSA_LDFLAGS) -lpthread
DEPFLAGS	:= -MMD -MP

SRC_DIR		:= src
TEST_DIR	:= tests
BUILD_DIR	:= build
TARGET		:= $(BUILD_DIR)/xiaozhi_client
TEST		?=
TEST_C_SRC	:= $(wildcard $(TEST_DIR)/$(TEST).c)
TEST_CC_SRC	:= $(wildcard $(TEST_DIR)/$(TEST).cc)
TEST_SRC	:= $(or $(TEST_C_SRC),$(TEST_CC_SRC))

# APP_SRCS will be updated task-by-task as the new client modules land.
APP_SRCS	:= \
	$(SRC_DIR)/app.c \
	$(SRC_DIR)/audio_buffer.c \
	$(SRC_DIR)/audio_capture.c \
	$(SRC_DIR)/audio_playback.c \
	$(SRC_DIR)/cJSON.c \
	$(SRC_DIR)/config.c \
	$(SRC_DIR)/event_queue.c \
	$(SRC_DIR)/main.c \
	$(SRC_DIR)/opus_codec.c \
	$(SRC_DIR)/ota_client.c \
	$(SRC_DIR)/control_plane/control_protocol.c \
	$(SRC_DIR)/session.c \
	$(SRC_DIR)/xiaozhi_client.c \
	$(SRC_DIR)/xiaozhi_protocol.c \
	$(SRC_DIR)/log.c
CPP_SRCS	:=
TEST_SUPPORT_SRCS := \
	$(SRC_DIR)/app.c \
	$(SRC_DIR)/audio_buffer.c \
	$(SRC_DIR)/audio_capture.c \
	$(SRC_DIR)/audio_playback.c \
	$(SRC_DIR)/cJSON.c \
	$(SRC_DIR)/event_queue.c \
	$(SRC_DIR)/log.c \
	$(SRC_DIR)/config.c \
	$(SRC_DIR)/opus_codec.c \
	$(SRC_DIR)/ota_client.c \
	$(SRC_DIR)/control_plane/control_protocol.c \
	$(SRC_DIR)/control_plane/control_events.c \
	$(SRC_DIR)/session.c \
	$(SRC_DIR)/xiaozhi_client.c \
	$(SRC_DIR)/xiaozhi_protocol.c

APP_OBJS	:= $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(APP_SRCS))
CPP_OBJS	:= $(patsubst $(SRC_DIR)/%.cc,$(BUILD_DIR)/%.o,$(CPP_SRCS))
TEST_SUPPORT_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(TEST_SUPPORT_SRCS))
TEST_BIN	:= $(BUILD_DIR)/tests/$(TEST)
DEPS		:= $(sort $(APP_OBJS:.o=.d) $(CPP_OBJS:.o=.d) $(TEST_SUPPORT_OBJS:.o=.d))

.PHONY: all clean test

all: $(TARGET)

test:
	@$(MAKE) --no-print-directory $(TEST_BIN) TEST=$(TEST)
	./$(TEST_BIN)

$(TARGET): $(APP_OBJS) $(CPP_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(TEST_BIN): $(TEST_SRC) $(TEST_SUPPORT_OBJS) $(CPP_OBJS) | $(BUILD_DIR)/tests
	@if [ -z "$(TEST_SRC)" ]; then echo "missing test source for $(TEST)"; exit 1; fi
	@mkdir -p $(BUILD_DIR)/tests
	@if printf '%s\n' "$(TEST_SRC)" | grep -q '\.cc$$'; then \
		$(CXX) $(CXXFLAGS) -o $@ $(TEST_SRC) $(TEST_SUPPORT_OBJS) $(CPP_OBJS) $(LDFLAGS); \
	else \
		$(CC) $(COMMON_CFLAGS) -o $@ $(TEST_SRC) $(TEST_SUPPORT_OBJS) $(CPP_OBJS) $(LDFLAGS); \
	fi

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(COMMON_CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cc | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c -o $@ $<

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/tests: | $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/tests

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPS)
