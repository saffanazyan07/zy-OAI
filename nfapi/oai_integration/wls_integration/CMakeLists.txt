if (OAI_WLS)
    message(STATUS "Building with support for WLS")
    add_library(wls_integration_lib
            wls_pnf.c
    )

    find_library(WLS_LIB wls REQUIRED)
    if (NOT WLS_LIB)
        message(FATAL_ERROR "-- wls library not found")
    else ()
        message(STATUS "  wls library found in ${WLS_LIB}")
    endif ()

    find_library(hugetlbfs_LIB hugetlbfs REQUIRED)
    if (NOT hugetlbfs_LIB)
        message(FATAL_ERROR "-- hugetlbfs library not found")
    else ()
        message(STATUS "  hugetlbfs library found in ${hugetlbfs_LIB}")
    endif ()

    pkg_check_modules(dpdk REQUIRED libdpdk=20.11.3)
    target_link_libraries(wls_integration_lib PRIVATE ${dpdk_LINK_LIBRARIES})
    target_include_directories(wls_integration_lib PRIVATE ${dpdk_INCLUDE_DIRS})

    target_compile_definitions(wls_integration_lib PUBLIC ENABLE_WLS)
    target_link_libraries(wls_integration_lib PRIVATE NFAPI_LIB)
    target_link_libraries(wls_integration_lib PRIVATE asn1_lte_rrc_hdrs asn1_nr_rrc_hdrs)
    target_link_libraries(wls_integration_lib PRIVATE nr_fapi_p5 nr_fapi_p7)
    target_link_libraries(wls_integration_lib PRIVATE "${WLS_LIB}" "${hugetlbfs_LIB}")
else ()
    message(STATUS "No Support for WLS")
endif ()
