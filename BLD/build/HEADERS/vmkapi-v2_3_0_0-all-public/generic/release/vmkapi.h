/* **********************************************************
 * Copyright 2004 - 2014 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 * VMKernel external API
 */

#ifndef _VMKAPI_H_
#define _VMKAPI_H_

#if defined(__cplusplus)
extern "C" {
#endif

/** \cond never */

/*
 * Determine if we're compiling in user mode or kernel mode
 */
#if defined(USERLEVEL) || !defined(__KERNEL__)
# define VMK_BUILDING_FOR_USER_MODE
#else
# define VMK_BUILDING_FOR_KERNEL_MODE
#endif


/*
 * For certain VMware-internal build environments we must turn
 * on all of vmkapi from a single switch.
 */
#if defined(VMK_ENABLE_ALL_VMKAPIS) || defined(VMK_ENABLE_ALL_BC_VMKAPIS)
# undef VMK_DEVKIT_HAS_API_VMKAPI_BASE
# undef VMK_DEVKIT_HAS_API_VMKAPI_CORE
# undef VMK_DEVKIT_HAS_API_VMKAPI_DEVICE
# undef VMK_DEVKIT_HAS_API_VMKAPI_DVFILTER
# undef VMK_DEVKIT_HAS_API_VMKAPI_ISCSI
# undef VMK_DEVKIT_HAS_API_VMKAPI_ISCSI_NET
# undef VMK_DEVKIT_HAS_API_VMKAPI_NET
# undef VMK_DEVKIT_HAS_API_VMKAPI_RDMA
# undef VMK_DEVKIT_HAS_API_VMKAPI_SOCKETS
# undef VMK_DEVKIT_HAS_API_VMKAPI_SCSI
# undef VMK_DEVKIT_HAS_API_VMKAPI_SYSTEM_HEALTH
# undef VMK_DEVKIT_HAS_API_VMKAPI_MPP
# undef VMK_DEVKIT_HAS_API_VMKAPI_NMP
# undef VMK_DEVKIT_HAS_API_VMKAPI_NPIV
# undef VMK_DEVKIT_HAS_API_VMKAPI_VDS
# undef VMK_DEVKIT_HAS_API_VMKAPI_GPU
# undef VMK_DEVKIT_HAS_API_VMKAPI_NMI
# undef VMK_DEVKIT_HAS_API_VMKAPI_EXPERIMENTAL
# undef VMK_DEVKIT_USES_PUBLIC_APIS
# undef VMK_DEVKIT_USES_BINARY_COMPATIBLE_APIS
# undef VMK_DEVKIT_USES_PRIVATE_APIS
# undef VMK_DEVKIT_USES_DEPRECATED_APIS
# define VMK_DEVKIT_HAS_API_VMKAPI_BASE
# define VMK_DEVKIT_HAS_API_VMKAPI_CORE
# define VMK_DEVKIT_HAS_API_VMKAPI_DEVICE
# define VMK_DEVKIT_HAS_API_VMKAPI_DVFILTER
# define VMK_DEVKIT_HAS_API_VMKAPI_ISCSI
# define VMK_DEVKIT_HAS_API_VMKAPI_ISCSI_NET
# define VMK_DEVKIT_HAS_API_VMKAPI_NET
# define VMK_DEVKIT_HAS_API_VMKAPI_RDMA
# define VMK_DEVKIT_HAS_API_VMKAPI_SOCKETS
# define VMK_DEVKIT_HAS_API_VMKAPI_SCSI
# define VMK_DEVKIT_HAS_API_VMKAPI_SYSTEM_HEALTH
# define VMK_DEVKIT_HAS_API_VMKAPI_MPP
# define VMK_DEVKIT_HAS_API_VMKAPI_NMP
# define VMK_DEVKIT_HAS_API_VMKAPI_NPIV
# define VMK_DEVKIT_HAS_API_VMKAPI_VDS
# define VMK_DEVKIT_HAS_API_VMKAPI_GPU
# define VMK_DEVKIT_HAS_API_VMKAPI_NMI
# define VMK_DEVKIT_HAS_API_VMKAPI_EXPERIMENTAL
# define VMK_DEVKIT_USES_PUBLIC_APIS
# define VMK_DEVKIT_USES_BINARY_COMPATIBLE_APIS
# define VMK_DEVKIT_USES_PRIVATE_APIS
# define VMK_DEVKIT_USES_DEPRECATED_APIS
#endif

/* plus other (e.g. binary-incompatible) vmkapis */
#ifdef VMK_ENABLE_ALL_VMKAPIS
# undef VMK_DEVKIT_USES_BINARY_INCOMPATIBLE_APIS
# undef VMK_DEVKIT_IS_DDK
# define VMK_DEVKIT_USES_BINARY_INCOMPATIBLE_APIS
# define VMK_DEVKIT_IS_DDK
#endif


#if !defined(VMK_ENABLE_ALL_VMKAPIS) && !defined(VMK_ENABLE_ALL_BC_VMKAPIS)
/*
 * Don't emit warnings for consumers who are allowed to use these APIs.
 */
#ifdef VMK_DEVKIT_USES_BINARY_INCOMPATIBLE_APIS
# pragma message "WARNING: VMK_DEVKIT_USES_BINARY_INCOMPATIBLE_APIS enabled. The resulting binary will not be compatible with future vSphere versions!"
#endif

#ifdef VMK_DEVKIT_USES_DEPRECATED_APIS
# pragma message "WARNING: VMK_DEVKIT_USES_DEPRECATED_APIS enabled. This module is using deprecated interfaces that may be removed in future API releases!"
#endif
#endif


/*
 * Allow vmkapi headers to be included when pulled in from
 * this header only. All other cases should cause an error.
 *
 * This should NEVER be used anywhere but this header!
 */
#define VMK_HEADER_INCLUDED_FROM_VMKAPI_H

/** \endcond never */

/*
 ***********************************************************************
 * Devkit-based Header Inclusions.
 ***********************************************************************
 */

/*
 * Base
 */
#ifdef VMK_DEVKIT_HAS_API_VMKAPI_BASE

#if defined(VMK_DEVKIT_USES_PUBLIC_APIS) && \
    defined(VMK_DEVKIT_USES_BINARY_COMPATIBLE_APIS)

#if (defined(VMK_BUILDING_FOR_KERNEL_MODE) || \
     defined(VMK_BUILDING_FOR_USER_MODE))
