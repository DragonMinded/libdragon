/**
 * @file dragonfs.c
 * @brief DragonFS
 * @ingroup dfs
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <errno.h>
#include "libdragon.h"
#include "system.h"
#include "dfsinternal.h"
#include "rompak_internal.h"
#include "utils.h"

/**
 * @brief Directory walking flags 
 */
enum
{
    /** @brief Walk the directory structure for the purpose of changing directories */
    WALK_CHDIR,
    /** @brief Walk the directory structure for the purpose of opening a file or directory */
    WALK_OPEN
};

/**
 * @brief Directory walking return flags 
 */
enum
{
    /** @brief Return any file or directory found */
    TYPE_ANY,
    /** @brief Return a file from directory walk */
    TYPE_FILE,
    /** @brief Return a directory from a directory walk */
    TYPE_DIR
};

/** @brief Base filesystem pointer */
static uint32_t base_ptr = 0;
/** @brief Base pointer for lookup data */
static dfs_file_lookup_t *file_lookup;
/** @brief Base ROM address for path data */
static uint32_t path_rom_ofs;
/** @brief Directory pointer stack */
static uint32_t directories[MAX_DIRECTORY_DEPTH];
/** @brief Depth into directory pointer stack */
static uint32_t directory_top = 0;
/** @brief Pointer to next directory entry set when doing a directory walk */
static directory_entry_t *next_entry = 0;
/** @brief Convert an open file pointer to a handle */
#define OPENFILE_TO_HANDLE(file)        ((int)PhysicalAddr(file))
/** @brief Convert a handle to an open file pointer */
#define HANDLE_TO_OPENFILE(handle)      ((dfs_open_file_t*)((uint32_t)(handle) | 0x80000000))

/**
 * @brief Read a sector from cartspace
 *
 * This function handles fetching a sector from cartspace into RDRAM using
 * DMA.
 *
 * @param[in]  cart_loc
 *             Pointer to cartridge location
 * @param[out] ram_loc
 *             Pointer to RAM buffer to place the read sector
 */
static inline void grab_sector(void *cart_loc, void *ram_loc)
{
    /* Make sure we have fresh cache */
    data_cache_hit_writeback_invalidate(ram_loc, SECTOR_SIZE);

    dma_read((void *)(((uint32_t)ram_loc) & 0x1FFFFFFF), (uint32_t)cart_loc, SECTOR_SIZE);
}

/**
 * @brief Look up a sector number based on offset
 *
 * Given a byte offset from the start of a filesystem, this
 * function will return the sector that this byte offset falls
 * into.
 * 
 * @param[in] loc
 *            Offset in bytes
 *
 * @return The sector number corresponding to the offset
 */
static inline int sector_from_loc(uint32_t loc)
{
    return (loc / SECTOR_PAYLOAD);
}

/**
 * @brief Look up a byte offset into a sector
 *
 * Given a byte offset from the start of a filesystem, this
 * function will return the offset into the current sector.
 * This essentially clamps the ouput from 0 to #SECTOR_PAYLOAD.
 *
 * @param[in] loc
 *            Offset in bytes
 *
 * @return The offset into a sector
 */
static inline int offset_into_sector(uint32_t loc)
{
    return loc % SECTOR_PAYLOAD;
}

/**
 * @brief Look up the remaining data size in a sector
 *
 * Given a byte offset from the start of a filesystem, this
 * function will return the number of bytes from the current
 * location to the end of the sector.
 *
 * @param[in] loc
 *            Offset in bytes
 *
 * @return The number of bytes left in a sector based on an offset
 */
static inline int data_left_in_sector(uint32_t loc)
{
    return SECTOR_PAYLOAD - offset_into_sector(loc);
}

/**
 * @brief Return the file flags given a directory entry
 *
 * @param[in] dirent
 *            Directory entry to retrieve flags from
 *
 * @return The flags portion of a directory entry
 */
static inline uint32_t get_flags(directory_entry_t *dirent)
{
    return (dirent->flags >> 28) & 0x0000000F;
}

/**
 * @brief Return the file size given a directory entry
 *
 * @param[in] dirent
 *            Directory entry to retrieve size from
 *
 * @return The size of the file represented by the directory entry
 */
