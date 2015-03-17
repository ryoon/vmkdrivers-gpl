/* **********************************************************
 * Copyright 2012 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 * vmkapi_scsi_vport.h --
 *
 *    Defines the vmkernel specific virtual port API used
 *    to interact with NPIV VPORT aware native drivers.
 */

#ifndef _VMKAPI_SCSI_VPORT_H
#define _VMKAPI_SCSI_VPORT_H

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "npiv/vmkapi_npiv_wwn.h"

/*
 ***********************************************************************
 * vmk_ScsiVportCreate --                                         */ /**
 *
 * \brief Create a virtual port on the physical device indicated.
 *
 * The driver should create a virtual port representation on the
 * physical device passed using the world wide name pair passed
 * in wwpn and wwnn. The vmk_ScsiAdapter created to represent the
 * vport is passed back in vAdapter.
 *
 * \note This function is allowed to block in the driver.
 *
 * \param[in]  device	Handle to physical device the virtual port
 *                      should be created on.
 * \param[in]  wwpn     Pointer to the world wide port name of the
 *                      virtual port to be created.
 * \param[in]  wwnn     Pointer to the world wide node name of the
 *                      virtual port to be created.
 * \param[out] vAdapter	Handle to the vmk_ScsiAdapter created on
 *                      the physical device for the vport.
 *
 * \retval VMK_BAD_PARAM   A parameter or the device is not valid.
 * \retval VMK_NO_MEMORY   Unable to allocate memory for vport handle.
 * \retval VMK_OK          Success
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_ScsiVportCreate)(vmk_Device device,
                                                vmk_VportWwn *wwpn,
                                                vmk_VportWwn *wwnn,
                                                vmk_ScsiAdapter **vAdapter);

/*
 ***********************************************************************
 * vmk_ScsiVportDelete --                                         */ /**
 *
 * \brief Delete the virtual port passed from a physical device.
 *
 * The driver should stop driving this virtual port, and release
 * its resources. The vmk_ScsiAdapter vAdapter should be deallocated.
 *
 * \note The vAdapter passed has already been quiesced at the PSA
 *       layer and is not used by PSA for IOs.
 *
 * \note This function is allowed to block in the driver.
 *
 * \param[in]  device	Handle to physical device the virtual port was
 *                      created on.
 * \param[in]  vAdapter	Handle to the vmk_ScsiAdapter to be removed
 *                      from the physical device.
 *
 * \retval VMK_BAD_PARAM   A parameter or the device is not valid.
 * \retval VMK_OK          Success
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_ScsiVportDelete)(vmk_Device device,
                                                vmk_ScsiAdapter *vAdapter);

/*
 ***********************************************************************
 * vmk_ScsiVportGetInfo --                                        */ /**
 *
 * \brief Return information about virtual ports on the physical device.
 *
 * Return information about how many virtual ports this physical
 * device can support and how many are in use currently.
 *
 * \note This function is allowed to block in the driver.
 *
 * \param[in]  device	      Handle to physical device to get the info from.
 * \param[out] vports_max     Return maximum number of vports allowed.
 * \param[out] vports_inuse   Return how many of the vports are in use.
 *
 * \retval VMK_OK       Success
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_ScsiVportGetInfo)(vmk_Device device,
                                                 vmk_uint32 *vports_max,
                                                 vmk_uint32 *vports_inuse);

/**
 * \brief Vport Operations
 */
typedef struct vmk_ScsiVportOps {
   /** \brief Create a virtual port */
   vmk_ScsiVportCreate createVport;
   /** \brief Delete a virtual port */
   vmk_ScsiVportDelete deleteVport;
   /** \brief Get virtual port information from physical device */
   vmk_ScsiVportGetInfo getVportInfo;
} vmk_ScsiVportOps;


/*
 ***********************************************************************
 * vmk_ScsiRegisterVportOps --                                    */ /**
 *
 * \brief Register Vport operations for a device.
 *
 * Register virtual port operations a physical device can perform.
 * If VportOps are registered, it is assumed the device can create
 * and delete virtual ports and return information about virtual ports.
 * If VportOps are not registered, it is assumed the device is not
 * NPIV vport capable.
 *
 * \note This function will not block.
 *
 * \param[in]  device	Handle to logical device the VportOps should
 *                      be registered on.
 * \param[in]  ops      VportOps registration data.
 *
 * \retval VMK_BAD_PARAM   Device or ops argument is NULL or device
 *                         is not fully registered yet.
 * \retval VMK_NO_MEMORY   No memory to register.
 * \retval VMK_EXISTS      Already registered.
 * \retval VMK_OK          Successfully registered VportOps.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_ScsiRegisterVportOps(vmk_Device device,
                         vmk_ScsiVportOps *ops);

#endif /* _VMKAPI_SCSI_VPORT_H */
