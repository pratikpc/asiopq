# CMakeList.txt : CMake project for lib, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project(asiopq
	VERSION 1.0
	LANGUAGES CXX)

find_package(Boost COMPONENTS REQUIRED)
find_package(PostgreSQL REQUIRED)
find_package(Threads REQUIRED)
# Add source to this project's executable.
add_library (${PROJECT_NAME} STATIC)

target_include_directories(${PROJECT_NAME} 
	PUBLIC
		include
)
target_sources(${PROJECT_NAME}
	PRIVATE
	include/asiopq/Connection.hpp
	src/asiopq/Connection.cpp
	include/asiopq/ResultPtr.hpp
	src/asiopq/ResultPtr.cpp
	include/asiopq/ValuePtr.hpp
	src/asiopq/ValuePtr.cpp
	include/asiopq/NotifyPtr.hpp
	src/asiopq/NotifyPtr.cpp
)

target_compile_features(${PROJECT_NAME}
	PUBLIC
		cxx_std_20
)

target_link_libraries(${PROJECT_NAME} 
	PUBLIC
		Boost::boost
		Boost::cobalt
		PostgreSQL::PostgreSQL
		Threads::Threads
)

# TODO: Add tests and install targets if needed.
