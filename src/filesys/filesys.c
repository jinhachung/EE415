#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "threads/malloc.h"
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) 
{
  block_sector_t inode_sector = 0;
  char *base, target[NAME_MAX + 1];
  struct dir *dir = NULL;
  bool is_dir, success;

  base = (char *)calloc (1, strlen (name) + 1);

  is_dir = false;
  success = dir_parse (name, base, target);
  if (success)
    dir = dir_open_dir (base);
  free (base);

  success = (dir != NULL
             && free_map_allocate (1, &inode_sector)
             && inode_create (inode_sector, initial_size, is_dir)
             && dir_add (dir, target, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  struct inode *inode = NULL;

  inode = filesys_open_path (name);

  if (inode == NULL)
    return NULL;

  if (inode_is_dir (inode))
    return NULL;

  return file_open (inode);
}

struct dir *
filesys_open_dir (const char *name)
{
  struct inode *inode = NULL;
  
  inode = filesys_open_path (name);

  if (inode_is_dir (inode))
    return dir_open (inode);
  
  return NULL;
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  char *base, target[NAME_MAX + 1];
  struct dir *dir = NULL;
  bool success;

  base = (char *)calloc (1, strlen (name) + 1);
  success = dir_parse (name, base, target);
  if (success)
    dir = dir_open_dir (base);
  free (base);

  success = dir != NULL && dir_remove (dir, target);
  dir_close (dir); 

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}

struct inode*
filesys_open_path (const char *path)
{
  struct dir *dir;
  struct inode *inode = NULL;
  char *base, name[NAME_MAX + 1] = "\0";  
  bool success;

  base = (char *)calloc (1, strlen (path) + 1);
  success = dir_parse (path, base, name);

  if (!success) {
    free (base);
    return inode;
  }

  dir = dir_open_dir (base);
  free (base);
  if (dir != NULL) {
    if (strlen (name) == 0)
      inode = dir_get_inode (dir);
    else
      dir_lookup (dir, name, &inode);
      dir_close (dir);
  }
  return inode;
}
