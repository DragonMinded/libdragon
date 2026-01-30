# model64 Mesh Viewer

A minimal model64 viewer example for libdragon.

It demonstrates ROM/SD asset loading, model switching, and animation playback.

**Controls**

- L/R: switch model
- C-Left/C-Right: switch animation
- Z: toggle ROM/SD source

**SD usage**

Copy `filesystem/models/*.model64` to `sd:/models/`.
If SD is not mounted, assets load always from ROM (DFS).

Tested on my SummerCart64.

---

### How to extend this code

#### Export GLB Mesh without Texture

Use GLB when you want a quick, untextured model.
1) Export from Blender as `.glb`.
2) Drop it in `assets/` and add it to `models[]` in the top of `meshviewer.c`.

#### Export GLTF + BIN + PNG Texture

Use this when you need textures.
1) Export as glTF Separate: `.gltf` + `.bin` + `.png`.
2) Keep the PNG next to the `.gltf` (same folder).
3) Add the model to `models[]` in `meshviewer.c` and rebuild.
