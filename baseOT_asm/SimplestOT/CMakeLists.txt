#project(SimplestOT  C C++ ASM)

enable_language(ASM)

file(GLOB_RECURSE SRC_SIMPLE_LIB 
	${CMAKE_CURRENT_SOURCE_DIR}/*.c
	${CMAKE_CURRENT_SOURCE_DIR}/*.s)

add_library(SimplestOTASM STATIC ${SRC_SIMPLE_LIB})
target_link_libraries(SimplestOTASM PUBLIC cryptoTools)
target_link_options(SimplestOTASM PUBLIC -fPIC -no-pie)



# make projects that include libOTe use this as an include folder
target_include_directories(SimplestOTASM PUBLIC
                    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/..>
                    $<INSTALL_INTERFACE:include>) 
target_include_directories(SimplestOTASM PUBLIC
                    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
                    $<INSTALL_INTERFACE:include>) 
