# CMakeList.txt : CMake project for lib, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)
project(main LANGUAGES CXX)

# Add source to this project's executable.
add_executable (${PROJECT_NAME} "main.cpp" )

target_link_libraries(${PROJECT_NAME}
	PRIVATE
		asiopq
)


# TODO: Add tests and install targets if needed.
