#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "dragonfs.h"
#include "dfsinternal.h"

/* Directory walking flags */
enum
{
    WALK_CHDIR,
    WALK_OPEN
};

/* Directory walking return flags */
enum
{
    TYPE_ANY,
    TYPE_FILE,
    TYPE_DIR
};

/* Internal filesystem stuff */
static void *base_ptr = 0;
static open_file_t open_files[MAX_OPEN_FILES];
static uint32_t directories[MAX_DIRECTORY_DEPTH];
static uint32_t directory_top = 0;
static directory_entry_t *next_entry = 0;

/* Handling DMA from ROM to RAM */
static inline void grab_sector(void *cart_loc, void *ram_loc)
{
    memcpy( ram_loc, cart_loc, SECTOR_SIZE );
}

/* File lookup*/
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

/* Sector offset code for file reading */
static inline int sector_from_loc(uint32_t loc)
{
    return (loc / SECTOR_PAYLOAD);
}

static inline int offset_into_sector(uint32_t loc)
{
    return loc % SECTOR_PAYLOAD;
}

static inline int data_left_in_sector(uint32_t loc)
{
    return SECTOR_PAYLOAD - offset_into_sector(loc);
}

/* Easier decoding of directory information */
static inline uint32_t get_flags(directory_entry_t *dirent)
{
    return (dirent->flags >> 24) & 0x000000FF;
}

static inline uint32_t get_size(directory_entry_t *dirent)
{
    return dirent->flags & 0x00FFFFFF;
}

/* Functions for easier traversal of directories */
static inline directory_entry_t *get_first_entry(directory_entry_t *dirent)
{
    return (directory_entry_t *)(dirent->file_pointer ? (dirent->file_pointer + base_ptr) : 0);
}

static inline directory_entry_t *get_next_entry(directory_entry_t *dirent)
{
    return (directory_entry_t *)(dirent->next_entry ? (dirent->next_entry + base_ptr) : 0);
}

/* Functions for easier traversal of fiels */
static inline file_entry_t *get_first_sector(directory_entry_t *dirent)
{
    return (file_entry_t *)(dirent->file_pointer ? (dirent->file_pointer + base_ptr) : 0);
}

static inline file_entry_t *get_next_sector(file_entry_t *fileent)
{
    return (file_entry_t *)(fileent->next_sector ? (fileent->next_sector + base_ptr) : 0);
}

/* Sector walking */
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

/* For directory stack */
static inline void clear_directory()
{
    directory_top = 0;
}

static inline void push_directory(directory_entry_t *dirent)
{
    if(directory_top < MAX_DIRECTORY_DEPTH)
    {
        /* Order of execution for assignment undefined in C, lets force it */
        directories[directory_top] = (uint32_t)dirent;

        directory_top++;
    }
}

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

static inline directory_entry_t *peek_directory()
{
    if(directory_top > 0)
    {
        return (directory_entry_t *)directories[directory_top-1];
    }

    return (directory_entry_t *)(base_ptr + SECTOR_SIZE);
}

/* Parse out the next token in a path delimited by '\' */
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

/* Find a directory node in the current path given a name */
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