static inline uint32_t get_size(directory_entry_t *dirent)
{
    return dirent->flags & 0x0FFFFFFF;
}

/**
 * @brief Get the directory pointer from a directory entry
 *
 * This function is used to grab the first directory entry of a subdirectory given
 * the current directory pointer.
 *
 * @param[in] dirent
 *            Directory entry to retrieve directory pointer from
 *
 * @return A pointer to the directory represented by the directory entry.
 */
static inline directory_entry_t *get_first_entry(directory_entry_t *dirent)
{
    return (directory_entry_t *)(dirent->file_pointer ? (dirent->file_pointer + base_ptr) : 0);
}

/**
 * @brief Get the next directory entry
 *
 * @param[in] dirent
 *            Directory entry to retrieve next entry from
 *
 * @return A pointer to the next directory entry after the current directory entry.
 */
static inline directory_entry_t *get_next_entry(directory_entry_t *dirent)
{
    return (directory_entry_t *)(dirent->next_entry ? (dirent->next_entry + base_ptr) : 0);
}

/**
 * @brief Get the file starting location from a directory entry
 *
 * This function is used to grab the starting location of a file given the current
 * directory pointer.
 *
 * @param[in] dirent
 *            Directory entry to retrieve file pointer from
 *
 * @return A location of the start of the file.
 */
static inline uint32_t get_start_location(directory_entry_t *dirent)
{
    return (dirent->file_pointer ? (dirent->file_pointer + base_ptr) : 0);
}

/**
 * @brief Reset the directory stack to the root
 */
static inline void clear_directory()
{
    directory_top = 0;
}

/**
 * @brief Push a directory onto the stack
 *
 * @param[in] dirent
 *            Directory entry to push onto the stack
 */
static inline void push_directory(directory_entry_t *dirent)
{
    if(directory_top < MAX_DIRECTORY_DEPTH)
    {
        /* Order of execution for assignment undefined in C, lets force it */
        directories[directory_top] = (uint32_t)dirent;

        directory_top++;
    }
}

/**
 * @brief Pop a directory from the stack
 *
 * @return The directory entry on the top of the stack
 */
static inline directory_entry_t *pop_directory()
{
    if(directory_top > 0)
    {
        /* Order of execution for assignment undefined in C */
        directory_top--;

        return (directory_entry_t *)directories[directory_top];
    }

    /* Just return the root pointer */
    return (directory_entry_t *)(base_ptr + SECTOR_SIZE);
}

/**
 * @brief Peek at the top directory on the stack
 *
 * @return The directory entry on the top of the stack
 */
static inline directory_entry_t *peek_directory()
{
    if(directory_top > 0)
    {
        return (directory_entry_t *)directories[directory_top-1];
    }

    return (directory_entry_t *)(base_ptr + SECTOR_SIZE);
}

/**
 * @brief Parse out the next token in a path delimited by '\\'
 *
 * @param[in]  path
 *             Current path to extract next token from
 * @param[out] token
 *             String buffer to place the extracted token
 *
 * @return The rest of path that was not parsed.
 */
static char *get_next_token(char *path, char *token)
{
    if(!path)
    {
        /* Can't do shit */
        return 0;
    }

    if(path[0] != 0) 
    {
        /* We have at least one character in our owth */
        if(path[0] == '/')
        {
            /* Root of path indicator */
            if(token)
            {
                /* Just return the root indicator */
                token[0] = '/';
                token[1] = 0;
            }

            if(*(path + 1) == 0)
            {
                /* Don't return anything if we are at the end */
                return 0;
            }

            /* Don't iterate over this same thing next time */
            return path + 1;
        }
        else
        {
            /* Keep track of copied token so far to not buffer overflow */
            int copied = 0;

            while(path[0] != '/')
            {
                if(path[0] == 0)
                {
                    /* Hit the end */
                    if(token)
                    {
                        /* Cap off the end */
                        token[0] = 0;
                    }
                    
                    /* Nothing comes after this string */
                    return 0;
                }

                /* Only copy over if we need to */
                if(token && copied < MAX_FILENAME_LEN)
                {
                    token[0] = path[0];
                    token++;
                    copied++;
                }

                /* Next entry */
                path++;
            }

            if(token)
            {
                /* Cap off the end */
                token[0] = 0;
            }

            /* Only way we can be here is if the current character is '\' */
            if(*(path + 1) == 0)
            {
                return 0;
            }

            return path + 1;
        }
    }
    else
    {
        if(token)
        {
            /* No token */
            token[0] = 0;
        }

        /* Can't return a shorter path, there was none! */
        return 0;
    }
}

