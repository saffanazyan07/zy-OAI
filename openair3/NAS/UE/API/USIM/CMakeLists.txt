add_library(usim_lib OBJECT
    ${OPENAIR_DIR}/common/utils/mem/memory.c
    aka_functions.c
    usim_api.c
)

target_include_directories(usim_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
