# we need at least gcc-11 to build xran, so let's enforce it here
# (because xran might not check it, and we have control here)
if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 11.0
   OR CMAKE_C_COMPILER_VERSION VERSION_LESS 11.0)
 message(FATAL_ERROR "you need to have at least gcc/g++-11 to use 7.2 FHI through xran")
endif()

# use env var PKG_CONFIG_PATH to override paths to libdpdk.pc
pkg_check_modules(dpdk REQUIRED libdpdk)
pkg_check_modules(numa REQUIRED numa)

add_library(oran_fhlib_5g MODULE
            oran_isolate.c
            oaioran.c
            oran-config.c
            oran-init.c
)

find_package(xran REQUIRED)
if(xran_VERSION VERSION_EQUAL 5.1.4)
  target_compile_definitions(oran_fhlib_5g PRIVATE E_RELEASE)
elseif(xran_VERSION VERSION_EQUAL 6.1.1)
  target_compile_definitions(oran_fhlib_5g PRIVATE F_RELEASE)
else()
  message(FATAL_ERROR "Found xran version ${xran_VERSION} but needed 5.1.4 (E release) or 6.1.1 (F release)")
endif()

# Ignore xran-specific warning: we don't care/can't change the following warning, so suppress
# alignment 1 of 'struct XYZ' is less than 2
add_compile_options(-Wno-packed-not-aligned)

set_target_properties(oran_fhlib_5g PROPERTIES COMPILE_FLAGS "-fvisibility=hidden -march=native")
target_link_libraries(oran_fhlib_5g PRIVATE xran::xran)
target_link_libraries(oran_fhlib_5g PRIVATE ${dpdk_LINK_LIBRARIES})
target_include_directories(oran_fhlib_5g PRIVATE ${dpdk_INCLUDE_DIRS})
target_link_libraries(oran_fhlib_5g PRIVATE pthread dl rt m numa)
target_link_libraries(oran_fhlib_5g PRIVATE ${T_LIB})

# TODO: can be removed?
target_include_directories(oran_fhlib_5g PRIVATE ${xran_INCLUDE_DIRS}/../src/)

set_target_properties(oran_fhlib_5g PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
add_custom_command(TARGET oran_fhlib_5g POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E create_symlink liboran_fhlib_5g.so liboai_transpro.so
                   WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

message(STATUS "Temporary added -DOAI_FHI72")
target_compile_definitions(nr-softmodem PUBLIC OAI_FHI72)
