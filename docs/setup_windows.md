## Step 1: Prerequisites

Make sure to install the following, if they have not been already:

- [Visual Studio](https://visualstudio.microsoft.com/downloads/)
- [CMake](https://cmake.org/download/)
- [Vulkan SDK](https://vulkan.lunarg.com/sdk/home#windows)

## Step 2: Setting up the project

- Download the Git repo.
- Start CMake and enter the `Where is the source code` with the directory of the Git repo, where there is a CMakeLists.txt file

![photo](/docs/assets/files.png)

- Enter the `Where to build the binaries` with that same directory, but with an extension: `build`, as you see in the picture below

![photo](/docs/assets/cmake0.png)

- Press the `Configure button`
- When asked to create the new directory, press ok
- A window should pop up with the following options:

![photo](/docs/assets/cmake1.png)

- In `Specify the generator for this project`, select the version of Visual Studio that you have downloaded (in my case, it's 2019)
- In `Optional platform for generator`, select x64
- Tick `Use default native compilers`
- Press finish and wait for configuring to finish
- Press the `Generate` button next to `Configure` and wait for the generating to finish
- Now click on `Open Project`
- Visual Studio will open and you should see a `Solution Explorer` where you can navigate around the project - if you don't see this, press `Ctrl-Alt-L` to pop it open

![photo](/docs/assets/vs0.png)

- Right click on `vkPhysics_client` and click on `Set up as StartUp Project`

## Step 3: Build and run

- Now build with `Ctrl-Shift-B`
- Run with `Ctrl-F5`
