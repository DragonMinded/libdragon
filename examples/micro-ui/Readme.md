# MicroUI N64

This example contains the immediate-mode UI (micro-UI) as an N64 port.<br>
It's mainly intended for quick and easy debugging and editor UIs, similar to imgui.<br>
The main focus lies on being able to create UI elements on the fly from everywhere in your code,<br>
with the ability to change variables by passing them as pointers.<br>
For more complex interactive game UIs, a different library should be used.<br>

Interaction with the UI is handled with a mouse-cursor.<br>
This cursor can either be a real N64-mouse, or simulated mouse via the control-stick.

The library itself is contained in `lib/microui.c`, whereas the N64 specific wrapper is in `lib/microuiN64.c`.

## Links
Original repo: https://github.com/rxi/microui