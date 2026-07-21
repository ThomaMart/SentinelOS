idf_component_register(
    SRCS
        "diag.c"

    INCLUDE_DIRS
        "."

    REQUIRES
        storage
        esp_system
        espcoredump
)
