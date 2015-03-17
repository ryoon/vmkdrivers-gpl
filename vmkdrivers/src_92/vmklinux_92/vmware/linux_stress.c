/* **********************************************************
 * Copyright 2003, 2007-2008, 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * 
 * linux_stress.c --
 * 
 *	Support for stress code in linux drivers.
 *
 *	Linux_stress contains common helper functions for  
 *	the stress code in linux drivers.
 * 
 */


#ifdef MODULE
#define __NO_VERSION__
#include <linux/module.h>
#endif
#define __VMKLNX_NO_IP_FAST_CSUM__ 1
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/timer.h>
#include <linux/timex.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>

#define VMKLNX_LOG_HANDLE LinStress
#include "vmklinux_log.h"

#define MAX_FREE_LIST_LEN 20
static struct timer_list freeListTimer;
static struct sk_buff *freeList;
static unsigned freeListLen;
static spinlock_t freeListLock = SPIN_LOCK_UNLOCKED;
static unsigned char inCleanUp;


static void FreeSkb(unsigned long);
static void ReleaseFreeList(void);
void LinStress_SetupStress(void);
void LinStress_CleanupStress(void);
void LinStress_AppendSkbToFreeList(struct sk_buff *);
void LinStress_CorruptSkbData(struct sk_buff *, unsigned int, unsigned int);

/*
 *-----------------------------------------------------------------------------
 * 
 * FreeSkb --
 *    
 *    Release socket buffers from the free list. This is a timer callback 
 *    function that is used to periodically release buffers from the list 
 *    of buffers that had been retained by VMWARE_STRESS_HW_RETAIN_BUF.
 *
 * Results:
 * 
 *    None
 *
 * Side effects:
 *    
 *    Release socket buffers from the free list till
 *      freeListLen < MAX_FREE_LIST_LEN
 *     *-----------------------------------------------------------------------------  
 */    
static void
FreeSkb(unsigned long unused)
{  
   struct sk_buff *curBuf, *tmpBuf;
   
   spin_lock(&freeListLock);
   
   curBuf = freeList;
   tmpBuf = NULL;
   
   if (inCleanUp) {
      spin_unlock(&freeListLock);
      return;
   }
   if (freeList) {
      unsigned skip =
    (freeListLen < MAX_FREE_LIST_LEN)?freeListLen : MAX_FREE_LIST_LEN;
      freeListLen = --skip;
      VMKLNX_DEBUG(1, "freeListLen = %u, skip = %u", freeListLen, skip);
      if (skip == 0) {
    freeList = NULL;
      } else {
    while (--skip > 0) {
       curBuf  = curBuf->next;
    }
    tmpBuf = curBuf;
    curBuf = curBuf->next;
    tmpBuf->next = NULL;
      }
   }
   spin_unlock(&freeListLock);
   
   /* free up the rest */
   while (curBuf) {
      tmpBuf = curBuf->next;
      VMKLNX_DEBUG(3, "Released buffer %p", curBuf);
      kfree_skb(curBuf);
      curBuf = tmpBuf;
   }  
      
   freeListTimer.expires = jiffies+HZ;
   add_timer(&freeListTimer);
}

/*
 *-----------------------------------------------------------------------------
 * 
 * ReleaseFreeList --
 *    
 *    Release all the socket buffers in the free list. Called from the 
 *    cleanup code.
 *
 * Results:
 * 
 *    None
 *
 * Side effects:
 *    
 *    All the buffers in the free list are released.
 *    
 *-----------------------------------------------------------------------------  
 */
static void
ReleaseFreeList()
{
   struct sk_buff *tmpBuf;
   spin_lock(&freeListLock);
   inCleanUp = 1;
   spin_unlock(&freeListLock);
   while (freeList) {
      tmpBuf   = freeList;
      freeList = freeList->next;
      kfree_skb(tmpBuf);
   }
   freeList = NULL;
   freeListLen = 0;
   VMKLNX_DEBUG(2, "Released Free List");
}

