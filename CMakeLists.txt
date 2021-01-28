cmake_minimum_required(VERSION 3.10)

# set the project name
project(Gluboy)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)


if(UNIX AND NOT APPLE)
	set(LINUX TRUE)
endif()

if(LINUX)
# Use the package PkgConfig to detect GTK+ headers/library files
FIND_PACKAGE(PkgConfig REQUIRED)
PKG_CHECK_MODULES(GTK3 REQUIRED gtk+-3.0)

# Setup CMake to use GTK+, tell the compiler where to look for headers
# and to the linker where to look for libraries
INCLUDE_DIRECTORIES(${GTK3_INCLUDE_DIRS})
LINK_DIRECTORIES(${GTK3_LIBRARY_DIRS})

# Add other flags to the compiler
ADD_DEFINITIONS(${GTK3_CFLAGS_OTHER})

file(GLOB PLATFORM_SOURCES ./libs/nfd/nfd_gtk.c)
endif()



file(GLOB SOURCES 
	./src/*.cpp
	./libs/nfd/nfd_common.c
	./libs/imgui/*.cpp
)
# add the executable
add_executable(Gluboy ${SOURCES} ${PLATFORM_SOURCES})

# Use folders for nice tree in Visual Studio and XCode
set_property(GLOBAL PROPERTY USE_FOLDERS ON)


TARGET_LINK_LIBRARIES(Gluboy openal glfw GL ${GTK3_LIBRARIES})

target_include_directories(Gluboy PUBLIC
        "${PROJECT_SOURCE_DIR}/libs/imgui"
		"${PROJECT_SOURCE_DIR}/libs/nfd/include"
        )
        