/**
 * @brief Find a directory node in the current path given a name
 *
 * @param[in] name
 *            Name of the file or directory in question
 * @param[in] cur_node
 *            Directory entry to start search from
 *
 * @return The directory entry matching the name requested or NULL if not found.
 */
static directory_entry_t *find_dirent(char *name, directory_entry_t *cur_node)
{
    while(cur_node)
    {
        /* Fetch sector off of 'disk' */
        directory_entry_t node;
        grab_sector(cur_node, &node);

        /* Do a string comparison on the filename */
        if(strcmp(node.path, name) == 0)
        {
            /* We have a match! */
            return cur_node;
        }

        /* Follow linked list */
        cur_node = get_next_entry(&node);
    }

    /* Couldn't find entry */
    return 0;
}

/**
 * @brief Walk a path string, either changing directories or finding the right path
 *
 * If mode is WALK_CHDIR, the result of this function is entering into the new
 * directory on success, or the old directory being returned on failure.
 *
 * If mode is WALK_OPEN, the result of this function is the directory remains
 * unchanged and a pointer to the directory entry for the requested file or
 * directory is returned.  If it is a file, the directory entry for the file
 * itself is returned.  If it is a directory, the directory entry of the first
 * file or directory inside that directory is returned. 
 *
 * The type specifier allows a person to specify that only a directory or file
 * should be returned.  This works for WALK_OPEN only.
 * 
 * @param[in]     path
 *                The path to walk through
 * @param[in]     mode
 *                Either #WALK_CHDIR or #WALK_OPEN.
 * @param[in,out] dirent
 *                Directory entry to start at, directory entry finished at
 * @param[in]     type
 *                Either #TYPE_ANY, #TYPE_FILE, or #TYPE_DIR.
 *
 * @return DFS_ESUCCESS on successful recurse, or a negative error on failure.
 */
static int recurse_path(const char * const path, int mode, directory_entry_t **dirent, int type)
{
    int ret = DFS_ESUCCESS;
    char token[MAX_FILENAME_LEN+1];
    char *cur_path = (char *)path;
    uint32_t dir_stack[MAX_DIRECTORY_DEPTH];
    uint32_t dir_loc = directory_top;
    int last_type = TYPE_ANY;
    int ignore = 1; // Do not, by default, read again during the first while

    /* Initialize token to avoid -Werror=maybe-uninitialized errors */
    token[0] = 0;

    /* Save directory stack */
    memcpy(dir_stack, directories, sizeof(uint32_t) * MAX_DIRECTORY_DEPTH);

    /* Grab first token, make sure it isn't root */
    cur_path = get_next_token(cur_path, token);

    if(strcmp(token, "/") == 0)
    {
        /* It is an absolute path */
        clear_directory();

        /* Ensure that we remember this as a directory */
        last_type = TYPE_DIR;

        /* We need to read through the first while loop */
        ignore = 0;
    }

    /* Loop through the rest */
    while(cur_path || ignore)
    {
        /* Grab out the next token */
        if(!ignore) { cur_path = get_next_token(cur_path, token); }
        ignore = 0;

        if(strcmp(token, "/") == 0 || 
           strcmp(token, ".") == 0)
        {
            /* Ignore, this is current directory */
            last_type = TYPE_DIR;
        }
        else if(strcmp(token, "..") == 0)
        {
            /* Up one directory */
            pop_directory();

            last_type = TYPE_DIR;
        }
        else
        {
            /* Find directory entry, push */
            directory_entry_t *tmp_node = find_dirent(token, peek_directory());

            if(tmp_node)
            {
                /* Grab node, make sure it is a directory, push subdirectory, try again! */
                directory_entry_t node;
                grab_sector(tmp_node, &node);

                uint32_t flags = get_flags(&node);

                if(FILETYPE(flags) == FLAGS_DIR)
                {
                    /* Push subdirectory onto stack and loop */
                    push_directory(get_first_entry(&node));
                    last_type = TYPE_DIR;
                }
                else
                {
                    if(mode == WALK_CHDIR)
                    {
                        /* Not found, this is a file */
                        ret = DFS_ENOFILE;
                        break;
                    }
                    else
                    {
                        last_type = TYPE_FILE;

                        /* Only count if this is the last thing we are doing */
                        if(!cur_path)
                        {
                            /* Push file entry onto stack in preparation of a return */
                            push_directory(tmp_node);
                        }
                        else
                        {
                            /* Not found, this is a file */
                            ret = DFS_ENOFILE;
                            break;
                        }
                    }
                }
            }
            else
            {
                /* Not found! */
                ret = DFS_ENOFILE;
                break;
            }
        }
    }

    if(type != TYPE_ANY && type != last_type)
    {
        /* Found an entry, but it was the wrong type! */
        ret = DFS_ENOFILE;
    }

    if(mode == WALK_OPEN)
    {
        /* Must return the node found if we found one */
        if(ret == DFS_ESUCCESS && dirent)
        {
            *dirent = peek_directory();
        }
    }

    if(mode == WALK_OPEN || ret != DFS_ESUCCESS)
    {
        /* Restore stack */
        directory_top = dir_loc;
        memcpy(directories, dir_stack, sizeof(uint32_t) * MAX_DIRECTORY_DEPTH);
    }

    return ret;
}


