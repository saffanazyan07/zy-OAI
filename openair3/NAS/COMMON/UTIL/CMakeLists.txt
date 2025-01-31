add_library(nas_utils OBJECT
    TLVDecoder.c
    OctetString.c
)

target_include_directories(nas_utils PRIVATE .)
