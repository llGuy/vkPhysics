## Step 1: Prerequisites

Make sure to install the following, if they have not been already:

- CMake - to install: `sudo apt-get install cmake`
- GLFW - to install: `sudo apt-get install libglfw3` and `sudo apt-get install libglfw3-dev`
- Vulkan SDK - to install, follow this [link](https://packages.lunarg.com/) and click on the version you want

## Step 2: Setting up the project

- Clone the repo
- Go to `tools/scripts` : `cd tools/scripts`
- Run setup_linux.sh : `sh setup_linux.sh`

## Step 3: Build and run
- `setup_linux.sh` will setup the project *and* build, if you with to just build in the future, you could run build_linux.sh : `sh build_linux.sh`
- To run the client application, in the tools/scripts directory, run `run_client_linux.sh` : `sh run_client_linux.sh`
