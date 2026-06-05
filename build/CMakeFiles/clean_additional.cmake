# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles/snipQ_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/snipQ_autogen.dir/ParseCache.txt"
  "snipQ_autogen"
  )
endif()
