add_library(oran_fhlib_5g_mplane MODULE
            init-mplane.c
)

set(MPLANE_PATH  "/usr/local" CACHE STRING "M-plane: libyang and libnetconf2 path") # can be in /opt/ directory, if multiple versions exist
set(ENV{PKG_CONFIG_PATH} "${MPLANE_PATH}/lib/pkgconfig")

pkg_check_modules(libyang REQUIRED libyang)
pkg_check_modules(libnetconf2 REQUIRED libnetconf2)

if(libyang_VERSION VERSION_LESS_EQUAL 1.0.240 AND libnetconf2_VERSION VERSION_LESS_EQUAL 1.1.46)
  target_compile_definitions(oran_fhlib_5g_mplane PRIVATE MPLANE_V1)
elseif((libyang_VERSION VERSION_GREATER 1.0.240 AND libyang_VERSION VERSION_LESS_EQUAL 2.1.111) AND (libnetconf2_VERSION VERSION_GREATER 1.1.46 AND libnetconf2_VERSION VERSION_LESS_EQUAL 2.1.37))
  target_compile_definitions(oran_fhlib_5g_mplane PRIVATE MPLANE_V2)
endif()

target_include_directories(oran_fhlib_5g_mplane PRIVATE ${libyang_INCLUDE_DIRS} ${libnetconf2_INCLUDE_DIRS})
target_link_libraries(oran_fhlib_5g_mplane PRIVATE ${libyang_LDFLAGS} ${libyang_LINK_LIBRARIES} ${libnetconf2_LINK_LIBRARIES})

find_package(LibXml2 REQUIRED)
target_link_libraries(oran_fhlib_5g_mplane PRIVATE LibXml2::LibXml2)

message(STATUS "Temporary added -DOAI_MPLANE")
target_compile_definitions(oran_fhlib_5g PUBLIC OAI_MPLANE)

target_link_libraries(oran_fhlib_5g_mplane PRIVATE oran_fhlib_5g)

set_target_properties(oran_fhlib_5g_mplane PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
add_custom_command(TARGET oran_fhlib_5g_mplane POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E create_symlink liboran_fhlib_5g_mplane.so liboai_transpro.so
                   WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
