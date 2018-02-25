/***************************************************************************
 * Copyright 2004-2008 VMware, Inc.  All rights reserved.
 ***************************************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * NMP                                                            */ /**
 * \addtogroup Storage
 * @{
 * \defgroup NMP Native Multipathing Interfaces
 * @{
 ***********************************************************************
 */

#ifndef _VMK_NMP_H_
#define _VMK_NMP_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#define VMK_NMP_MODULE_NAME "NMP"
#define VMK_NMP_LOCK_NAME(objectName) VMK_NMP_MODULE_NAME

typedef struct nmp_Device vmk_NmpDevice;
typedef struct nmp_PathGroupInt vmk_NmpPathGroup;

#define VMK_NMP_PATH_GROUP_STATE_MAGIC (('P'<<24)|('G'<<16)|('S'<<8))

/** Path Group states */
#define VMK_NMP_PATH_GROUP_STATES \
   VMK_NMP_PATH_GROUP_STATE_NUM(VMK_NMP_PATH_GROUP_STATE_ACTIVE, \
                             VMK_NMP_PATH_GROUP_STATE_MAGIC,    "active")     \
   VMK_NMP_PATH_GROUP_STATE(VMK_NMP_PATH_GROUP_STATE_ACTIVE_UO,     "active unoptimized")     \
   VMK_NMP_PATH_GROUP_STATE(VMK_NMP_PATH_GROUP_STATE_STANDBY,       "standby")     \
   VMK_NMP_PATH_GROUP_STATE(VMK_NMP_PATH_GROUP_STATE_UNAVAILABLE,   "unavailable") \
   VMK_NMP_PATH_GROUP_STATE(VMK_NMP_PATH_GROUP_STATE_DEAD,          "dead")        \
   VMK_NMP_PATH_GROUP_STATE(VMK_NMP_PATH_GROUP_STATE_OFF,           "off")         \
   VMK_NMP_PATH_GROUP_STATE(VMK_NMP_PATH_GROUP_STATE_PERM_LOSS,     "permanently lost") \


#define VMK_NMP_PATH_GROUP_STATE(name,description) name,
#define VMK_NMP_PATH_GROUP_STATE_NUM(name,value,description) name = value,

/*
 ***********************************************************************
 * vmk_NmpPathGroupState --                                       */ /**
 *
 * \ingroup NMP
 *
 * \brief Represents path group state
 ***********************************************************************
 */

typedef enum {
   VMK_NMP_PATH_GROUP_STATES
   VMK_NMP_PATH_GROUP_STATE_LAST
} vmk_NmpPathGroupState;
#undef VMK_NMP_PATH_GROUP_STATE
#undef VMK_NMP_PATH_GROUP_STATE_NUM

/*
 ***********************************************************************
 * vmk_NmpGetScsiDevice --                                        */ /**
 *
 * \ingroup NMP
 *
 * \brief Returns vmk_ScsiDevice for vmk_NmpDevice.
 *
 * \note This is a non-blocking call.
 *
 * \note Spin locks can be held while calling into this function
 *
 * \param[in] d   NMP device.
 * 
 * \return vmk_ScsiDevice corresponding to \em d.
 * \retval NULL ScsiDevice not created yet. This means that the NMP
 *              device is yet to be registered with the SCSI midlayer.
 *              An NMP device will not be registered with the SCSI
 *              midlayer till the \em pathClaimEnd entry point is
 *              called.
 *
 ***********************************************************************
 */
extern vmk_ScsiDevice *vmk_NmpGetScsiDevice(vmk_NmpDevice *d);

/*
 ***********************************************************************
 * vmk_NmpPathGetPriority --                                      */ /**
 *
 * \ingroup NMP
 *
 * \brief Returns SATP's (e.g. Arrays) path priority for given 
 *        vmk_ScsiPath.
 *
 * \note As setting path priority has no effect, you will get
 *       whatever priority you had set using vmk_NmpPathSetPriority().
 *
 * \note This is a non-blocking call.
 *
 * \note Spin locks can be held while calling into this function
 *
 * \param[in] p  Path to get priority for.
 *
 * \return Numeric priority of the path (higher number is better).
 *
 ***********************************************************************
 */
int vmk_NmpPathGetPriority(vmk_ScsiPath *p);

/*
 ***********************************************************************
 * vmk_NmpPathSetPriority --                                      */ /**
 *
 * \ingroup NMP
 *
 * \brief Sets SATP's (e.g. Arrays) path priority for given vmk_ScsiPath.
 *
 * \note Currently, setting path priority has no effect.
 *
 * \note This is a non-blocking call.
 *
 * \note Spin locks can be held while calling into this function
 *
 * \param[in] p      Path to set priority for.
 * \param[in] pri    Numeric priority to give the path 
 *                   (higher number is better).
 *
 ***********************************************************************
 */
void vmk_NmpPathSetPriority(vmk_ScsiPath *p, int pri);

