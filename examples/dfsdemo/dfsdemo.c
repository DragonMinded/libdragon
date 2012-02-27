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

char dir[512] = "rom://";

void chdir( const char * const dirent )
{
    /* Ghetto implementation */
    if( strcmp( dirent, ".." ) == 0 )
    {
        /* Go up one */
        int len = strlen( dir ) - 1;
        
        /* Stop going past the min */
        if( dir[len] == '/' && dir[len-1] == '/' && dir[len-2] == ':' )
        {
            return;
        }

        if( dir[len] == '/' )
        {
            dir[len] = 0;
            len--;
        }

        while( dir[len] != '/')
        {
            dir[len] = 0;
            len--;
        }
    }
    else
    {
        /* Add to end */
        strcat( dir, dirent );
        strcat( dir, "/" );
    }
}

int compare(const void * a, const void * b)
{
    direntry_t *first = (direntry_t *)a;
    direntry_t *second = (direntry_t *)b;

    if(first->type == DT_DIR && second->type != DT_DIR)
    {
        /* First should be first */
        return -1;
    }

    if(first->type != DT_DIR && second->type == DT_DIR)
    {
        /* First should be second */
        return 1;
    }

    return strcmp(first->filename, second->filename);
}

direntry_t *populate_dir(int *count)
{
    /* Grab a slot */
    direntry_t *list = malloc(sizeof(direntry_t));
    *count = 1;

    /* Grab first */
    dir_t buf;
    int ret = dir_findfirst(dir, &buf);

    if( ret != 0 ) 
    {
        /* Free stuff */
        free(list);
        *count = 0;

        /* Dir was bad! */
        return 0;
    }

    /* Copy in loop */
    while( ret == 0 )
    {
        list[(*count)-1].type = buf.d_type;
        strcpy(list[(*count)-1].filename, buf.d_name);

        /* Grab next */
        ret = dir_findnext(dir,&buf);

        if( ret == 0 )
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

    if( max == 0 )
    {
        printf( "No files in this dir..." );
    }

    for(int i = page; i < (page + max); i++)
    {
        if(i == cursor)
        {
            printf("> ");
        }
        else
        {
            printf("  ");
        }

        if(list[i].type == DT_DIR)
        {
            char tmpdir[(CONSOLE_WIDTH-5)+1];

            strncpy(tmpdir, list[i].filename, CONSOLE_WIDTH-5);
            tmpdir[CONSOLE_WIDTH-5] = 0;

            printf("[%s]\n", tmpdir);
        }
        else
        {
            char tmpdir[(CONSOLE_WIDTH-3)+1];

            strncpy(tmpdir, list[i].filename, CONSOLE_WIDTH-3);
            tmpdir[CONSOLE_WIDTH-3] = 0;

            printf("%s\n", tmpdir);
        }
    }
}

int main(void)
{
    /* enable interrupts (on the CPU) */
    init_interrupts();

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

    if(dfs_init( DFS_DEFAULT_LOCATION ) != DFS_ESUCCESS)
    {
        printf("Filesystem failed to start!\n");
    }
    else
    {
        direntry_t *list;
        int count = 0;
        int page = 0;
        int cursor = 0; 

        console_set_render_mode(RENDER_MANUAL);
        console_clear();

        list = populate_dir(&count);

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

            if(keys.c[0].C_right && list[cursor].type == DT_REG)
            {
                /* Module playing loop */
                MODULE *module = NULL;

                /* Concatenate to make file */
                char path[512];

                strcpy( path, dir );
                strcat( path, list[cursor].filename );

                module = Player_Load(path, 256, 0);
                
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
    
                            printf("\n\n\n%c Playing module", c);                        
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

            if(keys.c[0].L)
            {
                /* Open the SD card */
                strcpy( dir, "sd://" );

                /* Populate new directory */
                free_dir(list);
                list = populate_dir(&count);

                page = 0;
                cursor = 0;
            }

            if(keys.c[0].R)
            {
                /* Open the ROM FS card */
                strcpy( dir, "rom://" );

                /* Populate new directory */
                free_dir(list);
                list = populate_dir(&count);

                page = 0;
                cursor = 0;
            }

            if(keys.c[0].A && list[cursor].type == DT_DIR)
            {
                /* Change directories */
                chdir(list[cursor].filename);
       
                /* Populate new directory */
                free_dir(list);
                list = populate_dir(&count);

                page = 0;
                cursor = 0;
            }

            if(keys.c[0].B)
            {
                /* Up! */
                chdir("..");
       
                /* Populate new directory */
                free_dir(list);
                list = populate_dir(&count);

                page = 0;
                cursor = 0;
            }
        }
    }

    while(1);

    return 0;
}