#include "lib/vmkapi_compiler.h"
#include "lib/vmkapi_const.h"
#include "base/vmkapi_status.h"
#include "lib/vmkapi_types.h"
#include "lib/vmkapi_util.h"
#include "base/vmkapi_module_const.h"
#include "base/vmkapi_worldlet_types.h"
#include "base/vmkapi_module_types.h"
#include "base/vmkapi_assert.h"
#include "lib/vmkapi_list.h"
#include "lib/vmkapi_name.h"
#include "base/vmkapi_mgmt_types.h"
#endif

#if defined(VMK_BUILDING_FOR_USER_MODE)
#include "base/vmkapi_mgmt_lib.h"
#endif

#if defined(VMK_BUILDING_FOR_KERNEL_MODE)
#include "core/vmkapi_cpu.h"
#include "platform/vmkapi_platform.h"
#include "platform/x86/vmkapi_pagetable.h"
#include "lib/vmkapi_lib.h"
#include "lib/vmkapi_bits.h"
#include "lib/vmkapi_libc.h"
#include "core/vmkapi_memdefs.h"
#include "lib/vmkapi_parse.h"
#include "lib/vmkapi_revision.h"
#include "lib/vmkapi_slist.h"
#include "lib/vmkapi_cslist.h"
#include "core/vmkapi_device_types.h"
#include "platform/x86/vmkapi_ioport_types.h"
#include "platform/x86/vmkapi_ioresource.h"
#include "platform/x86/vmkapi_ioport.h"
#include "core/vmkapi_core.h"
#include "core/vmkapi_machine_memory.h"
#include "core/vmkapi_mapping.h"
#include "core/vmkapi_largespace.h"
#include "core/vmkapi_mempool.h"
#include "base/vmkapi_heap.h"
#include "lib/vmkapi_string.h"
#include "core/vmkapi_bus.h"
#include "core/vmkapi_device.h"
#include "core/vmkapi_driver.h"
#include "core/vmkapi_logical_bus.h"
#include "core/vmkapi_scatter_gather.h"
#include "core/vmkapi_scatter_gather_virt.h"
#include "core/vmkapi_dma.h"
#include "core/vmkapi_lock_domain.h"
#include "core/vmkapi_spinlock.h"
#include "base/vmkapi_platform.h"
#include "base/vmkapi_atomic.h"
#include "base/vmkapi_versioned_atomic.h"
#endif

