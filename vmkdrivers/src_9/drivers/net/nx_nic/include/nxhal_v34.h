/*
 * Copyright (C) 2003 - 2009 NetXen, Inc.
 * All rights reserved.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 * 
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.
 * 
 * Contact Information:
 * licensing@netxen.com
 * NetXen, Inc.
 * 18922 Forge Drive
 * Cupertino, CA 95014
 */
#ifndef _NXHAL_V34_H_
#define _NXHAL_V34_H_

#include "nxhal_cmn.h"

nx_rcode_t
nx_fw_cmd_v34_create_ctx(nx_host_rx_ctx_t *prx_ctx,
                         nx_host_tx_ctx_t *ptx_ctx,
                         struct nx_dma_alloc_s *hostrq);

U32
issue_v34_cmd(nx_dev_handle_t drv_handle,
              U32 pci_func,
              U32 version,
              U32 arg1,
              U32 arg2);

nx_rcode_t
nx_fw_cmd_v34_create_ctx(nx_host_rx_ctx_t *prx_ctx,
                         nx_host_tx_ctx_t *ptx_ctx,
                         struct nx_dma_alloc_s *hostrq);

#endif