/*
 ***********************************************************************
 * vmk_NmpPathGetPathGroupState --                                */ /**
 *
 * \ingroup NMP
 *
 * \brief Returns Path Group state of the given path.
 *
 * Path group state for a particular path is updated by the SATPs.
 *
 * \note This is a non-blocking call.
 *
 * \note Spin locks can be held while calling into this function
 *
 * \param[in] p   Path to get group state for.
 *
 ***********************************************************************
 */
vmk_NmpPathGroupState vmk_NmpPathGetPathGroupState(vmk_ScsiPath *p);

/*
 ***********************************************************************
 * vmk_NmpPathGetNext --                                          */ /**
 *
 * \ingroup NMP
 *
 * \brief Path iterator within path group. 
 * 
 * This returns the path next to the current path within a path group.
 *
 * \note This is a non-blocking call.
 *
 * \note Spin locks can be held while calling into this function
 *
 * \param[in] pg  Path Group to iterate within.
 * \param[in] p   Current path. If p is NULL, first path
 *                within the group is returned.
 *
 * \return Next path within the Path Group.
 * \retval NULL   End of the path list for the Path Group.
 *
 ***********************************************************************
 */
vmk_ScsiPath *vmk_NmpPathGetNext(vmk_NmpPathGroup *pg, vmk_ScsiPath *p);

/*
 ***********************************************************************
 * vmk_NmpPathGroupGetState --                                    */ /**
 *
 * \ingroup NMP
 *
 * \brief Returns the state associated with a given path group.
 *
 * \note This is a non-blocking call.
 *
 * \note Spin locks can be held while calling into this function
 *
 * \param[in] pg  Path Group to get state for.
 *
 * \return One of the possible group states associated with the given
 *         path group.
 * \retval  VMK_NMP_PATH_GROUP_STATE_ACTIVE      Active state
 * \retval  VMK_NMP_PATH_GROUP_STATE_ACTIVE_UO   Active Unoptimized state
 * \retval  VMK_NMP_PATH_GROUP_STATE_STANDBY     Standby state
 * \retval  VMK_NMP_PATH_GROUP_STATE_UNAVAILABLE Unavailable state
 * \retval  VMK_NMP_PATH_GROUP_STATE_DEAD        Dead state
 * \retval  VMK_NMP_PATH_GROUP_STATE_OFF         Off state
 *
 ***********************************************************************
 */
vmk_NmpPathGroupState vmk_NmpPathGroupGetState(vmk_NmpPathGroup *pg);

/*
 ***********************************************************************
 * vmk_NmpPathGroupStateToString --                               */ /** 
 *
 * \ingroup NMP
 *
 * \brief Convert vmk_NmpPathGroupState enum to string.
 * 
 * \note This is a non-blocking call.
 *
 * \note Spin locks can be held while calling into this function
 *
 * \param[in] state    State to convert to string.
 *
 * \return String describing the path group state.
 *
 ***********************************************************************
 */
const char *vmk_NmpPathGroupStateToString(vmk_NmpPathGroupState state);

/*
 ***********************************************************************
 * vmk_NmpPathGroupMovePath --                                    */ /**
 *
 * \ingroup NMP
 *
 * \brief Move path to a new group.
 *
 * This function should be invoked by SATPs when updating the path 
 * state. It also logs a message about the state change.
 * 
 * \note This is a non-blocking call.
 *
 * \note Spin locks can be held while calling into this function
 *
 * \param[in] p      Path to move.
 * \param[in] state  Move path to Path Group with this state.
 *
 * \retval VMK_EXISTS   Path was already in the new Path Group.
 * \retval VMK_FAILURE  Failed to move Path. Transitions out of 
 *                      VMK_SCSI_PATH_STATE_OFF to a different 
 *                      state are not allowed.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_NmpPathGroupMovePath(
   vmk_ScsiPath *p,
   vmk_NmpPathGroupState state);

/*
 ***********************************************************************
 * vmk_NmpPathGroupFind --                                        */ /**
 *
 * \ingroup NMP
 *
 * \brief For a given device, find the path group corresponding to a
 *        particular state.
 *
 * \note This is a non-blocking call.
 *
 * \note Spin locks can be held while calling into this function
 *
 * \param[in] d      Device to find Path Group for.
 * \param[in] state  Path Group state to look for.
 *
 * \return The path group whose path state matches \em state.
 * \warning   For undefined \em state, the results are undefined.
 *
 ***********************************************************************
 */
vmk_NmpPathGroup *vmk_NmpPathGroupFind(
   vmk_NmpDevice *d,
   vmk_NmpPathGroupState state);

