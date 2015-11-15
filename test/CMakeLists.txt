file(GLOB_RECURSE HEADER_FILES ../include/*.hpp)

add_executable(tests ${HEADER_FILES} tests.cpp)
