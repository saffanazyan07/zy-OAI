add_library(nas_common_ies_lib OBJECT
    AuthenticationResponseParameter.c
    NasKeySetIdentifier.c
)

target_include_directories(nas_common_ies_lib PUBLIC .)
