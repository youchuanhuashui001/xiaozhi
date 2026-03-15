# WebSocket 服务器实现计划

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 使用 C 语言和 libwebsockets 库构建一个多线程 WebSocket 服务器，支持传感器数据推送和远程控制指令。

**Architecture:** 主线程运行 lws 事件循环处理 WebSocket/HTTP 连接，采集线程通过 mutex 保护的环形缓冲队列传递模拟传感器数据。JSON 协议用于客户端与服务端通信。

**Tech Stack:** C (C11), libwebsockets, pthreads, POSIX sockets, cJSON (轻量 JSON 解析库，内嵌源码)

---

## 前置条件

安装开发依赖（当前系统缺少 `libwebsockets-dev`）：

```bash
sudo apt install libwebsockets-dev
```

---

### Task 1: 项目骨架与 Makefile

**Files:**
- Create: `Makefile`
- Create: `src/main.c`

**Step 1: 创建 Makefile**

```makefile
CC		?= gcc
CROSS_COMPILE	?=

ifdef CROSS_COMPILE
CC		:= $(CROSS_COMPILE)gcc
LWS_CFLAGS	?= -I$(LWS_ARM_PREFIX)/include
LWS_LDFLAGS	?= -L$(LWS_ARM_PREFIX)/lib -lwebsockets
else
LWS_CFLAGS	:= $(shell pkg-config --cflags libwebsockets)
LWS_LDFLAGS	:= $(shell pkg-config --libs libwebsockets)
endif

CFLAGS		:= -Wall -Wextra -std=c11 -D_GNU_SOURCE $(LWS_CFLAGS)
LDFLAGS		:= $(LWS_LDFLAGS) -lpthread

SRC_DIR		:= src
BUILD_DIR	:= build
TARGET		:= $(BUILD_DIR)/ws_server

SRCS		:= $(wildcard $(SRC_DIR)/*.c)
OBJS		:= $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)
```

**Step 2: 创建最小化 main.c**

```c
#include <libwebsockets.h>
#include <signal.h>
#include <stdio.h>

static int interrupted = 0;

static void sigint_handler(int sig)
{
	(void)sig;
	interrupted = 1;
}

int main(void)
{
	struct lws_context_creation_info info;
	struct lws_context *context;

	signal(SIGINT, sigint_handler);

	memset(&info, 0, sizeof(info));
	info.port = 8080;

	context = lws_create_context(&info);
	if (!context) {
		fprintf(stderr, "lws 上下文创建失败\n");
		return 1;
	}

	printf("WebSocket 服务器已启动，监听端口 %d\n", info.port);

	while (!interrupted)
		lws_service(context, 100);

	lws_context_destroy(context);
	printf("服务器已关闭\n");

	return 0;
}
```

**Step 3: 编译验证**

```bash
make clean && make
```

Expected: 编译成功，生成 `build/ws_server`

**Step 4: 运行验证**

```bash
./build/ws_server
```

Expected: 输出 "WebSocket 服务器已启动，监听端口 8080"，Ctrl+C 退出

**Step 5: Commit**

```bash
git init
git add Makefile src/main.c
git commit -m "feat: 项目骨架和最小化 lws 服务器"
```

---

### Task 2: 线程安全消息队列 (msg_queue)

**Files:**
- Create: `src/msg_queue.h`
- Create: `src/msg_queue.c`

**Step 1: 编写 msg_queue.h**

定义消息队列接口：
- `msg_queue_t` 结构体（mutex + 环形缓冲区 + 头尾索引 + 容量）
- `msg_queue_init(queue, capacity)` — 初始化队列
- `msg_queue_push(queue, data, len)` — 写入消息（满时丢弃最旧）
- `msg_queue_pop(queue, buf, buf_size)` — 读取消息（返回长度，空时返回 0）
- `msg_queue_destroy(queue)` — 销毁队列

每个消息为一个固定大小（如 256 字节）的字符串槽位。

**Step 2: 实现 msg_queue.c**

用 `pthread_mutex_t` 保护所有读写操作，环形缓冲区通过取模实现循环。

**Step 3: 编译验证**

```bash
make clean && make
```

Expected: 编译通过无警告

**Step 4: Commit**

```bash
git add src/msg_queue.h src/msg_queue.c
git commit -m "feat: 线程安全环形缓冲消息队列"
```

---

### Task 3: 模拟传感器采集线程 (sensor_sim)

**Files:**
- Create: `src/sensor_sim.h`
- Create: `src/sensor_sim.c`

**Step 1: 编写 sensor_sim.h**

定义接口：
- `sensor_sim_start(queue)` — 启动采集线程，传入消息队列指针
- `sensor_sim_stop()` — 通知线程退出并 join

**Step 2: 实现 sensor_sim.c**

采集线程函数：
- 每 2 秒生成一次模拟数据
- 温度: 20.0 ~ 35.0 随机浮动
- 湿度: 40 ~ 80 随机浮动
- 电压: 3.0 ~ 3.6 随机浮动
- 格式化为 JSON 字符串：`{"type":"sensor","data":{"temp":25.3,"humidity":60,"voltage":3.28}}`
- 写入消息队列

**Step 3: 在 main.c 中集成**

启动采集线程，主循环中打印从队列读取的消息（临时调试用）。

**Step 4: 编译运行验证**

```bash
make clean && make && ./build/ws_server
```

Expected: 每 2 秒输出一条模拟传感器 JSON 数据

**Step 5: Commit**

```bash
git add src/sensor_sim.h src/sensor_sim.c src/main.c
git commit -m "feat: 模拟传感器采集线程"
```

---

### Task 4: 控制指令解析 (command)

