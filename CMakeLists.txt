cmake_minimum_required(VERSION "3.2")

project(vkPhysics)

set (CMAKE_CXX_STANDARD 11)

file(GLOB_RECURSE GAME_SOURCES "source/game/*.cpp" "source/game/*.hpp")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSTB_IMAGE_IMPLEMENTATION -D_MBCS -DCIMGUI_DEFINE_ENUMS_AND_STRUCTS")

if(NOT WIN32)
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
endif()

# Setup dependencies
include_directories("${CMAKE_SOURCE_DIR}/dependencies/stb")
include_directories("${CMAKE_SOURCE_DIR}/dependencies/glm")
include_directories("${CMAKE_SOURCE_DIR}/dependencies/imgui/include")

find_package(Vulkan)

set(BUILD_GRAPHICAL_APPLICATION false)

if(NOT Vulkan_FOUND)
  # Bundled version is .lib and is only for Windows machines for now
  include_directories("${CMAKE_SOURCE_DIR}/dependencies/vulkan/include")

  if(WIN32)
    link_directories("${CMAKE_SOURCE_DIR}/dependencies/vulkan/lib")
    set(BUILD_GRAPHICAL_APPLICATION true)
  endif()

  message("Using bundled Vulkan version")
else(NOT Vulkan_FOUND)
  # Use package libs and includes that were found
  include_directories("${Vulkan_INCLUDE_DIRS}")
  set(BUILD_GRAPHICAL_APPLICATION true)

  message("Using preinstalled Vulkan")
endif()

file(GLOB_RECURSE COMMON_SOURCES "source/common/*.cpp" "source/common/*.hpp")
add_library(common STATIC "${COMMON_SOURCES}")
set_target_properties(common PROPERTIES PREFIX "")

if(BUILD_GRAPHICAL_APPLICATION)
  file(GLOB_RECURSE RENDERER_SOURCES "source/renderer/*.cpp" "source/renderer/*.hpp" "dependencies/imgui/lib/*.cpp")
  add_library(renderer STATIC "${RENDERER_SOURCES}")
  target_include_directories(renderer PUBLIC "${CMAKE_SOURCE_DIR}/source")
  set_target_properties(renderer PROPERTIES PREFIX "")
  target_link_libraries(renderer "${Vulkan_LIBRARY}" "common")
  target_compile_definitions(renderer PUBLIC "LINK_AGAINST_RENDERER")
endif()

if(WIN32)
  link_directories("${CMAKE_SOURCE_DIR}/dependencies/glfw/lib")
  target_link_libraries(renderer "user32.lib" "gdi32.lib" "xinput.lib" "ws2_32.lib" "winmm.lib" "msvcrt.lib" "glfw3.lib")
  target_include_directories(renderer PUBLIC "${CMAKE_SOURCE_DIR}/dependencies/glfw/include")
else (WIN32)
  target_link_libraries(renderer "glfw")
endif()

if(BUILD_GRAPHICAL_APPLICATION)
  add_executable(vkPhysics_client "${GAME_SOURCES}")
  target_include_directories(vkPhysics_client PUBLIC "${CMAKE_SOURCE_DIR}/source")
  target_link_libraries(vkPhysics_client "renderer" "common")
  target_compile_definitions(renderer PUBLIC "LINK_AGAINST_RENDERER")
endif()

add_executable(vkPhysics_server "${GAME_SOURCES}")
target_include_directories(vkPhysics_server PUBLIC "${CMAKE_SOURCE_DIR}/source")
target_link_libraries(vkPhysics_server PUBLIC "common")
