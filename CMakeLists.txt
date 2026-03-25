cmake_minimum_required(VERSION 3.16)

project(zibby-service VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ZIBBY_ENABLE_TESTS "Build tests" ON)
option(ZIBBY_ENABLE_PLUGINS "Enable plugin system" ON)
option(ZIBBY_ENABLE_CALLS "Enable calls module" OFF)
option(ZIBBY_ENABLE_TUI "Enable TUI module" ON)

include(GNUInstallDirs)

find_package(Boost 1.70 REQUIRED COMPONENTS filesystem system program_options)
find_package(OpenSSL REQUIRED)

find_package(SQLite3 REQUIRED)

set(ZIBBY_COMMON_SOURCES
    src/core/service.cpp
    src/core/config.cpp
    src/core/database.cpp
    src/core/crypto.cpp
    src/core/network.cpp
    src/utils/logger.cpp
    src/api/api_server.cpp
)

add_executable(zibby-service
    src/core/main.cpp
    ${ZIBBY_COMMON_SOURCES}
)

target_include_directories(zibby-service
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(zibby-service
    PRIVATE
        ZIBBY_VERSION="${PROJECT_VERSION}"
)

if(MSVC)
    target_compile_options(zibby-service PRIVATE /W4 /permissive-)
else()
    target_compile_options(zibby-service PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(zibby-service
    PRIVATE
        Boost::filesystem
        Boost::system
        Boost::program_options
        OpenSSL::SSL
        OpenSSL::Crypto
        SQLite::SQLite3
)

install(TARGETS zibby-service
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

set(CPACK_PACKAGE_NAME "zibby-service")
set(CPACK_PACKAGE_VENDOR "Zibby")
set(CPACK_PACKAGE_CONTACT "maintainers@zibby.local")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_GENERATOR "ZIP;TGZ")

if(UNIX AND NOT APPLE)
    list(APPEND CPACK_GENERATOR "DEB;RPM")
endif()

if(WIN32)
    list(APPEND CPACK_GENERATOR "NSIS")
endif()

include(CPack)

if(ZIBBY_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
