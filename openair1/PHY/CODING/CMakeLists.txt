add_library(coding MODULE
            3gpplte_sse.c
            3gpplte.c
            3gpplte_turbo_decoder_sse_8bit.c
            3gpplte_turbo_decoder_sse_16bit.c
            3gpplte_turbo_decoder_avx2_16bit.c
            3gpplte_turbo_decoder.c
)
set_target_properties(coding PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_library(ldpc_orig MODULE
            nrLDPC_decoder/nrLDPC_decoder.c
            nrLDPC_encoder/ldpc_encoder.c
)
set_target_properties(ldpc_orig PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_link_libraries(ldpc_orig PRIVATE ldpc_gen_HEADERS)

add_library(ldpc_optim MODULE
            nrLDPC_decoder/nrLDPC_decoder.c
            nrLDPC_encoder/ldpc_encoder_optim.c
)
set_target_properties(ldpc_optim PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_link_libraries(ldpc_optim PRIVATE ldpc_gen_HEADERS)

add_library(ldpc_optim8seg MODULE
            nrLDPC_decoder/nrLDPC_decoder.c
            nrLDPC_encoder/ldpc_encoder_optim8seg.c
)
set_target_properties(ldpc_optim8seg PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_link_libraries(ldpc_optim8seg PRIVATE ldpc_gen_HEADERS)

add_library(ldpc_optim8segmulti MODULE
            nrLDPC_decoder/nrLDPC_decoder.c
            nrLDPC_encoder/ldpc_encoder_optim8segmulti.c
)
set_target_properties(ldpc_optim8segmulti PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_link_libraries(ldpc_optim8segmulti PRIVATE ldpc_gen_HEADERS)

add_custom_target(nrLDPC_decoder_kernels_CL
                  COMMAND gcc nrLDPC_decoder/nrLDPC_decoder_CL.c -dD -DNRLDPC_KERNEL_SOURCE -E -o ${CMAKE_CURRENT_BINARY_DIR}/nrLDPC_decoder_kernels_CL.clc
                  SOURCES nrLDPC_decoder/nrLDPC_decoder_CL.c
                  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
add_library(ldpc_cl MODULE
            nrLDPC_decoder/nrLDPC_decoder_CL.c
            nrLDPC_encoder/ldpc_encoder_optim8segmulti.c
)
set_target_properties(ldpc_cl PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_link_libraries(ldpc_cl PRIVATE OpenCL)
add_dependencies(ldpc_cl nrLDPC_decoder_kernels_CL)

##############################################
# Base CUDA setting
##############################################

add_boolean_option(ENABLE_LDPC_CUDA OFF "Build support for CUDA" OFF)
if (ENABLE_LDPC_CUDA)
  find_package(CUDA REQUIRED)
  if (NOT CUDA_FOUND)
    message(FATAL_ERROR "no CUDA found")
  endif()
  SET(CUDA_NVCC_FLAG "${CUDA_NVCC_FLAGS};-arch=sm_60;")
  SET(CUDA_VERBOSE_BUILD ON)
  cuda_add_library(ldpc_cuda MODULE
                   nrLDPC_decoder_LYC/nrLDPC_decoder_LYC.cu
                   nrLDPC_encoder/ldpc_encoder_optim8segmulti.c
  )
  set_target_properties(ldpc_cuda PROPERTIES CUDA_SEPARABLE_COMPILATION ON)
  set_target_properties(ldpc_cuda PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
endif()

add_dependencies(ldpctest ldpc_orig ldpc_optim ldpc_optim8seg ldpc_optim8segmulti)
if (ENABLE_LDPC_CUDA)
  add_dependencies(ldpctest ldpc_cuda)
endif()

add_subdirectory(nrLDPC_coding)
