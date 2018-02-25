 /*
  *  Copyright 2013 Cisco Systems, Inc.  All rights reserved.
  */

#ifndef _ENIC_CLSF_H_
#define _ENIC_CLSF_H_

#include "vnic_dev.h"
#include "enic.h"

#define ENIC_CLSF_EXPIRE_COUNT 128

int enic_addfltr_mac_vlan(struct enic *enic, u8 *macaddr,
	u16 vlan_id, u16 rq_id, u16 *filter_id);
int enic_delfltr(struct enic *enic, u16 filter_id);
int enic_addfltr_5t(struct enic *enic, struct flow_keys *keys, u16 rq);
void enic_rfs_flw_tbl_init(struct enic *enic);
void enic_rfs_flw_tbl_free(struct enic *enic);
struct enic_rfs_fltr_node *htbl_fltr_search(struct enic *enic, u16 fltr_id);

static inline void enic_rfs_timer_init(struct enic *enic) {}
static inline void enic_rfs_timer_free(struct enic *enic) {}

#endif /* _ENIC_CLSF_H_ */
