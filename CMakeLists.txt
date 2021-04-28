cmake_minimum_required(VERSION "3.2")

project(vkPhysics)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSTB_IMAGE_IMPLEMENTATION -D_MBCS -DCIMGUI_DEFINE_ENUMS_AND_STRUCTS -DCURL_STATICLIB")
# This will be useful if on Windows
set(CURL_LIBRARIES "")

# Get source files
file(GLOB_RECURSE COMMON_SOURCES "source/common/*.cpp" "source/common/*.hpp" "dependencies/sha/sha1.cpp")
file(GLOB_RECURSE CLIENT_SOURCES "source/client/*.cpp" "source/client/*.hpp")
file(GLOB_RECURSE SERVER_SOURCES "source/server/*.cpp" "source/server/*.hpp")
file(GLOB_RECURSE NET_SOURCES "source/net/*.cpp" "source/net/*.hpp")
file(GLOB_RECURSE ENGINE_SOURCES "source/engine/*.cpp" "source/engine/*.hpp")
file(GLOB_RECURSE RENDERER_SOURCES "source/renderer/*.cpp" "source/renderer/*.hpp" "dependencies/imgui/lib/*.cpp")
file(GLOB_RECURSE AUDIO_SOURCES "source/audio/*.cpp" "source/audio/*.hpp")
file(GLOB_RECURSE UIUX_SOURCES "source/uiux/*.cpp" "source/uiux/*.hpp")

# Include directories for all targets
include_directories("${CMAKE_SOURCE_DIR}/dependencies/stb")
include_directories("${CMAKE_SOURCE_DIR}/dependencies/glm")
include_directories("${CMAKE_SOURCE_DIR}/dependencies/sha")
include_directories("${CMAKE_SOURCE_DIR}/dependencies/imgui/include")
include_directories("${CMAKE_SOURCE_DIR}/source/common")
include_directories("${CMAKE_SOURCE_DIR}/source/net")
include_directories("${CMAKE_SOURCE_DIR}/source/engine")
include_directories("${CMAKE_SOURCE_DIR}/source/uiux/")
include_directories("${CMAKE_SOURCE_DIR}/source/renderer/include")
include_directories("${CMAKE_SOURCE_DIR}/source/audio")

# Find packages
find_package(Vulkan)

if (NOT WIN32)
  find_package(OpenAL)
endif()

find_package(CURL)

# Threading library
if (NOT WIN32)
  link_libraries("pthread")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wformat-security")
endif()

# Create common
add_library(common STATIC "${COMMON_SOURCES}")
target_compile_definitions(common PUBLIC PROJECT_ROOT="${CMAKE_SOURCE_DIR}")

# Create engine
add_library(engine STATIC "${ENGINE_SOURCES}")
target_compile_definitions(engine PUBLIC PROJECT_ROOT="${CMAKE_SOURCE_DIR}")

if (OpenAL_FOUND)
  message(STATUS "Found OpenAL package in system ${OPENAL_INCLUDE_DIR} ${OPENAL_LIBRARY}")
  include_directories("${OPENAL_INCLUDE_DIR}")
else()
  # Do something special for Windows as usual
  include_directories("${CMAKE_SOURCE_DIR}/dependencies/openal-soft-bin-include/include")
endif()

if(Vulkan_FOUND)
  message(STATUS "Found Vulkan package in system ${Vulkan_LIBRARY}")
  set(BUILD_CLIENT true)
  include_directories("${Vulkan_INCLUDE_DIRS}")
else(Vulkan_FOUND)
  message(WARNING "Failed to find Vulkan package in system")

  if(WIN32)
    message(STATUS "On Windows, using bundled vulkan version")
    link_directories("${CMAKE_SOURCE_DIR}/dependencies/vulkan/lib")
    include_directories("${CMAKE_SOURCE_DIR}/dependencies/vulkan/include")
    set(BUILD_CLIENT true)
  else(WIN32)
    set(BUILD_CLIENT false)
    message(WARNING "Cannot use bundled vulkan libraries, on Linux - not buliding client")
  endif()
endif()

