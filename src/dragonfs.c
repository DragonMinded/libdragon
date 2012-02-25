/**
 * @file dragonfs.c
 * @brief DragonFS
 * @ingroup dfs
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include "libdragon.h"
#include "system.h"
#include "dfsinternal.h"

/**
 * @defgroup dfs DragonFS
 * @brief DragonFS filesystem implementation and newlib hooks.
 *
 * DragonFS is a read only ROM filesystem for the N64.  It provides an interface
 * that homebrew developers can use to load resources from cartridge space that
 * were not available at compile time.  This can mean sprites or other game assets,
 * or the filesystem can be appended at a later time if the homebrew developer wishes
 * end users to be able to insert custom levels, music or other assets.  It is loosely
 * based off of FAT with consideration into application and limitations of the N64.
 *
 * The filesystem can be generated using 'mkdfs' which is included in the 'tools'
 * directory of libdragon.  Due to the read-only nature, DFS does not support empty
 * files or empty directories.  Attempting to create a filesystem with either of
 * these using 'mkdfs' will result in an error.  If a filesystem contains either empty
 * files or empty directories, the result of manipulating the filesystem is undefined.
 *
 * DragonFS does not support writing, renaming or symlinking of files.  It supports only
 * file and directory types.
 *
 * DFS files have a maximum size of 16,777,216 bytes.  Directories can have an unlimited
 * number of files in them.  Each token (separated by a / in the path) can be 243 characters
 * maximum.  Directories can be 100 levels deep at maximum.  There can be 4 files open
 * simultaneously.
 *
 * When DFS is initialized, it will register itself with newlib using 'rom:/' as a prefix.
 * Files can be accessed either with standard POSIX functions and the 'rom:/' prefix or
 * with DFS API calls and no prefix.  Files can be opened using both sets of API calls
 * simultaneously as long as no more than four files are open at any one time.
 * @{
 */

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
/** @brief Open file tracking */
static open_file_t open_files[MAX_OPEN_FILES];
/** @brief Directory pointer stack */
static uint32_t directories[MAX_DIRECTORY_DEPTH];
/** @brief Depth into directory pointer stack */
static uint32_t directory_top = 0;
/** @brief Pointer to next directory entry set when doing a directory walk */
static directory_entry_t *next_entry = 0;

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
    
    /* Fresh cache again */
    data_cache_hit_invalidate(ram_loc, SECTOR_SIZE);
}

/**
 * @brief Find a free open file structure
 *
 * @return A pointer to an open file structure or NULL if no more open file structures.
 */
static open_file_t *find_free_file()
{
    for(int i = 0; i < MAX_OPEN_FILES; i++)
    {
        if(!open_files[i].handle)
        {
            /* Found one! */
            return &open_files[i];
        }
    }

    /* No free files */
    return 0;
}

/**
 * @brief Find an open file structure based on a handle
 *
 * @param[in] x
 *            The file handle given to the open file
 *
 * @return A pointer to an open file structure or NULL if no file matches the handle
 */
