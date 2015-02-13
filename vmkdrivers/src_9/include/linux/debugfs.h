/*
 *  debugfs.h - a tiny little debug file system
 *
 *  Copyright (C) 2004 Greg Kroah-Hartman <greg@kroah.com>
 *  Copyright (C) 2004 IBM Inc.
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License version
 *	2 as published by the Free Software Foundation.
 *
 *  debugfs is for people to use instead of /proc or /sys.
 *  See Documentation/DocBook/kernel-api for more details.
 */

#ifndef _DEBUGFS_H_
#define _DEBUGFS_H_

#include <linux/fs.h>

#include <linux/types.h>

struct file_operations;

struct debugfs_blob_wrapper {
	void *data;
	unsigned long size;
};

#if defined(CONFIG_DEBUG_FS)
struct dentry *debugfs_create_file(const char *name, mode_t mode,
				   struct dentry *parent, void *data,
				   const struct file_operations *fops);

struct dentry *debugfs_create_dir(const char *name, struct dentry *parent);

void debugfs_remove(struct dentry *dentry);

struct dentry *debugfs_create_u8(const char *name, mode_t mode,
				 struct dentry *parent, u8 *value);
struct dentry *debugfs_create_u16(const char *name, mode_t mode,
				  struct dentry *parent, u16 *value);
struct dentry *debugfs_create_u32(const char *name, mode_t mode,
				  struct dentry *parent, u32 *value);
struct dentry *debugfs_create_bool(const char *name, mode_t mode,
				  struct dentry *parent, u32 *value);

struct dentry *debugfs_create_blob(const char *name, mode_t mode,
				  struct dentry *parent,
				  struct debugfs_blob_wrapper *blob);
#else

#include <linux/err.h>

/* 
 * We do not return NULL from these functions if CONFIG_DEBUG_FS is not enabled
 * so users have a chance to detect if there was a real error or not.  We don't
 * want to duplicate the design decision mistakes of procfs and devfs again.
 */


/**
 * debugfs_create_file - create a file in the debugfs filesystem
 * @name: a pointer to a string containing the name of the file to create.
 * @mode: the permission that the file should have
 * @parent: a pointer to the parent dentry for this file.  This should be a
 *          directory dentry if set.  If this paramater is NULL, then the
 *          file will be created in the root of the debugfs filesystem.
 * @data: a pointer to something that the caller will want to get to later
 *        on.  The inode.i_private pointer will point to this value on
 *        the open() call.
 * @fops: a pointer to a struct file_operations that should be used for
 *        this file.
 *
 * This is the basic "create a file" function for debugfs.  It allows for a
 * wide range of flexibility in createing a file, or a directory (if you
 * want to create a directory, the debugfs_create_dir() function is
 * recommended to be used instead.)
 *
 * This function will return a pointer to a dentry if it succeeds.  This
 * pointer must be passed to the debugfs_remove() function when the file is
 * to be removed (no automatic cleanup happens if your module is unloaded,
 * you are responsible here.)  If an error occurs, %NULL will be returned.
 *
 * If debugfs is not enabled in the kernel, the value -%ENODEV will be
 * returned.
 *
 * ESX Deviation Notes:
 * debugfs is not enabled in ESX.
 *
 * RETURN VALUE:
 * Always -%ENODEV (-ENODEV converted to a pointer).
 */
/* _VMKLNX_CODECHECK_: debugfs_create_file */
static inline struct dentry *debugfs_create_file(const char *name, mode_t mode,
					struct dentry *parent, void *data,
					const struct file_operations *fops)
{
	return ERR_PTR(-ENODEV);
}

/**
 * debugfs_create_dir - create a directory in the debugfs filesystem
 * @name: a pointer to a string containing the name of the directory to
 *        create.
 * @parent: a pointer to the parent dentry for this file.  This should be a
 *          directory dentry if set.  If this paramater is NULL, then the
 *          directory will be created in the root of the debugfs filesystem.
 *
 * This function creates a directory in debugfs with the given name.
 *
 * This function will return a pointer to a dentry if it succeeds.  This
 * pointer must be passed to the debugfs_remove() function when the file is
 * to be removed (no automatic cleanup happens if your module is unloaded,
 * you are responsible here.)  If an error occurs, %NULL will be returned.
 *
 * If debugfs is not enabled in the kernel, the value -%ENODEV will be
 * returned.
 *
 * ESX Deviation Notes:
 * debugfs is not enabled in ESX.
 *
 * RETURN VALUE:
 * Always -%ENODEV (-ENODEV converted to a pointer).
 */
/* _VMKLNX_CODECHECK_: debugfs_create_dir */
static inline struct dentry *debugfs_create_dir(const char *name,
						struct dentry *parent)
{
	return ERR_PTR(-ENODEV);
}

static inline void debugfs_remove(struct dentry *dentry)
{ }

static inline struct dentry *debugfs_create_u8(const char *name, mode_t mode,
					       struct dentry *parent,
					       u8 *value)
{
	return ERR_PTR(-ENODEV);
}

static inline struct dentry *debugfs_create_u16(const char *name, mode_t mode,
						struct dentry *parent,
						u16 *value)
{
	return ERR_PTR(-ENODEV);
}

static inline struct dentry *debugfs_create_u32(const char *name, mode_t mode,
						struct dentry *parent,
						u32 *value)
{
	return ERR_PTR(-ENODEV);
}

static inline struct dentry *debugfs_create_bool(const char *name, mode_t mode,
						 struct dentry *parent,
						 u32 *value)
{
	return ERR_PTR(-ENODEV);
}

static inline struct dentry *debugfs_create_blob(const char *name, mode_t mode,
				  struct dentry *parent,
				  struct debugfs_blob_wrapper *blob)
{
	return ERR_PTR(-ENODEV);
}

#endif

#endif
