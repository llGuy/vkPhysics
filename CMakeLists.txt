cmake_minimum_required(VERSION "3.2")


project(vkPhysics)


set(CMAKE_CXX_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSTB_IMAGE_IMPLEMENTATION -D_MBCS -DCIMGUI_DEFINE_ENUMS_AND_STRUCTS")
# This will be useful if on Windows
set(CURL_LIBRARIES "")

# Get source files
file(GLOB_RECURSE COMMON_SOURCES "source/common/*.cpp" "source/common/*.hpp" "dependencies/sha/sha1.cpp")
file(GLOB_RECURSE CLIENT_SOURCES "source/client/*.cpp" "source/client/*.hpp")
file(GLOB_RECURSE SERVER_SOURCES "source/server/*.cpp" "source/server/*.hpp")
file(GLOB_RECURSE RENDERER_SOURCES "source/renderer/*.cpp" "source/renderer/*.hpp" "dependencies/imgui/lib/*.cpp")
file(GLOB_RECURSE ENGINE_SOURCES "source/engine/*.cpp" "source/engine/*.hpp")

# Include directories for all targets
include_directories("${CMAKE_SOURCE_DIR}/dependencies/stb")
include_directories("${CMAKE_SOURCE_DIR}/dependencies/glm")
include_directories("${CMAKE_SOURCE_DIR}/dependencies/sha")
include_directories("${CMAKE_SOURCE_DIR}/dependencies/imgui/include")
include_directories("${CMAKE_SOURCE_DIR}/source")
include_directories("${CMAKE_SOURCE_DIR}/source/renderer/include")

# Find packages
find_package(Vulkan)
find_package(CURL)

# Threading library
if (NOT WIN32)
  link_libraries("pthread")
endif()

# Create common
add_library(common STATIC "${COMMON_SOURCES}")
target_compile_definitions(common PUBLIC PROJECT_ROOT="${CMAKE_SOURCE_DIR}")

# Create engine
add_library(engine STATIC "${ENGINE_SOURCES}")
target_compile_definitions(engine PUBLIC PROJECT_ROOT="${CMAKE_SOURCE_DIR}")

# Do not worry, this will be cleaned up after net refactor
target_include_directories(common PUBLIC "${CMAKE_SOURCE_DIR}/source/engine/")
target_include_directories(engine PUBLIC "${CMAKE_SOURCE_DIR}/source/common/")

if(Vulkan_FOUND)
  message(STATUS "Found Vulkan package in system")
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

  target_include_directories(renderer PUBLIC "engine")
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
endif()

if(CURL_FOUND)
  message("Found CURL")
  include_directories("${CURL_INCLUDE_DIRS}")
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

# Create client (recheck if statement for clarity - TODO: merge renderer with client)
if(BUILD_CLIENT)
  add_executable(vkPhysics_client "${CLIENT_SOURCES}")
  target_link_libraries(vkPhysics_client PUBLIC "renderer" "common" "engine" "${CURL_LIBRARIES}")
  target_compile_definitions(vkPhysics_client PUBLIC "LINK_AGAINST_RENDERER")
endif()

# Create server (server can get built on whatever platform with or without vulkan support)
add_executable(vkPhysics_server "${SERVER_SOURCES}")
target_link_libraries(vkPhysics_server PUBLIC "common" "engine" "${CURL_LIBRARIES}")
message("${$CURL_LIBRARIES}")

if(WIN32)
  target_link_libraries(vkPhysics_server PUBLIC "winmm.lib" "wldap32.lib" "crypt32.lib" "normaliz.lib")
  target_compile_definitions(vkPhysics_server PUBLIC CURL_STATICLIB)

  target_link_libraries(vkPhysics_client PUBLIC "wldap32.lib" "crypt32.lib" "normaliz.lib")
  target_compile_definitions(vkPhysics_client PUBLIC CURL_STATICLIB)

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
 
