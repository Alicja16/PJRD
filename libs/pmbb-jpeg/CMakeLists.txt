cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

#=========================================================================================================================================
# library PUT-Multimedia-Building-Blocks - JPEG
#=========================================================================================================================================
set(LIB_CURRENT_NAME "pmbb-jpeg")
set(LIB_DEPENDENCIES "${LIB_PMBB_BASE_NAME}" "${LIB_PMBB_CORE_NAME}" "${LIB_PMBB_CMPR_NAME}")

set(LIB_PMBB_JPEG_NAME "${LIB_CURRENT_NAME}")
set(LIB_PMBB_JPEG_NAME "${LIB_CURRENT_NAME}" PARENT_SCOPE)

if(NOT PMBB_GENERATE_MULTI_MICROARCH_LEVEL_BINARIES)  
  PMBB_setup_lib_common_default()
else() #PMBB_GENERATE_MULTI_MICROARCH_LEVEL_BINARIES
  PMBB_setup_lib_common_multiarch()
endif() #PMBB_GENERATE_MULTI_MICROARCH_LEVEL_BINARIES

#=========================================================================================================================================
# Testing can be enabled for non-PMBB_GENERATE_MULTI_MICROARCH_LEVEL_BINARIES builds only
#=========================================================================================================================================
if(CMAKE_TESTING_ENABLED AND (NOT PMBB_GENERATE_MULTI_MICROARCH_LEVEL_BINARIES))
  set(LIST_TESTS "Transform" "Quant" "Scan" "OptUtils" "EntropyUtils" "Huffman" "EntropyHuffman" "JFIF")
  PMBB_setup_lib_test()
endif()
  
