# vkPhysics

![photo](/assets/screenshots/screenshot.png)

For now, an engine which will hopefully become a real-time multiplayer game!

# Build

## Windows

In the directory containing CMakeLists.txt:

- Create a directory called `build`

- Run CMake and specify the binaries directory to be this new build directory (also specify x64 and prompted)

- Build in the IDE you are using and it should be up and running!

## Linux

- In the directory containing CMakeLists.txt

- `mkdir build`, `cd build`, `cmake -DCMAKE_BUILD_TYPE=DEBUG ..`, `make`
