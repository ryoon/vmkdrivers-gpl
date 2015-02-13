/* **********************************************************
 * Copyright 2006 - 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Network Types                                                  */ /**
 * \addtogroup Network
 *@{
 ***********************************************************************
 */
#ifndef _VMKAPI_NET_TYPES_H_
#define _VMKAPI_NET_TYPES_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/*
 ***********************************************************************
 * Vlan                                                           */ /**
 * \defgroup Vlan Virtual Lan
 *@{
 ***********************************************************************
 */

/**
 * \brief Identifier number for vlan group.
 */

typedef vmk_uint16        vmk_VlanID;

/**
 * \brief 802.1p priority value for vlan tag.
 */

typedef vmk_uint8        vmk_VlanPriority;

/*
 * \brief Symbolic names for 802.1p priority values.
 *
 * Following the published version in 802.1Q-2005 Annex G.
 * Actual ranking in order of least to most important is not strictly numerical.
 * Priority 0 is the default priority and is ranked specially.
 * The ranking, from least to most important, is
 *    1,  0,  2,  3,  4,  5,  6,  7
 * or, using the corresponding 2-letter acronyms from 802.1Q,
 *    BK, BE, EE, CA, VI, VO, IC, NC
 */
enum {
   VMK_VLAN_PRIORITY_MINIMUM = 0,

   VMK_VLAN_PRIORITY_BE = VMK_VLAN_PRIORITY_MINIMUM,
   VMK_VLAN_PRIORITY_BEST_EFFORT = VMK_VLAN_PRIORITY_BE,

   VMK_VLAN_PRIORITY_BK = 1,
   VMK_VLAN_PRIORITY_BACKGROUND = VMK_VLAN_PRIORITY_BK,

   VMK_VLAN_PRIORITY_EE = 2,
   VMK_VLAN_PRIORITY_EXCELLENT_EFFORT = VMK_VLAN_PRIORITY_EE,

   VMK_VLAN_PRIORITY_CA = 3,
   VMK_VLAN_PRIORITY_CRITICAL_APPS = VMK_VLAN_PRIORITY_CA,

   VMK_VLAN_PRIORITY_VI = 4,
   VMK_VLAN_PRIORITY_VIDEO = VMK_VLAN_PRIORITY_VI,

   VMK_VLAN_PRIORITY_VO = 5,
   VMK_VLAN_PRIORITY_VOICE = VMK_VLAN_PRIORITY_VO,

   VMK_VLAN_PRIORITY_IC = 6,
   VMK_VLAN_PRIORITY_INTERNETWORK_CONROL = VMK_VLAN_PRIORITY_IC,

   VMK_VLAN_PRIORITY_NC = 7,
   VMK_VLAN_PRIORITY_NETWORK_CONROL = VMK_VLAN_PRIORITY_NC,

   VMK_VLAN_PRIORITY_MAXIMUM = VMK_VLAN_PRIORITY_NC,

   VMK_VLAN_NUM_PRIORITIES = VMK_VLAN_PRIORITY_MAXIMUM+1,

   VMK_VLAN_PRIORITY_INVALID = (vmk_VlanPriority)~0U
};

/**
 * \ingroup Passthru
 * \brief Enumeration of passthru types.
 */
typedef enum vmk_NetPTType {
   VMK_PT_UPT  = 1,
   VMK_PT_CDPT = 2,
} vmk_NetPTType;

/**
 * \brief Ethernet address length
 */
#define VMK_ETH_ADDR_LENGTH           6

/**
 * \brief Ethernet address type
 */
typedef vmk_uint8 vmk_EthAddress[VMK_ETH_ADDR_LENGTH];

/** Invalid identification number for a port */
#define VMK_VSWITCH_INVALID_PORT_ID 0

/**
 * \brief Identifier number for port on a virtual switch.
 */
typedef vmk_uint32 vmk_SwitchPortID;

/**
 * \brief Tag associated to a resource pool.
 */
typedef vmk_uint16 vmk_PortsetResPoolTag;

/**
 * \brief Default resource pool tag.
 */
#define VMK_PORTSET_DEFAULT_RESPOOL_TAG 0

/**
 * \brief Invalid resource pool tag.
 */
#define VMK_PORTSET_INVALID_RESPOOL_TAG ((vmk_PortsetResPoolTag)-1)

#endif /* _VMKAPI_NET_TYPES_H_ */
/** @} */
/** @} */
