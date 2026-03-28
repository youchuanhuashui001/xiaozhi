set(XIAOZHI_CTEST_SOURCES
    test_app_bootstrap.c
    test_audio_buffer.c
    test_audio_playback.c
    test_build_smoke.c
    test_config.c
    test_control_events.c
    test_control_protocol.c
    test_daemon_runtime.c
    test_event_queue.c
    test_opus_codec.c
    test_ota_client.c
    test_session.c
    test_xiaozhi_client.c
    test_xiaozhi_protocol.c
)

foreach(test_src IN LISTS XIAOZHI_CTEST_SOURCES)
    get_filename_component(test_name "${test_src}" NAME_WE)

    add_executable(${test_name} ${test_src})
    target_include_directories(${test_name} PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(${test_name} PRIVATE xiaozhi_core)

    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

# Helper executable used by Python network smoke tests.
add_executable(test_control_plane_server_runner test_control_plane_server_runner.c)
target_include_directories(test_control_plane_server_runner PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(test_control_plane_server_runner PRIVATE xiaozhi_core)
