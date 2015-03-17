/*
 * Copyright 2010 Cisco Systems, Inc.  All rights reserved.
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

#ifndef _ENIC_IOCTL_H_
#define _ENIC_IOCTL_H_

#include "vnic_devcmd.h"

#ifndef SIOCDEVPRIVATE
#define SIOCDEVPRIVATE			0x89F0	/* to 89FF */
#endif

#define ENIC_DEVCMD			SIOCDEVPRIVATE
#define ENIC_DEVCMD_PROXY		SIOCDEVPRIVATE+1
#define ENIC_DRVCMD			SIOCDEVPRIVATE+2

#define ENIC_PROXY_TYPE_NONE		0
#define ENIC_PROXY_TYPE_BDF		1

enum enic_drvcmd_cmd {

	DRVCMD_NONE = 0,
	/* 
	 * out:	(u64)a0= Num of WQs this enic owns.
	 * */   
	DRVCMD_GET_NUM_WQ = 1,
                
	/*      
	 * out:	(u64)a0= Num of RQs this enic owns.
	 * */
	DRVCMD_GET_NUM_RQ = 2,
        
        /*      
	 * out:	(u64)a0= Num of CQs this enic owns.
	 * */
	DRVCMD_GET_NUM_CQ = 3,

	/*
	 * out:	(u64)a0= Num of INTRs this enic owns.
	 * */
	DRVCMD_GET_NUM_INTRS = 4,

	/*
	 * out: (u64)a0= BDF of this enic.
	 * */
	DRVCMD_GET_SBDF = 5,
};

union enic_ioctl {
	struct {
		enum vnic_devcmd_cmd cmd;
		u64 a0;
		u64 a1;
		int wait;
		int proxy_type;
		union {
			u16 proxy_bdf;
			u32 proxy_index;
		} proxy_key;
	} devcmd;

	struct {
		enum enic_drvcmd_cmd cmd;
		u64 a0;
		u64 a1;
	} drvcmd;
};

#endif /* _ENIC_IOCTL_H_ */
