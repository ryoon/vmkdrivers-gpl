/* **********************************************************
 * Copyright 2007 - 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * iSCSI Externally Exported Interfaces                           */ /**
 * \defgroup ISCSI ISCSI Interfaces
 * @{
 *
 * \defgroup  IscsiTransport iSCSI Transport public interfaces
 *
 * Vmkernel-specific iSCSI constants & types which are shared with
 * user-mode and driver code.
 * @{
 *
 ***********************************************************************
 */

#ifndef _VMKAPI_ISCSI_TRANSPORT_COMPAT_H
#define _VMKAPI_ISCSI_TRANSPORT_COMPAT_H

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif

#include "scsi/vmkapi_scsi_types.h"
#include "net/vmkapi_net_types.h"

/* iSCSI Transport API Revision */
#define VMK_ISCSI_TRANSPORT_REVISION_MAJOR         1
#define VMK_ISCSI_TRANSPORT_REVISION_MINOR         0
#define VMK_ISCSI_TRANSPORT_REVISION_UPDATE        0
#define VMK_ISCSI_TRANSPORT_REVISION_PATCH_LEVEL   0

#define VMK_ISCSI_TRANSPORT_REVISION         \
   VMK_REVISION_NUMBER(VMK_ISCSI_TRANSPORT)
/** \endcond never */

#define VMK_ISCSI_MAX_PARAM_BUFFER_SZ     (4096)
#define VMK_ISCSI_MAX_SSID_BUFFER_SZ      (1024)
#define VMK_ISCSI_MAX_TRANSPORT_NAME_SZ   (256)
#define VMK_ISCSI_MAX_TRANSPORT_DESC_SZ   (256)

/**
 * \brief iSCSI Parameters
 *
 * iSCSI parameters are passed from user space code and then resent
 * to the media driver.  The Media Drivers use these by name. The data
 * for the parameter is always passed in as an ASCII string. For
 * instance numeric parameter will be passed in as "1234\0".
 */
typedef enum {
   /**
    * \brief Specify Max Receive Data Segment Length.
    *
    */
   VMK_ISCSI_CONN_PARAM_MAX_RECV_DLENGTH   =   0,

   /**
    * \brief Specify Max Transmit Data Segment Length.
    *
    */
   VMK_ISCSI_CONN_PARAM_MAX_XMIT_DLENGTH   =   1,

   /**
    * \brief Enable/Disable Header Digest.
    *
    */
   VMK_ISCSI_CONN_PARAM_HDRDGST_EN         =   2,

   /**
    * \brief Enable/Disable Data Digest.
    *
    */
   VMK_ISCSI_CONN_PARAM_DATADGST_EN        =   3,

   /**
    * \brief Enable/Disable Initital R2T.
    *
    */
   VMK_ISCSI_SESS_PARAM_INITIAL_R2T_EN     =   4,

   /**
    * \brief Specifiy Maximum Outstanding R2T.
    *
    */
   VMK_ISCSI_SESS_PARAM_MAX_R2T            =   5,

   /**
    * \brief Enable/Disable Immediate Data.
    *
    */
   VMK_ISCSI_SESS_PARAM_IMM_DATA_EN        =   6,

   /**
    * \brief Specify First Burst Size.
    *
    */
   VMK_ISCSI_SESS_PARAM_FIRST_BURST        =   7,

   /**
    * \brief Specify Max Burst Size.
    *
    */
   VMK_ISCSI_SESS_PARAM_MAX_BURST          =   8,

   /**
    * \brief Enable/Disable Ordered PDU.
    *
    */
   VMK_ISCSI_SESS_PARAM_PDU_INORDER_EN     =   9,

   /**
    * \brief Enable/Disable Inorder Data Sequence Delivery.
    *
    */
   VMK_ISCSI_SESS_PARAM_DATASEQ_INORDER_EN =  10,

   /**
    * \brief Specify Error Recovery Level.
    *
    */
   VMK_ISCSI_SESS_PARAM_ERL                =  11,

   /**
    * \brief Enable/Disable IF Marker.
    *
    */
   VMK_ISCSI_CONN_PARAM_IFMARKER_EN        =  12,

   /**
    * \brief Enable/Disable OF Marker.
    *
    */
   VMK_ISCSI_CONN_PARAM_OFMARKER_EN        =  13,

   /**
    * \brief Expected StatSN.
    *
    */
   VMK_ISCSI_CONN_PARAM_EXP_STATSN         =  14,

   /**
    * \brief Specify Target Name.
    *
    */
   VMK_ISCSI_SESS_PARAM_TARGET_NAME        =  15,

   /**
    * \brief Specify Target Portal Group Tag.
    *
    */
   VMK_ISCSI_SESS_PARAM_TPGT               =  16,

   /**
    * \brief Specify Persistent Target Address (i.e Group Address).
    *
    */
   VMK_ISCSI_CONN_PARAM_PERSISTENT_ADDRESS =  17,

   /**
    * \brief Specify Persistent Socket Port.
    *
    */
   VMK_ISCSI_CONN_PARAM_PERSISTENT_PORT    =  18,

   /**
    * \brief Specify Recovery Timeout.
    *
    */
   VMK_ISCSI_SESS_PARAM_SESS_RECOVERY_TMO  =  19,

   /**
    * \brief Connected port.
    *
    */
   VMK_ISCSI_CONN_PARAM_CONN_PORT          =  20,

   /**
    * \brief Connected Target address.
    *
    */
   VMK_ISCSI_CONN_PARAM_CONN_ADDRESS       =  21,

   /**
    * \brief Specify outgoing authentication user name.
    *
    */
   VMK_ISCSI_SESS_PARAM_USERNAME           =  22,

   /**
    * \brief Specify incoming authentication user name.
    *
    */
   VMK_ISCSI_SESS_PARAM_USERNAME_IN        =  23,

   /**
    * \brief Specify outgoing authentication password.
    *
    */
   VMK_ISCSI_SESS_PARAM_PASSWORD           =  24,

   /**
    * \brief Specify incoming authentication password.
    *
    */
   VMK_ISCSI_SESS_PARAM_PASSWORD_IN        =  25,

   /**
    * \brief Enable/Disable FastAbort.
    *
    */
   VMK_ISCSI_SESS_PARAM_FAST_ABORT         =  26,

   /**
    * \brief Specify "Initiator Session ID" for the session.
    *
    */
   VMK_ISCSI_SESS_PARAM_ISID               =  27,

   /**
    * \brief Specify persistent ID used to uniqley identify the session.
    *
    */
   VMK_ISCSI_SESS_PARAM_SSID               =  28,

   /**
    * \brief Report maximum session supported by transport driver.
    *
    */
   VMK_ISCSI_TRANS_PARAM_MAX_SESSIONS      =  29,

   /** \cond never */
   VMK_ISCSI_TRANS_ISCSI_PARAM_MAX
   /** \endcond never */
} vmk_IscsiTransIscsiParam;

