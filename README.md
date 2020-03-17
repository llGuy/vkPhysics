# vkPhysics

![photo](/assets/screenshots/screenshot.png)

For now, an engine which will hopefully become a real-time multiplayer game!

# Build

## Windows

In the directory containing CMakeLists.txt:

- Create a directory called `build`

- Run CMake and specify the binaries directory to be this new build directory (also specify x64 and prompted)

- Build in the IDE you are using and it should be up and running!

- Repo comes with bundled vulkan lib / headers, BUT if the shaders have trouble being loaded, may need to install Vulkan SDK

## Linux

- Install Vulkan SDK: `sudo apt update`, `sudo apt install vulkan-sdk`

- In the directory containing CMakeLists.txt

- `mkdir build`, `cd build`, `cmake -DCMAKE_BUILD_TYPE=DEBUG ..`, `make`
