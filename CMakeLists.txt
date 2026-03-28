cmake_minimum_required(VERSION 3.20)
project(xiaozhi C CXX)

enable_testing()
include(cmake/Dependencies.cmake)

set(CORE_SRCS
    src/app.c
    src/audio_buffer.c
    src/audio_capture.c
    src/audio_playback.c
    src/cJSON.c
    src/command.c
    src/config.c
    src/control_plane/control_events.c
    src/control_plane/control_plane_server.c
    src/control_plane/control_protocol.c
    src/daemon/daemon_runtime.c
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
target_include_directories(xiaozhi_core PRIVATE src)
target_link_libraries(xiaozhi_core PUBLIC ${XIAOZHI_THIRD_PARTY_TARGETS})

add_executable(xiaozhi_daemon src/daemon/main_daemon.c)
target_link_libraries(xiaozhi_daemon PRIVATE xiaozhi_core)
target_include_directories(xiaozhi_daemon PRIVATE src)

add_subdirectory(gui EXCLUDE_FROM_ALL)
