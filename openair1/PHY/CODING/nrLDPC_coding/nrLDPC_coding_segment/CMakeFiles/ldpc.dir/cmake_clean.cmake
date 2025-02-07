file(REMOVE_RECURSE
  "../../../../../libldpc.pdb"
  "../../../../../libldpc.so"
)

# Per-language clean rules from dependency scanning.
foreach(lang C)
  include(CMakeFiles/ldpc.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
