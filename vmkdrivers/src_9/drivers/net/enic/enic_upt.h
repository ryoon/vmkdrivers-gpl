/*
 * Copyright 2008 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
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
 *
 */

#ifndef _ENIC_UPT_H_
#define _ENIC_UPT_H_

enum enic_upt_oob {
	ENIC_UPT_OOB_STATS,
	ENIC_UPT_OOB_RSS,
	ENIC_UPT_OOB_BOUNCE,
	ENIC_UPT_OOB_NOTIFY,
	ENIC_UPT_OOB_MAX
};

struct enic;
void enic_upt_link_down(struct enic *enic);
void enic_upt_prepare_for(struct enic *enic);
int enic_upt_recover_from(struct enic *enic);
void *enic_upt_alloc_bounce_buf(struct enic *enic, size_t size,
	dma_addr_t *dma_handle);
void enic_upt_free_bounce_buf(struct enic *enic, size_t size,
	void *vaddr, dma_addr_t dma_handle);
VMK_ReturnStatus enic_upt_ops(void *clientData, vmk_NetPTOP op, void *args);

#endif
