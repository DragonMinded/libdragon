#include <libdragon.h>

//Typedef for symbol
typedef void (*print_func)(int time);

int main(void)
{
    //Initialize debugging output
    debug_init_emulog();
    debug_init_usblog();
    //Initialize joypad/controller
    joypad_init();
    //Initialize DFS
    dfs_init(DFS_DEFAULT_LOCATION);
    
    //Init timer
    int time = 0;
    
    //Initialize console
    console_init();
    console_set_render_mode(RENDER_MANUAL);
    
    //Start with overlay 1 loaded
    void *ovl_handle = dlopen("rom:/overlay1.dso", RTLD_LOCAL); //Load overlay 1
    print_func ovl_func = dlsym(ovl_handle, "overlay_print"); //Find overlay_print symbol
    while(1)
    {
        console_clear();
        //Print header
        printf("Overlay Demo\n");
        printf("ovl_handle=%p, ovl_func=%p\n", ovl_handle, ovl_func);
        printf("time=%d\n", time);
        printf("\n");
        //Print overlay specific data
        ovl_func(time);
        //Print footer
        printf("\nControls:\n");
        printf("A: Load Overlay 1\n");
        printf("B: Load Overlay 2\n");
        console_render();
        //Handle buttons
        joypad_poll();
        joypad_buttons_t keys = joypad_get_buttons_pressed(JOYPAD_PORT_1);
        if(keys.a) {
            //Switch to overlay 1
            dlclose(ovl_handle); //Close previous loaded overlay
            ovl_handle = dlopen("rom:/overlay1.dso", RTLD_LOCAL); //Load overlay 1
            ovl_func = dlsym(ovl_handle, "overlay_print"); //Find overlay_print symbol
            time = 0; //Reset timer
        }
        if(keys.b) {
            //Switch to overlay 2
            dlclose(ovl_handle); //Close previous loaded overlay
            ovl_handle = dlopen("rom:/overlay2.dso", RTLD_LOCAL); //Load overlay 2
            ovl_func = dlsym(ovl_handle, "overlay_print"); //Find overlay_print symbol
            time = 0; //Reset timer
        }
        //Increment timer
        time = (time+1)%1000;
    }
}
