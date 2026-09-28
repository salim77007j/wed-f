cmake_minimum_required(VERSION 3.21)

project(wed VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(WED_BUILD_TESTS "Build the self-test harness" ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets WebEngineWidgets WebEngineCore Sql Network)

# ------------------------------------------------------------------ Rust core
set(WED_RUST_DIR ${CMAKE_SOURCE_DIR}/src/core)
set(WED_RUST_TARGET ${CMAKE_CURRENT_BINARY_DIR}/rust)
if(WIN32)
  set(WED_RUST_LIB ${WED_RUST_TARGET}/release/wed_core.lib)
else()
  set(WED_RUST_LIB ${WED_RUST_TARGET}/release/libwed_core.a)
endif()

find_program(CARGO_EXE cargo REQUIRED)

add_custom_target(
  wed_rust ALL
  COMMAND ${CARGO_EXE} build --release --manifest-path ${WED_RUST_DIR}/Cargo.toml
          --target-dir ${WED_RUST_TARGET}
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  BYPRODUCTS ${WED_RUST_LIB}
  COMMENT "Building Rust privacy core"
  VERBATIM)

# ------------------------------------------------------------------ Resources
qt_add_resources(wed_res "wed"
  PREFIX "/"
  FILES
    src/resources/themes/light.qss
    src/resources/themes/dark.qss
    src/resources/js/fingerprint.js
    src/resources/js/cosmetic.js
    src/resources/lists/easylist.txt
    src/resources/lists/easyprivacy.txt
    src/resources/pages/offline-start.html
)

# ------------------------------------------------------------------ Executable
set(WED_SOURCES
  src/main.cpp
  src/app/AppSettings.cpp
  src/app/Database.cpp
  src/app/SessionManager.cpp
  src/app/ThemeManager.cpp
  src/privacy/PrivacyEngine.cpp
  src/privacy/RequestInterceptor.cpp
  src/web/WebPage.cpp
  src/web/WebView.cpp
  src/web/DownloadManager.cpp
  src/ui/MainWindow.cpp
  src/ui/TabWidget.cpp
  src/ui/BrowserTab.cpp
  src/ui/AddressBar.cpp
  src/ui/Toolbar.cpp
  src/ui/StartPage.cpp
  src/ui/Panels.cpp
  src/ui/SettingsDialog.cpp
  src/ui/PrivacyDashboard.cpp
  src/ui/FindBar.cpp
)

add_executable(wed WIN32 ${WED_SOURCES} src/app/AppSettings.h src/app/Database.h
  src/app/SessionManager.h src/app/ThemeManager.h src/privacy/PrivacyEngine.h
  src/privacy/RequestInterceptor.h src/web/WebPage.h src/web/WebView.h
  src/web/DownloadManager.h src/ui/MainWindow.h src/ui/TabWidget.h
  src/ui/BrowserTab.h src/ui/AddressBar.h src/ui/Toolbar.h src/ui/StartPage.h
  src/ui/Panels.h src/ui/SettingsDialog.h src/ui/PrivacyDashboard.h
  src/ui/FindBar.h)

set_target_properties(wed PROPERTIES OUTPUT_NAME "wed-browser")

target_link_libraries(wed PRIVATE
  Qt6::Widgets Qt6::WebEngineWidgets Qt6::WebEngineCore Qt6::Sql Qt6::Network
  ${WED_RUST_LIB})

add_dependencies(wed wed_rust)

if(UNIX AND NOT APPLE)
  target_link_libraries(wed PRIVATE dl pthread m)
endif()

target_include_directories(wed PRIVATE src)

# Self-test harness drives the real browser headlessly (screenshots, scripted runs)
if(WED_BUILD_TESTS)
  add_executable(wed-selftest tests/selftest.cpp)
  target_link_libraries(wed-selftest PRIVATE
    Qt6::Widgets Qt6::WebEngineWidgets Qt6::Sql Qt6::Network ${WED_RUST_LIB})
  add_dependencies(wed-selftest wed_rust)
  target_include_directories(wed-selftest PRIVATE src)
endif()

install(TARGETS wed RUNTIME DESTINATION bin)
