# CS 6983 Coding Assignment 2

This project renders a low-poly robot arm model in OpenGL using a Blinn–Phong lighting shader. The model includes a pedestal, arm links, joints, wrist, and two-finger gripper.

## Files

- `src/main.cpp` — Loads the robot model, sets up the scene and camera, and runs the OpenGL render loop.
- `src/gl_loader.hpp` — Loads OpenGL functions used by the application.
- `shaders/blinn_phong_vert.glsl` — Transforms model vertices and passes position, normal, and material data to the fragment shader.
- `shaders/blinn_phong_frag.glsl` — Calculates the final pixel colors using Blinn–Phong lighting.
- `assets/robot_arm.obj` — Robot arm mesh used by the application.
- `assets/robot_arm.mtl` — Material colors for the OBJ model.
- `assets/robot_arm.blend` — Editable Blender scene for the robot arm.
- `assets/build_robot_arm_obj.py` — Generates the OBJ and MTL model files.
- `assets/create_robot_arm_blend.py` — Creates the robot arm Blender scene.
- `CMakeLists.txt` — Configures the C++ project and its GLFW and GLM dependencies.

## Build and run

Requirements: CMake 3.24 or newer, a C++17 compiler, and Git. CMake downloads GLFW and GLM during the first configure.

From the project directory, run:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/cs6983_opengl_intro
```

On Windows, build and run with:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
build\Release\cs6983_opengl_intro.exe
```

Drag with the left mouse button or use the arrow keys to orbit the camera. Use the scroll wheel or `-`/`=` to zoom. Press Escape to quit.

To recreate the OBJ and MTL files, run:

```sh
python3 assets/build_robot_arm_obj.py
```

To recreate the Blender scene, open Blender's Scripting workspace, load `assets/create_robot_arm_blend.py`, and run it.

## AI Usage

An AI assistant can be used while completing this assignment to explain code, suggest fixes, or help debug problems. It can assist with these files:

- `src/main.cpp` — understanding the OBJ loader, camera controls, and render loop.
- `shaders/blinn_phong_vert.glsl` and `shaders/blinn_phong_frag.glsl` — explaining shader stages and lighting calculations.
- `assets/build_robot_arm_obj.py` and `assets/create_robot_arm_blend.py` — understanding how the model files and Blender scene are generated.
- `CMakeLists.txt` — explaining how to configure and build the project.

Review any AI suggestions, make sure you understand changes before using them, and follow your course's rules for AI assistance.

## Reflection answers

### 1. What part took the most effort, and how did you work through it?

One challenging part was getting the robot arm model to appear correctly in the OpenGL program. The program needs to read the OBJ model and its material file. So, if the model doesn’t appear as expected, checking file paths and model data can help find the problem. And working through this can show how a model file connects to the code that draws it.

### 2. What was your biggest learning outcome?

I learned that a 3D object is built from points, surfaces, and normals. Normals describe which way a surface faces, which helps the program calculate lighting. Next was material colors and lights affect how the object looks on screen. I also got to know that shaders handle different parts of drawing an object.

### 3. What do the vertex shader and fragment shader each do?

The vertex shader works with the model’s points, called vertices. It transforms those points so they can be placed in the camera’s view and passes information, such as surface direction and material color, to the next stage. And the fragment shader uses that information and lighting to calculate the color of each pixel.

### 4. How does this rendering/modeling work connect to Physical AI?

According to me, a robot model gives a computer a visual representation of the robot’s body. A rendered view can show how the robot may look from a camera’s viewpoint. Simulations can help researchers study robot designs and test ideas before using hardware. This assignment focuses on modeling and rendering, which are useful parts of building a robot simulation.