#if (defined(VMK_BUILDING_FOR_KERNEL_MODE) || \
     defined(VMK_BUILDING_FOR_USER_MODE))
#include "base/vmkapi_world.h"
#endif

#if defined(VMK_BUILDING_FOR_KERNEL_MODE)
#include "core/vmkapi_user.h"
#include "core/vmkapi_accounting.h"
#include "base/vmkapi_char.h"
#include "base/vmkapi_config.h"
#include "base/vmkapi_helper.h"
#include "base/vmkapi_logging.h"
#include "core/vmkapi_pcpustorage.h"
#include "base/vmkapi_sem.h"
#include "base/vmkapi_slab.h"
#include "base/vmkapi_stress.h"
#include "base/vmkapi_system.h"
#include "base/vmkapi_time.h"
#include "base/vmkapi_module.h"
#include "base/vmkapi_mgmt.h"
#include "core/vmkapi_worldstorage.h"
#include "system_health/vmkapi_system_health.h"
#include "system_health/vmkapi_vob.h"
#include "core/vmkapi_vmuuid.h"
#include "lib/vmkapi_hash.h"
#include "lib/vmkapi_priq.h"
#include "core/vmkapi_dump.h"
#include "core/vmkapi_pageslab.h"
#include "lib/vmkapi_bitvector.h"

#if !defined(VMK_DEVKIT_HAS_NATIVE_DDK)
#include "base/vmkapi_char_legacy.h"
#endif

#endif
#endif

#if defined(VMK_DEVKIT_USES_PUBLIC_APIS) && \
    defined(VMK_DEVKIT_USES_DEPRECATED_APIS)

#if defined(VMK_BUILDING_FOR_KERNEL_MODE)

#include "core/vmkapi_spinlock_deprecated.h"
#include "base/vmkapi_world_deprecated.h"

#endif

#endif

#if defined(VMK_DEVKIT_USES_PUBLIC_APIS) && \
    defined(VMK_DEVKIT_USES_BINARY_INCOMPATIBLE_APIS)

#if defined(VMK_BUILDING_FOR_KERNEL_MODE)

#include "base/vmkapi_module_incompat.h"
#include "platform/vmkapi_platform_incompat.h"

#if !defined(VMK_DEVKIT_HAS_NATIVE_DDK)
#include "base/vmkapi_lock.h"
#include "core/vmkapi_bh.h"
#include "core/vmkapi_bus_incompat.h"
#include "core/vmkapi_context_incompat.h"
#include "core/vmkapi_cpu_incompat.h"
#include "core/vmkapi_dma_incompat.h"
#include "base/vmkapi_worldlet.h"
#include "base/vmkapi_entropy.h"
#include "base/vmkapi_proc.h"
#include "base/vmkapi_time_incompat.h"
#include "core/vmkapi_affinitymask_incompat.h"
#include "base/vmkapi_world_incompat.h"
#include "base/vmkapi_heap_incompat.h"
#include "core/vmkapi_pageslab_incompat.h"
#include "core/vmkapi_preempt_incompat.h"
#endif
#endif

#endif

#if defined(VMK_DEVKIT_USES_PRIVATE_APIS) && \
    defined(VMK_DEVKIT_USES_BINARY_INCOMPATIBLE_APIS)

#if defined(VMK_BUILDING_FOR_KERNEL_MODE)
#include "core/vmkapi_device_incompat.h"
#endif

#endif

#endif

/*
 * Core
 */
#if defined(VMK_DEVKIT_HAS_API_VMKAPI_CORE)

#if defined(VMK_DEVKIT_USES_PRIVATE_APIS) && \
    defined(VMK_DEVKIT_USES_BINARY_COMPATIBLE_APIS)

#if defined(VMK_BUILDING_FOR_KERNEL_MODE)
#include "base/vmkapi_stress_priv.h"
#endif

#endif

#endif

/*
 * NMI
 */
#if defined(VMK_DEVKIT_HAS_API_VMKAPI_NMI)

#if defined(VMK_DEVKIT_USES_PRIVATE_APIS) && \
    defined(VMK_DEVKIT_USES_BINARY_COMPATIBLE_APIS)

#if defined(VMK_BUILDING_FOR_KERNEL_MODE)
#include "core/vmkapi_nmi.h"
#endif