static void init_dfs_lookup(directory_entry_t *id_node)
{
    uint32_t romaddr = get_start_location(id_node);
    uint32_t size = id_node->next_entry;
    file_lookup = malloc(size);
    data_cache_hit_writeback_invalidate(file_lookup, size);
    dma_read(file_lookup, romaddr, size);
    if(file_lookup->path_ofs != 0) {
        path_rom_ofs = base_ptr+file_lookup->path_ofs;
    }
}

/**
 * @brief Helper functioner to initialize the filesystem
 *
 * @param[in] base_fs_loc
 *            Location of the filesystem
 *
 * @return DFS_ESUCCESS on successful initialization or a negative error on failure.
 */
static int __dfs_init(uint32_t base_fs_loc)
{
    /* Check to see if it passes the check */
    directory_entry_t id_node;
    grab_sector((void *)base_fs_loc, &id_node);

    if(id_node.flags == ROOT_FLAGS && !strcmp(id_node.path, ROOT_PATH))
    {
        /* Passes, set up the FS */
        base_ptr = base_fs_loc;
        clear_directory();
        if(id_node.file_pointer == 0) {
            return DFS_EBADFS;
        }
        init_dfs_lookup(&id_node);
        /* Good FS */
        return DFS_ESUCCESS;
    }

    /* Failed! */
    return DFS_EBADFS;
}

static int __dfs_chdir(const char * const path)
{
    /* Reset directory listing */
    next_entry = 0;

    if(!path)
    {
        /* Bad input! */
        return DFS_EBADINPUT;
    }

    return recurse_path(path, WALK_CHDIR, 0, TYPE_ANY);
}

/**
 * @brief Change directories to the specified path.  
 *
 * Supports absolute and relative 
 *
 * @param[in] path
 *            Relative or absolute path to change directories to
 * 
 * @return DFS_ESUCCESS on success or a negative value on error.
 */
int dfs_chdir(const char * const path)
{
    return __dfs_chdir(path);
}

static int __dfs_findfirst(const char * const path, char *buf, directory_entry_t **next_entry)
{
    directory_entry_t *dirent;
    int ret = recurse_path(path, WALK_OPEN, &dirent, TYPE_DIR);

    /* Ensure that if this fails, they can't call findnext */
    *next_entry = 0;

    if(ret != DFS_ESUCCESS)
    {
        /* File not found, or other error */
        return ret;
    }

    /* We now have the pointer to the first entry */
    directory_entry_t t_node;
    grab_sector(dirent, &t_node);

    if(buf)
    {
        strcpy(buf, t_node.path);    
    }
    
    /* Set up directory to point to next entry */
    *next_entry = get_next_entry(&t_node);

    return get_flags(&t_node);
}

