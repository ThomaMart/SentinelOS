idf_component_register(
    SRCS
        "config.c"

    INCLUDE_DIRS
        "."
        # Local, git-ignored secrets (Wi-Fi credentials). Provision by copying
        # wifi_credentials.example.h to firmware/secrets/wifi_credentials.h.
        "../../secrets"
)
