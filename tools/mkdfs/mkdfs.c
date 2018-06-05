#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/param.h>
#include "dragonfs.h"
#include "dfsinternal.h"

#if BYTE_ORDER == BIG_ENDIAN
#define SWAPLONG(i) (i)
#else
#define SWAPLONG(i) (((uint32_t)(i & 0xFF000000) >> 24) | ((uint32_t)(i & 0x00FF0000) >>  8) | ((uint32_t)(i & 0x0000FF00) <<  8) | ((uint32_t)(i & 0x000000FF) << 24))
#endif

uint8_t *dfs = NULL;
uint32_t fs_size = 0;

/* Offset from start of filesystem */
inline uint32_t sector_offset(void *sector)
{
    uint32_t x = (uint8_t *)sector - dfs;

    return x;
}

inline void *sector_to_memory(uint32_t offset)
{
    return (void *)(dfs + offset);
}

/* Add a new sector to the filesystem, return that sector pointer */
uint32_t new_sector()
{
    void *end;

    if(!dfs)
    {
        dfs = malloc(SECTOR_SIZE);
        fs_size = SECTOR_SIZE;

        end = dfs;
    }
    else
    {
        dfs = realloc(dfs, fs_size + SECTOR_SIZE);

        end = dfs + fs_size;
        fs_size += SECTOR_SIZE;
    }

    /* Zero out last bytes */
    memset(end, 0, SECTOR_SIZE);

    return sector_offset(end);
}

void kill_fs()
{
    if(dfs)
    {
        free(dfs);
    }
}

void print_help(const char * const prog_name)
{
    fprintf(stderr, "Usage: %s <File> <Directory>\n", prog_name);
    fprintf(stderr, "  where <File> is the resulting filesystem image\n");
    fprintf(stderr, "  and <Directory> is the directory (including subdirectories) to include\n");
}

uint32_t add_file(const char * const file, uint32_t *size)
{
    uint32_t first_sector = 0;
    uint32_t cur_sector = 0;
    FILE *fp;

    printf("Adding '%s' to filesystem image.\n", file);

    fp = fopen(file, "r");

    if(!fp)
    {
        fprintf(stderr, "Cannot open file '%s' for read!\n", file);
        return 0;
    }

    /* Start off fresh */
    *size = 0;

    for(;;)
    {
        uint8_t t_buf[SECTOR_PAYLOAD];

        int num_read = fread(t_buf, 1, SECTOR_PAYLOAD, fp);

        if(num_read < 0)
        {
            /* Wat? */
            fprintf(stderr, "Cannot add all contents of file '%s' to filesystem!\n", file);
            fclose(fp);
            return 0;
        }

        if(num_read > 0)
        {
            file_entry_t *tmp_sector = 0;
            uint32_t new_node = new_sector();

            tmp_sector = sector_to_memory(new_node);
            tmp_sector->next_sector = 0; // Ensure that if this is the last one, we don't reference wrong
            memcpy(tmp_sector->data, t_buf, num_read);

            /* Remember how many bytes we read in */
            *size += num_read;

            if(cur_sector)
            {
                tmp_sector = sector_to_memory(cur_sector);
                tmp_sector->next_sector = SWAPLONG(new_node);
            }

            cur_sector = new_node;

            if(!first_sector)
            {
                /* Remember first sector in */
                first_sector = new_node;
            }
        }
        else
        {
            /* Done! */
            break;
        }
    }

    return first_sector;
}

