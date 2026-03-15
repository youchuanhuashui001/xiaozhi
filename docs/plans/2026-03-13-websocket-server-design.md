# WebSocket 服务器设计文档

## 概述

在 Linux PC 上使用 C 语言开发一个基于 `libwebsockets` 的 WebSocket 服务器，支持传感器数据采集推送和远程控制指令，后续移植到 imx6ull 开发板。

## 架构：多线程事件循环

- **主线程**：运行 `lws` 事件循环，处理 WebSocket/HTTP 协议、广播传感器数据、响应控制指令
- **采集线程**：独立 pthread，周期性生成模拟传感器数据，通过线程安全消息队列传递给主线程
- **通信机制**：mutex 保护的环形缓冲区（ring buffer）

## 文件结构

```
web-socket/
├── Makefile                 # 支持 PC 和 CROSS_COMPILE 交叉编译
├── src/
│   ├── main.c               # 入口，初始化 lws，启动采集线程
│   ├── ws_server.c/.h       # WebSocket 协议回调
│   ├── http_server.c/.h     # HTTP 静态文件服务
│   ├── msg_queue.c/.h       # 线程安全环形缓冲消息队列
│   ├── sensor_sim.c/.h      # 模拟传感器采集线程
│   └── command.c/.h         # 控制指令解析与执行
├── web/
│   └── index.html           # 浏览器 WebSocket 客户端
└── docs/
    └── plans/
```

## 消息协议（JSON）

```json
// 传感器数据推送
{"type": "sensor", "data": {"temp": 25.3, "humidity": 60, "voltage": 3.28}}

// 客户端控制指令
{"type": "cmd", "action": "set_led", "params": {"id": 1, "state": "on"}}

// 服务端指令响应
{"type": "cmd_result", "action": "set_led", "status": "ok"}

// 错误响应
{"type": "error", "msg": "invalid json"}
```

## 错误处理

| 场景 | 处理方式 |
|------|----------|
| 客户端断开 | 从客户端列表移除 |
| 非法 JSON | 返回错误响应 |
| 未知指令 | 返回错误响应 |
| 队列满 | 丢弃最旧数据，日志告警 |
| 采集线程异常 | 主线程检测并重启 |

## 移植策略

1. PC 上用 `libwebsockets-dev` 开发调试
2. 通过 `make CROSS_COMPILE=arm-linux-gnueabihf-` 交叉编译
3. 需为 ARM 单独编译 `libwebsockets` 依赖
