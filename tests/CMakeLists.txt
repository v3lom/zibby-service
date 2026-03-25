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
