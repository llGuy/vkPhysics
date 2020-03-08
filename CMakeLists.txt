cmake_minimum_required(VERSION "3.2")

project(vkPhysics)

file(GLOB_RECURSE SOURCES "source/*.cpp" "source/*.hpp" "dependencies/imgui/lib/*.cpp")

include_directories("${CMAKE_SOURCE_DIR}/dependencies/stb")
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

  link_libraries("user32.lib" "gdi32.lib" "xinput.lib" "ws2_32.lib" "winmm.lib" "msvcrt.lib")

else(WIN32)

  link_libraries("glfw")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
  
endif(WIN32)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSTB_IMAGE_IMPLEMENTATION -D_MBCS -DCIMGUI_DEFINE_ENUMS_AND_STRUCTS")

set(KTX_DIR ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/ktx)

set(KTX_SOURCES
    ${KTX_DIR}/lib/texture.c
    ${KTX_DIR}/lib/hashlist.c
    ${KTX_DIR}/lib/checkheader.c
    ${KTX_DIR}/lib/swap.c
    ${KTX_DIR}/lib/memstream.c
    ${KTX_DIR}/lib/filestream.c
    )
  
set(KTX_INCLUDE
    ${KTX_DIR}/include
    ${KTX_DIR}/lib
    ${KTX_DIR}/other_include
)

add_library(ktx ${KTX_SOURCES})
target_include_directories(ktx PUBLIC ${KTX_INCLUDE})

add_executable(vkPhysics "${SOURCES}")

target_link_libraries(vkPhysics ktx)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/binaries)