/**
 * \brief iSCSI HBA/host parameters
 */
typedef enum {
   VMK_ISCSI_HOST_PARAM_HWADDRESS      = 0,
   VMK_ISCSI_HOST_PARAM_INITIATOR_NAME = 1,
   VMK_ISCSI_HOST_PARAM_NETDEV_NAME    = 2,
   VMK_ISCSI_HOST_PARAM_IPADDRESS      = 3,
   /** \cond never */
   VMK_ISCSI_TRANS_HOST_PARAM_MAX
   /** \endcond never */
} vmk_IscsiTransHostParam;

/**
 * \brief iSCSI transport discovery types
 */
typedef enum {
   VMK_ISCSI_TGT_DSCVR_SEND_TARGETS  = 1,
   VMK_ISCSI_TGT_DSCVR_ISNS          = 2,
   VMK_ISCSI_TGT_DSCVR_SLP           = 3,
} vmk_IscsiTransTgtDiscvr;

/**
 * \brief iSCSI protocol specific error code
 */
typedef enum {

   /**
    * \brief No Error.
    *
    */
   VMK_ISCSI_OK                  =    0,

   /**
    * \brief Data packet sequence number incorrect.
    *
    */
   VMK_ISCSI_ERR_DATASN          = 1001,

   /**
    * \brief Data packet offset number incorrect.
    *
    */
   VMK_ISCSI_ERR_DATA_OFFSET     = 1002,

   /**
    * \brief Exceeded Max cmd sequence number.
    *
    */
   VMK_ISCSI_ERR_MAX_CMDSN       = 1003,

   /**
    * \brief Command sequence number error.
    *
    */
   VMK_ISCSI_ERR_EXP_CMDSN       = 1004,

   /**
    * \brief Invalid iSCSI OP code.
    *
    */
   VMK_ISCSI_ERR_BAD_OPCODE      = 1005,

   /**
    * \brief Data length error.
    *
    */
   VMK_ISCSI_ERR_DATALEN         = 1006,

   /**
    * \brief AHS Length error.
    *
    */
   VMK_ISCSI_ERR_AHSLEN          = 1007,

   /**
    * \brief Generic Protocol violation.
    *
    */
   VMK_ISCSI_ERR_PROTO           = 1008,

   /**
    * \brief Invalid LUN.
    *
    */
   VMK_ISCSI_ERR_LUN             = 1009,

   /**
    * \brief Invalid ITT.
    *
    */
   VMK_ISCSI_ERR_BAD_ITT         = 1010,

   /**
    * \brief Connection Failure.
    *
    */
   VMK_ISCSI_ERR_CONN_FAILED     = 1011,

   /**
    * \brief Ready to send sequence error.
    *
    */
   VMK_ISCSI_ERR_R2TSN           = 1012,

   /**
    * \brief Session Failed.
    *
    */
   VMK_ISCSI_ERR_SESSION_FAILED  = 1013,

   /**
    * \brief Header Digest invalid.
    *
    */
   VMK_ISCSI_ERR_HDR_DGST        = 1014,

   /**
    * \brief Data digest invalid.
    *
    */
   VMK_ISCSI_ERR_DATA_DGST       = 1015,

   /**
    * \brief Invalid/Unsupported Parameter.
    *
    */
   VMK_ISCSI_ERR_PARAM_NOT_FOUND = 1016,

   /**
    * \brief Invalid iSCSI command.
    *
    */
   VMK_ISCSI_ERR_NO_SCSI_CMD     = 1017,
} vmk_IscsiTransIscsiErrorCode;

