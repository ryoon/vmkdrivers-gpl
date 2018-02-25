/***************************************************************************
 * Copyright 2009 VMware, Inc.  All rights reserved.
 ***************************************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * SCSI binary incompatible interfaces                            */ /**
 * \addtogroup SCSI
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_SCSI_INCOMPAT_H_
#define _VMKAPI_SCSI_INCOMPAT_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "mpp/vmkapi_mpp_types.h"


/*
 ***********************************************************************
 * vmk_ScsiScanDeleteAdapterPaths --                              */ /**
 *
 * \ingroup SCSI
 *
 * \brief Delete all dead physical paths originating from the adapter.
 *
 * The paths that are missing are automatically unregistered from the
 * storage stack. If any paths are left, the adapter can not be
 * removed.
 *
 * The sparse luns, max lun id and lun mask settings affect which
 * paths on this adapter are actually scanned.
 *
 * \note If this routine returns an error, some paths may have been
 *       sucessfully deleted.
 * \note This function may block.
 *
 * \param[in] vmkAdapter  Pointer to the adapter to remove
 *                        all dead paths
 *
 * \retval VMK_BUSY     The requested adapter is currently being 
 *                      scanned by some other context.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_ScsiScanDeleteAdapterPaths(
   vmk_ScsiAdapter *vmkAdapter);

/**
 ***********************************************************************
 *                                                                */ /**
 * \brief PSA Filter plugin vmk_ScsiPlugin specific data & operations.
 * 
 *  vmk_ScsiPlugin data for plugin type VMK_SCSI_PLUGIN_TYPE_FILTER.
 *
 ***********************************************************************
 */
typedef struct vmk_ScsiFilterPluginSpecific {
   /**
    * \brief Filter priority
    *
    */
   vmk_ScsiPluginPriority priority;

   /**
    * \brief Offer a device to the plugin for claiming
    *
    *   Give the plugin a chance to claim a device. The plugin is the
    *   owner of the device for the duration of the call; To retain
    *   ownership of the device beyond that call, the plugin must return
    *   VMK_OK.
    *
    *   If it decides not to claim the device, the plugin must drain all
    *   the IOs it has issued to the device and return a failure code.
    *  
    *   \param[in]  device Scsi device to claim
    *   \param[out] filterPrivateData filter's private data
    *   associated with the claimed device. This data will be
    *   passed in ->notify(), ->ioctl() and ->unclaim() calls
    */
   VMK_ReturnStatus (*claim)(vmk_ScsiDevice *device, void **filterPrivateData);
   /**
    * \brief Tell the plugin to unclaim a device
    *
    *   Tell the plugin to unclaim the specified device.
    *   The plugin can perform blocking tasks before returning.
    */
   void (*unclaim)(void *filterPrivateData);
   /**
    * \brief Notify the plugin of an issued I/O
    *
    *   Tells the plugin about an issued I/O and allows it to provide a
    *   replacement I/O.
    */
   VMK_ReturnStatus (*notify)(void *filterPrivateData,
                              vmk_ScsiCommand *vmkCmd,
                              vmk_ScsiCommand **replacement);

   /**
    * \brief Issue a device ioctl request
    *
    */
   VMK_ReturnStatus (*ioctl)(void *filterPrivateData,
                             vmk_ScsiPluginIoctl cmd,
                             vmk_ScsiPluginIoctlData *data);
} vmk_ScsiFilterPluginSpecific;

VMK_ASSERT_LIST(FILTER,
                VMK_ASSERT_ON_COMPILE(sizeof(vmk_ScsiFilterPluginSpecific) <=
                                      sizeof(((vmk_ScsiPlugin*)(0))->u));
)

#endif  /* _VMKAPI_SCSI_INCOMPAT_H_ */
/** @} */