static int __dfs_findnext(char *buf, directory_entry_t **next_entry)
{
    if(!*next_entry)
    {
        /* No file found */
        return FLAGS_EOF;
    }

    /* We already calculated the pointer, just grab the information */
    directory_entry_t t_node;
    grab_sector(*next_entry, &t_node);

    if(buf)
    {
        strcpy(buf, t_node.path);    
    }
    
    /* Set up directory to point to next entry */
    *next_entry = get_next_entry(&t_node);

    return get_flags(&t_node);
}

/**
 * @brief Find the first file or directory in a directory listing.
 *
 * Supports absolute and relative.  If the path is invalid, returns a negative DFS_errno.  If
 * a file or directory is found, returns the flags of the entry and copies the name into buf.
 *
 * @param[in]  path
 *             The path to look for files in
 * @param[out] buf
 *             Buffer to place the name of the file or directory found
 *
 * @return The flags (#FLAGS_FILE, #FLAGS_DIR, #FLAGS_EOF) or a negative value on error.
 */
int dfs_dir_findfirst(const char * const path, char *buf)
{
    return __dfs_findfirst(path, buf, &next_entry);
}

/**
 * @brief Find the next file or directory in a directory listing. 
 *
 * @note Should be called after doing a #dfs_dir_findfirst.
 *
 * @param[out] buf
 *             Buffer to place the name of the next file or directory found
 *
 * @return The flags (#FLAGS_FILE, #FLAGS_DIR, #FLAGS_EOF) or a negative value on error.
 */
int dfs_dir_findnext(char *buf)
{
    return __dfs_findnext(buf, &next_entry);
}

static uint32_t prime_hash(const char *str, uint32_t prime)
{
    uint32_t hash = 0;
    while(*str) {
        char c = *str++;
        hash = (hash * prime) + c;
    }
    return hash;
}

static int32_t lookup_file_hash_entry(uint32_t hash)
{
    int32_t left = 0;
    int32_t right = file_lookup->num_files-1;
    int32_t result = -1;
    while(left <= right) {
        int32_t mid = left+((right-left)/2);
        if(file_lookup->files[mid].path_hash == hash) {
            result = mid;
            right = mid-1;
        } else if(file_lookup->files[mid].path_hash < hash) {
            left = mid+1;
        } else {
            right = mid-1;
        }
    }
    return result;
}

static dfs_file_entry_t *lookup_file_path_entry(const char *const path, int32_t start_index, uint32_t hash)
{
    size_t buf_len = ROUND_UP(file_lookup->path_buf_size, 16);
    char alignas(16) path_buf[buf_len];
    uint32_t src_path_len = strlen(path)+1;
    if(src_path_len % 2 != 0) {
        src_path_len++;
    }
    memset(path_buf, 0, buf_len);
    for(int32_t i=start_index; file_lookup->files[i].path_hash == hash; i++) {
        uint32_t path_ofs = file_lookup->files[i].path_ofs;
        uint32_t path_len = path_ofs >> 20;
        if(src_path_len != path_len) {
            continue;
        }
        path_ofs &= (1 << 20)-1;
        data_cache_hit_writeback_invalidate(path_buf, path_len);
        dma_read(path_buf, path_rom_ofs+path_ofs, path_len);
        if(strcmp(path, path_buf) == 0) {
            return &file_lookup->files[i];
        }
    }
    return NULL;
}

static dfs_file_entry_t *lookup_file_entry(const char *const path)
{
    uint32_t hash = prime_hash(path, DFS_LOOKUP_PRIME);
    int32_t index = lookup_file_hash_entry(hash);
    if(index == -1) {
        return NULL;
    }
    return lookup_file_path_entry(path, index, hash);
}

int dfs_open(const char *path)
{
    dfs_open_file_t *file;
    //Skip initial slash
    if(path[0] == '/') {
        path++;
    }
    dfs_file_entry_t *entry = lookup_file_entry(path);
    if(!entry)
    {
        //File not found
        return DFS_ENOFILE;
    }
    /* Try to find a free slot */
    file = malloc(sizeof(dfs_open_file_t));
    if(!file)
    {
        return DFS_ENOMEM;
    }
    //Set file data
    file->size = entry->data_len;
    file->loc = 0;
    file->cart_start_loc = base_ptr+entry->data_ofs;
    return OPENFILE_TO_HANDLE(file);
}