uint32_t add_directory(const char * const path)
{
    directory_entry_t *tmp_entry;
    uint32_t first_entry = 0;
    uint32_t cur_entry = 0;
    DIR *dirp;
    struct dirent *dp;

    if((dirp = opendir(path)) == NULL)
    {
        return 0;
    }

    do {
        if((dp = readdir(dirp)) != NULL)
        {
            if(strcmp(dp->d_name, ".") == 0)
            {
                /* Ignore */
            }
            else if(strcmp(dp->d_name, "..") == 0)
            {
                /* Ignore */
            }
            else
            {
                char *file = malloc(strlen(path) + strlen(dp->d_name) + 2);
                struct stat stats;

                if(!file)
                {
                    /* Out of memory */
                    return 0;
                }

                strcpy(file, path);

                /* Only add a / if there isn't one */
                if(path[strlen(path) - 1] != '/')
                {
                    strcat(file, "/");
                }

                strcat(file, dp->d_name);

                /* Figure out if it is a directory or regular (windows doesn't include d_type in dirent) */
                stat( file, &stats );

                if(S_ISREG(stats.st_mode))
                {
                    uint32_t new_entry = new_sector();
                    uint32_t file_size = 0;

                    tmp_entry = sector_to_memory(new_entry);
                    tmp_entry->next_entry = 0;

                    /* Copy over filename */
                    strncpy(tmp_entry->path, dp->d_name, MAX_FILENAME_LEN);
                    tmp_entry->path[MAX_FILENAME_LEN] = 0;

                    uint32_t new_file = add_file(file, &file_size);

                    if(!new_file)
                    {
                        free(file);
                        return 0;
                    }

                    tmp_entry = sector_to_memory(new_entry);
                    tmp_entry->file_pointer = SWAPLONG(new_file);

                    tmp_entry->flags = SWAPLONG(((FLAGS_FILE << 24) | (file_size & 0x00FFFFFF)));

                    if(cur_entry)
                    {
                        /* Link up! */
                        tmp_entry = sector_to_memory(cur_entry);
                        tmp_entry->next_entry = SWAPLONG(new_entry);
                    }

                    /* This is now the current working entry */
                    cur_entry = new_entry;
                }
                else if(S_ISDIR(stats.st_mode))
                {
                    uint32_t new_entry = new_sector();

                    tmp_entry = sector_to_memory(new_entry);

                    tmp_entry->flags = SWAPLONG(FLAGS_DIR << 24); /* Size doesn't matter for directories */
                    tmp_entry->next_entry = 0;

                    /* Copy over filename */
                    strncpy(tmp_entry->path, dp->d_name, MAX_FILENAME_LEN);
                    tmp_entry->path[MAX_FILENAME_LEN] = 0;

                    uint32_t new_directory = add_directory(file);

                    if(!new_directory)
                    {
                        free(file);
                        return 0;
                    }

                    tmp_entry = sector_to_memory(new_entry);
                    tmp_entry->file_pointer = SWAPLONG(new_directory);

                    if(cur_entry)
                    {
                        /* Link up! */
                        tmp_entry = sector_to_memory(cur_entry);
                        tmp_entry->next_entry = SWAPLONG(new_entry);
                    }

                    /* This is now the current working entry */
                    cur_entry = new_entry;
                }

                free(file);

                if(!first_entry)
                {
                    /* Return pointer to first file on list */
                    first_entry = cur_entry;
                }
            }
        }
    } while (dp != NULL);

    closedir(dirp);

    /* Will return 0 if we don't find any entries (don't support directories without files) */
    return first_entry;
}

int main(int argc, char *argv[])
{
    if(argc != 3)
    {
        print_help(argv[0]);
        return -1;
    }

    /* Add in identifier */
    directory_entry_t *id = sector_to_memory(new_sector());

    if(!id)
    {
        fprintf(stderr, "Out of memory!\n");

        return -1;
    }

    id->flags = SWAPLONG(FLAGS_ID);
    id->next_entry = SWAPLONG(NEXTENTRY_ID);
    strcpy(id->path, "DragonFS 1.0");

    if(!add_directory(argv[2]))
    {
        /* Error adding directory */
        fprintf(stderr, "Error creating filesystem.\n");

        kill_fs();

        return -1;
    }

    /* Write out filesystem */
    FILE *fp = fopen(argv[1], "w");

    if(!fp)
    {
        /* Error writing file out */
        fprintf(stderr, "Error opening '%s' for writing.\n", argv[1]);

        kill_fs();
    }

    fwrite(dfs, 1, fs_size, fp);
    fclose(fp);

    kill_fs();

    return 0;
}
