add_test(
    NAME zibby_help
    COMMAND $<TARGET_FILE:zibby-service> --help
)

add_test(
    NAME zibby_version
    COMMAND $<TARGET_FILE:zibby-service> --version
)
