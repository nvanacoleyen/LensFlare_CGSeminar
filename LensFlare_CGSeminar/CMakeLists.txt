cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(ComputerGraphics C CXX)

# Set CMake to use C++17 or later if not already set
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable RTTI explicitly for MSVC
if (MSVC)
    add_compile_options(/GR) # Enable Runtime Type Information
endif()

# Set this before including framework such that it knows to use the OpenGL4.5 version of GLAD
if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/framework")
	# Create framework library and include CMake scripts (compiler warnings, sanitizers and static analyzers).
	add_subdirectory("framework")
else()
	# During development the framework lives in parent folder.
	add_subdirectory("../../../framework/" "${CMAKE_BINARY_DIR}/framework/")
endif()

add_executable(FinalProject
    "src/application.cpp"
    "src/ray_transfer_matrices.cpp"
	"src/ray_transfer_matrices.h"
	"src/lens_system.h"
	"src/lens_system.cpp"
	"src/quad.cpp"
	"src/quad.h"
	"src/camera.cpp"
	"src/camera.h"
	"src/utils.h"
	"src/utils.cpp"
	"src/preset_lens_systems.cpp"
	"src/preset_lens_systems.h" 
	"src/starburst.cpp"
	"src/starburst.h"
	"src/reverse_coating.cpp"
	"src/reverse_coating.h"
	"src/lens_solver.h"
	"src/lens_solver.cpp")
target_compile_features(FinalProject PRIVATE cxx_std_17)
target_link_libraries(FinalProject PRIVATE CGFramework)
enable_sanitizers(FinalProject)
set_project_warnings(FinalProject)

# Copy all files in the resources folder to the build directory after every successful build.
add_custom_command(TARGET FinalProject POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	"${CMAKE_CURRENT_LIST_DIR}/resources/" "$<TARGET_FILE_DIR:FinalProject>/resources/")

# We would like to copy the files when they changed. Even if no *.cpp files were modified (and
# thus no build is triggered). We tell CMake that the executable depends on the shader files in
# the build directory. We also tell it how to generate those files (by copying them from the
# shaders folder in this directory). The gather all glsl files in the shaders folder when CMake
# is configured. So if you were to add a shader file then you need to configure CMake again.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/shaders/")

set(shader_copies "")
# List all *.glsl files in the shaders folder
file(GLOB shader_sources "${CMAKE_CURRENT_LIST_DIR}/shaders/*.glsl")
foreach (shader_file IN LISTS shader_sources)
	get_filename_component(file_name ${shader_file} NAME)
	message("shader_file: ${file_name}")
	add_custom_command(
		OUTPUT "${CMAKE_BINARY_DIR}/shaders/${file_name}"
		COMMAND ${CMAKE_COMMAND} -E copy
		"${CMAKE_CURRENT_LIST_DIR}/shaders/${file_name}"
		"${CMAKE_BINARY_DIR}/shaders/${file_name}"
		DEPENDS "${CMAKE_CURRENT_LIST_DIR}/shaders/${file_name}"
		)
	LIST(APPEND shader_copies "${CMAKE_BINARY_DIR}/shaders/${file_name}")
endforeach()
add_custom_target(copy_shaders DEPENDS ${shader_copies})
add_dependencies(FinalProject copy_shaders)

add_custom_command(TARGET FinalProject POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_LIST_DIR}/framework/third_party/opencv/build/x64/vc16/bin/opencv_world4100d.dll"
    $<TARGET_FILE_DIR:FinalProject>
)

add_custom_command(TARGET FinalProject POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_LIST_DIR}/framework/third_party/opencv/build/x64/vc16/bin/opencv_world4100.dll"
    $<TARGET_FILE_DIR:FinalProject>
)

add_custom_command(TARGET FinalProject POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<TARGET_FILE_DIR:FinalProject>/framework/third_party/pagmo2/pagmo.dll"
    $<TARGET_FILE_DIR:FinalProject>    
)

add_custom_command(TARGET FinalProject POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_LIST_DIR}/framework/third_party/onetbb/redist/intel64/vc14/tbb12_debug.dll"
    $<TARGET_FILE_DIR:FinalProject>
)


