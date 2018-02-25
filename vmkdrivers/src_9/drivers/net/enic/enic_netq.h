/*
 *	Copyright 2013 Cisco Systems, Inc.  All rights reserved.
 */
#ifndef ENIC_NETQ_H
#define ENIC_NETQ_H

#if defined( __VMKLNX__) && defined(__VMKNETDDI_QUEUEOPS__)
int enic_netq_enabled(struct enic *enic);
int enic_netqueue_ops(vmknetddi_queueops_op_t op, void *args);
#endif

#endif /* ENIC_NETQ_H */
