idf_component_register(
    SRCS
        "test_app_main.c"
        "test_protocol_parser.c"
        "../../../protocol_parser.c"

    INCLUDE_DIRS
        "../../.."

    PRIV_REQUIRES
        unity

    WHOLE_ARCHIVE
)