**Files:**
- Create: `src/cJSON.h` — 内嵌 cJSON 单头文件
- Create: `src/cJSON.c` — 内嵌 cJSON 实现
- Create: `src/command.h`
- Create: `src/command.c`

**Step 1: 引入 cJSON**

从 https://github.com/DaveGamble/cJSON 下载 `cJSON.h` 和 `cJSON.c`（MIT 协议），放入 `src/` 目录。

**Step 2: 编写 command.h / command.c**

定义接口：
- `command_parse(json_str, response_buf, buf_size)` — 解析 JSON 指令，生成响应

支持的指令：
- `set_led`: 控制 LED（模拟打印）
- `get_status`: 返回当前模拟状态
- 未知指令: 返回错误 JSON

**Step 3: 编译验证**

```bash
make clean && make
```

Expected: 编译通过无警告

**Step 4: Commit**

```bash
git add src/cJSON.h src/cJSON.c src/command.h src/command.c
git commit -m "feat: JSON 控制指令解析模块"
```

---

### Task 5: WebSocket 协议处理 (ws_server)

**Files:**
- Create: `src/ws_server.h`
- Create: `src/ws_server.c`
- Modify: `src/main.c`

**Step 1: 编写 ws_server.h**

定义：
- `ws_protocol_callback` — lws 协议回调函数
- `ws_broadcast_sensor_data(msg)` — 广播传感器数据给所有连接的客户端

**Step 2: 实现 ws_server.c**

处理 lws 回调事件：
- `LWS_CALLBACK_ESTABLISHED` — 新客户端连接，加入客户端链表
- `LWS_CALLBACK_RECEIVE` — 收到消息，调用 `command_parse` 处理，回写响应
- `LWS_CALLBACK_SERVER_WRITEABLE` — 发送排队的传感器数据
- `LWS_CALLBACK_CLOSED` — 客户端断开，从链表移除

使用 lws 的 per-session 数据保存每个客户端的发送缓冲区。

**Step 3: 在 main.c 中注册 WebSocket 协议**

在 `lws_context_creation_info` 中添加协议列表，设置定时器回调轮询消息队列并广播。

**Step 4: 编译运行验证 (wscat)**

```bash
make clean && make && ./build/ws_server &
# 使用 wscat 或 websocat 连接测试
wscat -c ws://localhost:8080
```

Expected:
- 连接成功后每 2 秒收到传感器 JSON
- 发送 `{"type":"cmd","action":"set_led","params":{"id":1,"state":"on"}}` 收到响应
- 发送非法 JSON 收到错误提示

**Step 5: Commit**

```bash
git add src/ws_server.h src/ws_server.c src/main.c
git commit -m "feat: WebSocket 协议处理和传感器数据广播"
```

---

### Task 6: HTTP 静态文件服务 (http_server)

**Files:**
- Create: `src/http_server.h`
- Create: `src/http_server.c`
- Modify: `src/main.c`

**Step 1: 编写 http_server.h / http_server.c**

使用 lws 内置的 HTTP 协议回调，配置 `mount` 指向 `web/` 目录，支持 `index.html` 默认页面。

**Step 2: 在 main.c 中添加 HTTP mount 配置**

设置 `lws_http_mount` 结构体，将 `/` 映射到 `web/` 目录。

**Step 3: 编译验证**

```bash
make clean && make
```

Expected: 编译通过

**Step 4: Commit**

```bash
git add src/http_server.h src/http_server.c src/main.c
git commit -m "feat: HTTP 静态文件服务"
```

---

### Task 7: HTML 客户端页面

**Files:**
- Create: `web/index.html`

**Step 1: 编写 HTML 客户端**

功能包含：
- 自动连接 WebSocket（`ws://host:port`）
- 实时显示传感器数据（温度/湿度/电压）
- 控制面板：LED 开关按钮
- 连接状态指示器
- 消息日志区域

**Step 2: 完整集成测试**

```bash
make clean && make && ./build/ws_server
```

浏览器打开 `http://localhost:8080`

Expected:
- 页面正常加载
- WebSocket 自动连接
- 实时显示传感器数据
- 点击 LED 按钮发送控制指令

**Step 3: Commit**

```bash
git add web/index.html
git commit -m "feat: 浏览器 WebSocket 客户端页面"
```

---

### Task 8: 完整集成测试

**Step 1: 编译并启动服务器**

```bash
make clean && make && ./build/ws_server
```

**Step 2: 浏览器测试**

打开 `http://localhost:8080`，验证数据推送和控制指令。

**Step 3: 命令行测试**

```bash
# 测试传感器数据推送
wscat -c ws://localhost:8080 -w 10

# 测试控制指令
echo '{"type":"cmd","action":"set_led","params":{"id":1,"state":"on"}}' | wscat -c ws://localhost:8080
```

**Step 4: 多客户端测试**

同时打开多个浏览器标签或 wscat 连接，验证所有客户端均收到数据。

**Step 5: 错误处理测试**

```bash
echo 'invalid json' | wscat -c ws://localhost:8080
echo '{"type":"cmd","action":"unknown_cmd"}' | wscat -c ws://localhost:8080
```

Expected: 收到错误响应，服务器不崩溃

**Step 6: Commit**

```bash
git add -A
git commit -m "feat: 完整集成测试通过"
```

---

## 验证计划

### 自动化测试
- 每个 Task 完成后立即编译验证 `make clean && make`
- Task 5 和 Task 8 使用 `wscat` 进行 WebSocket 功能测试

### 手动测试
- Task 7/8：浏览器打开 `http://localhost:8080` 验证 HTML 客户端
- 多标签页测试多客户端广播
- 发送非法数据测试错误处理