#endif

#endif

/*
 * Device
 */
#if defined(VMK_DEVKIT_HAS_API_VMKAPI_DEVICE) ||                                \
    defined(VMK_DEVKIT_HAS_API_VMKAPI_SCSI) ||                                  \
    defined(VMK_DEVKIT_HAS_API_VMKAPI_NET)

#if defined(VMK_DEVKIT_USES_PUBLIC_APIS) &&                                     \
    defined(VMK_DEVKIT_USES_BINARY_COMPATIBLE_APIS)

#if defined(VMK_BUILDING_FOR_KERNEL_MODE)
#include "hardware/vmkapi_pci_types.h"
#include "hardware/vmkapi_intr_types.h"
#include "hardware/vmkapi_psa_types.h"
#include "hardware/vmkapi_pci.h"
#include "hardware/vmkapi_intr.h"
#endif

#endif

#if defined(VMK_DEVKIT_USES_PUBLIC_APIS) &&                                     \
    defined(VMK_DEVKIT_USES_BINARY_INCOMPATIBLE_APIS) && \
    !defined(VMK_DEVKIT_HAS_NATIVE_DDK)

#if defined(VMK_BUILDING_FOR_KERNEL_MODE)
#include "hardware/vmkapi_acpi.h"
#include "hardware/vmkapi_isa.h"
#include "hardware/vmkapi_input.h"
#include "hardware/vmkapi_pci_incompat.h"
#include "hardware/vmkapi_intr_incompat.h"
#endif

#endif

#endif

/*
 * DVFilter
 */
#ifdef VMK_DEVKIT_HAS_API_VMKAPI_DVFILTER

#if defined(VMK_DEVKIT_USES_PUBLIC_APIS) && \
    defined(VMK_DEVKIT_USES_BINARY_COMPATIBLE_APIS)

#if (defined(VMK_BUILDING_FOR_KERNEL_MODE) && \
     !defined(VMK_DEVKIT_HAS_NATIVE_DDK))
#include "dvfilter/vmkapi_dvfilter.h"
#endif

#endif

#endif

/*
 * VDS
 */
#ifdef VMK_DEVKIT_HAS_API_VMKAPI_VDS

#if defined(VMK_DEVKIT_USES_PUBLIC_APIS) && \
    defined(VMK_DEVKIT_USES_BINARY_COMPATIBLE_APIS)


#if ((defined(VMK_BUILDING_FOR_KERNEL_MODE) || \
      defined(VMK_BUILDING_FOR_USER_MODE)) && \
      !defined(VMK_DEVKIT_HAS_NATIVE_DDK))
#include "vds/vmkapi_vds_respools.h"
#include "vds/vmkapi_vds_prop.h"
#endif

#if (defined(VMK_BUILDING_FOR_KERNEL_MODE) && \
     !defined(VMK_DEVKIT_HAS_NATIVE_DDK))
#include "net/vmkapi_net_port.h"
#include "net/vmkapi_net_portset.h"
#include "net/vmkapi_net_uplink_service.h"
#include "vds/vmkapi_vds_portset.h"
#include "vds/vmkapi_vds_prop.h"

#if defined(VMK_DEVKIT_USES_DEPRECATED_APIS)
#include "net/vmkapi_net_port_deprecated.h"
#include "net/vmkapi_net_portset_deprecated.h"
#endif

#endif

#endif

#if defined(VMK_DEVKIT_USES_VDS_PRIVATE_APIS)

#if defined(VMK_BUILDING_FOR_KERNEL_MODE)
#include "hardware/vmkapi_pci.h"
#include "hardware/vmkapi_intr.h"
#endif

#endif

#if defined(VMK_DEVKIT_USES_PRIVATE_APIS) && \
    defined(VMK_DEVKIT_USES_BINARY_INCOMPATIBLE_APIS)

#if defined(VMK_BUILDING_FOR_KERNEL_MODE)
#include "vds/vmkapi_vds_lldp.h"
#include "vds/vmkapi_vds_port_incompat.h"
#endif

#endif

#if defined(VMK_DEVKIT_USES_PRIVATE_APIS) && \
    defined(VMK_DEVKIT_USES_BINARY_COMPATIBLE_APIS)

#if (defined(VMK_BUILDING_FOR_KERNEL_MODE) && \
     !defined(VMK_DEVKIT_HAS_NATIVE_DDK))
