# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

project ("asiopq-root")

# Include build dependencies
include(FetchContent)

FetchContent_Declare(
	Boost
	URL https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-1.84.0.tar.xz
	URL_MD5 893b5203b862eb9bbd08553e24ff146a
	DOWNLOAD_EXTRACT_TIMESTAMP ON
	EXCLUDE_FROM_ALL
	OVERRIDE_FIND_PACKAGE
)

set(BOOST_INCLUDE_LIBRARIES asio cobalt)
FetchContent_MakeAvailable(Boost)


# Include sub-projects.
add_subdirectory ("asiopq")
add_subdirectory ("example")