/**
 * \brief Flags for stopConnection
 */
typedef enum {
   /**
    * \brief The connection is going to be terminated.
    *
    */
   VMK_ISCSI_STOP_CONN_TERM         =  0x1,

   /**
    * \brief The connection will be recovered.
    *
    */
   VMK_ISCSI_STOP_CONN_RECOVER      =  0x3,

   /**
    * \brief For cleanup following reporting a connection error.
    *
    */
   VMK_ISCSI_STOP_CONN_CLEANUP_ONLY = 0xff
} vmk_IscsiStopConnMode;

/**
 * \brief Transport capabilities.
 *
 * These flags presents iSCSI Data-Path capabilities.
 */
typedef enum {
   /**
    * \brief Transport driver supports Error Recovery Level 0.
    *
    */
   VMK_ISCSI_CAP_RECOVERY_L0           = 0,

   /**
    * \brief Transport driver supports Error Recovery Level 1.
    *
    */
   VMK_ISCSI_CAP_RECOVERY_L1           = 1,

   /**
    * \brief Transport driver supports Error Recovery Level 2.
    *
    */
   VMK_ISCSI_CAP_RECOVERY_L2           = 2,

   /**
    * \brief Transport driver supports multiple outstanding R2T's.
    *
    */
   VMK_ISCSI_CAP_MULTI_R2T             = 3,

   /**
    * \brief Transport driver supports Header Digest.
    *
    */
   VMK_ISCSI_CAP_HDRDGST               = 4,

   /**
    * \brief Transport driver supports Data Digest.
    *
    */
   VMK_ISCSI_CAP_DATADGST              = 5,

   /**
    * \brief Transport driver supports Multiple Connections per session.
    *
    */
   VMK_ISCSI_CAP_MULTI_CONN            = 6,

   /**
    * \brief Transport driver supports Text Negotiation.
    *
    */
   VMK_ISCSI_CAP_TEXT_NEGO             = 7,

   /**
    * \brief Transport driver supports Marker PDU's.
    *
    */
   VMK_ISCSI_CAP_MARKERS               = 8,

   /**
    * \brief Transport driver supports Firmware Data Base.
    *
    */
   VMK_ISCSI_CAP_FW_DB                 = 9,

   /**
    * \brief Transport driver supports Offloaded SendTargets Discovery.
    *
    */
   VMK_ISCSI_CAP_SENDTARGETS_OFFLOAD   = 10,

   /**
    * \brief Transport driver supports Data Path Offload.
    *
    */
   VMK_ISCSI_CAP_DATA_PATH_OFFLOAD     = 11,

   /**
    * \brief Session Creation with Target and Channel ID assigned by
    *        transport.
    *
    */
   VMK_ISCSI_CAP_SESSION_PERSISTENT    = 12,

   /**
    * \brief Transport driver supports IPV6.
    *
    */
   VMK_ISCSI_CAP_IPV6                  = 13,

   /**
    * \brief Transport driver supports RDMA protocol.
    *
    */
   VMK_ISCSI_CAP_RDMA                  = 14,

   /**
    * \brief User space socket creation style transport driver.
    *
    */
   VMK_ISCSI_CAP_USER_POLL             = 15,

   /**
    * \brief Transport driver supports Kernel space poll.
    *
    */
   VMK_ISCSI_CAP_KERNEL_POLL           = 16,

   /**
    * \brief Transport driver supports cleanup primitive to connection
    *        stop.
    *
    */
   VMK_ISCSI_CAP_CONN_CLEANUP          = 17,
   /** \cond never */
   VMK_ISCSI_CAP_MAX
   /** \endcond never */
} vmk_IscsiTransCapabilities;

/**
 * \brief iSCSI transport property types
 *
 */
typedef enum {
   VMK_ISCSI_TRANSPORT_PROPERTY_TYPE_UNKNOWN    = 0,
   /**
    * \brief ISCSI connection/session params
    *
    * \note This is a mask type property and 'value' if given
    *       during add/remove is ignored.
    */
   VMK_ISCSI_TRANSPORT_PROPERTY_ISCSI_PARAM     = 1,

   /**
    * \brief ISCSI host/hba params
    *
    * \note This is a mask type property and 'value' if given
    *       during add/remove is ignored.
    */
   VMK_ISCSI_TRANSPORT_PROPERTY_HOST_PARAM      = 2,

   /**
    * \brief ISCSI transport driver capabilities
    *
    * \note This is a mask type property and 'value' if given
    *       during add/remove is ignored.
    */
   VMK_ISCSI_TRANSPORT_PROPERTY_CAPABILITIES    = 3,

   /** \cond never */
   VMK_ISCSI_TRANSPORT_PROPERTY_TYPE_MAX
   /** \endcond never */
} vmk_IscsiTransPropertyType;