#include "vds/vmkapi_vds_port_priv.h"
#endif

#endif

#endif

/*
 * Net
 */
#ifdef VMK_DEVKIT_HAS_API_VMKAPI_NET

#if defined(VMK_DEVKIT_USES_PUBLIC_APIS) && \
    defined(VMK_DEVKIT_USES_BINARY_COMPATIBLE_APIS)

#if defined(VMK_BUILDING_FOR_KERNEL_MODE)
#include "net/vmkapi_net_types.h"
#include "net/vmkapi_net_eth.h"
#include "net/vmkapi_net_pkt.h"
#include "net/vmkapi_net_pktlist.h"
#include "net/vmkapi_net_pt.h"
#include "net/vmkapi_net_dcb.h"
#include "net/vmkapi_net_poll.h"
#include "net/vmkapi_net_vlan.h"
#include "net/vmkapi_net_queue.h"
#include "net/vmkapi_net_csum.h"
#include "net/vmkapi_net_dump.h"
#include "net/vmkapi_net_uplink.h"
#include "net/vmkapi_net_pkt_header.h"
#include "net/vmkapi_net_proto_misc.h"
#include "net/vmkapi_net_pkt_capture.h"

#if defined(VMK_DEVKIT_USES_DEPRECATED_APIS)
#include "net/vmkapi_net_pkt_deprecated.h"
#include "net/vmkapi_net_pkt_header_deprecated.h"
#endif

#if !defined(VMK_DEVKIT_HAS_NATIVE_DDK)
#include "net/vmkapi_net_portset_message.h"
#endif

#endif

#if (defined(VMK_BUILDING_FOR_KERNEL_MODE) || \
     defined(VMK_BUILDING_FOR_USER_MODE))
#include "net/vmkapi_net_proto.h"
#endif

#endif

#if defined(VMK_DEVKIT_USES_PUBLIC_APIS) && \
    defined(VMK_DEVKIT_USES_BINARY_INCOMPATIBLE_APIS)

#if defined(VMK_BUILDING_FOR_KERNEL_MODE)
#include "net/vmkapi_net_queue_incompat.h"
#include "net/vmkapi_net_uplink_incompat.h"
#include "net/vmkapi_net_shaper_incompat.h"
#include "net/vmkapi_net_pkt_incompat.h"
#include "net/vmkapi_net_dcb_incompat.h"
#include "net/vmkapi_net_port_incompat.h"
#endif

#endif


#if defined(VMK_DEVKIT_USES_PRIVATE_APIS) && \
    defined(VMK_DEVKIT_USES_BINARY_COMPATIBLE_APIS)

#if defined(VMK_BUILDING_FOR_KERNEL_MODE)
#include "net/vmkapi_net_ncs_priv.h"
#include "net/vmkapi_net_iochain_priv.h"
#include "net/vmkapi_net_pkt_priv.h"
#include "net/vmkapi_net_port_priv.h"
#include "net/vmkapi_net_vnic_priv.h"
#include "net/vmkapi_net_lacp_priv.h"
#include "net/vmkapi_net_tcpip_priv.h"
#include "net/vmkapi_net_event_priv.h"
#include "net/vmkapi_net_portset_priv.h"
#include "net/vmkapi_net_opaque_network_priv.h"
#include "net/vmkapi_net_uplink_priv.h"
#include "net/vmkapi_net_pkt_vlan_priv.h"
#include "net/vmkapi_net_uplink_service_priv.h"
#include "net/vmkapi_net_pt_priv.h"
#include "net/vmkapi_net_team_priv.h"
#include "net/vmkapi_net_overlay_priv.h"
#include "net/vmkapi_net_pktlist_priv.h"
#endif

#if (defined(VMK_BUILDING_FOR_KERNEL_MODE) || \
     defined(VMK_BUILDING_FOR_USER_MODE))
#include "net/vmkapi_net_overlay_ext.h"
#endif

#endif


#if defined(VMK_DEVKIT_USES_PRIVATE_APIS) && \
    defined(VMK_DEVKIT_USES_BINARY_INCOMPATIBLE_APIS)

#if defined(VMK_BUILDING_FOR_KERNEL_MODE)
#include "net/vmkapi_net_portset_message_incompat.h"
#include "net/vmkapi_net_team_incompat.h"
#include "net/vmkapi_net_portset_incompat.h"
#endif