/*
 *-----------------------------------------------------------------------------
 * 
 * LinStress_CorruptSkbData --
 *    
 *    Corrupt at most len bytes of the buffer pointed to by skb, starting
 *      at the given offset into the frame.
 *    
 *
 * Results:
 * 
 *    None
 *
 * Side effects:
 *    
 *    The buffer is corrupted.
 *    
 *-----------------------------------------------------------------------------  
 */
void
LinStress_CorruptSkbData(struct sk_buff* skb,
                           unsigned int len, unsigned int offset)
{
   unsigned long i, j;
   char *frame = skb->data + offset;
   unsigned int head_len;
   int maxLen;
   unsigned int k = 0;

   head_len = skb->len - skb->data_len;
   maxLen = ((head_len - offset) < len)? (head_len - offset):len;

   if (maxLen <= 0) {
      VMKLNX_DEBUG(2, "too small a packet to do anything");
      return;
   }

   for (i = 0; i < maxLen; i+= sizeof(cycles_t)) {
      cycles_t cycles = get_cycles();
      for (j = 0; j < sizeof(cycles_t); j++) {
         if ((i + j) >= maxLen) {
            break;
         }
         *frame++ = ((char *)(&cycles))[j];
         k++;
      }
   }
   VMKLNX_DEBUG(2, "Finished Corrupting skb %p in %d loops", skb, k);
   return;

}

/*
 *----------------------------------------------------------------------------
 *
 *  LinStress_CorruptRxData --
 *
 *    Go deep into the skb and corrupt it.
 *
 *    XXX what this really wants to do is corrupt data after all of the 
 *        layer 4 protocol headers.  currently it just assumes IPV4 with
 *        no more than 4 bytes of options for TCP.
 *
 *  Returns:
 *    None.
 *
 *  Side-effects:
 *    None.
 *----------------------------------------------------------------------------
 */

struct NetPktMemHdr;
void
LinStress_CorruptRxData(struct NetPktMemHdr *pkt, struct sk_buff *skb)
{
}

/*
 *----------------------------------------------------------------------------
 *
 *  LinStress_CorruptEthHdr --
 * 
 *    Corrupt just the ethernet header.
 *
 *  Returns:
 *    None.
 *
 *  Side-effects:
 *    None.
 *
 *
 *----------------------------------------------------------------------------
 */

void
LinStress_CorruptEthHdr(struct sk_buff *skb)
{
   LinStress_CorruptSkbData(skb, 14, 0);
}

/*
 *-----------------------------------------------------------------------------
 * 
 * LinStress_SetupStress --
 *    
 * Init function. Initializes the data structures.
 * 
 * 
 * Results:
 * 
 *    None
 *
 * Side effects:
 *    
 *    Local data structures are initialized.
 *    
 *-----------------------------------------------------------------------------  
 */
void
LinStress_SetupStress()
{
   VMKLNX_CREATE_LOG();

   init_timer(&freeListTimer);
   freeListTimer.function = (void (*)(unsigned long))&FreeSkb;
   freeListTimer.data = 0;
   freeListTimer.expires = jiffies + HZ;
   add_timer(&freeListTimer);
   spin_lock_init(&freeListLock);
   VMKLNX_DEBUG(1, "Stress Helper initialized");
}

/*
 *-----------------------------------------------------------------------------
 * 
 * LinStress_CleanupStress --
 *    
 * Exit function Clean up all the data structures. 
 * Results:
 * 
 *    None
 *
 * Side effects:
 *    
 *    All data structures are cleaned here.
 *    
 *-----------------------------------------------------------------------------  
 */
void
LinStress_CleanupStress()
{
   del_timer_sync(&freeListTimer);
   ReleaseFreeList();
   VMKLNX_DEBUG(1, "Finished stress cleanup");
   VMKLNX_DESTROY_LOG();
}

