/*
 * Copyright 2011 Cisco Systems, Inc.  All rights reserved.
 *
 * This program is free software; you may redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * [Insert appropriate license here when releasing outside of Cisco]
 * $Id: fnic_procfs.c 111268 2012-08-27 17:11:40Z atungara $
 */


/*
 * This file contains routines to create an interface to
 * fnic in /proc/ on ESX 5.0 (MN). MN does not support
 * sysfs, but it does support /proc/. So we'll use that
 * to set fnic_log_level to the required debug level, and
 * pass it on to fnic after fnic is loaded.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>

#define FNIC_LOG_FILE "fnic_log_level"
#define FNIC_MODULE_DIR "fnic"

#define FNIC_PROCFS_MAX_BUF_LEN 80

extern unsigned int fnic_log_level;
static char procfs_buf[FNIC_PROCFS_MAX_BUF_LEN];
static unsigned long procfs_buf_size;

static struct proc_dir_entry *fnic_log_level_file;
static struct proc_dir_entry *fnic_dir;

int f_l_l_proc_read(char *buf, char **buf_locn,
		    off_t offset, int buf_len, int *eof,
		    void *data);
int f_l_l_proc_write(struct file *filp, const char *buf,
		     unsigned long count, void *data);
int init_fnic_procfs(void);
void teardown_fnic_procfs(void);

int f_l_l_proc_read(char *buf, char **buf_locn,
		    off_t offset, int buf_len, int *eof,
		    void *data)
{

	int ret;
	if (offset > 0) {
		/* We're finished reading */
		ret = 0;
	} else {
		/* fill up buf, return buf size */
		memcpy(buf, procfs_buf, procfs_buf_size);
		ret = procfs_buf_size;
	}

	return ret;
}

int f_l_l_proc_write(struct file *filp, const char __user *buf,
		     unsigned long count, void *data)
{
	unsigned long fnic_log_val;

	procfs_buf_size = (count > FNIC_PROCFS_MAX_BUF_LEN) ?
		FNIC_PROCFS_MAX_BUF_LEN : count;

	/* write out the data to our kernel buffer */
	if (copy_from_user(procfs_buf, buf, procfs_buf_size)) {
		printk(KERN_ALERT "\nFailed to copy user buf to kernel buf\n");
		return -EFAULT;
	}

	/* Ok, now we need to convert this char buffer to an int */
	fnic_log_val = simple_strtoul(procfs_buf, NULL, 0);

	/*
	 * If fnic_log_val is a valid value, set fnic_log_level to it. Else,
	 * keep it at its previous value.
	 */
	fnic_log_level = (fnic_log_val > 0 && fnic_log_val <= 7) ?
			 fnic_log_val : fnic_log_level;

	/* Returning the correct size of the buffer read in will allow the
	 * user to break out of an echo "value" > /proc/fnic/fnic_log_level
	 */
	return procfs_buf_size;
}

int init_fnic_procfs(void)
{

	int err = 0;

	/* First, create a directory called "fnic" under /proc/ */
	fnic_dir = proc_mkdir(FNIC_MODULE_DIR, NULL);
	if (fnic_dir == NULL) {
		printk(KERN_ALERT "Create /proc/fnic/ failed \n");
		err = -ENOMEM;
		goto remove_fnic_dir;
	}

	/* Then, create a file called "fnic_log_level" under /proc/fnic/ */
	fnic_log_level_file = create_proc_entry(FNIC_LOG_FILE, 0644, fnic_dir);
	if (!fnic_log_level_file) {
		printk(KERN_ALERT "Create /proc/fnic/fnic_log_level failed\n");
		err = -ENOMEM;
		goto remove_fnic_log_level_file;
	}

	/*
	 * Initialize the proc read and write routines that will let users
	 * write to and read respectively, from these files.
	 */
	fnic_log_level_file->read_proc = f_l_l_proc_read;
	fnic_log_level_file->write_proc = f_l_l_proc_write;
	fnic_log_level_file->owner = THIS_MODULE;
	fnic_log_level_file->mode = S_IFREG | S_IRUGO;
	fnic_log_level_file->size = 2*FNIC_PROCFS_MAX_BUF_LEN;

	return err;

remove_fnic_log_level_file:
	remove_proc_entry(FNIC_LOG_FILE, fnic_log_level_file);
remove_fnic_dir:
	remove_proc_entry(FNIC_MODULE_DIR, fnic_dir);

	return err;
}

void teardown_fnic_procfs(void)
{
	printk(KERN_INFO "Tearing down procfs for fnic\n");
	remove_proc_entry(FNIC_LOG_FILE, fnic_dir);
	remove_proc_entry(FNIC_MODULE_DIR, NULL);
	return;
}
