# N64 Minimal Menu Example

https://github.com/user-attachments/assets/21dbfc01-8fab-4bb7-8beb-f1846de273df

![A pixelated game title screen featuring the "Libdragon" logo, which consists of a stylized red dragon curling around the white text "Libdragon." Below the logo, a menu set against a low-poly, orange-to-green gradient background displays three options in a white, pixelated font: "Start Game," "Options," and "Credits." The "Start Game" option is highlighted by small arrow cursors on either side.](image.png)

An N64 program that showcases how one can write a simple main menu complete with a background, logo, music, and SFX.

Pressing "Start Game" does nothing as you are meant to write code to respond to that option below in [menudemo.c](./src/menudemo.c).

```c
if (menuIndex == 0) { // Start game was pressed
    // Do your stuff here
    ;
}
```

## How to Build N64 Minimal Menu Example
This tutorial assumes you have your N64 Toolchain set up including GCC for MIPS64.

Run make to build this project:

```bash
libdragon make
```

---

## Licenses

Everything in the src folder is licensed under The Unlicense.

In the assets folder the following assets are in the public domain:
- [bap.wav - Aftersol](./assets/bap.wav)
- [bop.wav - Aftersol](./assets/bop.wav)
- [logo.ci4.png - Spooky Илюха and Cedar Branch](https://github.com/DragonMinded/libdragon/wiki/Logos)
- [miafan2010 - You Would Be Here](https://modarchive.org/index.php?request=view_by_moduleid&query=172936)
- [madameberry - background.png](https://opengameart.org/content/public-domain-backgrounds)
