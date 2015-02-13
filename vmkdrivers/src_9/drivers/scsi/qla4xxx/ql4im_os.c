/*
 * QLogic iSCSI HBA Driver
 * Copyright (c)  2003-2006 QLogic Corporation
 *
 * See LICENSE.qla4xxx for copyright and licensing details.
 */

/******************************************************************************
 *             Please see release.txt for revision history.                   *
 *                                                                            *
 ******************************************************************************
 * Function Table of Contents:
 ****************************************************************************/

#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/klist.h>
#include <asm/semaphore.h>

#include "ql4_def.h"
#include "ql4im_def.h"

/* Restrict compilation to 2.6.18 or greater */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
#error "This module does not support kernel versions earlier than 2.6.18"
#endif

#define QL4_MODULE_NAME	"qla4xxx"
	
extern struct klist *qla4xxx_hostlist_ptr;
static struct klist *ha_list;
static int hba_count = 0;

/*unsigned dbg_level = (QL_DBG_1 | QL_DBG_2 | QL_DBG_4 | QL_DBG_11 | QL_DBG_12);*/
unsigned dbg_level = 0;

static struct hba_ioctl *hba[EXT_DEF_MAX_HBAS];

static struct class *apidev_class = NULL;
static int apidev_major;

extern char *qla4xxx_version_str;
uint8_t drvr_major = 5;
uint8_t drvr_minor = 0;
uint8_t drvr_patch = 5;
uint8_t drvr_beta = 9;
char drvr_ver[40];
#define DEFAULT_VER	"5.00.05b9-k"

static int
apidev_ioctl(struct inode *inode, struct file *fp, unsigned int cmd,
    unsigned long arg)
{
	return (qla4xxx_ioctl((int)cmd, (void*)arg));
}

#ifdef CONFIG_COMPAT
static long
qla4xxx_ioctl32(struct file *file, unsigned int cmd, unsigned long arg)
{
	int rval = -ENOIOCTLCMD;

	lock_kernel();
	rval = qla4xxx_ioctl((int)cmd, (void*)arg);
	unlock_kernel();

	return rval;
}
#endif

static struct file_operations apidev_fops = {
	.owner = THIS_MODULE,
	.ioctl = apidev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = qla4xxx_ioctl32,
#endif
};

static char *getval(char *ver, uint8_t *val)
{
	*val = 0;

        while (ver &&(*ver != '\0')&&(*ver != '.')&&(*ver != '-')&&
                (*ver >= '0')&&(*ver <= '9')) {
                *val = *val * 10 + *ver - '0';
                ver++;
        }
        return ver;
}

struct hba_ioctl *ql4im_get_adapter_handle(uint16_t instance)
{
	if (instance >= EXT_DEF_MAX_HBAS) 
		return NULL;
	return hba[instance];
}

static int ql4_ioctl_alloc(int hba_idx, struct klist_node *node)
{
	struct hba_ioctl *haioctl;

	haioctl = kzalloc(sizeof(struct hba_ioctl), GFP_ATOMIC);
	if (haioctl == NULL)
		return -ENOMEM;
	
	memset(haioctl, 0, sizeof(struct hba_ioctl));
	hba[hba_idx] = haioctl;

	haioctl->ha  = (struct scsi_qla_host *)node;
	haioctl->dma_v = pci_alloc_consistent(haioctl->ha->pdev,
					QL_DMA_BUF_SIZE,
					&haioctl->dma_p);

	if (haioctl->dma_v == NULL) {
		printk(KERN_WARNING "qisitoctl: %s: %d: pcialloc failed\n",
			__func__, hba_idx);
		kfree(haioctl);
		hba[hba_idx] = NULL;
		return -ENOMEM;
	}
#ifdef __VMKLNX__
	init_MUTEX(&haioctl->ioctl_sem);
#else
	mutex_init(&haioctl->ioctl_sem);
#endif

	haioctl->dma_len = QL_DMA_BUF_SIZE;

	return 0;
}

