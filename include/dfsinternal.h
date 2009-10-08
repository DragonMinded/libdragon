#ifndef __LIBDRAGON_DFSINTERNAL_H
#define __LIBDRAGON_DFSINTERNAL_H

#define FLAGS_ID        0xFFFFFFFF
#define NEXTENTRY_ID    0xDEADBEEF

#define SECTOR_SIZE     256
#define SECTOR_PAYLOAD  252

struct directory_entry
{
    uint32_t flags;
    uint32_t next_entry;
    char path[MAX_FILENAME_LEN+1];
    uint32_t file_pointer;
} __attribute__((__packed__));

typedef struct directory_entry directory_entry_t;

struct file_entry
{
    uint32_t next_sector;
    uint8_t data[SECTOR_PAYLOAD];
} __attribute__((__packed__));

typedef struct file_entry file_entry_t;

typedef struct open_file
{
    file_entry_t cur_sector;
    file_entry_t *start_sector;
    uint32_t handle;
    uint32_t size;
    uint32_t loc;
    uint32_t sector_number;
    /* If this isn't here, the second file opened will fail due to cur_sector
     * not being on a 8 byte aligned boundary, so I just aligned it to 512
     * bytes. */
    uint8_t padding[236];
} open_file_t;

#endif
