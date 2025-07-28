add_library(nr_coding_segment_utils OBJECT
  nrLDPC_load.c
  nr_rate_matching.c
)

add_library(nr_coding_segment_decoder OBJECT
  nrLDPC_coding_segment_decoder.c
)

add_library(nr_coding_segment_encoder OBJECT
  nrLDPC_coding_segment_encoder.c
)

#ensure that the T header files are generated before targets depending on them
if (${T_TRACER})
  add_dependencies(nr_coding_segment_utils generate_T)
  add_dependencies(nr_coding_segment_decoder generate_T)
  add_dependencies(nr_coding_segment_encoder generate_T)
endif (${T_TRACER})

add_library(ldpc MODULE)
target_link_libraries(ldpc PRIVATE
  nr_coding_segment_decoder
  nr_coding_segment_encoder
  nr_coding_segment_utils
)
set_target_properties(ldpc PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_dependencies(nr-softmodem ldpc)
add_dependencies(nr-uesoftmodem ldpc)
add_dependencies(nr_ulsim ldpc)
add_dependencies(nr_ulschsim ldpc)
add_dependencies(nr_dlsim ldpc)
add_dependencies(nr_dlschsim ldpc)

add_dependencies(ldpc ldpc_orig ldpc_optim ldpc_optim8seg ldpc_optim8segmulti)
if (ENABLE_LDPC_CUDA)
  add_dependencies(ldpc ldpc_cuda)
endif()