/**
 * \brief Endpoint poll status. Indicates whether or not availability of data on the
 *        connection endpoint.
 *
 */
typedef enum vmk_IscsiTransPollStatus {
   /** \brief Indicates the endpoint has no data available */
   VMK_ISCSI_TRANSPORT_POLL_STATUS_NO_DATA         = 0,

   /** \brief Indicates the endpoint has some data available */
   VMK_ISCSI_TRANSPORT_POLL_STATUS_DATA_AVAILABLE  = 1,
} vmk_IscsiTransPollStatus;

/**
 * \brief Generic Property, an opaque handle for the
 *        iscsi transport supported properties.
 *
 */
typedef struct vmk_IscsiTransGenericProperty {
   /**
    * \brief Property type.
    *
    */
   vmk_IscsiTransPropertyType propertyType;
} vmk_IscsiTransGenericProperty;

/**
 * \brief Session/connection parameter property.
 *
 */
typedef vmk_IscsiTransGenericProperty *vmk_IscsiTransIscsiParamProperty;

/*
 ***********************************************************************
 * vmk_IscsiTransportCreateIscsiParamProperty--                   */ /**
 *
 * \brief Create an IscsiParam property object.
 *
 * \note This function will not block.
 *
 * \param[out] prop Pointer to the property object where allocated
 *                  object is returned.
 *
 * \retval  VMK_OK
 * \retval  VMK_BUSY
 * \retval  VMK_BAD_PARAM
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportCreateIscsiParamProperty(
   vmk_IscsiTransIscsiParamProperty *prop);

/*
 ***********************************************************************
 * vmk_IscsiTransportAddIscsiParamProperty--                      */ /**
 *
 * \brief Add an supported IscsiParam for a given property object.
 *
 * \note This function will not block.
 *
 * \param[in]  prop     Property object.
 * \param[in]  param    The IscsiParam property item to be added.
 *
 * \note It is not an error to add the same parameter to a property
 *       more than once.
 *
 * \retval  VMK_OK
 * \retval  VMK_BUSY
 * \retval  VMK_BAD_PARAM
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_IscsiTransportAddIscsiParamProperty(
   vmk_IscsiTransIscsiParamProperty prop,
   vmk_IscsiTransIscsiParam         param);

/*
 ***********************************************************************
 * vmk_IscsiTransportRemoveIscsiParamProperty--                   */ /**
 *
 * \brief Remove an IscsiParam property item from the list for a
 *        given property object.
 *
 * \note This function will not block.
 *
 * \param[in]  prop  iSCSI Parameter property object.
 * \param[in]  param The IscsiParam property item to be removed.
 *
 * \note It is not an error to remove the same parameter from a
 *       property more than once.
 *
 * \retval  VMK_OK
 * \retval  VMK_BUSY
 * \retval  VMK_BAD_PARAM
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_IscsiTransportRemoveIscsiParamProperty(
   vmk_IscsiTransIscsiParamProperty prop,
   vmk_IscsiTransIscsiParam         param);

/*
 ***********************************************************************
 * vmk_IscsiTransportDestroyIscsiParamProperty--                  */ /**
 *
 * \brief Destroy an IscsiParams property object and associated
 *        resources.
 *
 * \note This function will not block.
 *
 * \param[in] prop Property object to be destroyed.
 *
 * \retval  VMK_OK
 * \retval  VMK_BUSY
 * \retval  VMK_BAD_PARAM
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportDestroyIscsiParamProperty(
   vmk_IscsiTransIscsiParamProperty prop);

/**
 * \brief Host parameter property.
 *
 */
typedef vmk_IscsiTransGenericProperty *vmk_IscsiTransHostParamProperty;
/*
 ***********************************************************************
 * vmk_IscsiTransportCreateHostParamProperty--                    */ /**
 *
 * \brief Create an HostParam property object.
 *
 * \note This function will not block.
 *
 * \param[out] prop Pointer to the property object where allocated
 *                  object is returned.
 *
 * \retval  VMK_OK
 * \retval  VMK_BUSY
 * \retval  VMK_BAD_PARAM
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportCreateHostParamProperty(
   vmk_IscsiTransHostParamProperty *prop);

/*
 ***********************************************************************
 * vmk_IscsiTransportAddHostParamProperty--                       */ /**
 *
 * \brief Add an supported HostParam for a given property object.
 *
 * \note This function will not block.
 *
 * \param[in]  prop     Property object.
 * \param[in]  param    The HostParam property item to be added.
 *
 * \note It is not an error to add the same parameter to a property
 *       more than once.
 *
 * \retval  VMK_OK
 * \retval  VMK_BUSY
 * \retval  VMK_BAD_PARAM
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_IscsiTransportAddHostParamProperty(
   vmk_IscsiTransHostParamProperty prop,
   vmk_IscsiTransHostParam         param);

/*
 ***********************************************************************
 * vmk_IscsiTransportRemoveHostParamProperty--                    */ /**
 *
 * \brief Remove an HostParam property item from the list for a
 *        given property object.
 *
 * \note This function will not block.
 *
 * \param[in]  prop  iSCSI Parameter property object.
 * \param[in]  param The HostParam property item to be removed.
 *
 * \note It is not an error to remove the same parameter from a
 *       property more than once.
 *
 * \retval  VMK_OK
 * \retval  VMK_BUSY
 * \retval  VMK_BAD_PARAM
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_IscsiTransportRemoveHostParamProperty(
   vmk_IscsiTransHostParamProperty prop,
   vmk_IscsiTransHostParam         param);

/*
 ***********************************************************************
 * vmk_IscsiTransportDestroyHostParamProperty--                   */ /**
 *
 * \brief Destroy an HostParams property object and associated
 *        resources.
 *
 * \note This function will not block.
 *
 * \param[in] prop Property object to be destroyed.
 *
 * \retval  VMK_OK
 * \retval  VMK_BUSY
 * \retval  VMK_BAD_PARAM
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportDestroyHostParamProperty(
   vmk_IscsiTransHostParamProperty prop);

/**
 * \brief Transport capabilities property.
 *
 */
