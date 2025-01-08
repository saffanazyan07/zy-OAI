add_library(nr_coding_segment_utils OBJECT
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