int dfs_close(uint32_t handle)
{
    dfs_open_file_t *file = HANDLE_TO_OPENFILE(handle);

    if(!file)
    {
        return DFS_EBADHANDLE;
    }

    /* Free the open file */
    free(file);

    return DFS_ESUCCESS;
}

int dfs_seek(uint32_t handle, int offset, int origin)
{
    dfs_open_file_t *file = HANDLE_TO_OPENFILE(handle);

    if(!file)
    {
        return DFS_EBADHANDLE;
    }

    switch(origin)
    {
        case SEEK_SET:
            /* From the beginning */
            if(offset < 0)
            { 
                file->loc = 0; 
            }
            else
            {
                file->loc = offset;
            }

            break;
        case SEEK_CUR:
        {
            /* From the current position */
            int new_offset = (int)file->loc + offset;

            if(new_offset < 0)
            {
                new_offset = 0;
            }

            file->loc = new_offset;

            break;
        }
        case SEEK_END:
        {
            /* From the end of the file */
            int new_offset = (int)file->size + offset;

            if(new_offset < 0)
            {
                new_offset = 0;
            }

            file->loc = new_offset;

            break;
        }
        default:
            /* Impossible */
            return DFS_EBADINPUT;
    }

    /* Lets get some bounds checking */
    if(file->loc > file->size)
    {
        file->loc = file->size;
    }

    return DFS_ESUCCESS;
}

int dfs_tell(uint32_t handle)
{
    /* The good thing is that the location is always in the file structure */
    dfs_open_file_t *file = HANDLE_TO_OPENFILE(handle);

    if(!file)
    {
        return DFS_EBADHANDLE;
    }

    return file->loc;
}

int dfs_read(void * const buf, int size, int count, uint32_t handle)
{
    /* This is where we do all the work */
    dfs_open_file_t *file = HANDLE_TO_OPENFILE(handle);

    if(!file)
    {
        return DFS_EBADHANDLE;
    }

    /* What are they doing? */
    if(!buf)
    {
        return DFS_EBADINPUT;
    }

    int to_read = size * count;

    /* Bounds check to make sure we don't read past the end */
    if(file->loc + to_read > file->size)
    {
        /* It will, lets shorten it */
        to_read = file->size - file->loc;
    }

    if (!to_read)
        return 0;

    /* Fast-path. If possibly, we want to DMA directly into the destination
     * buffer, without using any intermediate buffers. We can do that only if
     * the buffer and the ROM location have the same 2-byte phase.
     */
    if (LIKELY(!(((uint32_t)buf ^ (uint32_t)file->loc) & 1)))
    {
        /* Calculate ROM address. NOTE: do this before invalidation,
         * in case the file object is false-sharing the buffer. */
        uint32_t rom_address = file->cart_start_loc + file->loc;

        /* 16-byte alignment: we can simply invalidate the buffer. */
        if ((((uint32_t)buf | to_read) & 15) == 0)
            data_cache_hit_invalidate(buf, to_read);
        else
            data_cache_hit_writeback_invalidate(buf, to_read);

        dma_read(buf, rom_address, to_read);

        file->loc += to_read;
        return to_read;
    }

    /* It was not possible to perform a direct DMA read into the destination
     * buffer. Use an intermediate buffer on the stack to perform the read. */
    uint8_t *data = buf;
    const int CHUNK_SIZE = 512;

    /* Allocate the buffer, aligned to 16 bytes */
    uint8_t chunk_base[CHUNK_SIZE+16] __attribute__((aligned(16)));
    data_cache_hit_invalidate(chunk_base, CHUNK_SIZE+16);

    /* Get a pointer to it with the same 2-byte phase of the file current location.
     * This guarantees that we can actually DMA into it directly. */
    uint8_t *chunk = UncachedAddr(chunk_base);
    if (file->loc & 1)
        chunk++;

    while(to_read)
    {
        int n = MIN(to_read, CHUNK_SIZE);

        /* Read the data from ROM into the stack buffer, and then and
         * copy it to the destination buffer. */
        dma_read(chunk, file->cart_start_loc + file->loc, n);
        memcpy(data, chunk, n);

        file->loc += n;
        data += n;
        to_read -= n;
    }

    return (void*)data - buf;
}

