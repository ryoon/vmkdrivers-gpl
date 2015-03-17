/*
 * Portions Copyright 2012 VMware, Inc.
 */
#if defined(__VMKLNX__)
/*
 * 08/07/2007: import EXPORT_SYMBOL() to export zlib_inflate_blob()
 */
#include <linux/module.h>
#endif /* defined(__VMKLNX__) */
#include <linux/zutil.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

/**
 *  zlib_inflate_blob - initialize zlib, unpack binary blob, clean up zlib 
 *  @gunzip_buf: a pointer to specify next_out in a z_stream where next_out defines next output byte should be 
 *  @sz: remaining free space at next_out  
 *  @buf: a pointer to define next_in in a z_stream where next_in specifies next input byte should be 
 *  @len: the number of bytes available at next_in 
 *
 *  This is a utility function to initialize, unpack, and clean up zlib.  
 *
 *  RETURN VALUE:
 *  len or negative error code 
 *
 */
/* _VMKLNX_CODECHECK_: zlib_inflate_blob */
int zlib_inflate_blob(void *gunzip_buf, unsigned int sz,
                      const void *buf, unsigned int len)
{
        const u8 *zbuf = buf;
        struct z_stream_s *strm;
        int rc;
        
#if defined(__VMKLNX__)
	VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
#endif
        rc = -ENOMEM;
        strm = kmalloc(sizeof(*strm), GFP_KERNEL);
        if (strm == NULL)
                goto gunzip_nomem1;
        strm->workspace = kmalloc(zlib_inflate_workspacesize(), GFP_KERNEL);
        if (strm->workspace == NULL)
                goto gunzip_nomem2;
        
        /* gzip header (1f,8b,08... 10 bytes total + possible asciz filename)
         * expected to be stripped from input
         */
        strm->next_in = zbuf;
        strm->avail_in = len;
        strm->next_out = gunzip_buf;
        strm->avail_out = sz;
        
        rc = zlib_inflateInit2(strm, -MAX_WBITS);
        if (rc == Z_OK) {
                rc = zlib_inflate(strm, Z_FINISH);
                /* after Z_FINISH, only Z_STREAM_END is "we unpacked it all" */
                if (rc == Z_STREAM_END)
                        rc = sz - strm->avail_out;
                else
                        rc = -EINVAL;
                zlib_inflateEnd(strm);
        } else
                rc = -EINVAL;
        
        kfree(strm->workspace);
 gunzip_nomem2:
        kfree(strm);
 gunzip_nomem1:
        return rc; /* returns Z_OK (0) if successful */
}
#if defined(__VMKLNX__)
EXPORT_SYMBOL(zlib_inflate_blob);
#endif /* defined(__VMKLNX__) */