typedef vmk_IscsiTransGenericProperty *vmk_IscsiTransCapabilitiesProperty;

/*
 ***********************************************************************
 * vmk_IscsiTransportCreateCapabilitiesProperty--                 */ /**
 *
 * \brief Create an Capabilities property object.
 *
 * \note This function will not block.
 *
 * \param[out] prop Pointer to the property object where allocated
 *                  object is returned.
 *
 * \retval  VMK_OK
 * \retval  VMK_BUSY
 * \retval  VMK_BAD_PARAM
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportCreateCapabilitiesProperty(
   vmk_IscsiTransCapabilitiesProperty *prop);

/*
 ***********************************************************************
 * vmk_IscsiTransportAddCapabilitiesProperty--                    */ /**
 *
 * \brief Add an supported Capabilities for a given property object.
 *
 * \note This function will not block.
 *
 * \param[in]  prop  Property object.
 * \param[in]  cap   The Capabilities property item to be added.
 *
 * \note It is not an error to add the same capability to a property
 *       more than once.
 *
 * \retval  VMK_OK
 * \retval  VMK_BUSY
 * \retval  VMK_BAD_PARAM
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_IscsiTransportAddCapabilitiesProperty(
   vmk_IscsiTransCapabilitiesProperty prop,
   vmk_IscsiTransCapabilities         cap);

/*
 ***********************************************************************
 * vmk_IscsiTransportRemoveCapabilitiesProperty--                 */ /**
 *
 * \brief Remove an Capabilities property item from the list for a
 *        given property object.
 *
 * \note This function will not block.
 *
 * \param[in]  prop  iSCSI Parameter property object.
 * \param[in]  cap   The Capabilities property item to be removed.
 *
 * \note It is not an error to remove the same capability from a
 *       property more than once.
 *
 * \retval  VMK_OK
 * \retval  VMK_BUSY
 * \retval  VMK_BAD_PARAM
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_IscsiTransportRemoveCapabilitiesProperty(
   vmk_IscsiTransCapabilitiesProperty prop,
   vmk_IscsiTransCapabilities         cap);

/*
 ***********************************************************************
 * vmk_IscsiTransportDestroyCapabilitiesProperty--                */ /**
 *
 * \brief Destroy an Capabilitiess property object and associated
 *        resources.
 *
 * \note This function will not block.
 *
 * \param[in] prop Property object to be destroyed.
 *
 * \retval  VMK_OK
 * \retval  VMK_BUSY
 * \retval  VMK_BAD_PARAM
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportDestroyCapabilitiesProperty(
   vmk_IscsiTransCapabilitiesProperty prop);

/**
 * \brief Transport driver limit "min-max" specifiers.
 *
 */
typedef struct {
   /**
    * \brief Transport driver's minimum supported value.
    *
    * This structure to be used
    */
   vmk_uint32 min;

   /**
    * \brief Transport driver's maximum supported value.
    *
    */
   vmk_uint32 max;
} vmk_IscsiTransportLimitMinMax;

/**
 * \brief Transport driver limit "list" specifiers.
 */
typedef struct {

   /**
    * \brief Max number of allowed values in "value".
    *
    */
   vmk_uint32 count;

   /**
    * \brief List of values.
    *
    */
   vmk_uint32 value[0];
} vmk_IscsiTransportLimitList;

/**
 * \brief Transport driver limit specifiers -- limit type.
 */
typedef enum {
   /**
    * \brief Transport driver does not support limits for the param.
    *
    */
   VMK_ISCSI_TRANPORT_LIMIT_TYPE_UNSUPPORTED    = 0,

   /**
    * \brief Limits is of type Min-Max format.
    *
    */
   VMK_ISCSI_TRANPORT_LIMIT_TYPE_MINMAX         = 1,

   /**
    * \brief Limits is of type list format.
    *
    */
   VMK_ISCSI_TRANPORT_LIMIT_TYPE_LIST           = 2
} vmk_IscsiTransTransportParamLimitType;

