/* **********************************************************
 * Copyright 2006 - 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * VLAN                                                          */ /**
 * \addtogroup Network
 *@{
 * \defgroup VLAN VLAN Tag Management
 *@{
 *
 * VLANs (IEEE 802.1Q) provide for logical groupings of stations or
 * switch ports, allowing communications as if all stations or ports
 * were on the same physical LAN segment. 
 * 
 ***********************************************************************
 */
#ifndef _VMKAPI_NET_VLAN_H_
#define _VMKAPI_NET_VLAN_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief Largest valid VLAN ID
 */
#define VMK_VLAN_ID_MAX 4094

/**
 * \brief VLAN ID reserved by vmkernel to denote Virtual Machine Guest
 *        Tagging (VGT Mode)
 */
#define VMK_VLAN_ID_GUEST_TAGGING (VMK_VLAN_ID_MAX + 1)

/**
 * \brief VLAN Bitmap array size
 */
#define VMK_VLAN_BITMAP_ARRAY_SIZE 64

/**
 * \brief Bitmap stores enablement information of 4096 VLANs. The 0/1 value
 *        of Xth bit in bitmap indicates whether vlanID X is disabled/enabled.
 *
 * Vmkernel reserves vlanID 4095 to denote Virtual Machine Guest Tagging
 * (VGT Mode). Virtual Switch Tagging (VST) mode supports vlanID ranges
 * from 1 to 4094.
 *
 * \note vmk_VLANBitmap variable must be configured using vmk_VLANBitmap
 *       (Set/Get/Clr/Init) APIs. 
 */
typedef struct vmk_VLANBitmap {
    /** 64 elements * 64 bits/element = 4096 bits. */ 
   vmk_uint64 bits[VMK_VLAN_BITMAP_ARRAY_SIZE];
} vmk_VLANBitmap;

/*
 ***********************************************************************
 * vmk_VLANBitmapSet --                                           */ /**
 *
 * \brief Set vlanID in vlan bitmap.
 *
 * \note Valid vlanID is in range [0, 4095]
 *
 * \param[in]    bitmap          Bitmap that denotes vlan enablement
 * \param[in]    vlanID          Numeric ID of vlan to be set
 *
 * \retval       VMK_OK          ID was set in bitmap.
 * \retval       VMK_BAD_PARAM   vlanID parameter out of range
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_VLANBitmapSet(vmk_VLANBitmap *bitmap,
                                   vmk_VlanID vlanID);

/*
 ***********************************************************************
 * vmk_VLANBitmapGet --                                           */ /** 
 *
 * \brief Get whether vlanID is set in vlan bitmap.
 *
 * \note Valid vlanID is in range [0, 4095]
 *
 * \param[in]    bitmap          Bitmap that denotes vlan enablement
 * \param[in]    vlanID          Numeric ID of vlan
 *
 * \retval       VMK_TRUE        vlanID is set
 * \retval       VMK_FALSE       vlanID is unset
 ***********************************************************************
 */
vmk_Bool vmk_VLANBitmapGet(vmk_VLANBitmap *bitmap,
                           vmk_VlanID vlanID);

/*
 ***********************************************************************
 * vmk_VLANBitmapClr --                                           */ /**
 *
 * \brief Clear vlanID in vlan bitmap.
 *
 * \note Valid vlanID is in rage [0, 4095]
 *
 * \param[in]    bitmap          Bitmap that denotes vlan enablement
 * \param[in]    vlanID          Numeric ID of vlan
 * 
 * \retval       VMK_OK          ID was cleared in bitmap.
 * \retval       VMK_BAD_PARAM   vlanID parameter out of range
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_VLANBitmapClr(vmk_VLANBitmap *bitmap,
                                   vmk_VlanID vlanID);

/*
***********************************************************************
* vmk_VLANBitmapInit --                                          */ /**
*
* \brief Clear all vlanIDs in vlan bitmap.
*
* \param[in]    bitmap          Bitmap that denotes vlan enablement
*
* \retval       VMK_OK
***********************************************************************
*/

VMK_ReturnStatus vmk_VLANBitmapInit(vmk_VLANBitmap *bitmap);

#endif /* _VMKAPI_NET_VLAN_H_ */
/** @} */
/** @} */
