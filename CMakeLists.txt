cmake_minimum_required(VERSION 3.10)

# set the project name
project(Gluboy)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(UNIX AND NOT APPLE)
	set(LINUX TRUE)
endif()

if(LINUX AND NOT EMSCRIPTEN)
# Use the package PkgConfig to detect GTK+ headers/library files
FIND_PACKAGE(PkgConfig REQUIRED)
PKG_CHECK_MODULES(GTK3 REQUIRED gtk+-3.0)

# Setup CMake to use GTK+, tell the compiler where to look for headers
# and to the linker where to look for libraries
INCLUDE_DIRECTORIES(${GTK3_INCLUDE_DIRS})
LINK_DIRECTORIES(${GTK3_LIBRARY_DIRS})

ADD_DEFINITIONS(${GTK3_CFLAGS_OTHER})

file(GLOB PLATFORM_SOURCES ./libs/nfd/nfd_gtk.c)
endif()


file(GLOB SOURCES 
	./src/*.cpp
	./libs/imgui/*.cpp
)

if(NOT EMSCRIPTEN)
	add_compile_definitions(IMGUI_IMPL_OPENGL_LOADER_GL3W)
	file(GLOB NFD_SOURCES 
		./libs/gl3w/GL/*.c
		./libs/nfd/nfd_common.c)
else()
	add_link_options ("SHELL:-s USE_GLFW=3"  "SHELL:-s USE_WEBGL2=1" "SHELL:-s FULL_ES3=1" "SHELL:-s WASM=1" )
	add_link_options ("SHELL:-s ALLOW_MEMORY_GROWTH=1"  "SHELL:-s DISABLE_EXCEPTION_CATCHING=1" "SHELL:-s NO_EXIT_RUNTIME=0" )
	add_link_options ("SHELL:-s ASSERTIONS=1")
endif()

# Use folders for nice tree in Visual Studio and XCode
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(WIN32)
	set(CORE_MSVC_USE_COMMON_OPTIONS off)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
	set(OPENAL_LIBRARY OpenAL32)
	set(GLFW_LIBRARY glfw3 opengl32)
	file(GLOB PLATFORM_SOURCES 
			./libs/nfd/nfd_win.cpp
			./icon/Gluboy.rc)

else()
	if(NOT EMSCRIPTEN)
		find_library(OPENAL_LIBRARY openal OpenAL al OpenAL32 )
	endif()
	if(APPLE)
		set(CMAKE_C_FLAGS "-x objective-c")
		find_library(GL_LIBRARY OpenGL)
		find_library(COCOA_LIBRARY Cocoa)
		find_library(IOKIT_LIBRARY IOKit)
		find_library(CORE_VIDEO_LIBRARY CoreVideo)
		set(GLFW_LIBRARY glfw3)
		file(GLOB PLATFORM_SOURCES 
			./libs/nfd/nfd_cocoa.m)
	else()
		set(GLFW_LIBRARY glfw GL)
	endif()
endif()

# add the executable
add_executable(Gluboy ${SOURCES} ${NFD_SOURCES} ${PLATFORM_SOURCES})

TARGET_LINK_LIBRARIES(Gluboy 
						${OPENAL_LIBRARY}
						${COCOA_LIBRARY} 
						${GL_LIBRARY} 
						${IOKIT_LIBRARY} 
						${CORE_VIDEO_LIBRARY} 
						${GLFW_LIBRARY}
						${GTK3_LIBRARIES}
						${CMAKE_DL_LIBS})

target_include_directories(Gluboy PUBLIC
        "${PROJECT_SOURCE_DIR}/libs/gl3w"
        "${PROJECT_SOURCE_DIR}/libs/"
        "${PROJECT_SOURCE_DIR}/libs/imgui"
		"${PROJECT_SOURCE_DIR}/libs/nfd/include"
        )
        
if(WIN32)
	target_link_options(Gluboy PUBLIC "LINKER:/subsystem:windows,/entry:mainCRTStartup")
	target_link_directories(Gluboy PUBLIC
		"${PROJECT_SOURCE_DIR}/libs/OpenAL/libs/"
		"${PROJECT_SOURCE_DIR}/libs/glfw/lib-vc2012/")
	target_include_directories(Gluboy PUBLIC
        "${PROJECT_SOURCE_DIR}/libs/glfw/include"
		"${PROJECT_SOURCE_DIR}/libs/OpenAl/include"
        )

endif()

if(APPLE)
	target_link_directories(Gluboy PUBLIC
		"${PROJECT_SOURCE_DIR}/libs/glfw/lib-macos/")
	target_include_directories(Gluboy PUBLIC
        "${PROJECT_SOURCE_DIR}/libs/glfw/include"
        )

endif()

file(COPY icon DESTINATION ${CMAKE_BINARY_DIR})