/**
 * \brief Specify driver min-max/list limit for iSCSI parameters.
 *
 * Once the driver registers it's transport template, iSCSI transport
 * queries the driver for min-max values supported by driver for range
 * related iSCSI parameters described in vmk_IscsiTransIscsiParam. If
 * the driver doesn't support min-max values, it must set the "type" to
 * VMK_ISCSI_TRANPORT_LIMIT_TYPE_UNSUPPORTED and return VMK_OK. In that
 * case the transport uses RFC defined ranges as the min-max values as
 * valid values. If the driver does support min-max/list values, then
 * it must set the "type" to either VMK_ISCSI_TRANPORT_LIMIT_TYPE_LIST
 * or VMK_ISCSI_TRANPORT_LIMIT_TYPE_MINMAX and fill in the "limit"
 * accordingly. The given min-max/list values must be with in the
 * allowed min-max/list as specififed in RFC (if specified in iSCSI
 * specification) or as documented by iSCSI configuration guide.
 *
 * The driver can also specify it's default preferred value. The
 * transport will try to use that for session/connection as far as
 * possible.
 *
 * Driver is queried for following list of parameters to get driver's
 * min-max/allowed list values (transport's min-max and default values
 * also documented here):
 *
 *    VMK_ISCSI_CONN_PARAM_MAX_RECV_DLENGTH:
 *          min:512, max: 16MB-1 default:256K
 *
 *    VMK_ISCSI_SESS_PARAM_FIRST_BURST:
 *          min:512, max: 16MB-1 default:256K
 *
 *    VMK_ISCSI_SESS_PARAM_MAX_BURST:
 *          min:512, max: 16MB-1 default:256K
 *
 *    VMK_ISCSI_SESS_PARAM_PDU_INORDER_EN:
 *          list:Yes=1,N0=0 default:Yes
 *
 *    VMK_ISCSI_SESS_PARAM_DATASEQ_INORDER_EN:
 *          list:Yes=1,No= default:Yes
 *
 *    VMK_ISCSI_SESS_PARAM_INITIAL_R2T_EN:
 *          list:Yes=1,No=0 default:No
 *
 *    VMK_ISCSI_SESS_PARAM_MAX_R2T:
 *          min:1, max: 64K default:1
 *
 *    VMK_ISCSI_SESS_PARAM_IMM_DATA_EN:
 *          list:Yes=1,No=0 default:Yes
 *
 *    VMK_ISCSI_SESS_PARAM_ERL:
 *          min:0 max:0 default:0
 *
 *    VMK_ISCSI_CONN_PARAM_HDRDGST_EN:
 *          list:DIGEST_NONE=0,DIGEST_CRC32=1 default: both
 *
 *    VMK_ISCSI_CONN_PARAM_DATADGST_EN:
 *          list:NONE=0,CRC32=1 default: both
 *
 *    VMK_ISCSI_TRANSPORT_PARAM_MAX_SESSIONS:
 *          list:1-4096 default:4096
 *
 ***********************************************************************
 */
typedef struct {
   /**
    * \brief Limit type
    *
    * As documented in vmk_IscsiTransTransportParamLimitType.
    */
   vmk_IscsiTransTransportParamLimitType type;

   /**
    * \brief Indicates if media driver has a preferred value for
    *        the parameter.
    *
    */
   vmk_Bool hasPreferred;

   /**
    * \brief Transport driver's preferred value.
    *
    * If hasPreferred == VMK_TRUE, iSCSI transport will use this as
    * the default value.
    */
   vmk_uint32 preferred;

   /**
    * \brief Parameter name
    *
    * As documented in vmk_IscsiTransIscsiParam.
    */
   vmk_IscsiTransIscsiParam param;

   union{
      vmk_IscsiTransportLimitMinMax minMax;
      vmk_IscsiTransportLimitList list;
   } limit;
} vmk_IscsiTransTransportParamLimits;

#define VMK_ISCSI_STATS_CUSTOM_DESC_MAX 64

/**
 * \brief iSCSI transport connection custom stats.
 */
typedef struct {
   /**
    * \brief Custom statistic name.
    *
    */
   vmk_int8 desc[VMK_ISCSI_STATS_CUSTOM_DESC_MAX];

   /**
    * \brief Custom statistic data.
    *
    */
   vmk_uint64 value;
} vmk_IscsiTransCustomIscsiStats;

/**
 * \brief iSCSI transport connection MIB-II stats
 */