int dfs_size(uint32_t handle)
{
    dfs_open_file_t *file = HANDLE_TO_OPENFILE(handle);

    if(!file)
    {
        /* Will still count as EOF */
        return DFS_EBADHANDLE;
    }

    return file->size;
}

uint32_t dfs_rom_addr(const char *path)
{
    //Skip initial slash
    if(path[0] == '/') {
        path++;
    }
    dfs_file_entry_t *entry = lookup_file_entry(path);
    
    if(!entry)
    {
        //File not found
        return 0;
    }
    return base_ptr+entry->data_ofs;
}

int dfs_eof(uint32_t handle)
{
    dfs_open_file_t *file = HANDLE_TO_OPENFILE(handle);

    if(!file)
    {
        /* Will still count as EOF */
        return DFS_EBADHANDLE;
    }

    if(file->loc == file->size)
    {
        /* Yup, eof */
        return 1;
    }

    /* Nope! */
    return 0;
}

/**
 * @brief Newlib-compatible open
 *
 * @param[in] name
 *            Absolute path of the file to open
 * @param[in] flags
 *            POSIX file flags
 *
 * @return A newlib-compatible file handle.
 */
static void *__open( char *name, int flags )
{
    /* Always want a consistent interface */
    __dfs_chdir("/");

    /* We disregard flags here */
    int handle = dfs_open( name );
    if (handle <= 0) {
        switch (handle) {
        case DFS_EBADINPUT:  errno = EINVAL; break;
        case DFS_ENOFILE:    errno = ENOENT; break;
        case DFS_EBADFS:     errno = ENODEV; break;
        case DFS_ENFILE:     errno = ENFILE; break;
        case DFS_EBADHANDLE: errno = EBADF;  break;
        default:             errno = EPERM;  break;
        }
        return NULL;
    }
    return (void *)handle;
}

/**
 * @brief Newlib-compatible fstat
 *
 * @param[in]  file
 *             File pointer as returned by #__open
 * @param[out] st
 *             Stat structure to populate
 *
 * @return 0.
 */
static int __fstat( void *file, struct stat *st )
{
    st->st_dev = 0;
    st->st_ino = 0;
    st->st_mode = S_IFREG;
    st->st_nlink = 1;
    st->st_uid = 0;
    st->st_gid = 0;
    st->st_rdev = 0;
    st->st_size = dfs_size( (uint32_t)file );
    st->st_atime = 0;
    st->st_mtime = 0;
    st->st_ctime = 0;
    st->st_blksize = 0;
    st->st_blocks = 0;
    //st->st_attr = S_IAREAD | S_IAREAD;

    return 0;
}

/**
 * @brief Newlib-compatible lseek
 *
 * @param[in] file
 *            File pointer as returned by #__open
 * @param[in] ptr
 *            Offset based on dir
 * @param[in] dir
 *            A direction to seek from.  Either #SEEK_SET, #SEEK_CUR or #SEEK_END
 *
 * @return The new position in the file after the seek.
 */
static int __lseek( void *file, int ptr, int dir )
{
    dfs_seek( (uint32_t)file, ptr, dir );

    return dfs_tell( (uint32_t)file );
}

/**
 * @brief Newlib-compatible read
 *
 * @param[in]  file
 *             File pointer as returned by #__open
 * @param[out] ptr
 *             Pointer to buffer to read to
 * @param[in]  len
 *             Length in bytes to read
 *
 * @return The actual amount of data read.
 */
static int __read( void *file, uint8_t *ptr, int len )
{
    return dfs_read( ptr, 1, len, (uint32_t)file );
}

/**
 * @brief Newlib-compatible close
 *
 * @param[in] file
 *            File pointer as returned by #__open
 *
 * @return 0 on success or a negative error otherwise.
 */
static int __close( void *file )
{
    return dfs_close( (uint32_t)file );
}

/**
 * @brief Newlib-compatible findfirst
 *
 * @param[in]  path
 *             Absolute path of the directory to walk
 * @param[out] dir
 *             Directory structure to populate with information on the first entry found
 *
 * @return 0 on success or a negative value on failure.
 */
