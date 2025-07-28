add_library(fgs_ies_lib OBJECT
            NasKeySetIdentifier.c
            FGSMobileIdentity.c
)

target_include_directories(fgs_ies_lib PUBLIC
                           ${CMAKE_SOURCE_DIR}
                           ${CMAKE_CURRENT_SOURCE_DIR}
)