typedef struct {
        /* octets */
        vmk_uint64 txdata_octets;
        vmk_uint64 rxdata_octets;

        /* xmit pdus */
        vmk_uint32 noptx_pdus;
        vmk_uint32 scsicmd_pdus;
        vmk_uint32 tmfcmd_pdus;
        vmk_uint32 login_pdus;
        vmk_uint32 text_pdus;
        vmk_uint32 dataout_pdus;
        vmk_uint32 logout_pdus;
        vmk_uint32 snack_pdus;

        /* recv pdus */
        vmk_uint32 noprx_pdus;
        vmk_uint32 scsirsp_pdus;
        vmk_uint32 tmfrsp_pdus;
        vmk_uint32 textrsp_pdus;
        vmk_uint32 datain_pdus;
        vmk_uint32 logoutrsp_pdus;
        vmk_uint32 r2t_pdus;
        vmk_uint32 async_pdus;
        vmk_uint32 rjt_pdus;

        /* errors */
        vmk_uint32 digest_err;
        vmk_uint32 timeout_err;

        /*
         * iSCSI Custom Statistics support, i.e. Transport could
         * extend existing MIB statistics with its own specific
         * statistics up to ISCSI_STATS_CUSTOM_MAX.
         */
        vmk_uint32 custom_length;
        vmk_IscsiTransCustomIscsiStats custom[0]
               VMK_ATTRIBUTE_ALIGN(sizeof(vmk_uint64));
} vmk_IscsiTransIscsiStats;

typedef vmk_AddrCookie vmk_IscsiNetHandle;

