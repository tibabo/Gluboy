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