/* Walk a path string, either changing directories or finding the right path

   If mode is WALK_CHDIR, the result of this function is entering into the new
   directory on success, or the old directory being returned on failure.

   If mode is WALK_OPEN, the result of this function is the directory remains
   unchanged and a pointer to the directory entry for the requested file or
   directory is returned.  If it is a file, the directory entry for the file
   itself is returned.  If it is a directory, the directory entry of the first
   file or directory inside that directory is returned. 
 
   The type specifier allows a person to specify that only a directory or file
   should be returned.  This works for WALK_OPEN only. */
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

        if( (token[0] == '/' || token[0] == '.') )
        {
	    if(token[1] == '.')
		pop_directory();/* Up one directory */

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

/* Initialize the filesystem.  */
int dfs_init_pc(void *base_fs_loc, int tries)
{
    /* Check to see if it passes the check */
    while(tries != 0)
    {
        directory_entry_t id_node;
        grab_sector(base_fs_loc, &id_node);

        if(id_node.flags == FLAGS_ID && id_node.next_entry == NEXTENTRY_ID)
        {
            /* Passes, set up the FS */
            base_ptr = base_fs_loc;
            clear_directory();

            memset(open_files, 0, sizeof(open_files));

            /* Good FS */
            return DFS_ESUCCESS;
        }

        tries--;
    }

    /* Failed! */
    return DFS_EBADFS;
}

/* Change directories to the specified path.  Supports absolute and relative */
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

/* Find the first file or directory in a directory listing.  Supports absolute
   and relative.  If the path is invalid, returns a negative DFS_errno.  If
   a file or directory is found, returns the flags of the entry and copies the
   name into buf. */
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

/* Find the next file or directory in a directory listing.  Should be called
   after doing a dfs_dir_findfirst. */
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

/* Check if we have any free file handles, and if we do, try
   to open the file specified.  Supports absolute and relative
   paths */
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

/* Close an already open file handle.  Basically just frees up the
   file structure. */
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
    open_file_t *file = find_open_file(handle);

    if(!file)
    {
        return DFS_EBADHANDLE;
    }

    return file->loc;
}

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

void pr_depth( int depth )
{
    for( int i = 0; i < depth; i++ )
    {
        printf( " " );
    }
}

void list_dir( char *directory, int depth )
{
    char path[512];

    int dir = dfs_dir_findfirst( directory, path );

    do
    {
        pr_depth( depth );
        printf( "%s\n", path );

        if( FILETYPE( dir ) == FLAGS_DIR )
        {
            list_dir( path, depth + 2 );
        }
    } while( (dir = dfs_dir_findnext( path )) != FLAGS_EOF );
}

int main( int argc, char *argv[] )
{
    if( argc < 3 )
    {
        return -1;
    }

    if( argv[1][0] != '-' )
    {
        return -1;
    }

    switch( argv[1][1] )
    {
        case 'l':
        case 'L':
        {
            /* List files in DFS */
            FILE *fp = fopen( argv[2], "rb" );

            fseek( fp, 0, SEEK_END );
            int lSize = ftell( fp );
            fseek( fp, 0, SEEK_SET );
            
            void *filesystem = malloc( lSize );
            fread( filesystem, 1, lSize, fp );
            fclose( fp );

            dfs_init_pc( filesystem, 1 );

            list_dir( "/", 0 );

            free( filesystem );
            break;
        }
        case 'e':
        case 'E':
        {
            /* Extract file */
            FILE *fp = fopen( argv[2], "rb" );

            fseek( fp, 0, SEEK_END );
            int lSize = ftell( fp );
            fseek( fp, 0, SEEK_SET );
            
            void *filesystem = malloc( lSize );
            fread( filesystem, 1, lSize, fp );
            fclose( fp );

            dfs_init_pc( filesystem, 1 );
            
            int fl = dfs_open( argv[3] );
            uint8_t *data = malloc( dfs_size( fl ) );

            dfs_read( data, 1, dfs_size( fl ), fl );
            fwrite( data, 1, dfs_size( fl ), stdout );
            dfs_close( fl );

            free( filesystem );
            free( data );

            break;
        }
        case 's':
        case 'S':
        {
            /* Extract file with another file open (test multiple files) */
            FILE *fp = fopen( argv[2], "rb" );

            fseek( fp, 0, SEEK_END );
            int lSize = ftell( fp );
            fseek( fp, 0, SEEK_SET );
            
            void *filesystem = malloc( lSize );
            fread( filesystem, 1, lSize, fp );
            fclose( fp );

            dfs_init_pc( filesystem, 1 );

            int nu = dfs_open( argv[4] );
            uint32_t unused;
            dfs_read( &unused, 1, 4, nu );
            
            int fl = dfs_open( argv[3] );
            uint8_t *data = malloc( dfs_size( fl ) );

            dfs_read( data, 1, dfs_size( fl ), fl );
            fwrite( data, 1, dfs_size( fl ), stdout );
            dfs_close( fl );
            dfs_close( nu );

            free( filesystem );
            free( data );

            break;
        }
    }

    return 0;
}
