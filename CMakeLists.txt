cmake_minimum_required(VERSION 3.18)

# ---- Project ----

project(sfee LANGUAGES CXX)

include(FetchContent)

FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz DOWNLOAD_EXTRACT_TIMESTAMP YES)
FetchContent_MakeAvailable(json)

FetchContent_Declare(simpleini GIT_REPOSITORY https://github.com/brofield/simpleini.git)
FetchContent_MakeAvailable(simpleini)

# ---- Include guards ----

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
	message(
		FATAL_ERROR
			"In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
)
endif()

if (NOT TARGET xbyak)
	add_subdirectory(../sfse/xbyak xbyak)
endif()

file(GLOB headers CONFIGURE_DEPENDS *.h)
file(GLOB sources CONFIGURE_DEPENDS *.cpp)

include_directories(${PROJECT_NAME} PUBLIC ../sfse)

source_group(
	${PROJECT_NAME}
	FILES
	${headers}
	${sources}
)

include(cmake/helpers/generate_product_version.cmake)
generate_product_version(
   VersionFilesOutputVariable
   NAME "Starfield Engine Extender"
   ICON ""
   VERSION_MAJOR 1
   VERSION_MINOR 0
   VERSION_PATCH 0
   VERSION_REVISION 1
)

add_library(
	${PROJECT_NAME}
	SHARED
	${headers}
	${sources}
	${VersionFilesOutputVariable}
)

# Build sfse_common as static library
file(GLOB sfse_common_header "../sfse/sfse_common/*.h")
file(GLOB sfse_common_sources "../sfse/sfse_common/*.cpp")
add_library(sfse_common_static STATIC ${sfse_common_header} ${sfse_common_sources})
target_include_directories(sfse_common_static PUBLIC ../sfse)
set_target_properties(sfse_common_static PROPERTIES LINKER_LANGUAGE CXX MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_link_libraries(sfse_common_static PUBLIC)
target_compile_features(sfse_common_static PUBLIC cxx_std_20)

# Build sfse as static library
file(GLOB sfse_header "../sfse/sfse/*.h")
file(GLOB sfse_sources "../sfse/sfse/*.cpp")
include(../sfse/cmake/versioning.cmake)
add_library(sfse_static STATIC ${sfse_header} ${sfse_sources} ${xbyak_SOURCE_DIR})
target_compile_definitions(sfse_static PRIVATE RUNTIME RUNTIME_VERSION=${RUNTIME_VERSION_PACKED})
target_include_directories(sfse_static PUBLIC ../sfse ../sfse/xbyak)
set_target_properties(sfse_static PROPERTIES LINKER_LANGUAGE CXX MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_link_libraries(sfse_static PUBLIC sfse_common_static)
target_compile_features(sfse_static PUBLIC cxx_std_20)

# Link the new lib to the statically build libs
include(cmake/configuration.cmake)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)
target_include_directories(${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..> $<INSTALL_INTERFACE:include>)
target_link_libraries(${PROJECT_NAME} PUBLIC sfse_common_static sfse_static)

target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)
include_directories(${PROJECT_NAME} PRIVATE ${simpleini_SOURCE_DIR})

set_target_properties(
	${PROJECT_NAME}
	sfse_common_static
	sfse_static
	PROPERTIES
		MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

include(cmake/installation.cmake)