#endif

#endif

/*
 * iSCSI Transport APIs
 */
#ifdef VMK_DEVKIT_HAS_API_VMKAPI_ISCSI

#if defined(VMK_DEVKIT_USES_PUBLIC_APIS) && \
    defined(VMK_DEVKIT_USES_BINARY_COMPATIBLE_APIS)

#if defined(VMK_BUILDING_FOR_KERNEL_MODE)
#include "iscsi/vmkapi_iscsi_transport_compat.h"
#endif

#endif

#if defined(VMK_DEVKIT_USES_PUBLIC_APIS) && \
    defined(VMK_DEVKIT_USES_BINARY_INCOMPATIBLE_APIS)

#if defined(VMK_BUILDING_FOR_KERNEL_MODE)
#include "iscsi/vmkapi_iscsi_transport_incompat.h"
#endif

#endif

#endif

/*
 * Sockets
 */
#ifdef VMK_DEVKIT_HAS_API_VMKAPI_SOCKETS

#if defined(VMK_DEVKIT_USES_PUBLIC_APIS) && \
    defined(VMK_DEVKIT_USES_BINARY_COMPATIBLE_APIS)

#if defined(VMK_BUILDING_FOR_KERNEL_MODE)
#include "sockets/vmkapi_socket_util.h"
#include "sockets/vmkapi_socket.h"
#include "sockets/vmkapi_socket_ip.h"
#endif

#endif

#if defined(VMK_DEVKIT_USES_PUBLIC_APIS) && \
    defined(VMK_DEVKIT_USES_BINARY_INCOMPATIBLE_APIS)

#if defined(VMK_BUILDING_FOR_KERNEL_MODE)
#include "sockets/vmkapi_socket_ip6.h"
#include "sockets/vmkapi_socket_vmklink.h"
#endif

#endif

#if defined(VMK_DEVKIT_USES_PRIVATE_APIS) && \
    defined(VMK_DEVKIT_USES_BINARY_COMPATIBLE_APIS)

#if defined(VMK_BUILDING_FOR_KERNEL_MODE)
#include "sockets/vmkapi_socket_priv.h"
#endif

#endif
#endif


/*
 * SCSI
 */
#ifdef VMK_DEVKIT_HAS_API_VMKAPI_SCSI

#if defined(VMK_DEVKIT_USES_PUBLIC_APIS) && \
    defined(VMK_DEVKIT_USES_BINARY_COMPATIBLE_APIS)

#if defined(VMK_BUILDING_FOR_KERNEL_MODE) || \
    defined(VMK_BUILDING_FOR_USER_MODE)
#include "scsi/vmkapi_scsi_ext.h"
#endif

#if defined(VMK_BUILDING_FOR_KERNEL_MODE)
#include "scsi/vmkapi_scsi.h"
#include "scsi/vmkapi_scsi_mgmt_types.h"
#include "scsi/vmkapi_scsi_device.h"
#include "scsi/vmkapi_scsi_vmware.h"
#include "scsi/vmkapi_scsi_iodm.h"
#endif

#endif

#if defined(VMK_DEVKIT_USES_PUBLIC_APIS) && \
    defined(VMK_DEVKIT_USES_BINARY_INCOMPATIBLE_APIS) && \
    !defined(VMK_DEVKIT_HAS_NATIVE_DDK)

#if defined(VMK_BUILDING_FOR_KERNEL_MODE)
#include "scsi/vmkapi_scsi_incompat.h"
#endif

#endif

#endif

/*
 * SYSTEM_HEALTH
 */
#ifdef VMK_DEVKIT_HAS_API_VMKAPI_SYSTEM_HEALTH

#if defined(VMK_DEVKIT_USES_PUBLIC_APIS) && \
    defined(VMK_DEVKIT_USES_BINARY_COMPATIBLE_APIS)

#if defined(VMK_BUILDING_FOR_KERNEL_MODE) || \
    defined(VMK_BUILDING_FOR_USER_MODE)
#include "system_health/vmkapi_system_health.h"
#include "system_health/vmkapi_vob.h"
#endif

#endif

#endif

/*
 * MPP
 */
#ifdef VMK_DEVKIT_HAS_API_VMKAPI_MPP

#if defined(VMK_DEVKIT_USES_PUBLIC_APIS) && \
    defined(VMK_DEVKIT_USES_BINARY_COMPATIBLE_APIS)

