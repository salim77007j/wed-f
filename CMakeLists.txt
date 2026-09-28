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

find_package(Qt6 6.7 REQUIRED COMPONENTS Widgets WebEngineWidgets WebEngineCore Sql Network PdfWidgets)

# ------------------------------------------------------------------ Rust core
set(WED_RUST_DIR ${CMAKE_SOURCE_DIR}/src/core)
set(WED_RUST_TARGET ${CMAKE_CURRENT_BINARY_DIR}/rust)
if(WIN32)
  set(WED_RUST_LIB ${WED_RUST_TARGET}/release/wed_core.lib)
else()
  set(WED_RUST_LIB ${WED_RUST_TARGET}/release/libwed_core.a)
endif()

find_program(CARGO_EXE cargo REQUIRED)

# user-space sysroot EGL (containers without system GL dev packages)
if(EXISTS "/home/z/sysroot/usr/lib/x86_64-linux-gnu/libEGL.so")
  set(CMAKE_SYSROOT_EGL /home/z/sysroot/usr/lib/x86_64-linux-gnu/libEGL.so)
else()
  set(CMAKE_SYSROOT_EGL EGL)
endif()

add_custom_target(
  wed_rust ALL
  COMMAND ${CARGO_EXE} build --release --manifest-path ${WED_RUST_DIR}/Cargo.toml
          --target-dir ${WED_RUST_TARGET}
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  BYPRODUCTS ${WED_RUST_LIB}
  COMMENT "Building Rust privacy core"
  VERBATIM)

# ------------------------------------------------------------------ app object library
set(WED_APP_SOURCES
  src/app/AppSettings.cpp
  src/app/Database.cpp
  src/app/SessionManager.cpp
  src/app/ThemeManager.cpp
  src/privacy/PrivacyEngine.cpp
  src/privacy/RequestInterceptor.cpp
  src/web/WebPage.cpp
  src/web/WebView.cpp
  src/web/DownloadManager.cpp
  src/web/ProfileCatalog.cpp
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

set(WED_APP_HEADERS
  src/app/AppSettings.h src/app/Database.h src/app/SessionManager.h
  src/app/ThemeManager.h src/app/Utils.h
  src/privacy/PrivacyEngine.h src/privacy/RequestInterceptor.h
  src/web/WebPage.h src/web/WebView.h src/web/DownloadManager.h
  src/web/ProfileCatalog.h src/ui/MainWindow.h src/ui/TabWidget.h
  src/ui/BrowserTab.h src/ui/AddressBar.h src/ui/Toolbar.h src/ui/StartPage.h
  src/ui/Panels.h src/ui/SettingsDialog.h src/ui/PrivacyDashboard.h
  src/ui/FindBar.h)

add_library(wed_app OBJECT ${WED_APP_SOURCES} ${WED_APP_HEADERS})
target_include_directories(wed_app PRIVATE src src/app src/ui src/web src/privacy)
target_link_libraries(wed_app PUBLIC Qt6::Widgets Qt6::WebEngineWidgets Qt6::WebEngineCore Qt6::Sql Qt6::Network Qt6::PdfWidgets)

set(WED_RES_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/resources/js/fingerprint.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/resources/lists/easylist.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/src/resources/lists/easyprivacy.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/src/resources/icons/app.png)

# ------------------------------------------------------------------ browser executable
add_executable(wed WIN32 src/main.cpp $<TARGET_OBJECTS:wed_app>)
qt_add_resources(wed "app"
    PREFIX "/"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/src/resources"
    FILES ${WED_RES_FILES})
set_target_properties(wed PROPERTIES OUTPUT_NAME "wed-browser")

target_link_libraries(wed PRIVATE
  Qt6::Widgets Qt6::WebEngineWidgets Qt6::WebEngineCore Qt6::Sql Qt6::Network Qt6::PdfWidgets
  ${WED_RUST_LIB})
add_dependencies(wed wed_rust)

if(UNIX AND NOT APPLE)
  target_link_libraries(wed PRIVATE dl pthread m ${CMAKE_SYSROOT_EGL})
endif()

target_include_directories(wed PRIVATE src src/app src/ui src/web src/privacy)

# ------------------------------------------------------------------ self-test harness
# Drives the real browser headlessly (screenshots, scripted runs).
if(WED_BUILD_TESTS)
  add_executable(wed-selftest tests/selftest.cpp $<TARGET_OBJECTS:wed_app>)
  qt_add_resources(wed-selftest "app"
      PREFIX "/"
      BASE "${CMAKE_CURRENT_SOURCE_DIR}/src/resources"
      FILES ${WED_RES_FILES})
  target_link_libraries(wed-selftest PRIVATE
    Qt6::Widgets Qt6::WebEngineWidgets Qt6::WebEngineCore Qt6::Sql Qt6::Network Qt6::PdfWidgets
    ${WED_RUST_LIB})
  add_dependencies(wed-selftest wed_rust)
  target_include_directories(wed-selftest PRIVATE src src/app src/ui src/web src/privacy)
  if(UNIX AND NOT APPLE)
    target_link_libraries(wed-selftest PRIVATE dl pthread m ${CMAKE_SYSROOT_EGL})
  endif()
endif()

install(TARGETS wed RUNTIME DESTINATION bin)
