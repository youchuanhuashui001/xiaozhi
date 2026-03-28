set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.2 COMPONENTS Core Gui Qml Quick WebSockets Test QUIET)

if (NOT Qt6_FOUND)
    message(STATUS "Qt6 not found, skip gui targets")
    return()
endif()

add_executable(xiaozhi_gui
    cpp/main.cpp
    cpp/ControlClient.cpp
    cpp/ConversationViewModel.cpp
)

target_include_directories(xiaozhi_gui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cpp)
target_link_libraries(xiaozhi_gui
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::WebSockets
)

qt_add_resources(xiaozhi_gui "xiaozhi_gui_qml"
    PREFIX "/"
    FILES
        qml/Main.qml
        qml/pages/ConnectionSettings.qml
        qml/pages/AudioHardware.qml
        qml/pages/LiveConversation.qml
)

add_executable(test_view_model
    tests/test_view_model.cpp
    cpp/ControlClient.cpp
    cpp/ConversationViewModel.cpp
)

target_include_directories(test_view_model PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cpp)
target_link_libraries(test_view_model
    PRIVATE
        Qt6::Core
        Qt6::WebSockets
        Qt6::Test
)

add_test(NAME test_view_model COMMAND test_view_model)

add_executable(test_qml_smoke
    tests/test_qml_smoke.cpp
)

target_link_libraries(test_qml_smoke
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::Test
)

qt_add_resources(test_qml_smoke "xiaozhi_gui_qml_smoke"
    PREFIX "/"
    FILES
        qml/Main.qml
        qml/pages/ConnectionSettings.qml
        qml/pages/AudioHardware.qml
        qml/pages/LiveConversation.qml
)

add_test(NAME test_qml_smoke COMMAND test_qml_smoke)
