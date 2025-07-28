add_library(fgs_lib OBJECT
            fgs_service_request.c
)

target_link_libraries(fgs_lib PUBLIC fgs_ies_lib)

target_include_directories(fgs_lib PUBLIC
                           ${CMAKE_SOURCE_DIR}
                           ${CMAKE_CURRENT_SOURCE_DIR}
)