static int __findfirst( char *path, dir_t *dir )
{
    /* Grab first entry, return if bad */
    int flags = __dfs_findfirst( path, dir->d_name, (struct directory_entry**)&dir->d_cookie );
    if( flags < 0 ) { return -1; }

    if( flags == FLAGS_FILE )
    {
        dir->d_type = DT_REG;
    }
    else if( flags == FLAGS_DIR )
    {
        dir->d_type = DT_DIR;
    }
    else
    {
        /* Unknown type */
        return -1;
    }

    /* Success */
    return 0;
}

/**
 * @brief Newlib-compatible findnext
 *
 * @param[out] dir
 *              Directory structure to populate with information on the next entry found
 *
 * @return 0 on success or a negative value on failure.
 */
static int __findnext( dir_t *dir )
{
    /* Grab first entry, return if bad */
    int flags = __dfs_findnext( dir->d_name, (struct directory_entry**)&dir->d_cookie );
    if( flags < 0 ) { return -1; }

    if( flags == FLAGS_FILE )
    {
        dir->d_type = DT_REG;
    }
    else if( flags == FLAGS_DIR )
    {
        dir->d_type = DT_DIR;
    }
    else
    {
        /* Unknown type */
        return -1;
    }

    /* Success */
    return 0;
}

/**
 * @brief Structure used for hooking DragonFS into newlib
 *
 * The following section of code is for bridging into newlib's filesystem hooks 
 * to allow posix access to DragonFS filesystem.
 */
static filesystem_t dragon_fs = {
    .open = __open,
    .fstat = __fstat,
    .lseek = __lseek,
    .read = __read,
    .close = __close,
    .findfirst = __findfirst,
    .findnext = __findnext,
};

/**
 * @brief Verify that the current emulator is compatible with DFS, and assert
 *        if it is not.
 */
static void __dfs_check_emulation(void)
{
    const uint32_t ROM_ADDR = 0x10000000;

    uint32_t data = io_read(ROM_ADDR);
    
    /* DFS requires working support for short DMA transfers. Verify that they
     * work and if they don't, assert out. */
    uint8_t buf[4] __attribute__((aligned(8))) = { 0xFF, 0xFF, 0xFF, 0xFF };
    data_cache_hit_writeback_invalidate(buf, 4);
    dma_read(buf, ROM_ADDR, 3);

    if (buf[0] == ((data >> 24) & 0xFF) &&
        buf[1] == ((data >> 16) & 0xFF) &&
        buf[2] == ((data >> 8) & 0xFF) &&
        buf[3] == 0xFF)
        return;

    assertf(0, "Your emulator is not accurate enough to run this ROM.\nSpecifically, it doesn't support accurate PI DMA");
}

int dfs_init(uint32_t base_fs_loc)
{
    /* Detect if we are running on emulator accurate enough to emulate DragonFS. */
    __dfs_check_emulation();

    if( base_fs_loc == DFS_DEFAULT_LOCATION )
    {
        /* Search for the DFS image location in the ROM */
        base_fs_loc = rompak_search_ext( ".dfs" );
        if( !base_fs_loc )
        {
            /* We could not find the DragonFS via rompak.
             * For backward compatibility, fallback to the address we used
             * to hardcode as default. */
            base_fs_loc = 0x10101000;
        }
        /* Convert the address to virtual (as expected for base_fs_loc). */
        base_fs_loc |= 0xA0000000;
    }

    /* Try opening the filesystem */
    int ret = __dfs_init( base_fs_loc );

    if( ret != DFS_ESUCCESS )
    {
        /* Failed, return so */
        return ret;
    }

    /* Succeeded, push our filesystem into newlib */
    attach_filesystem( "rom:/", &dragon_fs );

    return DFS_ESUCCESS;
}

const char *dfs_strerror(int error)
{
    switch (error) {
    case DFS_ESUCCESS:   return "Success";
    case DFS_EBADFS:     return "Bad filesystem";
    case DFS_ENOFILE:    return "File not found";
    case DFS_EBADINPUT:  return "Invalid argument";
    case DFS_ENFILE:     return "No free file handles";
    case DFS_EBADHANDLE: return "Bad file handle";
    default:             return "Unknown error";
    }
}
