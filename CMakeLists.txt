cmake_minimum_required(VERSION "3.2")

project(vkPhysics)

set (CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSTB_IMAGE_IMPLEMENTATION -D_MBCS -DCIMGUI_DEFINE_ENUMS_AND_STRUCTS")

if(NOT WIN32)
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
endif()

# Create common library
file(GLOB_RECURSE COMMON_SOURCES "source/common/*.cpp" "source/common/*.hpp")
add_library(common STATIC "${COMMON_SOURCES}")
set_target_properties(common PROPERTIES PREFIX "")
target_include_directories(common PUBLIC "${CMAKE_SOURCE_DIR}/depdendencies/glm")
target_compile_definitions(common PUBLIC PROJECT_ROOT="${CMAKE_SOURCE_DIR}")

# Create renderer library
include_directories("${CMAKE_SOURCE_DIR}/dependencies/stb")
include_directories("${CMAKE_SOURCE_DIR}/dependencies/glm")
include_directories("${CMAKE_SOURCE_DIR}/dependencies/imgui/include")

find_package(Vulkan)

# Only build renderer target if Vulkan was found
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

if(BUILD_GRAPHICAL_APPLICATION)
  file(GLOB_RECURSE RENDERER_SOURCES "source/renderer/*.cpp" "source/renderer/*.hpp" "dependencies/imgui/lib/*.cpp")
  add_library(renderer STATIC "${RENDERER_SOURCES}")
  target_include_directories(renderer PUBLIC "${CMAKE_SOURCE_DIR}/source" "${CMAKE_SOURCE_DIR}/dependencies/stb" "${CMAKE_SOURCE_DIR}/imgui/include")
  set_target_properties(renderer PROPERTIES PREFIX "")
  target_link_libraries(renderer "${Vulkan_LIBRARY}" "common")
  target_compile_definitions(renderer PUBLIC "LINK_AGAINST_RENDERER")

  if(WIN32)
    # For Win32, GLFW dependency is provided, for Linux, it is not
    link_directories("${CMAKE_SOURCE_DIR}/dependencies/glfw/lib")
    target_link_libraries(renderer PUBLIC "user32.lib" "gdi32.lib" "xinput.lib" "ws2_32.lib" "winmm.lib" "msvcrt.lib" "glfw3.lib" "msvcrtd.lib" "libcmtd.lib" "ucrtd.lib")
    target_include_directories(renderer PUBLIC "${CMAKE_SOURCE_DIR}/dependencies/glfw/include")
  else (WIN32)
    target_link_libraries(renderer "glfw")
  endif()
endif()

# Create client application (with graphical interface)
file(GLOB_RECURSE GAME_SOURCES "source/game/*.cpp" "source/game/*.hpp")

if(BUILD_GRAPHICAL_APPLICATION)
  add_executable(vkPhysics_client "${GAME_SOURCES}")
  target_include_directories(vkPhysics_client PUBLIC "${CMAKE_SOURCE_DIR}/source")
  target_link_libraries(vkPhysics_client PUBLIC "renderer" "common")
  target_compile_definitions(vkPhysics_client PUBLIC "LINK_AGAINST_RENDERER")
  target_compile_definitions(renderer PUBLIC "LINK_AGAINST_RENDERER")
endif()

# Create server application (without graphical interface)
add_executable(vkPhysics_server "${GAME_SOURCES}")
target_include_directories(vkPhysics_server PUBLIC "${CMAKE_SOURCE_DIR}/source")
target_link_libraries(vkPhysics_server PUBLIC "common")