# Create renderer 
if(BUILD_CLIENT)
  add_library(renderer STATIC "${RENDERER_SOURCES}")
  target_compile_definitions(renderer PUBLIC "LINK_AGAINST_RENDERER")
  if(WIN32)
    link_directories("${CMAKE_SOURCE_DIR}/dependencies/glfw/lib")
    target_link_libraries(renderer PUBLIC "user32.lib" "gdi32.lib" "xinput.lib" "ws2_32.lib" "winmm.lib" "msvcrt.lib" "glfw3.lib" "msvcrtd.lib" "libcmtd.lib" "ucrtd.lib")
    target_include_directories(renderer PUBLIC "${CMAKE_SOURCE_DIR}/dependencies/glfw/include")

    if (Vulkan_FOUND)
      target_link_libraries(renderer PUBLIC "${Vulkan_LIBRARY}" "common")
    else (Vulkan_FOUND)
      target_link_libraries(renderer PUBLIC "vulkan-1.lib")
    endif (Vulkan_FOUND)
  else(WIN32)
    target_link_libraries(renderer PUBLIC "${Vulkan_LIBRARY}" "common" "glfw")
  endif()

  # Create uiux
  add_library(uiux STATIC "${UIUX_SOURCES}")
  target_compile_definitions(uiux PUBLIC PROJECT_ROOT="${CMAKE_SOURCE_DIR}")
  target_link_libraries(uiux PUBLIC "renderer")

  # Create audio
  add_library(audio STATIC "${AUDIO_SOURCES}")
  if (WIN32)
    target_link_libraries(audio PUBLIC "${CMAKE_SOURCE_DIR}/dependencies/openal-soft-bin-include/bin/Debug/OpenAL32.lib")
  else()
    target_link_libraries(audio PUBLIC "${OPENAL_LIBRARY}")
  endif()
endif()

if(CURL_FOUND)
  message("Found CURL")
  include_directories("${CURL_INCLUDE_DIRS}")
  message("${CURL_INCLUDE_DIRS}")
else(CURL_FOUND)
  message("Didn't find curl - using bundled curl")

  set(CURL_LIBRARIES "")

  if (WIN32)
    message("On Windows, can use bundled curl binaries")
    include_directories("${CMAKE_SOURCE_DIR}/dependencies/curl/x64/include")

    # set(CURL_LIBRARIES "${CMAKE_SOURCE_DIR}/dependencies/curl/x64/lib/libcurl_a.lib" PARENT_SCOPE)
    target_link_libraries(common PUBLIC "${CMAKE_SOURCE_DIR}/dependencies/curl/x64/lib/libcurl_a.lib")
  else (WIN32)
    message(FATAL_ERROR "Not on Windows, cannot use bundled curl binaries")
  endif()
endif()

# Create net
add_library(net STATIC "${NET_SOURCES}")
target_compile_definitions(net PUBLIC PROJECT_ROOT="${CMAKE_SOURCE_DIR}" -DCURL_STATICLIB)

# Create client (recheck if statement for clarity - TODO: merge renderer with client)
if(BUILD_CLIENT)
  add_executable(vkPhysics_client "${CLIENT_SOURCES}")
  target_link_libraries(vkPhysics_client PUBLIC "renderer" "common" "net" "engine" "uiux" "audio" "${CURL_LIBRARIES}" "${OPENAL_LIBRARY}")
  target_compile_definitions(vkPhysics_client PUBLIC "LINK_AGAINST_RENDERER")
endif()

# Create server (server can get built on whatever platform with or without vulkan support)
add_executable(vkPhysics_server "${SERVER_SOURCES}")
target_link_libraries(vkPhysics_server PUBLIC "common" "net" "engine" "${CURL_LIBRARIES}")
message("${$CURL_LIBRARIES}")

if(WIN32)
  target_link_libraries(vkPhysics_server PUBLIC "winmm.lib" "wldap32.lib" "crypt32.lib" "normaliz.lib")

  target_link_libraries(vkPhysics_client PUBLIC "wldap32.lib" "crypt32.lib" "normaliz.lib")

  target_link_libraries(common PUBLIC "wldap32.lib" "crypt32.lib" "normaliz.lib")
  target_compile_definitions(common PUBLIC CURL_STATICLIB)
  
  INCLUDE (TestBigEndian)
  TEST_BIG_ENDIAN(ENDIAN)
  if (ENDIAN)
    message("Big endian")
	target_compile_definitions(common PUBLIC BIG_ENDIAN_MACHINE)
  else (ENDIAN)
    message("Small endian")
	target_compile_definitions(common PUBLIC SMALL_ENDIAN_MACHINE)
  endif (ENDIAN)
endif(WIN32)
 
