# PCG Camera Utility

## What is it?

This utility allows users to draw a rectangle on the screen (to draw the location of a webcam video feed), and then displays the offsets of the video from the screen edges (supports multiple monitors).

This program only supports Windows, and there are no plans to make it cross-platform (as of yet).

There may be plans in the future to allow the program to integrate directly with OBS Studio / Streamlabs OBS Desktop.

## Building from source

To build from source, the MSVC compiler is needed. This can be installed by installing [Visual Studio](https://visualstudio.microsoft.com/vs/community/), and adding the 'Desktop Development with C++' workload (shown to you during/after installation or on first launch). Once you have Visual Studio and the C++ workload, verify the path to `vcvarsall.bat` is correct inside `./tools/build.bat`.

To build the project, simply navigate to `./tools/` and open a Command Prompt there<sup>*1</sup>. Then, run the command:

> build -release

The program will be built to `./build/release/`.

<sup>*1:</sup> Ensure the current working directory is set to the tools directory. If it isn't, use the `cd` command to navigate to it within Command Prompt. Alternatively; navigate to the folder in Windows Explorer, click the address bar, type "cmd", and press Enter.
