cmake_minimum_required(VERSION 3.16)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()

project(zibby-service VERSION 1.0.0.0 LANGUAGES CXX)

set(PROJECT_AUTHOR "v3lom")

# Configure a header file to pass some of the CMake settings to the source code
configure_file(
  "${PROJECT_SOURCE_DIR}/src/version.h.in"
  "${PROJECT_BINARY_DIR}/version.h"
)

# Add the binary directory to the include path to find version.h
include_directories("${PROJECT_BINARY_DIR}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTS "Build tests" ON)
option(BUILD_PLUGINS "Enable plugin system" ON)
option(BUILD_CALLS "Enable calls module" OFF)
option(BUILD_TUI "Enable TUI module" ON)
option(USE_STATIC_LIBS "Link static libs when possible" OFF)
option(USE_SQLCIPHER "Use SQLCipher-compatible SQLite build" OFF)

option(ZIBBY_ENABLE_PANEL "Build Qt control panel (Windows beta UI)" ON)

# Backward-compatible options
option(ZIBBY_ENABLE_TESTS "Build tests" ${BUILD_TESTS})
option(ZIBBY_ENABLE_PLUGINS "Enable plugin system" ${BUILD_PLUGINS})
option(ZIBBY_ENABLE_CALLS "Enable calls module" ${BUILD_CALLS})
option(ZIBBY_ENABLE_TUI "Enable TUI module" ${BUILD_TUI})
option(ZIBBY_USE_SQLCIPHER "Use SQLCipher-compatible SQLite build" ${USE_SQLCIPHER})

set(BUILD_TESTS ${ZIBBY_ENABLE_TESTS})
set(BUILD_PLUGINS ${ZIBBY_ENABLE_PLUGINS})
set(BUILD_CALLS ${ZIBBY_ENABLE_CALLS})
set(BUILD_TUI ${ZIBBY_ENABLE_TUI})
set(USE_SQLCIPHER ${ZIBBY_USE_SQLCIPHER})

include(GNUInstallDirs)

find_package(Boost 1.70 REQUIRED COMPONENTS filesystem system program_options)
find_package(OpenSSL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)

if(TARGET Boost::filesystem AND TARGET Boost::system AND TARGET Boost::program_options)
    set(ZIBBY_BOOST_LIBS Boost::filesystem Boost::system Boost::program_options)
else()
    set(ZIBBY_BOOST_LIBS ${Boost_LIBRARIES})
    include_directories(${Boost_INCLUDE_DIRS})
endif()

if(TARGET SQLite3::SQLite3)
    set(ZIBBY_SQLITE_LIB SQLite3::SQLite3)
else()
    set(ZIBBY_SQLITE_LIB ${SQLite3_LIBRARIES})
    include_directories(${SQLite3_INCLUDE_DIRS})
endif()

set(ZIBBY_COMMON_SOURCES
    src/core/service.cpp
    src/core/config.cpp
    src/core/database.cpp
    src/core/crypto.cpp
    src/core/message.cpp
    src/core/profile.cpp
    src/core/network.cpp
    src/core/update_checker.cpp
    src/cli/cli.cpp
    src/cli/commands.cpp
    src/cli/tui.cpp
    src/utils/logger.cpp
    src/api/api_server.cpp
)

if(WIN32)
    set(ZIBBY_ICON_PATH "${CMAKE_SOURCE_DIR}/assets/zibby-icon.ico")
    configure_file(
        "${CMAKE_SOURCE_DIR}/src/platform/windows/zibby_service.rc.in"
        "${PROJECT_BINARY_DIR}/zibby_service.rc"
        @ONLY
    )
    list(APPEND ZIBBY_COMMON_SOURCES "${PROJECT_BINARY_DIR}/zibby_service.rc")
    list(APPEND ZIBBY_COMMON_SOURCES src/platform/windows/windows_service.cpp)
    list(APPEND ZIBBY_COMMON_SOURCES src/platform/windows/autostart.cpp)
endif()

add_executable(zibby-service
    src/core/main.cpp
    ${ZIBBY_COMMON_SOURCES}
)

if(WIN32 AND ZIBBY_ENABLE_PANEL)
    # Help CMake locate Qt6 when building with MSYS2/MinGW on Windows.
    # This keeps the default behavior (auto-detect) but adds a common prefix.
    if(EXISTS "C:/msys64/mingw64/lib/cmake/Qt6")
        list(APPEND CMAKE_PREFIX_PATH "C:/msys64/mingw64")
    endif()

    find_package(Qt6 QUIET COMPONENTS Widgets Network)
    if(Qt6_FOUND)
        # Embed panel into zibby-service (single-exe distribution).
        set(CMAKE_AUTOMOC ON)

        target_sources(zibby-service PRIVATE
            src/panel/main_window.h
            src/panel/main_window.cpp
            src/panel/api_client.h
            src/panel/api_client.cpp
            src/panel/peer_list_model.h
            src/panel/peer_list_model.cpp
            src/panel/peer_delegate.h
            src/panel/peer_delegate.cpp
            src/panel/message_list_model.h
            src/panel/message_list_model.cpp
            src/panel/message_delegate.h
            src/panel/message_delegate.cpp
            src/panel/panel_entry.h
            src/panel/panel_entry.cpp
        )

        set_target_properties(zibby-service PROPERTIES
            AUTOMOC ON
        )

        target_link_libraries(zibby-service PRIVATE Qt6::Widgets Qt6::Network)

        if(WIN32)
            target_link_libraries(zibby-service PRIVATE ws2_32 mswsock)
        endif()

        target_compile_definitions(zibby-service PRIVATE ZIBBY_HAS_PANEL=1)
    else()
        message(WARNING "Qt6 not found: skipping zibby-panel build")
    endif()
endif()

target_include_directories(zibby-service
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(zibby-service
    PRIVATE
        ZIBBY_VERSION="${PROJECT_VERSION}"
    $<$<BOOL:${USE_SQLCIPHER}>:ZIBBY_USE_SQLCIPHER>
)

if(USE_STATIC_LIBS)
    set_target_properties(zibby-service PROPERTIES
        LINK_SEARCH_START_STATIC ON
        LINK_SEARCH_END_STATIC ON
    )
endif()

if(MSVC)
    target_compile_options(zibby-service PRIVATE /W4 /permissive-)
else()
    target_compile_options(zibby-service PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(zibby-service
    PRIVATE
        ${ZIBBY_BOOST_LIBS}
        OpenSSL::SSL
        OpenSSL::Crypto
        ${ZIBBY_SQLITE_LIB}
    Threads::Threads
)

if(WIN32)
    target_link_libraries(zibby-service PRIVATE ws2_32 mswsock advapi32 shell32)
endif()

install(TARGETS zibby-service
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/config/config.json.example
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/zibby
    RENAME config.json
)

if(UNIX AND NOT APPLE)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/config/zibby.service.example
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/systemd/user
        RENAME zibby.service
    )
endif()

install(DIRECTORY DESTINATION ${CMAKE_INSTALL_BINDIR}/plugins)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/assets/zibby-icon.ico
    DESTINATION ${CMAKE_INSTALL_DATADIR}/zibby
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/assets/zibby-icon.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/zibby
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
    DESTINATION ${CMAKE_INSTALL_DATADIR}/zibby
)

set(CPACK_PACKAGE_NAME "zibby-service")
set(CPACK_PACKAGE_VENDOR "Zibby")
set(CPACK_PACKAGE_CONTACT "security@zibby.local")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Zibby decentralized IP messenger service by v3lom (https://github.com/v3lom/zibby-service)")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_SOURCE_DIR}/installers")
set(CPACK_GENERATOR "TGZ;ZIP")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/v3lom/zibby-service")
set(CPACK_NSIS_DISPLAY_NAME "zibby-service")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "zibby-service")

if(WIN32)
    # Best-effort Windows service setup during install/uninstall.
    # If the installer is not elevated, these commands will fail harmlessly.
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS [[
        ; Best-effort: prefer Windows Service; fallback to HKCU autostart if not elevated
        ExecWait '"$INSTDIR\\bin\\zibby-service.exe" --install-best-effort'
    ]])

    set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS [[
        ; Best-effort cleanup: stop/remove service and disable HKCU autostart
        ExecWait '"$INSTDIR\\bin\\zibby-service.exe" --uninstall-best-effort'
    ]])
endif()

if(UNIX AND NOT APPLE)
    list(APPEND CPACK_GENERATOR "DEB;RPM")
endif()

if(WIN32)
    find_program(MAKENSIS_EXECUTABLE makensis)
    if(MAKENSIS_EXECUTABLE)
        list(APPEND CPACK_GENERATOR "NSIS")
    else()
        message(WARNING "NSIS (makensis) was not found. Skipping NSIS package generation.")
    endif()
endif()

include(CPack)

if(WIN32)
    add_custom_target(generate_checksums
        COMMAND powershell -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_checksums.ps1" -ArtifactsDir "${CMAKE_SOURCE_DIR}/installers"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating SHA256 checksums for installer artifacts"
    )
else()
    add_custom_target(generate_checksums
        COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_checksums.sh" "${CMAKE_SOURCE_DIR}/installers"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating SHA256 checksums for installer artifacts"
    )
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/docs/doxygen/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)

find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
    )
else()
    add_custom_target(docs
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/docs ${CMAKE_CURRENT_BINARY_DIR}/docs
        COMMENT "Doxygen not found, copying markdown docs"
    )
endif()

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
