# Set the minimum version
cmake_minimum_required(VERSION 3.0)

# Declare the project
project(LensPlanner LANGUAGES CXX)

# Set this before including framework such that it knows to use the OpenGL4.5 version of GLAD
if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/framework")
	# Create framework library and include CMake scripts (compiler warnings, sanitizers and static analyzers).
	add_subdirectory("framework")
else()
	# During development the framework lives in parent folder.
	add_subdirectory("../../../framework/" "${CMAKE_BINARY_DIR}/framework/")
endif()

# Set the CMake module path for find scripts
#set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules")

# Target properties
set(LENS_PLANNER_TARGET_NAME "LensPlanner")
set(LENS_PLANNER_VERSION_MAJOR 1)
set(LENS_PLANNER_VERSION_MINOR 0)
set(LENS_PLANNER_VERSION_PATCH 0)
set(LENS_PLANNER_VERSION ${LENS_PLANNER_VERSION_MAJOR}.${LENS_PLANNER_VERSION_MINOR}.${LENS_PLANNER_VERSION_PATCH})

set(LENS_PLANNER_INSTALL_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/bin CACHE PATH "Install folder for generated targets")
set(LENS_PLANNER_BINARY_INSTALL_FOLDER ${LENS_PLANNER_INSTALL_FOLDER})

list(APPEND CMAKE_PREFIX_PATH "C:/Qt/5.15.18/msvc2019_64/")
list(APPEND CMAKE_PREFIX_PATH "C:/Qt/5.15.18/msvc2019_64/bin/")
list(APPEND CMAKE_PREFIX_PATH "C:/Qt/5.15.18/msvc2019_64/plugins/")
set(QT_PLUGIN_PATH "C:/Qt/5.15.18/msvc2019_64/plugins/")

# Look for required libraries
#find_package(GLM REQUIRED)
#find_package(GLEW REQUIRED)
#find_package(OpenLensFlare REQUIRED)
find_package(Qt5 COMPONENTS Widgets REQUIRED)

# Add the libraries' include directory
#include_directories(${GLM_INCLUDE_DIRS})
#include_directories(${GLEW_INCLUDE_DIR})
#include_directories("framework/third_party/OpenLensFlare/src")

# Let CMake handle all the necessary Qt tooling for us
set(CMAKE_AUTOMOC TRUE)
set(CMAKE_AUTOUIC TRUE)
set(CMAKE_AUTORCC TRUE)

# Enumerate all the files we are interested in
file(GLOB_RECURSE LENS_PLANNER_HEADER_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)
file(GLOB_RECURSE LENS_PLANNER_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB_RECURSE LENS_PLANNER_UI_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.ui)
file(GLOB_RECURSE LENS_PLANNER_QRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.qrc)

# Add the executable target
add_executable(${LENS_PLANNER_TARGET_NAME}
    ${LENS_PLANNER_HEADER_FILES} ${LENS_PLANNER_SOURCE_FILES}
    ${LENS_PLANNER_UI_FILES} ${LENS_PLANNER_QRC_FILES}
)

target_compile_features(LensPlanner PRIVATE cxx_std_20)
target_link_libraries(LensPlanner PRIVATE Qt5::Widgets CGFramework OpenLensFlare)
#enable_sanitizers(LensPlanner)
#set_project_warnings(LensPlanner)

# Copy Qt DLLs to the build directory
add_custom_command(TARGET ${LENS_PLANNER_TARGET_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
"C:/Qt/5.15.18/msvc2019_64/bin/Qt5Cored.dll"
"C:/Qt/5.15.18/msvc2019_64/bin/Qt5Widgetsd.dll"
"C:/Qt/5.15.18/msvc2019_64/bin/Qt5Guid.dll"
$<TARGET_FILE_DIR:${LENS_PLANNER_TARGET_NAME}>)

set(PLUGINS_DIR "${CMAKE_BINARY_DIR}/platforms")
file(MAKE_DIRECTORY ${PLUGINS_DIR})

add_custom_command(TARGET ${LENS_PLANNER_TARGET_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
"C:/Qt/5.15.18/msvc2019_64/plugins/platforms/qwindowsd.dll"
"C:/Qt/5.15.18/msvc2019_64/plugins/platforms/qwindowsd.pdb"
${PLUGINS_DIR})

# Link to the required libraries
#target_link_libraries(${LENS_PLANNER_TARGET_NAME} ${GLEW_LIBRARY})
#target_link_libraries(${LENS_PLANNER_TARGET_NAME} ${OpenLensFlare_LIBRARIES})

#target_link_libraries(${LENS_PLANNER_TARGET_NAME} opengl32)

# Configure the installed files
install(TARGETS ${LENS_PLANNER_TARGET_NAME}
    RUNTIME DESTINATION ${LENS_PLANNER_BINARY_INSTALL_FOLDER}
)
