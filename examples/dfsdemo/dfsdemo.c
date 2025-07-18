#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <libdragon.h>

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
    /* Initialize video */
    console_init();

    /* Initialize key detection */
    joypad_init();

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

            joypad_poll();
            joypad_buttons_t keys = joypad_get_buttons_pressed(JOYPAD_PORT_1);

            if(keys.d_up)
            {
                cursor--;
                new_scroll_pos(&cursor, &page, MAX_LIST, count);
            }

            if(keys.d_down)
            {
                cursor++;
                new_scroll_pos(&cursor, &page, MAX_LIST, count);
            }

            if(keys.c_right && list[cursor].type == DT_REG)
            {
                /* Concatenate to make file */
                char path[512];

                strcpy( path, dir );
                strcat( path, list[cursor].filename );

                FILE* f;
                f = fopen(path, "r");
                if (f == NULL) {
                    printf("Failed to open %s\n", path);
                } else {
                    printf("Hold A to scroll\n");
                    char buf[1024];
                    size_t nread;
                    while ((nread = fread(buf, sizeof(buf[0]), sizeof(buf) - 1, f)) != 0) {
                        buf[nread] = '\0';
                        char* s = buf;
                        while (s != NULL) {
                            char* s_next_line = strchr(s, '\n');
                            if (s_next_line == NULL) {
                                printf("%s", s);
                                s = NULL;
                            } else {
                                printf("%.*s\n", s_next_line - s, s);
                                console_render();
                                s = s_next_line + 1;

                                wait_ms(100);
                                joypad_poll();
                                while (!joypad_get_buttons(JOYPAD_PORT_1).a) {
                                    wait_ms(10);
                                    joypad_poll();
                                }
                            }
                        }
                    }
                    if (ferror(f)) {
                        printf("Error while reading %s\n", path);
                    }

                    fclose(f);
                }

                printf("Press B to quit\n");
                console_render();
                joypad_poll();
                while (!joypad_get_buttons(JOYPAD_PORT_1).b) {
                    wait_ms(10);
                    joypad_poll();
                }

                continue;
            }

            if(keys.l)
            {
                /* Open the SD card */
                strcpy( dir, "sd://" );

                /* Populate new directory */
                free_dir(list);
                list = populate_dir(&count);

                page = 0;
                cursor = 0;
            }

            if(keys.r)
            {
                /* Open the ROM FS card */
                strcpy( dir, "rom://" );

                /* Populate new directory */
                free_dir(list);
                list = populate_dir(&count);

                page = 0;
                cursor = 0;
            }

            if(keys.a && list[cursor].type == DT_DIR)
            {
                /* Change directories */
                chdir(list[cursor].filename);
       
                /* Populate new directory */
                free_dir(list);
                list = populate_dir(&count);

                page = 0;
                cursor = 0;
            }

            if(keys.b)
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

