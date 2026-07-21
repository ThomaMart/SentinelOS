# Test app pour le parser du protocole UART (firmware/components/protocol).
#
# Ne compile QUE protocol_parser.c (pure, sans Wi-Fi/OTA/display) -- pas la
# "protocol" component complète, pour rester un test unitaire rapide et
# isolé plutôt que de traîner toute la chaîne de dépendances de l'app.
cmake_minimum_required(VERSION 3.22)

set(COMPONENTS main)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(protocol_parser_test)