#if defined(VMK_BUILDING_FOR_KERNEL_MODE)
#include "mpp/vmkapi_mpp.h"
#endif

#endif

#endif

/*
 * NMP
 */
#ifdef VMK_DEVKIT_HAS_API_VMKAPI_NMP

#if defined(VMK_DEVKIT_USES_PUBLIC_APIS) && \
    defined(VMK_DEVKIT_USES_BINARY_COMPATIBLE_APIS)

#if defined(VMK_BUILDING_FOR_KERNEL_MODE)
#include "nmp/vmkapi_nmp.h"
#include "nmp/vmkapi_nmp_psp.h"
#include "nmp/vmkapi_nmp_satp.h"
#endif

#endif

#endif

/*
 * NPIV
 */
#ifdef VMK_DEVKIT_HAS_API_VMKAPI_NPIV

#if defined(VMK_DEVKIT_USES_PUBLIC_APIS) && \
    defined(VMK_DEVKIT_USES_BINARY_COMPATIBLE_APIS)

#if (defined(VMK_BUILDING_FOR_KERNEL_MODE) || \
     defined(VMK_BUILDING_FOR_USER_MODE))
#include "npiv/vmkapi_npiv_wwn.h"
#endif

#if defined(VMK_BUILDING_FOR_KERNEL_MODE)
#include "npiv/vmkapi_scsi_vport.h"
#endif

#endif

#if defined(VMK_DEVKIT_USES_PUBLIC_APIS) && \
    defined(VMK_DEVKIT_USES_BINARY_INCOMPATIBLE_APIS)

#if defined(VMK_BUILDING_FOR_KERNEL_MODE)
#include "npiv/vmkapi_npiv.h"
#endif

#endif

#endif

/*
 * RDMA
 */
#ifdef VMK_DEVKIT_HAS_API_VMKAPI_RDMA

#if defined(VMK_DEVKIT_USES_PUBLIC_APIS) && \
    defined(VMK_DEVKIT_USES_BINARY_COMPATIBLE_APIS)

#if defined(VMK_BUILDING_FOR_KERNEL_MODE)
#include "rdma/vmkapi_rdma_types.h"
#include "rdma/vmkapi_rdma_client.h"
#include "rdma/vmkapi_rdma_device.h"
#endif

#endif

#endif

/*
 * Experimental
 */
#ifdef VMK_DEVKIT_HAS_API_VMKAPI_EXPERIMENTAL

#if defined(VMK_DEVKIT_USES_PRIVATE_APIS) && \
    defined(VMK_DEVKIT_USES_BINARY_INCOMPATIBLE_APIS)

#if defined(VMK_BUILDING_FOR_KERNEL_MODE)
#include "experimental/base/vmkapi_world_exp.h"
#endif

#endif

#endif

/*
 ***********************************************************************
 * "Top Level" documentation groups.
 *
 * These are documentation groups that don't necessarily have a single
 * file that they can logically reside in. Most doc groups can simply
 * be defined in a specific header.
 *
 ***********************************************************************
 */


#if defined(VMK_DEVKIT_HAS_API_VMKAPI_SCSI) || \
    defined(VMK_DEVKIT_HAS_API_VMKAPI_MPP) || \
    defined(VMK_DEVKIT_HAS_API_VMKAPI_NMP) || \
    defined(VMK_DEVKIT_HAS_API_VMKAPI_NPIV)
/**
 * \defgroup Storage Storage
 * \{ \}
 */
#endif

#ifdef VMK_DEVKIT_HAS_API_VMKAPI_NET
/**
 * \defgroup Network Network
 * \{ \}
 */
#endif

/*
 ***********************************************************************
 * API Version information.
 ***********************************************************************
 */
/** \cond never */
#define VMKAPI_REVISION_MAJOR       2
#define VMKAPI_REVISION_MINOR       3
#define VMKAPI_REVISION_UPDATE      0
#define VMKAPI_REVISION_PATCH_LEVEL 0

#define VMKAPI_REVISION  VMK_REVISION_NUMBER(VMKAPI)
/** \endcond never */

/*
 * Don't allow vmkapis to be included outside of vmkapi.h
 *
 * Don't include any further vmkapi headers in this file after this point.
 */
/** \cond never */
#undef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
/** \endcond never */

#if defined(__cplusplus)
}
#endif

#endif /* _VMKAPI_H_ */