/*
 ***********************************************************************
 * vmk_NmpPathGroupGetNext --                                     */ /**
 *
 * \ingroup NMP
 *
 * \brief Path Group iterator.
 *
 * \note This is a non-blocking call.
 *
 * \note Spin locks can be held while calling into this function
 *
 * \param[in] d nmp device to iterate Path Groups for.
 * \param[in] pg Current Path Group. If pg is NULL, first Path Group
 *               is returned.
 *
 * \return The next path group.
 * \retval NULL   End of the Path Group list.
 * 
 ***********************************************************************
 */
vmk_NmpPathGroup *vmk_NmpPathGroupGetNext(
   vmk_NmpDevice *d,
   vmk_NmpPathGroup *pg);

/*
 ***********************************************************************
 * vmk_NmpGetVendorModelFromInquiry  --                           */ /**
 *
 * \ingroup NMP
 *
 * \brief Extract the model and vendor ASCII strings from the inquiry
 *        page.
 * 
 * \em inquiryInfo is obtained from a prior call to vmk_ScsiGetPathInquiry().
 *
 * \note This is a non-blocking call.
 *
 * \note Spin locks can be held while calling into this function
 *
 * \param[in] inquiryInfo  Standard inquiry data.
 * \param[out] vendorName  Vendor name.
 * \param[out] modelName   Model name.
 *
 * \return This routine always succeeds with VMK_OK status.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_NmpGetVendorModelFromInquiry(
   vmk_uint8  *inquiryInfo,
   vmk_uint8  *vendorName,
   vmk_uint8  *modelName);

/*
 ***********************************************************************
 * vmk_NmpGetTargetPortGroupSupportFromInquiry  --                */ /**
 *
 * \ingroup NMP
 *
 * \brief Return the target port group support (TPGS) field from the
 *        inquiry response.
 *
 * \em inquiryInfo is obtained by a prior call to
 * vmk_ScsiGetPathInquiry(). The TPGS value is as defined in SPC-3
 * Table 85.
 *
 * \note This is a non-blocking call.
 *
 * \note Spin locks can be held while calling into this function
 *
 * \param[in] inquiryInfo  Inquiry data.
 *
 * \return Supported target port group.
 *
 ***********************************************************************
 */
vmk_uint8 vmk_NmpGetTargetPortGroupSupportFromInquiry(
   vmk_uint8 *inquiryInfo);

/*
 ***********************************************************************
 * vmk_NmpIsDeviceReservedPath --                                 */ /**
 *
 * \ingroup PSP
 *
 * \brief Tell whether the passed path holds a SCSI-2 reservation
 *        on this device.
 *
 * \note This is a non-blocking call. 
 *
 * \note Spin locks can be held while calling into this function
 *
 * \param[in] nmpDev    NMP Device path is on.
 * \param[in] scsiPath  Path to check.
 *
 * \retval VMK_TRUE     This path has a SCSI-2 reservation for the
 *                      device.
 * \retval VMK_FALSE    This path does not have a SCSI-2 reservation
 *                      for the device.
 *
 ***********************************************************************
 */
vmk_Bool vmk_NmpIsDeviceReservedPath(
   vmk_NmpDevice *nmpDev,
   vmk_ScsiPath *scsiPath);

/*
 ***********************************************************************
 * vmk_NmpIsPathBlocked--                                         */ /**
 *
 * \ingroup PSP
 *
 * \brief Tell whether the passed path is eligible for I/O.
 *
 * This is used during SCSI-3 reservations only. If the device is used
 * for SCSI-3 reservation, then the paths to that device must have the
 * same registration key as that of the device to be eligible for I/O.
 *
 * \note This is a non-blocking call.
 *
 * \note Spin locks can be held while calling into this function
 *
 * \param[in] nmpDev    NMP Device path is on.
 * \param[in] scsiPath  Path to check.
 *
 * \retval VMK_TRUE     I/Os cannot be issued on this path.
 * \retval VMK_FALSE    This path is eligible for I/O.
 *
 ***********************************************************************
 */
vmk_Bool vmk_NmpIsPathBlocked(
   vmk_NmpDevice *nmpDev,
   vmk_ScsiPath *scsiPath);

/*
 ***********************************************************************
 * vmk_NmpGetDevicePReservedPath--                                */ /**
 *
 * \ingroup PSP
 *
 * \brief Returns path holding a SCSI-3 persistent reservation on device
 *
 * The caller should ensure that the device will not be unclaimed while
 * calling this routine.
 *
 * \note This is a non-blocking call.
 *
 * \note Spin locks can be held while calling into this function
 *
 * \param[in] nmpDev  NMP Device path is on.
 * \param[out] scsiPath Path that has a SCSI-3 reservation.
 *
 * \retval VMK_OK        Found a path with valid SCSI-3 persistent
 *                       reservation.
 * \retval VMK_NOT_FOUND Device does not hold SCSI-3 persistent
 *                       reservation.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_NmpGetDevicePReservedPath(
   vmk_NmpDevice *nmpDev,
   vmk_ScsiPath **scsiPath);

#endif /* _VMK_NMP_H_ */
/** @} */
/** @} */
