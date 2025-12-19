cmake_minimum_required(VERSION 3.16)
project(LibreSoundboard VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Widgets REQUIRED)
find_package(PkgConfig REQUIRED)
# Prefer CMake JACK config when available; otherwise use pkg-config
find_package(JACK)
if(NOT JACK_FOUND)
	pkg_check_modules(JACK REQUIRED jack)
endif()
pkg_check_modules(SNDFILE REQUIRED sndfile)
pkg_check_modules(MPG123 REQUIRED libmpg123)
pkg_check_modules(SAMPLERATE REQUIRED samplerate)

# Put runtime binaries under build/bin by default
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(BUILD_TESTING "Build tests" ON)

add_subdirectory(src)

if(BUILD_TESTING)
	enable_testing()
	include(FetchContent)
	FetchContent_Declare(
		catch2
		GIT_REPOSITORY https://github.com/catchorg/Catch2.git
		GIT_TAG v2.13.10
	)
	FetchContent_MakeAvailable(catch2)
	add_subdirectory(tests)
endif()

# Packaging
include(CPack)

# Formatting target (clang-format) - run: cmake --build . --target format
find_program(CLANG_FORMAT_EXE clang-format)
if(CLANG_FORMAT_EXE)
	file(GLOB_RECURSE ALL_SOURCE_FILES
		"${CMAKE_SOURCE_DIR}/src/*.cpp"
		"${CMAKE_SOURCE_DIR}/src/*.h"
		"${CMAKE_SOURCE_DIR}/tests/*.cpp"
	)
	add_custom_target(format
		COMMAND ${CLANG_FORMAT_EXE} -i ${ALL_SOURCE_FILES}
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		COMMENT "Running clang-format on source files"
	)
endif()
