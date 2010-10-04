#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <libdragon.h>
#include <mikmod.h>

#define MAX_LIST            20

typedef struct
{
    uint32_t type;
    char filename[MAX_FILENAME_LEN+1];
} direntry_t;

int compare(const void * a, const void * b)
{
    direntry_t *first = (direntry_t *)a;
    direntry_t *second = (direntry_t *)b;

    if(first->type == FLAGS_DIR && second->type != FLAGS_DIR)
    {
        /* First should be first */
        return -1;
    }

    if(first->type != FLAGS_DIR && second->type == FLAGS_DIR)
    {
        /* First should be second */
        return 1;
    }

    return strcmp(first->filename, second->filename);
}

direntry_t *populate_dir(const char * const dir, int *count)
{
    char buf[MAX_FILENAME_LEN+1];

    /* Grab a slot */
    direntry_t *list = malloc(sizeof(direntry_t));
    *count = 1;

    /* Grab first */
    int flags = dfs_dir_findfirst(dir, buf);

    if(flags < 0 || flags == FLAGS_EOF)
    {
        /* Free stuff */
        free(list);

        /* Dir was bad! */
        return 0;
    }

    /* Copy in loop */
    while(flags == FLAGS_FILE || flags == FLAGS_DIR)
    {
        list[(*count)-1].type = flags;
        strcpy(list[(*count)-1].filename, buf);

        /* Grab next */
        flags = dfs_dir_findnext(buf);

        if(flags == FLAGS_DIR || flags == FLAGS_FILE)
        {
            (*count)++;
            list = realloc(list, sizeof(direntry_t) * (*count));
        }
    }

    if(*count > 0)
    {
        /* Should sort! */
        qsort(list, *count, sizeof(direntry_t), compare);
    }

    return list;
}

void free_dir(direntry_t *dir)
{
    if(dir) { free(dir); }
}

void new_scroll_pos(int *cursor, int *page, int max, int count)
{
    /* Make sure windows too small can be calculated right */
    if(max > count) { max = count; }

    /* Bounds checking */
    if(*cursor >= count)
    {
        *cursor = count-1;
    }

    if(*cursor < 0)
    {
        *cursor = 0;
    }

    /* Scrolled up? */
    if(*cursor < *page)
    {
        *page = *cursor;
        return;
    }

    /* Scrolled down/ */
    if(*cursor >= (*page + max))
    {
        *page = (*cursor - max) + 1;
        return;
    }

    /* Nothing here, should be good! */
}

void display_dir(direntry_t *list, int cursor, int page, int max, int count)
{
    /* Page bounds checking */
    if(max > count)
    {
        max = count;
    }

    /* Cursor bounds checking */
    if(cursor >= (page + max))
    {
        cursor = (page + max) - 1;
    }

    if(cursor < page)
    {
        cursor = page;
    }

    for(int i = page; i < (page + max); i++)
    {
        if(i == cursor)
        {
            console_printf("> ");
        }
        else
        {
            console_printf("  ");
        }

        if(list[i].type == FLAGS_DIR)
        {
            char tmpdir[(CONSOLE_WIDTH-5)+1];

            strncpy(tmpdir, list[i].filename, CONSOLE_WIDTH-5);
            tmpdir[CONSOLE_WIDTH-5] = 0;

            console_printf("[%s]\n", tmpdir);
        }
        else
        {
            char tmpdir[(CONSOLE_WIDTH-3)+1];

            strncpy(tmpdir, list[i].filename, CONSOLE_WIDTH-3);
            tmpdir[CONSOLE_WIDTH-3] = 0;

            console_printf("%s\n", tmpdir);
        }
    }
}

int main(void)
{
    /* enable MI interrupts (on the CPU) */
    set_MI_interrupt(1,1);

    /* Initialize audio and video */
    audio_init(44100,2);
    console_init();

    /* Initialize key detection */
    controller_init();

    MikMod_RegisterAllDrivers();
    MikMod_RegisterAllLoaders();

    md_mode = 0;
    md_mode |= DMODE_16BITS;
    md_mode |= DMODE_SOFT_MUSIC;
    md_mode |= DMODE_SOFT_SNDFX;
    //md_mode |= DMODE_STEREO;
                                            
    md_mixfreq = audio_get_frequency();

    MikMod_Init("");

    if(dfs_init(0xB0100000) != DFS_ESUCCESS)
    {
        console_printf("Filesystem failed to start!\n");
    }
    else
    {
        direntry_t *list;
        int count = 0;
        int page = 0;
        int cursor = 0; 

        console_set_render_mode(RENDER_MANUAL);
        console_clear();

        dfs_chdir("/");
        list = populate_dir(".", &count);

        while(1)
        {
            console_clear();
            display_dir(list, cursor, page, MAX_LIST, count);
            console_render();

            controller_scan();
            struct controller_data keys = get_keys_down();

            if(keys.c[0].up)
            {
                cursor--;
                new_scroll_pos(&cursor, &page, MAX_LIST, count);
            }

            if(keys.c[0].down)
            {
                cursor++;
                new_scroll_pos(&cursor, &page, MAX_LIST, count);
            }

            if(keys.c[0].C_right && list[cursor].type == FLAGS_FILE)
            {
                /* Module playing loop */
                MODULE *module = NULL;

                module = Player_Load(list[cursor].filename, 256, 0);
                
                /* Ensure that first part of module doesn't get cut off */
                audio_write_silence();
                audio_write_silence();

                if(module)
                {
                    char c = '-';
                    int sw = 0;

                    Player_Start(module);

                    while(1)
                    {
                        if(sw == 5)
                        {
                            console_clear();
                            display_dir(list, cursor, page, MAX_LIST, count);

                            sw = 0;
                            switch(c)
                            {
                                case '-':
                                    c = '\\';
                                    break;
                                case '\\':
                                    c = '|';
                                    break;
                                case '|':
                                    c = '/';
                                    break;
                                case '/':
                                    c = '-';
                                    break;
                            }
    
                            console_printf("\n\n\n%c Playing module", c);                        
                            console_render();
                        }
                        else
                        {
                            sw++;
                        }

                        MikMod_Update();

                        controller_scan();
                        struct controller_data keys = get_keys_down();

                        if(keys.c[0].C_left || !Player_Active())
                        {
                            /* End playback */
                            audio_write_silence();
                            audio_write_silence();
                            audio_write_silence();
                            audio_write_silence();

                            break;
                        }
                    }
                
                    Player_Stop();
                    Player_Free(module);
                }
            }

            if(keys.c[0].A && list[cursor].type == FLAGS_DIR)
            {
                /* Change directories */
                dfs_chdir(list[cursor].filename);
       
                /* Populate new directory */
                free_dir(list);
                list = populate_dir(".", &count);

                page = 0;
                cursor = 0;
            }

            if(keys.c[0].B)
            {
                /* Up! */
                dfs_chdir("..");
       
                /* Populate new directory */
                free_dir(list);
                list = populate_dir(".", &count);

                page = 0;
                cursor = 0;
            }
        }
    }

    while(1);

    return 0;
}

