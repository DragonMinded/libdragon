# Magma Demo

This example is a showcase of libdragon's builtin high performance 3D graphics API, magma. 
It demonstrates how a relatively basic rendering engine could be built on top of magma's API using best practices to ensure reasonable performance.

This demo is quite complex and may not be the best introduction to magma's API. If you'd like to see a more simple example, check out the "mghello" example.
Also note that the specific approach shown in this example may not be suitable for every application or game. However, magma is a very flexible API and is meant to be suitable for practically any rendering scenario.

## Controls

| Input         | Action                                    |
| ------------- | ----------------------------------------- |
| Analog Stick  | Move camera                               |
| C-buttons     | Rotate camera                             |
| Z             | Hold to move the camera slowly            |
| D-pad up      | Increase number of objects                |
| D-pad down    | Decrease number of objects                |
| Start         | Toggle animations                         |
| L             | Toggle profiling overlay (if available)   |

## Profiling Overlay

The profiling overlay (toggled with L) shows internal profiling details about different ucodes and the RDP.
It was originally written by [Max Bebök aka HailToDodongo](https://github.com/HailToDodongo) and has been modified for this demo. The overlay takes a moment (up to 30 frames) to appear after it has been turned on.

Note that this feature is only available if libdragon has been compiled with RSP-side profiling enabled (otherwise it simply won't appear).
To enable it, go into `include/rspq_constants.h`, set `RSPQ_PROFILE` to 1 and rebuild both libdragon and this demo.