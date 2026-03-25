add_test(
    NAME zibby_help
    COMMAND $<TARGET_FILE:zibby-service> --help
)

add_test(
    NAME zibby_version
    COMMAND $<TARGET_FILE:zibby-service> --version
)

add_executable(zibby_message_flow_test
    unit/message_flow_test.cpp
    ${CMAKE_SOURCE_DIR}/src/core/database.cpp
    ${CMAKE_SOURCE_DIR}/src/core/message.cpp
    ${CMAKE_SOURCE_DIR}/src/core/crypto.cpp
)

target_include_directories(zibby_message_flow_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(zibby_message_flow_test
    PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
        SQLite3::SQLite3
        Boost::filesystem
)

add_test(
    NAME zibby_message_flow_test
    COMMAND $<TARGET_FILE:zibby_message_flow_test>
)

add_executable(zibby_profile_peer_test
    unit/profile_peer_test.cpp
    ${CMAKE_SOURCE_DIR}/src/core/database.cpp
    ${CMAKE_SOURCE_DIR}/src/core/profile.cpp
)

target_include_directories(zibby_profile_peer_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(zibby_profile_peer_test
    PRIVATE
        SQLite3::SQLite3
        Boost::filesystem
)

add_test(
    NAME zibby_profile_peer_test
    COMMAND $<TARGET_FILE:zibby_profile_peer_test>
)

add_executable(zibby_api_server_test
    unit/api_server_test.cpp
    ${CMAKE_SOURCE_DIR}/src/api/api_server.cpp
    ${CMAKE_SOURCE_DIR}/src/core/database.cpp
    ${CMAKE_SOURCE_DIR}/src/core/message.cpp
    ${CMAKE_SOURCE_DIR}/src/core/profile.cpp
    ${CMAKE_SOURCE_DIR}/src/core/crypto.cpp
    ${CMAKE_SOURCE_DIR}/src/core/network.cpp
)

target_include_directories(zibby_api_server_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(zibby_api_server_test
    PRIVATE
        ZIBBY_VERSION="test"
)

target_link_libraries(zibby_api_server_test
    PRIVATE
        Boost::filesystem
        Boost::system
        OpenSSL::SSL
        OpenSSL::Crypto
        SQLite3::SQLite3
        Threads::Threads
)

add_test(
    NAME zibby_api_server_test
    COMMAND $<TARGET_FILE:zibby_api_server_test>
)
