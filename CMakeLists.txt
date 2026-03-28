cmake_minimum_required(VERSION 3.20)
project(xiaozhi C CXX)

enable_testing()

set(CORE_SRCS
    src/app.c
    src/audio_buffer.c
    src/audio_capture.c
    src/audio_playback.c
    src/cJSON.c
    src/command.c
    src/config.c
    src/event_queue.c
    src/http_server.c
    src/log.c
    src/msg_queue.c
    src/opus_codec.c
    src/ota_client.c
    src/sensor_sim.c
    src/session.c
    src/ws_server.c
    src/xiaozhi_client.c
    src/xiaozhi_protocol.c
)

add_library(xiaozhi_core STATIC ${CORE_SRCS})

add_executable(xiaozhi_daemon src/daemon/main_daemon.c)
target_link_libraries(xiaozhi_daemon PRIVATE xiaozhi_core)

add_subdirectory(gui EXCLUDE_FROM_ALL)
