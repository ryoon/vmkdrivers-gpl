/*
 * Portions Copyright 2009 - 2011 VMware, Inc.
 */
#ifndef _FCOE_COMPAT_H
#define _FCOE_COMPAT_H

#if defined(__VMKLNX__)
#include <vmklinux_92/vmklinux_scsi.h>
#include <vmklinux_92/vmklinux_cna.h>

#define nr_cpu_ids	smp_num_cpus
#define USHORT_MAX   ((u16)(~0U))
#define false	0

#define FCOE_EXCH_POOL_SIZE( mp )	(sizeof(struct fc_exch_pool) + \
				         ((mp)->pool_max_index + 1) * sizeof(struct fc_exch *))

/*
 * PR 509860 -- Vmklinux does not currently support per PCPU variables.
 */
#define FCOE_PER_CPU_PTR(ptr, cpu, size) \
	(((cpu) < nr_cpu_ids) ? ((typeof(ptr)) ((char*)(ptr) + (cpu) * (size))) \
	                      : NULL)

#define cpu_online(cpu)       TRUE
#define cpumask_first(cpu_online_mask)	0

#undef DEFINE_PER_CPU
#define DEFINE_PER_CPU(type, var)	typeof(type *) var

#undef per_cpu
#define per_cpu(var, cpu)	(var)[ cpu ]

#undef get_cpu_var
#define get_cpu_var(var)	(var)[ smp_processor_id() ]

#undef free_percpu
#define free_percpu(var)	kfree(var)

#define vlan_dev_real_dev(netdev)	netdev
#define dev_queue_xmit(skb)	 vmklnx_cna_queue_xmit(skb)

#define get_page(page)	/* not defined in ESX */
#define skb_clone(skb, gfp_mask)	skb_get(skb)

#ifndef skb_tail_pointer
#define skb_tail_pointer(skb) skb->tail
#endif
/* _VMKLNX_CODECHECK_: put_unaligned_be64 */
static inline void put_unaligned_be64(u64 val, void *p)
{
   *((__be64 *)p) = cpu_to_be64(val);
}

/* _VMKLNX_CODECHECK_: get_unaligned_be64 */
static inline u64 get_unaligned_be64(const void *p)
{
   return be64_to_cpup((__be64 *)p);
}

/* _VMKLNX_CODECHECK_: dev_unicast_delete */
static inline int dev_unicast_delete(struct net_device *dev, void *addr)
{
    return 1;
}

/* _VMKLNX_CODECHECK_: dev_unicast_add */
static inline int dev_unicast_add(struct net_device *dev, void *addr)
{
    return 1;
}

/* _VMKLNX_CODECHECK_: fcoe_alloc_percpu */
static inline void *fcoe_alloc_percpu(size_t size, size_t align)
{
	void *ret;

	size = size * nr_cpu_ids;

	if ((ret = kmalloc(size, GFP_KERNEL)) != NULL) {
		memset(ret, 0, size);
	}

	return ret;
}

extern int vmklnx_fc_eh_bus_reset(struct scsi_cmnd *sc_cmd);

extern short libfc_lun_qdepth;

#else /* defined(__VMKLNX__) */
#define FCOE_PER_CPU_PTR(ptr, cpu, size) per_cpu_ptr(ptr, cpu)
#define fcoe_alloc_percpu(size, align) __alloc_percpu( size, align )	

#endif /* defined(__VMKLNX__) */


#endif  /* _FCOE_COMPAT_H */