static void ql4_ioctl_free(void)
{
	int i;
	struct hba_ioctl *haioctl;
	
	for (i = 0; i < EXT_DEF_MAX_HBAS; i++) {
		if ((haioctl = hba[i]) != NULL) {
			pci_free_consistent(haioctl->ha->pdev,
					PAGE_SIZE,
					haioctl->dma_v,
					haioctl->dma_p);
			kfree(haioctl);
		}
		hba[i] = NULL;
	}
}

#ifndef __VMKLNX__
uint32_t
ql4im_get_hba_count(void)
{
	return(hba_count);
}

static void get_drvr_version(void)
{
	char *ver;

	ver = ((char *)symbol_get(qla4xxx_version_str));
	if (ver == NULL) { 
		strcpy(drvr_ver, DEFAULT_VER);
		printk(KERN_INFO "symbol_get(qla4xxx_version_str) failed\n"); 
		return;
	}

	strcpy(drvr_ver, ver);
	symbol_put(qla4xxx_version_str);

	ver = drvr_ver;

        ver = getval(ver, &drvr_major);

        if (ver && *ver == '.') ver++;

        ver = getval(ver, &drvr_minor);

        if (ver && *ver == '.') ver++;

        ver = getval(ver, &drvr_patch);

	drvr_beta = 0;
        if (ver && *ver == 'b') {
                ver++;
                ver = getval(ver, &drvr_beta);
        }

	printk(KERN_INFO "drvr_ver %s major %d minor %d patch %d beta %d\n", 
		drvr_ver, drvr_major, drvr_minor, drvr_patch, drvr_beta);
}

static int ql4_ioctl_init(void)
{
	ENTER(__func__);

	apidev_class = class_create(THIS_MODULE, QL4_MODULE_NAME);
	if (IS_ERR(apidev_class)) {
		DEBUG2(printk("qisioctl: %s: Unable to sysfs class\n", __func__));
		apidev_class = NULL;
		return 1;
	}
	DEBUG4(printk("qisioctl: %s: apidev_class=%p.\n", __func__, apidev_class));

	apidev_major = register_chrdev(0, QL4_MODULE_NAME, &apidev_fops);
	if (apidev_major < 0) {
		DEBUG2(printk("qisioctl: %s: Unable to register CHAR device (%d)\n",
		    __func__, apidev_major));

		class_destroy(apidev_class);
		apidev_class = NULL;
		return apidev_major;
	}
	DEBUG4(printk("qisioctl: %s: apidev_major=%d.\n", __func__, apidev_major));

        class_device_create(apidev_class, NULL, MKDEV(apidev_major, 0), NULL,
            		QL4_MODULE_NAME);
	LEAVE(__func__);
	return 0;
}

static void ql4_ioctl_exit(void)
{
	ENTER(__func__);

	if (!apidev_class)
		return;

	class_device_destroy(apidev_class, MKDEV(apidev_major, 0));

	unregister_chrdev(apidev_major, QL4_MODULE_NAME);

	class_destroy(apidev_class);

	apidev_class = NULL;

	LEAVE(__func__);
}

static int ql4im_init(void)
{
	struct klist_node *node;
	struct klist **ql4_hl;
	struct klist_iter iter;

	ENTER( __func__ );

	memset(&hba, 0,  sizeof (*hba));
	ql4_hl = (struct klist **)symbol_get(qla4xxx_hostlist_ptr);

	if (ql4_hl == NULL) {
		printk("qistioctl: symbol_get failed qla4xxx_hostlist_ptr\n");
		return -ENODEV;
	}

	ha_list = *ql4_hl;

	if (ha_list == NULL) {
		printk("qistioctl: ha_list == NULL\n");
		return -ENODEV;
	}

	DEBUG1(printk("qisioctl: %s: ha_list %p.\n", __func__, ha_list));

	get_drvr_version();

	if (ql4_ioctl_init())
		return -ENODEV;

	klist_iter_init(ha_list, &iter);
	
	DEBUG1(printk("qisioctl: %s: klist_iter_init successful \n", __func__));

	while ((node = klist_next(&iter)) != NULL) {
		ql4_ioctl_alloc(hba_count, node);
		hba_count++;
		DEBUG1(printk("qisioctl: %s: node = %p\n", __func__, node));
	}

	klist_iter_exit(&iter);

	LEAVE( __func__ );

	printk(KERN_INFO "QLogic iSCSI IOCTL Module ver: %s\n", QL4IM_VERSION);

	return 0;
}