/*
 ***********************************************************************
 * vmk_IscsiTansportGetVmkNic --                                  */ /**
 *
 * \ingroup IscsiTransport
 * \brief Returns the vmkNIC name associated with a IscsiNetHandle.
 *
 * \note This function will not block.
 *
 * \param[in]  iscsiNetHandle   Net Handle for this connection.
 * \param[out] vmkNic           The vmkNic name.
 *
 * \retval     VMK_OK           Success.
 * \retval     VMK_BAD_PARAM    Not a valid iscsi Net Handle.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportGetVmkNic(
  vmk_IscsiNetHandle iscsiNetHandle,
  char *vmkNic);

/*
 ***********************************************************************
 * vmk_IscsiTransportGetUplink --                                 */ /**
 *
 * \ingroup IscsiTransport
 * \brief Returns the uplink name associated with an IscsiNetHandle.
 *
 * \note This function will not block.
 *
 * \param[in]  iscsiNetHandle   Net Handle for this connection.
 * \param[out] uplink           The uplink name.
 *
 * \retval     VMK_OK           Success.
 * \retval     VMK_BAD_PARAM    Not a valid iscsi Net Handle.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportGetUplink(
  vmk_IscsiNetHandle iscsiNetHandle,
  char *uplink);

/*
 ***********************************************************************
 * vmk_IscsiTransportGetSrcIP --                                  */ /**
 *
 * \ingroup IscsiTransport
 * \brief Returns the Source IP Addr and family for an IscsiNetHandle.
 *
 * \note This function will not block.
 *
 * \param[in]  iscsiNetHandle   Net Handle for this connection.
 * \param[out] srcFamily        Sockaddr family value.
 * \param[out] srcIPAddr        The source address.
 *
 * \retval     VMK_OK           Success.
 * \retval     VMK_BAD_PARAM    Not a valid iscsi Net Handle.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportGetSrcIP(
  vmk_IscsiNetHandle iscsiNetHandle,
  vmk_uint32 *srcFamily,
  vmk_uint8 *srcIPAddr);

/*
 ***********************************************************************
 * vmk_IscsiTransportGetSrcSubnet --                              */ /**
 *
 * \ingroup IscsiTransport
 * \brief Returns the source subnet associated with an IscsiNetHandle.
 *
 * \note This function will not block.
 *
 * \param[in]  iscsiNetHandle   Net Handle for this connection.
 * \param[out] srcFamily        Sockaddr family value.
 * \param[out] srcSubnet        The source subnet.
 *
 * \retval     VMK_OK           Success.
 * \retval     VMK_BAD_PARAM    Not a valid iscsi Net Handle.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportGetSrcSubnet(
  vmk_IscsiNetHandle iscsiNetHandle,
  vmk_uint32 *srcFamily,
  vmk_uint8 *srcSubnet);

/*
 ***********************************************************************
 * vmk_IscsiTransportGetDstIP --                                  */ /**
 *
 * \ingroup IscsiTransport
 * \brief Returns the destination IP addr and family for an IscsiNetHandle.
 *
 * \note This function will not block.
 *
 * \param[in]  iscsiNetHandle   Net Handle for this connection.
 * \param[out] dstFamily        Sockadr family value.
 * \param[out] dstIPAddr        The destination IP address.
 *
 * \retval     VMK_OK           Success.
 * \retval     VMK_BAD_PARAM    Not a valid iscsi Net Handle.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportGetDstIP(
  vmk_IscsiNetHandle iscsiNetHandle,
  vmk_uint32 *dstFamily,
  vmk_uint8 *dstIPAddr);

/*
 ***********************************************************************
 * vmk_IscsiTransportGetNextHopIP --                              */ /**
 *
 * \ingroup IscsiTransport
 * \brief Returns the next hop IP address associated with an IscsiNetHandle.
 *
 * \note This function will not block.
 *
 * \param[in]  iscsiNetHandle   Net Handle for this connection.
 * \param[out] nextHopFamily    Sockaddr family value.
 * \param[out] nextHopIPAddr    The next hop IP address.
 *
 * \retval     VMK_OK           Success.
 * \retval     VMK_BAD_PARAM    Not a valid iscsi Net Handle.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportGetNextHopIP(
  vmk_IscsiNetHandle iscsiNetHandle,
  vmk_uint32 *nextHopFamily,
  vmk_uint8 *nextHopIPAddr);

/*
 ***********************************************************************
 * vmk_IscsiTransportGetNextHopMAC --                             */ /**
 *
 * \ingroup IscsiTransport
 * \brief Returns the Next Hop MAC Address for an IscsiNetHandle.
 *
 * \note This function will not block.
 *
 * \param[in]  iscsiNetHandle   Net Handle for this connection.
 * \param[out] nextHopMAC       The MAC address of the next hop.
 *
 * \retval     VMK_OK           Success.
 * \retval     VMK_BAD_PARAM    Not a valid iscsi Net Handle.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportGetNextHopMAC(
  vmk_IscsiNetHandle iscsiNetHandle,
  vmk_EthAddress nextHopMAC);

/*
 ***********************************************************************
 * vmk_IscsiTransportGetSrcMAC --                                 */ /**
 *
 * \ingroup IscsiTransport
 * \brief Returns the source MAC Address associated with an IscsiNetHandle.
 *
 * \note This function will not block.
 *
 * \param[in]  iscsiNetHandle   Net Handle for this connection.
 * \param[out] srcMAC           The MAC address of the local interface.
 *
 * \retval     VMK_OK           Success.
 * \retval     VMK_BAD_PARAM    Not a valid iscsi Net Handle.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportGetSrcMAC(
  vmk_IscsiNetHandle iscsiNetHandle,
  vmk_EthAddress srcMAC);

/*
 ***********************************************************************
 * vmk_IscsiTransportGetMtu --                                    */ /**
 *
 * \ingroup IscsiTransport
 * \brief Returns the mtu associated with an IscsiNetHandle.
 *
 * \note This function will not block.
 *
 * \param[in]  iscsiNetHandle   Net Handle for this connection.
 * \param[out] mtu              The mtu.
 *
 * \retval     VMK_OK           Success.
 * \retval     VMK_BAD_PARAM    Not a valid iscsi Net Handle.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportGetMtu(
  vmk_IscsiNetHandle iscsiNetHandle,
  vmk_uint32 *mtu);

/*
 ***********************************************************************
 * vmk_IscsiTransportGetPmtu --                                   */ /**
 *
 * \ingroup IscsiTransport
 * \brief Returns the path mtu associated with an IscsiNetHandle.
 *
 * \note This function will not block.
 *
 * \param[in]  iscsiNetHandle   Net Handle for this connection.
 * \param[out] pmtu             The pmtu.
 *
 * \retval     VMK_OK           Success.
 * \retval     VMK_BAD_PARAM    Not a valid iscsi Net Handle.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportGetPmtu(
  vmk_IscsiNetHandle iscsiNetHandle,
  vmk_uint32 *pmtu);

/*
 ***********************************************************************
 * vmk_IscsiTransportGetVlan --                                   */ /**
 *
 * \ingroup IscsiTransport
 * \brief Returns the Vlan ID associated with an IscsiNetHandle.
 *
 * \note This function will not block.
 *
 * \param[in]  iscsiNetHandle   Net Handle for this connection.
 * \param[out] vlan             The vlan ID.
 *
 * \retval     VMK_OK           Success.
 * \retval     VMK_BAD_PARAM    Not a valid iscsi Net Handle.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportGetVlan(
  vmk_IscsiNetHandle iscsiNetHandle,
  vmk_uint32 *vlan);

/*
 ***********************************************************************
 * vmk_IscsiTransportGetPVlan --                                  */ /**
 *
 * \ingroup IscsiTransport
 * \brief Returns the private VLan ID associated with an IscsiNetHandle.
 *
 * \note This function will not block.
 *
 * \param[in]  iscsiNetHandle   Net Handle for this connection.
 * \param[out] pvlan            The private vlan ID.
 *
 * \retval     VMK_OK           Success.
 * \retval     VMK_BAD_PARAM    Not a valid iscsi Net Handle.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportGetPVlan(
  vmk_IscsiNetHandle iscsiNetHandle,
  vmk_uint32 *pvlan);

/*
 ***********************************************************************
 * vmk_IscsiTransportGetPortReservation --                        */ /**
 *
 * \ingroup IscsiTransport
 * \brief Returns the start and number of reserved TCP ports.
 *
 * \note This function will not block.
 *
 * \param[in]  iscsiNetHandle   Net Handle for this connection.
 * \param[out] firstPort        The number of the first reserved port.
 * \param[out] portCount        The number of reserved ports.
 *
 * \retval     VMK_OK           Success.
 * \retval     VMK_BAD_PARAM    Not a valid iscsi Net Handle.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportGetPortReservation(
  vmk_IscsiNetHandle iscsiNetHandle,
  vmk_uint32 *firstPort,
  vmk_uint32 *portCount);
#endif /* _VMKAPI_ISCSI_TRANSPORT_COMPAT_H */
/** @} */
/** @} */