static open_file_t *find_open_file(uint32_t x)
{
    if(x == 0) { return 0; }

    for(int i = 0; i < MAX_OPEN_FILES; i++)
    {
        if(open_files[i].handle == x)
        {
            /* Found it! */
            return &open_files[i];
        }
    }

    /* Couldn't find handle */
    return 0;
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
    return (dirent->flags >> 24) & 0x000000FF;
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
    return dirent->flags & 0x00FFFFFF;
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
 * @brief Get the file pointer from a directory entry
 *
 * This function is used to grab the first sector of a file given the current
 * directory pointer.
 *
 * @param[in] dirent
 *            Directory entry to retrieve file pointer from
 *
 * @return A pointer to the first sector of a file.
 */
static inline file_entry_t *get_first_sector(directory_entry_t *dirent)
{
    return (file_entry_t *)(dirent->file_pointer ? (dirent->file_pointer + base_ptr) : 0);
}

/**
 * @brief Get the next file sector given a current file sector
 *
 * @param[in] fileent
 *            File entry structure to retrieve next sector from
 *
 * @return A pointer to the next sector of a file.
 */
static inline file_entry_t *get_next_sector(file_entry_t *fileent)
{
    return (file_entry_t *)(fileent->next_sector ? (fileent->next_sector + base_ptr) : 0);
}

/**
 * @brief Walk forward in a file a specified number of sectors
 *
 * @param[in] file
 *            Open file structure
 * @param[in] num_sectors
 *            Number of sectors to advance the file
 */
static void walk_sectors(open_file_t *file, uint32_t num_sectors)
{
    /* Update the sector number */
    file->sector_number += num_sectors;

    /* Walk forward num_sectors */
    while(num_sectors)
    {
        file_entry_t *next_sector = get_next_sector(&file->cur_sector);
        grab_sector(next_sector, &file->cur_sector);

        num_sectors--;
    }
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

    if(id_node.flags == FLAGS_ID && id_node.next_entry == NEXTENTRY_ID)
    {
        /* Passes, set up the FS */
        base_ptr = base_fs_loc;
        clear_directory();

        memset(open_files, 0, sizeof(open_files));

        /* Good FS */
        return DFS_ESUCCESS;
    }

    /* Failed! */
    return DFS_EBADFS;
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
    directory_entry_t *dirent;
    int ret = recurse_path(path, WALK_OPEN, &dirent, TYPE_DIR);

    /* Ensure that if this fails, they can't call findnext */
    next_entry = 0;

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
    next_entry = get_next_entry(&t_node);

    return get_flags(&t_node);
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
    if(!next_entry)
    {
        /* No file found */
        return FLAGS_EOF;
    }

    /* We already calculated the pointer, just grab the information */
    directory_entry_t t_node;
    grab_sector(next_entry, &t_node);

    if(buf)
    {
        strcpy(buf, t_node.path);    
    }
    
    /* Set up directory to point to next entry */
    next_entry = get_next_entry(&t_node);

    return get_flags(&t_node);
}

/**
 * @brief Open a file given a path
 *
 * Check if we have any free file handles, and if we do, try
 * to open the file specified.  Supports absolute and relative
 * paths
 *
 * @param[in] path
 *            Path of the file to open
 *
 * @return A valid file handle to reference the file by or a negative error on failure.
 */
int dfs_open(const char * const path)
{
    /* Ensure we always open with a unique handle */
    static uint32_t next_handle = 1;

    /* Try to find a free slot */
    open_file_t *file = find_free_file();

    if(!file)
    {
        return DFS_ENOMEM;        
    }

    /* Try to find file */
    directory_entry_t *dirent;
    int ret = recurse_path(path, WALK_OPEN, &dirent, TYPE_FILE);

    if(ret != DFS_ESUCCESS)
    {
        /* File not found, or other error */
        return ret;
    }

    /* We now have the pointer to the file entry */
    directory_entry_t t_node;
    grab_sector(dirent, &t_node);

    /* Set up file handle */
    file->handle = next_handle++;
    file->size = get_size(&t_node);
    file->loc = 0;
    file->sector_number = 0;
    file->start_sector = get_first_sector(&t_node);
    grab_sector(file->start_sector, &file->cur_sector);

    return file->handle;
}

/**
 * @brief Close an already open file handle.
 *
 * @param[in] handle
 *            A valid file handle as returned from #dfs_open.
 *
 * @return DFS_ESUCCESS on success or a negative value on error.
 */
int dfs_close(uint32_t handle)
{
    open_file_t *file = find_open_file(handle);

    if(!file)
    {
        return DFS_EBADHANDLE;
    }

    /* Closing the handle is easy as zeroing out the file */
    memset(file, 0, sizeof(open_file_t));

    return DFS_ESUCCESS;
}

/**
 * @brief Seek to an offset in the file
 *
 * @param[in] handle
 *            A valid file handle as returned from #dfs_open.
 * @param[in] offset
 *            A byte offset from the origin to seek from.
 * @param[in] origin
 *            An offset to seek from.  Either #SEEK_SET, #SEEK_CUR or #SEEK_END.
 *  
 * @return DFS_ESUCCESS on success or a negative value on error.
 */
int dfs_seek(uint32_t handle, int offset, int origin)
{
    open_file_t *file = find_open_file(handle);

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

/**
 * @brief Return the current offset into a file
 *
 * @param[in] handle
 *            A valid file handle as returned from #dfs_open.
 *
 * @return The current byte offset into a file or a negative error on failure.
 */
int dfs_tell(uint32_t handle)
{
    /* The good thing is that the location is always in the file structure */
    open_file_t *file = find_open_file(handle);

    if(!file)
    {
        return DFS_EBADHANDLE;
    }

    return file->loc;
}

/**
 * @brief Read data from a file
 *
 * @param[out] buf
 *             Buffer to read into
 * @param[in]  size
 *             Size of each element to read
 * @param[in]  count
 *             Number of elements to read
 * @param[in]  handle
 *             A valid file handle as returned from #dfs_open.
 *
 * @return The actual number of bytes read or a negative value on failure.
 */
int dfs_read(void * const buf, int size, int count, uint32_t handle)
{
    /* This is where we do all the work */
    open_file_t *file = find_open_file(handle);

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
    int did_read = 0;

    /* Bounds check to make sure we don't read past the end */
    if(file->loc + to_read > file->size)
    {
        /* It will, lets shorten it */
        to_read = file->size - file->loc;
    }

    /* Soemthing we can actually incriment! */
    uint8_t *data = buf;

    /* Loop in, reading data in the cached sector */
    while(to_read)
    {
        /* Do we need to seek? */
        uint32_t t_sector = sector_from_loc(file->loc);
        
        if(t_sector != file->sector_number)
        {
            /* Must seek to new sector */
            if(t_sector > file->sector_number)
            {
                /* Easy, just walk forward */
                walk_sectors(file, t_sector - file->sector_number);
            }
            else
            {
                /* Start over, walk all the way */
                grab_sector(file->start_sector, &file->cur_sector);
                file->sector_number = 0;

                walk_sectors(file, t_sector);
            }
        }

        /* Only read as much as we currently have */
        int read_this_loop = to_read;
        if(read_this_loop > data_left_in_sector(file->loc))
        {
            read_this_loop = data_left_in_sector(file->loc);
        }

        /* Copy in */
        memcpy(data, file->cur_sector.data + offset_into_sector(file->loc), read_this_loop);
        data += read_this_loop;
        did_read += read_this_loop;
        file->loc += read_this_loop;

        to_read -= read_this_loop;
    }

    /* Return the count */
    return did_read;
}

/**
 * @brief Return the file size of an open file
 *
 * @param[in] handle
 *            A valid file handle as returned from #dfs_open.
 *
 * @return The file size in bytes or a negative value on failure.
 */
int dfs_size(uint32_t handle)
{
    open_file_t *file = find_open_file(handle);

    if(!file)
    {
        /* Will still count as EOF */
        return DFS_EBADHANDLE;
    }

    return file->size;
}

/**
 * @brief Return whether the end of file has been reached
 *
 * @param[in] handle
 *            A valid file handle as returned from #dfs_open.
 *
 * @return 1 if the end of file is reached, 0 if not, and a negative value on error.
 */
int dfs_eof(uint32_t handle)
{
    open_file_t *file = find_open_file(handle);

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
    dfs_chdir("/");

    /* We disregard flags here */
    return (void *)dfs_open( name );
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
    if( !path || !dir ) { return -1; }

    /* Grab first entry, return if bad */
    int flags = dfs_dir_findfirst( path, dir->d_name );
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
    if( !dir ) { return -1; }

    /* Grab first entry, return if bad */
    int flags = dfs_dir_findnext( dir->d_name );
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
    __open,
    __fstat,
    __lseek,
    __read,
    0,
    __close,
    0,
    __findfirst,
    __findnext
};

/**
 * @brief Initialize the filesystem.
 *
 * Given a base offset where the filesystem should be found, this function will
 * initialize the filesystem to read from cartridge space.  This function will
 * also register DragonFS with newlib so that standard POSIX file operations
 * work with DragonFS.
 *
 * @param[in] base_fs_loc
 *            Memory mapped location at which to find the filesystem.  This is normally
 *            0xB0000000 + the offset used when building your ROM + the size of the header
 *            file used.
 *
 * @return DFS_ESUCCESS on success or a negative error otherwise.
 */
int dfs_init(uint32_t base_fs_loc)
{
    /* Try normal (works on doctor v64) */
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

/** @} */
