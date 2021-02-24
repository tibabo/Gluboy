# Gluboy
Another gameboy emulator coded in C++ with Real-time rewind function, based on OpenAL, OpengGL and Dear-Imgui, tested with xbox 360 controller. On Windows MacOS and Linux ESP32
https://www.youtube.com/playlist?list=PLGmq0D2Nzi7aPrGjlnEikqj5I9ecjEhvL

## Build & Execution

To build it using cmake and Makefile use the following commands:

```shell
# Create a build folder
$ mkdir build

# Enter the build folder
$ cd build

# Generate a Makefile
$ cmake ..

# Build
$ make

# Finally execute the project
$ ./gluboy
``` 
You may need the follwing command on ubuntu
```shell
 sudo apt install libgtk-3-dev libopenal-dev libglfw3-dev
 
``` 
To build Emscripten Version
```shell
# Get the emsdk repo
git clone https://github.com/emscripten-core/emsdk.git

# Enter that directory
cd emsdk

# Fetch the latest version of the emsdk (not needed the first time you clone)
git pull

# Download and install the latest SDK tools.
./emsdk install latest

# Make the "latest" SDK "active" for the current user. (writes .emscripten file)
./emsdk activate latest

# Activate PATH and other environment variables in the current terminal
source ./emsdk_env.sh

# Go to Gluboy directory
cd ../Gluboy

# Create a build folder
$ mkdir build

# Enter the build folder
$ cd build

# Generate a Makefile
$ emcmake cmake .. 

# Build
$ emmake make 
``` 