static void __exit ql4im_exit(void)
{
	ENTER( __func__ );
	symbol_put(qla4xxx_hostlist_ptr);
	ql4_ioctl_free();
	ql4_ioctl_exit();
	LEAVE( __func__ );
}

module_init(ql4im_init);
module_exit(ql4im_exit);

MODULE_AUTHOR("QLogic Corporation");
MODULE_DESCRIPTION("QLogic iSCSI Driver IOCTL Module");
MODULE_VERSION(QL4IM_VERSION);
MODULE_LICENSE("GPL");

#else
#include "ql4_version.h"
uint32_t
ql4im_get_hba_count(void)
{
	int i;

	hba_count = 0;
	for (i = 0; i < EXT_DEF_MAX_HBAS; i++)
		if (hba[i] != NULL)
			hba_count++;
	return(hba_count);
}

static void get_drvr_version(void)
{
	char *ver = (char *)QLA4XXX_DRIVER_VERSION;

	strcpy(drvr_ver, ver);
	ver = drvr_ver;
        ver = getval(ver, &drvr_major);
        if (ver && *ver == '.') ver++;

        ver = getval(ver, &drvr_minor);
        if (ver && *ver == '.') ver++;

        ver = getval(ver, &drvr_patch);
	drvr_beta = 0;
        if (ver && *ver == 'b') {
                ver++;
                ver = getval(ver, &drvr_beta);
        }
	printk(KERN_INFO "qisioctl: %s: ver %s maj %d min %d pat %d beta %d\n",
		__func__, drvr_ver, drvr_major, drvr_minor, drvr_patch,
		drvr_beta);
}

static int ql4_ioctl_init(void)
{
	ENTER(__func__);

	apidev_major = register_chrdev(0, QL4_MODULE_NAME, &apidev_fops);
	if (apidev_major < 0) {
		DEBUG2(printk("qisioctl: %s: Unable to register CHAR device (%d)\n",
		    __func__, apidev_major));
		printk("qisioctl: %s: Unable to register CHAR device (%d)\n",
		    __func__, apidev_major);
		return apidev_major;
	}
	DEBUG4(printk("qisioctl: %s: apidev_major=%d.\n", __func__, apidev_major));
	printk("qisioctl: %s: apidev_major=%d.\n", __func__, apidev_major);

	LEAVE(__func__);
	return 0;
}

static void ql4_ioctl_exit(void)
{
	ENTER(__func__);
	unregister_chrdev(apidev_major, QL4_MODULE_NAME);
	LEAVE(__func__);
}

int ql4im_mem_alloc(int hba_idx, struct scsi_qla_host *ha)
{
	return ql4_ioctl_alloc(hba_idx, (struct klist_node *)ha);
}

void ql4im_mem_free(int hba_idx)
{
	struct hba_ioctl *haioctl;

	if ((haioctl = hba[hba_idx]) != NULL) {
		pci_free_consistent(haioctl->ha->pdev,
				PAGE_SIZE,
				haioctl->dma_v,
				haioctl->dma_p);
		kfree(haioctl);
	}
	hba[hba_idx] = NULL;
}

int ql4im_init(void)
{
	int i ;

	ENTER( __func__ );

	get_drvr_version();
	
	for (i = 0; i < EXT_DEF_MAX_HBAS; i++)
		hba[i] = NULL;

	if (ql4_ioctl_init())
		return -ENODEV;

	LEAVE(__func__);
	return 0;
}

void ql4im_exit(void)
{
	ENTER( __func__ );
	ql4_ioctl_exit();
	LEAVE( __func__ );
}

/* emulation of down_timeout() */
int
ql4im_down_timeout(struct semaphore *sema, unsigned long timeout)
{
        const unsigned int step = 100; /* msecs */
        unsigned int iterations = jiffies_to_msecs(timeout)/100;

        do {
                if (!down_trylock(sema))
                        return 0;
                if (msleep_interruptible(step))
                        break;
        } while (--iterations > 0);

        return -ETIMEDOUT;
}

#endif
