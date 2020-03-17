cmake_minimum_required(VERSION "3.2")

project(vkPhysics)

file(GLOB_RECURSE SOURCES "source/*.cpp" "source/*.hpp" "dependencies/imgui/lib/*.cpp")

include_directories("${CMAKE_SOURCE_DIR}/dependencies/stb")
include_directories("${CMAKE_SOURCE_DIR}/dependencies/glm")
include_directories("${CMAKE_SOURCE_DIR}/dependencies/imgui/include")



find_package(Vulkan)



if(NOT Vulkan_FOUND)

  link_directories("${CMAKE_SOURCE_DIR}/dependencies/vulkan/lib")
  include_directories("${CMAKE_SOURCE_DIR}/dependencies/vulkan/include")

  message("Using bundled Vulkan version")

else(NOT Vulkan_FOUND)

  include_directories("${Vulkan_INCLUDE_DIRS}")
  link_libraries("${Vulkan_LIBRARY}")

  message("Using preinstalled Vulkan")

endif()

if(WIN32)

  include_directories("${CMAKE_SOURCE_DIR}/dependencies/glfw/include")
  link_directories("${CMAKE_SOURCE_DIR}/dependencies/glfw/lib")
  link_libraries("user32.lib" "gdi32.lib" "xinput.lib" "ws2_32.lib" "winmm.lib" "msvcrt.lib" "glfw3.lib")

else(WIN32)

  link_libraries("glfw")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
  
endif(WIN32)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSTB_IMAGE_IMPLEMENTATION -D_MBCS -DCIMGUI_DEFINE_ENUMS_AND_STRUCTS")

add_executable(vkPhysics "${SOURCES}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/binaries)
