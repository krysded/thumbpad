cmake_minimum_required(VERSION 3.21)
project(thumbpad VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Gui DBus WaylandClient Quick QuickControls2)
find_package(LayerShellQt REQUIRED)

# Platform-independent core: layout model, gestures, rendering. Unit-tested.
add_library(thumbpad_core STATIC
    src/actions.cpp
    src/layout.cpp
    src/gesture.cpp
    src/renderer.cpp
)
target_include_directories(thumbpad_core PUBLIC src)
target_link_libraries(thumbpad_core PUBLIC Qt6::Gui)

add_executable(thumbpad
    src/main.cpp
    src/controller.cpp
    src/inputmethod.cpp
    src/uinput.cpp
    src/blockwindow.cpp
    src/keyboardui.cpp
    src/dbusiface.cpp
    src/usersettings.cpp
    src/settingswindow.cpp
)
qt_add_resources(thumbpad "qml" PREFIX "/" FILES qml/Settings.qml)
qt6_generate_wayland_protocol_client_sources(thumbpad
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/protocols/input-method-unstable-v1.xml
)
target_link_libraries(thumbpad PRIVATE
    thumbpad_core Qt6::DBus Qt6::WaylandClient Qt6::Quick Qt6::QuickControls2
    LayerShellQt::Interface
)
# Qt generates the protocol header in "core only" mode, but the protocol
# references wl_keyboard (grab_keyboard), which lives in the full client header.
target_compile_options(thumbpad PRIVATE -include wayland-client-protocol.h)
target_compile_definitions(thumbpad PRIVATE
    THUMBPAD_DATADIR="${CMAKE_INSTALL_PREFIX}/share/thumbpad"
)

enable_testing()
add_executable(thumbpad_tests tests/tests.cpp)
target_link_libraries(thumbpad_tests PRIVATE thumbpad_core)
target_compile_definitions(thumbpad_tests PRIVATE
    LAYOUT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/layouts"
)
add_test(NAME thumbpad_tests COMMAND thumbpad_tests)

install(TARGETS thumbpad RUNTIME DESTINATION bin)
install(DIRECTORY layouts/ DESTINATION share/thumbpad/layouts)
# KWin starts the keyboard from this file with its own environment, so use an
# absolute path rather than relying on PATH (e.g. for ~/.local installs).
configure_file(data/io.github.krysded.thumbpad.desktop.in
               ${CMAKE_CURRENT_BINARY_DIR}/io.github.krysded.thumbpad.desktop @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/io.github.krysded.thumbpad.desktop
        DESTINATION share/applications)
