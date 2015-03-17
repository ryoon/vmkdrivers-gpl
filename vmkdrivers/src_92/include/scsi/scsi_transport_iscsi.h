/************************************************************
 * Portions Copyright 2007-2009 VMware, Inc.  All rights reserved.
 ************************************************************/

/*
 * scsi_transport_iscsi.h --
 *
 *  Annex iSCSI Transport Module definitions
 */

#ifndef _ISCSI_TRANSPORT_H_
#define _ISCSI_TRANSPORT_H_

#include "vmkapi.h"

struct scsi_transport_template;
struct iscsi_hdr;
struct Scsi_Host;
struct sockaddr;

/**********************************************************************
 *
 * User Kernel API Definitions
 *
 **********************************************************************/

/*
 *  iSCSI Parameters are passed from user space code and then resent to the
 *  Media Driver.  The Media Drivers use these by name.   The data for the
 *  parameter is always passed in as an ASCII string.  For instance numeric
 *  parameter will be passed in as "1234\0"
 */
enum iscsi_param {
   ISCSI_PARAM_MAX_RECV_DLENGTH   =  VMK_ISCSI_CONN_PARAM_MAX_RECV_DLENGTH,
   ISCSI_PARAM_MAX_XMIT_DLENGTH   =  VMK_ISCSI_CONN_PARAM_MAX_XMIT_DLENGTH,
   ISCSI_PARAM_HDRDGST_EN         =  VMK_ISCSI_CONN_PARAM_HDRDGST_EN,
   ISCSI_PARAM_DATADGST_EN        =  VMK_ISCSI_CONN_PARAM_DATADGST_EN,
   ISCSI_PARAM_INITIAL_R2T_EN     =  VMK_ISCSI_SESS_PARAM_INITIAL_R2T_EN,
   ISCSI_PARAM_MAX_R2T            =  VMK_ISCSI_SESS_PARAM_MAX_R2T,
   ISCSI_PARAM_IMM_DATA_EN        =  VMK_ISCSI_SESS_PARAM_IMM_DATA_EN,
   ISCSI_PARAM_FIRST_BURST        =  VMK_ISCSI_SESS_PARAM_FIRST_BURST,
   ISCSI_PARAM_MAX_BURST          =  VMK_ISCSI_SESS_PARAM_MAX_BURST,
   ISCSI_PARAM_PDU_INORDER_EN     =  VMK_ISCSI_SESS_PARAM_PDU_INORDER_EN,
   ISCSI_PARAM_DATASEQ_INORDER_EN =  VMK_ISCSI_SESS_PARAM_DATASEQ_INORDER_EN,
   ISCSI_PARAM_ERL                =  VMK_ISCSI_SESS_PARAM_ERL,
   ISCSI_PARAM_IFMARKER_EN        =  VMK_ISCSI_CONN_PARAM_IFMARKER_EN,
   ISCSI_PARAM_OFMARKER_EN        =  VMK_ISCSI_CONN_PARAM_OFMARKER_EN,
   ISCSI_PARAM_EXP_STATSN         =  VMK_ISCSI_CONN_PARAM_EXP_STATSN,
   ISCSI_PARAM_TARGET_NAME        =  VMK_ISCSI_SESS_PARAM_TARGET_NAME,
   ISCSI_PARAM_TPGT               =  VMK_ISCSI_SESS_PARAM_TPGT,
   ISCSI_PARAM_PERSISTENT_ADDRESS =  VMK_ISCSI_CONN_PARAM_PERSISTENT_ADDRESS,
   ISCSI_PARAM_PERSISTENT_PORT    =  VMK_ISCSI_CONN_PARAM_PERSISTENT_PORT,
   ISCSI_PARAM_SESS_RECOVERY_TMO  =  VMK_ISCSI_SESS_PARAM_SESS_RECOVERY_TMO,
   ISCSI_PARAM_CONN_PORT          =  VMK_ISCSI_CONN_PARAM_CONN_PORT,
   ISCSI_PARAM_CONN_ADDRESS       =  VMK_ISCSI_CONN_PARAM_CONN_ADDRESS,
   ISCSI_PARAM_USERNAME           =  VMK_ISCSI_SESS_PARAM_USERNAME,
   ISCSI_PARAM_USERNAME_IN        =  VMK_ISCSI_SESS_PARAM_USERNAME_IN,
   ISCSI_PARAM_PASSWORD           =  VMK_ISCSI_SESS_PARAM_PASSWORD,
   ISCSI_PARAM_PASSWORD_IN        =  VMK_ISCSI_SESS_PARAM_PASSWORD_IN,
   ISCSI_PARAM_FAST_ABORT         =  VMK_ISCSI_SESS_PARAM_FAST_ABORT,
   ISCSI_PARAM_ISID               =  VMK_ISCSI_SESS_PARAM_ISID,
   ISCSI_PARAM_SSID               =  VMK_ISCSI_SESS_PARAM_SSID,
   ISCSI_PARAM_MAX_SESSIONS       =  VMK_ISCSI_TRANS_PARAM_MAX_SESSIONS,
};

#define ISCSI_INITIAL_R2T_EN     ( 1 << VMK_ISCSI_SESS_PARAM_INITIAL_R2T_EN    )
#define ISCSI_MAX_R2T            ( 1 << VMK_ISCSI_SESS_PARAM_MAX_R2T           )
#define ISCSI_IMM_DATA_EN        ( 1 << VMK_ISCSI_SESS_PARAM_IMM_DATA_EN       )
#define ISCSI_FIRST_BURST        ( 1 << VMK_ISCSI_SESS_PARAM_FIRST_BURST       )
#define ISCSI_MAX_BURST          ( 1 << VMK_ISCSI_SESS_PARAM_MAX_BURST         )
#define ISCSI_PDU_INORDER_EN     ( 1 << VMK_ISCSI_SESS_PARAM_PDU_INORDER_EN    )
#define ISCSI_DATASEQ_INORDER_EN ( 1 << VMK_ISCSI_SESS_PARAM_DATASEQ_INORDER_EN)
#define ISCSI_ERL                ( 1 << VMK_ISCSI_SESS_PARAM_ERL               )
#define ISCSI_TPGT               ( 1 << VMK_ISCSI_SESS_PARAM_TPGT              )
#define ISCSI_TARGET_NAME        ( 1 << VMK_ISCSI_SESS_PARAM_TARGET_NAME       )
#define ISCSI_SESS_RECOVERY_TMO  ( 1 << VMK_ISCSI_SESS_PARAM_SESS_RECOVERY_TMO )
#define ISCSI_USERNAME           ( 1 << VMK_ISCSI_SESS_PARAM_USERNAME          )
#define ISCSI_USERNAME_IN        ( 1 << VMK_ISCSI_SESS_PARAM_USERNAME_IN       )
#define ISCSI_PASSWORD           ( 1 << VMK_ISCSI_SESS_PARAM_PASSWORD          )
#define ISCSI_PASSWORD_IN        ( 1 << VMK_ISCSI_SESS_PARAM_PASSWORD_IN       )
#define ISCSI_FAST_ABORT         ( 1 << VMK_ISCSI_SESS_PARAM_FAST_ABORT        )
#define ISCSI_ISID               ( 1 << VMK_ISCSI_SESS_PARAM_ISID              )
#define ISCSI_SSID               ( 1 << VMK_ISCSI_SESS_PARAM_SSID              )
#define ISCSI_TPGT               ( 1 << VMK_ISCSI_SESS_PARAM_TPGT              )

#define ISCSI_CONN_PORT          ( 1 << VMK_ISCSI_CONN_PARAM_CONN_PORT         )
#define ISCSI_CONN_ADDRESS       ( 1 << VMK_ISCSI_CONN_PARAM_CONN_ADDRESS      )
#define ISCSI_MAX_RECV_DLENGTH   ( 1 << VMK_ISCSI_CONN_PARAM_MAX_RECV_DLENGTH  )
#define ISCSI_MAX_XMIT_DLENGTH   ( 1 << VMK_ISCSI_CONN_PARAM_MAX_XMIT_DLENGTH  )
#define ISCSI_HDRDGST_EN         ( 1 << VMK_ISCSI_CONN_PARAM_HDRDGST_EN        )
#define ISCSI_DATADGST_EN        ( 1 << VMK_ISCSI_CONN_PARAM_DATADGST_EN       )
#define ISCSI_IFMARKER_EN        ( 1 << VMK_ISCSI_CONN_PARAM_IFMARKER_EN       )
#define ISCSI_OFMARKER_EN        ( 1 << VMK_ISCSI_CONN_PARAM_OFMARKER_EN       )
#define ISCSI_EXP_STATSN         ( 1 << VMK_ISCSI_CONN_PARAM_EXP_STATSN        )
#define ISCSI_CONN_ADDRESS       ( 1 << VMK_ISCSI_CONN_PARAM_CONN_ADDRESS      )
#define ISCSI_PERSISTENT_ADDRESS ( 1 << VMK_ISCSI_CONN_PARAM_PERSISTENT_ADDRESS)
#define ISCSI_PERSISTENT_PORT    ( 1 << VMK_ISCSI_CONN_PARAM_PERSISTENT_PORT   )

#define ISCSI_MAX_SESSIONS       ( 1 << VMK_ISCSI_TRANS_PARAM_MAX_SESSIONS     )

enum iscsi_host_param {
   ISCSI_HOST_PARAM_HWADDRESS      = VMK_ISCSI_HOST_PARAM_HWADDRESS,
   ISCSI_HOST_PARAM_INITIATOR_NAME = VMK_ISCSI_HOST_PARAM_INITIATOR_NAME,
   ISCSI_HOST_PARAM_NETDEV_NAME    = VMK_ISCSI_HOST_PARAM_NETDEV_NAME,
   ISCSI_HOST_PARAM_IPADDRESS      = VMK_ISCSI_HOST_PARAM_IPADDRESS
};

#define VMK_ISCSI_HOST_HWADDRESS        ( 1 << VMK_ISCSI_HOST_PARAM_HWADDRESS      )
#define VMK_ISCSI_HOST_IPADDRESS        ( 1 << VMK_ISCSI_HOST_PARAM_IPADDRESS      )
#define VMK_ISCSI_HOST_INITIATOR_NAME   ( 1 << VMK_ISCSI_HOST_PARAM_INITIATOR_NAME )
#define VMK_ISCSI_HOST_NETDEV_NAME      ( 1 << VMK_ISCSI_HOST_PARAM_NETDEV_NAME    )

typedef enum iscsi_err {
   ISCSI_OK                  = VMK_ISCSI_OK,                   //OK
   ISCSI_ERR_DATASN          = VMK_ISCSI_ERR_DATASN,           //Data packet sequence number incorrect
   ISCSI_ERR_DATA_OFFSET     = VMK_ISCSI_ERR_DATA_OFFSET,      //Data packet offset number incorrect
   ISCSI_ERR_MAX_CMDSN       = VMK_ISCSI_ERR_MAX_CMDSN,        //Exceeded Max cmd sequence number
   ISCSI_ERR_EXP_CMDSN       = VMK_ISCSI_ERR_EXP_CMDSN,        //Command sequence number error
   ISCSI_ERR_BAD_OPCODE      = VMK_ISCSI_ERR_BAD_OPCODE,       //Invalid iSCSI OP code
   ISCSI_ERR_DATALEN         = VMK_ISCSI_ERR_DATALEN,          //Data length error
                                                               // ( i.e.: exceeded max data len, etc )
   ISCSI_ERR_AHSLEN          = VMK_ISCSI_ERR_AHSLEN,           //AHS Length error
   ISCSI_ERR_PROTO           = VMK_ISCSI_ERR_PROTO,            //Protocol violation ( A bit generic? )
   ISCSI_ERR_LUN             = VMK_ISCSI_ERR_LUN,              //Invalid LUN
   ISCSI_ERR_BAD_ITT         = VMK_ISCSI_ERR_BAD_ITT,          //Invalid ITT
   ISCSI_ERR_CONN_FAILED     = VMK_ISCSI_ERR_CONN_FAILED,      //Connection Failure
   ISCSI_ERR_R2TSN           = VMK_ISCSI_ERR_R2TSN,            //Ready to send sequence error
   ISCSI_ERR_SESSION_FAILED  = VMK_ISCSI_ERR_SESSION_FAILED,   //Session Failed ( Logout from target,
                                                               // all connections down )
   ISCSI_ERR_HDR_DGST        = VMK_ISCSI_ERR_HDR_DGST,         //Header Digest invalid
   ISCSI_ERR_DATA_DGST       = VMK_ISCSI_ERR_DATA_DGST,        //Data digest invalid
   ISCSI_ERR_PARAM_NOT_FOUND = VMK_ISCSI_ERR_PARAM_NOT_FOUND,  //Invalid/Unsupported Parameter
   ISCSI_ERR_NO_SCSI_CMD     = VMK_ISCSI_ERR_NO_SCSI_CMD,      //Invalid iSCSI command
} iSCSIErrCode_t;

/*
 * These flags describes reason of stop_conn() call
 */
#define STOP_CONN_TERM          VMK_ISCSI_STOP_CONN_TERM
#define STOP_CONN_RECOVER       VMK_ISCSI_STOP_CONN_RECOVER
#define STOP_CONN_CLEANUP_ONLY  VMK_ISCSI_STOP_CONN_CLEANUP_ONLY

/*
 * These flags presents iSCSI Data-Path capabilities.
 */
#define CAP_RECOVERY_L0         (1 << VMK_ISCSI_CAP_RECOVERY_L0)
#define CAP_RECOVERY_L1         (1 << VMK_ISCSI_CAP_RECOVERY_L1)
#define CAP_RECOVERY_L2         (1 << VMK_ISCSI_CAP_RECOVERY_L2)
#define CAP_MULTI_R2T           (1 << VMK_ISCSI_CAP_MULTI_R2T)
#define CAP_HDRDGST             (1 << VMK_ISCSI_CAP_HDRDGST)
#define CAP_DATADGST            (1 << VMK_ISCSI_CAP_DATADGST)
#define CAP_MULTI_CONN          (1 << VMK_ISCSI_CAP_MULTI_CONN)
#define CAP_TEXT_NEGO           (1 << VMK_ISCSI_CAP_TEXT_NEGO)
#define CAP_MARKERS             (1 << VMK_ISCSI_CAP_MARKERS)
#define CAP_FW_DB               (1 << VMK_ISCSI_CAP_FW_DB)
#define CAP_SENDTARGETS_OFFLOAD (1 << VMK_ISCSI_CAP_SENDTARGETS_OFFLOAD)
#define CAP_DATA_PATH_OFFLOAD   (1 << VMK_ISCSI_CAP_DATA_PATH_OFFLOAD)
#define CAP_SESSION_PERSISTENT  (1 << VMK_ISCSI_CAP_SESSION_PERSISTENT)
#define CAP_IPV6                (1 << VMK_ISCSI_CAP_IPV6)
#define CAP_RDMA                (1 << VMK_ISCSI_CAP_RDMA)
#define CAP_USER_POLL           (1 << VMK_ISCSI_CAP_USER_POLL)
#define CAP_KERNEL_POLL         (1 << VMK_ISCSI_CAP_KERNEL_POLL)
#define CAP_CONN_CLEANUP        (1 << VMK_ISCSI_CAP_CONN_CLEANUP)

/**********************************************************************
 *   Media Driver API Definitions
 **********************************************************************/
#define MAX_PARAM_BUFFER_SZ   VMK_ISCSI_MAX_PARAM_BUFFER_SZ

struct iscsi_cls_conn
{
   void                       *dd_data;  //This is private use for the Media
                                         //  Driver.  It should contain all
                                         //  pertinent information for the
                                         //  Media Driver to be able to manage
                                         //  a connection. The ML does not
                                         //  inspect the contents of this field.
   struct iscsi_cls_session   *session;  // Session holding this conn

   vmk_uint64                 last_rx_time_jiffies;

   void                       *private;
};

struct iscsi_cls_session
{
   void                       *dd_data;    //This is private use for the Media
                                           // Driver.  It should contain all
                                           // pertinent information for the
                                           // Media Driver to be able to carry
                                           // the session. The ML does not
                                           // check the contents of this field.

   struct iscsi_transport     *transport;  //transport holding this session

   struct Scsi_Host           *host;      //Host creating this session (opaque)
   vmk_uint32                 hostNo;      //Host number for this session

   vmk_uint32                 targetID;    //Target ID for this session
   vmk_uint32                 channelID;   //Channel ID for this session

   vmk_int32                  recovery_tmo;   // Timeout in seconds

   struct device              *device;         // Device structure pointer (opaque)

   void                       *private;
};


#define ISCSI_STATS_CUSTOM_DESC_MAX  VMK_ISCSI_STATS_CUSTOM_DESC_MAX

struct iscsi_stats_custom {
        vmk_int8 desc[ISCSI_STATS_CUSTOM_DESC_MAX];
        vmk_uint64 value;
};

struct iscsi_stats {
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
         * extend existing MIB statistics with its own specific statistics
         * up to ISCSI_STATS_CUSTOM_MAX
         */
        vmk_uint32 custom_length;
        struct iscsi_stats_custom custom[0]
                __attribute__ ((aligned (sizeof(vmk_uint64))));
};

enum iscsi_tgt_dscvr
{
   ISCSI_TGT_DSCVR_SEND_TARGETS  = VMK_ISCSI_TGT_DSCVR_SEND_TARGETS,
   ISCSI_TGT_DSCVR_ISNS          = VMK_ISCSI_TGT_DSCVR_ISNS,
   ISCSI_TGT_DSCVR_SLP           = VMK_ISCSI_TGT_DSCVR_SLP
};

struct iscsi_transport
{
   /* Pointer to module that owns this instance */
   struct module             *owner;

   /* Name of the module */
   char                      *name;

   /* Descriptive name of the module */
   char                      *description;

   /* Capabilities */
   vmk_uint64                 caps;
   vmk_IscsiTransCapabilitiesProperty capsProperty;

   /* Parameter mask. Controls the param types that are sent to
    *  get_session_param(),set_param(),get_conn_param()
    */
   vmk_uint64                 param_mask;
   vmk_IscsiTransIscsiParamProperty iscsiParamProperty;

   /*
    * Host Parameter mask, this controls the param types that
    * will be set to the Media Driver in the get_host_param()
    * call
    */
   vmk_uint64                 host_param_mask;
   vmk_IscsiTransHostParamProperty hostParamProperty;

   /*
    * Pointer to the scsi_host_template structure.  The template
    * must be filled in as IO requests will be sent to the Media
    * Drivers via the scsi templates queuecommand function
    */
   struct scsi_host_template *host_template;

   /* Connection data size */
   vmk_int32                  conndata_size;

   /* Session data size */
   vmk_int32                  sessiondata_size;

   /* Max Lun */
   vmk_int32                  max_lun;

   /* Max connections */
   vmk_int32                  max_conn;

   /* Maximum Command Length */
   vmk_int32                  max_cmd_len;

   /*
    * Create and initialize a session class object. Do not
    * start any connections.
    */
   struct iscsi_cls_session   *(*create_session)
                                (struct iscsi_transport *,
                                struct scsi_transport_template *,
                                vmk_uint16  maxCmds,
                                vmk_uint16  qDepth,
                                vmk_uint32  initialCmdSeqNum,
                                vmk_uint32* hostNumber);
   /*
    * Create and initialize a session class object. Do not
    * start any connections. ( persistent target version )
    */
   struct iscsi_cls_session   *(*create_session_persistent)
                                (struct iscsi_transport *,
                                void *,
                                vmk_uint16  maxCmds,
                                vmk_uint16  qDepth,
                                vmk_uint32  initialCmdSeqNum,
                                vmk_uint32  targetID,
                                vmk_uint32  channelID,
                                vmk_uint32* hostNumber);

   /* Destroy a session class object and tear down a session */
   void                       (*destroy_session)
                                (struct iscsi_cls_session *);
   /* Create a connection class object */
   struct iscsi_cls_conn     *(*create_conn)
                               (struct iscsi_cls_session *,
                               vmk_uint32);
   /* Bind a socket created in user space to a session. */
   vmk_int32                  (*bind_conn)
                                (struct iscsi_cls_session *,
                                 struct iscsi_cls_conn *,
                                 vmk_uint64, vmk_int32);
   /* Enable connection for IO ( Connect ) */
   vmk_int32                  (*start_conn)
                                (struct iscsi_cls_conn *);
   /* Stop IO on the connection ( Disconnect ). It is expected
    * that the driver completes this call in less than 100ms.
    */
   vmk_int32                  (*stop_conn)
                                (struct iscsi_cls_conn *,
                                 vmk_int32);
   /* Destroy a session connection */
   void                       (*destroy_conn)
                                (struct iscsi_cls_conn *);
   /* Retrieve session parameters from Media Driver */
   vmk_int32                  (*get_session_param)
                                (struct iscsi_cls_session *,
                                 enum iscsi_param,
                                 vmk_int8 *);
   /* Set connection parameters from Media Driver */
   vmk_int32                  (*set_param)
                                (struct iscsi_cls_conn *,
                                 enum iscsi_param,
                                 vmk_int8 *,
                                vmk_int32);
   /* Retrieve connection parameters from Media Driver */
   vmk_int32                  (*get_conn_param)
                                (struct iscsi_cls_conn *,
                                 enum iscsi_param,
                                 vmk_int8 *);
   /* Retrieve host configuration parameters from Media Driver */
   vmk_int32                  (*get_host_param)
                                (struct Scsi_Host *,
                                 enum iscsi_host_param,
                                 vmk_int8 *);

   /* Set a host configuration parameters for a Media Driver */
   vmk_int32                  (*set_host_param)
                                (struct Scsi_Host *,
                                 enum iscsi_host_param,
                                 vmk_int8 *valueToSet,
                                 vmk_int32 nBytesInBuffer);
   /*
    * Send a data PDU to a target. This is a redirect from an
    * external source, like the iscsid daemon.
    */
   vmk_int32                  (*send_pdu)
                                 (struct iscsi_cls_conn *,
                                  struct iscsi_hdr *,
                                  vmk_int8 *, vmk_uint32);
   /* Retrieve per connection statistics from the Media Driver */
   void                       (*get_stats)
                                 (struct iscsi_cls_conn *,
                                 struct iscsi_stats *);
   /*
    * This will be called by the ML when the session recovery
    * timer has expired and the driver will be opened back up
    * for IO.
    */
   void                       (*session_recovery_timedout)
                                 (struct iscsi_cls_session *session);
   /*
    * Connect to the specified destination through a Media Driver.
    * This can be used to get a handle on a socket like handle of
    * the Media Driver to allow user space to drive recovery
    * through iscsid. If the media driver can not handle the call
    * right away and would like the daemon to call the ep_connect()
    * again, the driver should return -EBUSY.
    *
    * It is expected that the driver completes this call in less than 100ms.
    */
   vmk_int32                  (*ep_connect)
                                 (struct sockaddr *dst_addr,
                                  vmk_int32 non_blocking,
                                  vmk_uint64 *ep_handle);

   /*
    * Connect to the specified destination through a Media Driver.
    * This can be used to get a handle on a socket like handle of
    * the Media Driver to allow user space to drive recovery
    * through iscsid. This extended version provides a handle
    * to the vmknic to use. If the media driver can not handle the call
    * right away and would like the daemon to call the ep_connect()
    * again, the driver should return -EBUSY.
    *
    * It is expected that the driver completes this call in less than 100ms.
    */
   vmk_int32                  (*ep_connect_extended)
                                 (struct sockaddr *dst_addr,
                                  vmk_int32 non_blocking,
                                  vmk_uint64 *ep_handle,
                                  vmk_IscsiNetHandle iscsiNetHandle);

   /*
    * Poll for an event from the Media Driver
    *   Not currently used by any Media Driver
    */
   vmk_int32                  (*ep_poll)
                                 (vmk_uint64 , vmk_int32);
   /*
    * Disconnect the socket channel though the Media Driver. It is expected
    * that the driver completes this call in less than 100ms.
    */
   void                       (*ep_disconnect)
                                 (vmk_int64);
   /*
    * Perform a target discovery on a specific ip address
    *   Not currently used by any Media Driver
    */
   vmk_int32                  (*tgt_dscvr)
                                 (struct Scsi_Host *,
                                  enum iscsi_tgt_dscvr,
                             vmk_uint32 enable,
                                  struct sockaddr *);

   vmk_int32                  (*get_transport_limit)
                                 (struct iscsi_transport *,
                                 enum iscsi_param,
                                 vmk_IscsiTransTransportParamLimits *,
                                 vmk_int32 listMaxLen);

   struct scsi_transport_template   *scsi_template; //template for scsi interface

   void                             *private;
};

/*  End Media Driver API Definitions */

/**********************************************************************
 *  Middle Layer Support Functions Prototypes
 **********************************************************************/

/**
 *  iscsi_alloc_session - Initialize Mid-Layer class object (representing a session)
 *  @host: Pointer to the scsi_host that will own this target
 *  @transport: The transport template used passed in as part of iscsi_register_transport
 *
 *  Initialize the Mid-Layer specific class object that represents a session.
 *  Note that the size of iscsi_cls_session will vary depending on the Mid-Layer
 *  implementation. (IE: Open iSCSI version vs VMware proprietary version) The
 *  new session is not added to the Mid-Layer list of sessions for this Media 
 *  Driver. Media Drivers should use iscsi_create_session.
 *
 *  RETURN VALUE: 
 *  Pointer to the newly allocated session.
 */
/* _VMKLNX_CODECHECK_: iscsi_alloc_session */
extern struct iscsi_cls_session * iscsi_alloc_session(
   struct Scsi_Host       *host,
   struct iscsi_transport *transport
);

/**
 *  iscsi_add_session -  Adds the session object to the Mid Layer
 *  @session: A session previously created with iscsi_alloc_session
 *  @target_id: The target ID for this session
 *  @channel: Channel ID for the iSCSI session
 *
 *  Adds the session object to the Mid Layer thus exposing the target to the 
 *  Operating System as a SCSI target.
 *
 *  RETURN VALUE: 
 *  0 on success, otherwise an OS error 
 */
/* _VMKLNX_CODECHECK_: iscsi_add_session */
extern int iscsi_add_session(
   struct iscsi_cls_session *session,
   unsigned int             target_id,
   unsigned int             channel
);

/**
 *  iscsi_if_create_session_done -  Callback to notify session is done
 *  @connection: The connection that is carrying the session 
 *
 *  This is a callback from a Media Driver when it has completed creating a 
 *  session or an existing session has gone into the "LOGGED" in state.  It 
 *  can be used by a Hardware Media Driver that brings its sessions up on 
 *  boot. (Currently Not Used)
 *
 *  RETURN VALUE: 
 *  0 on success, otherwise an OS error 
 */
/* _VMKLNX_CODECHECK_: iscsi_if_create_session_done */
extern int iscsi_if_create_session_done(
   struct iscsi_cls_conn *connection
);

/**
 *  iscsi_if_destroy_session_done -  Callback to notify session is removed. 
 *  @connection: The connection that is carrying the session 
 *
 *  This is a callback from the Media Driver to inform the Mid Layer that a 
 *  particular session was removed by the Media Driver. (Currently Not Used)
 *
 *  RETURN VALUE: 
 *  0 on success, otherwise an OS error 
 */
/* _VMKLNX_CODECHECK_: iscsi_if_destroy_session_done */
extern int iscsi_if_destroy_session_done(
   struct iscsi_cls_conn *connection
);

/**
 *  iscsi_if_connection_start_done -  Callback to notify connection is started 
 *  @connection: The connection that is carrying the session 
 *  @error: Error code for the operation 
 *
 *  This is a callback from a Media Driver when it has completed the 
 *  connection start operation. 
 *
 *  RETURN VALUE: 
 *  0 on success, otherwise an OS error 
 */
/* _VMKLNX_CODECHECK_: iscsi_if_connection_start_done */
extern int iscsi_if_connection_start_done(
   struct iscsi_cls_conn *connection, int error
);

/**
 *  iscsi_if_connection_stop_done -  Callback to notify connection is stopped 
 *  @connection: The connection that is carrying the session 
 *  @error: Error code for the operation 
 *
 *  This is a callback from a Media Driver when it has completed the 
 *  connection stop operation. 
 *
 *  RETURN VALUE: 
 *  0 on success, otherwise an OS error 
 */
/* _VMKLNX_CODECHECK_: iscsi_if_connection_stop_done */
extern int iscsi_if_connection_stop_done(
   struct iscsi_cls_conn *connection, int error
);

/**
 *  iscsi_create_session - Allocates a new iscsi_cls_session object 
 *  @host: The host that will be used to create the session 
 *  @transport: Pointer to the iscsi_transport template that was previously registered for this host (Media Driver) 
 *  @target_id:  Target ID for the session  
 *  @channel: Channel ID for the iSCSI session
 *
 *  This function allocates a new iscsi_cls_session object and adds it to 
 *  the Media Drivers list of sessions. This is done by calling 
 *  iscsi_alloc_session followed by iscsi_add_session.  Most Media Drivers 
 *  should use this interface.
 *
 *  RETURN VALUE: 
 *  Pointer to the newly created session.
 */
/* _VMKLNX_CODECHECK_: iscsi_create_session */
extern struct iscsi_cls_session * iscsi_create_session(
   struct Scsi_Host       *host,
   struct iscsi_transport *transport,
   vmk_uint32             target_id,
   vmk_uint32             channel
);

/**
 *  iscsi_offline_session - Offline a session (mark device as SDEV_OFFLINE) 
 *  @session: The session to offline from the Mid Layer 
 *
 *  Prevent additional I/O from being sent to the Media Driver through queue 
 *  command.  In addition update scsi_device states to mark this device 
 *  SDEV_OFFLINE.  Then notify the upper layer to rescan the path.
 *
 *  RETURN VALUE: 
 *  None
 */
/* _VMKLNX_CODECHECK_: iscsi_create_session */
extern  void iscsi_offline_session(
   struct iscsi_cls_session *session
);

/**
 * iscsi_lost_session - Report a session lost from Mid Layer
 * @sptr: The session lost from the Mid Layer
 *
 * Report that a session was lost and the target has authoritatively states
 * the session is not coming back. This marks the path associated with this
 * session lost, as a step to marking Permanent Device Loss (PDL).
 *
 * RETURN VALUE:
 * None
 */
/* _VMKLNX_CODECHECK_: iscsi_lost_session */
extern void iscsi_lost_session(
   struct iscsi_cls_session *sptr
);

/**
 *  iscsi_remove_session - Remove a session from Mid Layer
 *  @session: The session to remove from the Mid Layer
 *
 *  Remove the specified session from the Mid Layer. The Mid Layer is
 *  responsible for removing the scsi target from the SCSI layer as well as
 *  ensuring any recovery work is cleaned up.
 *
 *  RETURN VALUE:
 *  None
 */
/* _VMKLNX_CODECHECK_: iscsi_remove_session */
extern  void iscsi_remove_session(
   struct iscsi_cls_session *session
);

/**
 *  iscsi_free_session - Release the scsi device associated with a session 
 *  @session: The session to be freed
 *
 *  Release the scsi device associated with this session, this should have 
 *  the effect of cleaning up the session object if this is the last 
 *  reference to the session.  Delete this session's class object freeing 
 *  its memory.
 *
 *  RETURN VALUE: 
 *  None
 */
/* _VMKLNX_CODECHECK_: iscsi_free_session */
extern  void iscsi_free_session(
   struct iscsi_cls_session *session
);

/**
 *  iscsi_destroy_session - Removes the session from the Mid Layer 
 *  @session: Session to shutdown/destroy 
 *
 *  Reverse of iscsi_create_session.  Removes the session from the Mid Layer 
 *  and dereferences any scsi devices. 
 *
 *  RETURN VALUE: 
 *  0 on success otherwise an OS error 
 */
/* _VMKLNX_CODECHECK_: iscsi_destroy_session */
extern int iscsi_destroy_session(
   struct iscsi_cls_session *session
);

/**
 *  iscsi_create_conn - Create a new connection on a session 
 *  @session: The session that we are requesting a new connection on
 *  @connectionID: The connection ID/number
 *
 *  Create a new connection to the target on the specified session.
 *
 *  RETURN VALUE: 
 *  pointer to the newly created connection 
 */
/* _VMKLNX_CODECHECK_: iscsi_create_conn */
extern struct iscsi_cls_conn * iscsi_create_conn(
   struct iscsi_cls_session *session,
   vmk_uint32               connectionID
);

/**
 *  iscsi_destroy_conn - Close the connection and remove it from session list 
 *  @connection: The connection to shutdown
 *
 *  Close the connection at conn and remove it from the session list. Leave 
 *  the session in place.
 *
 *  RETURN VALUE: 
 *  0 on success otherwise an OS error 
 */
/* _VMKLNX_CODECHECK_: iscsi_destroy_conn */
extern int iscsi_destroy_conn(
   struct iscsi_cls_conn *connection
);

/**
 *  iscsi_unblock_session - Unblock a session (Allow additional IO to driver) 
 *  @session: The session to unblock 
 *
 *  Allow additional IO work to be sent to the driver through it's 
 *  queuecommand function. It may be necessary to block the Operating System 
 *  IO queue.
 *
 *  RETURN VALUE: 
 *  None, function must always succeed 
 */
/* _VMKLNX_CODECHECK_: iscsi_unblock_session */
extern  void iscsi_unblock_session(
   struct iscsi_cls_session *session
);

/**
 *  iscsi_block_session - Block a session (Prevent additional IO to driver) 
 *  @session: The session to block 
 *
 *  Prevent additional IO from being sent to the Media Driver through 
 *  queuecommand. Any session recovery work should still continue.
 *  It is important that all the pending and active IO's must be flushed
 *  and returned with appropriate error code to ESX SCSI mid-layer before
 *  calling this interface.
 *
 *  RETURN VALUE: 
 *  None, function must always succeed 
 */
/* _VMKLNX_CODECHECK_: iscsi_block_session */
extern  void iscsi_block_session(
   struct iscsi_cls_session *session
);

/**
 *  iscsi_register_transport - Register the Media Driver with the transport Mid Layer 
 *  @transport: definition for this Media Driver 
 *
 *  Register this Media Driver with the transport Mid-Layer. This is the first 
 *  step required in enabling the Media Driver to function in the Open iSCSI 
 *  framework.  The Mid-Layer will then query the driver for various
 *  information as well as signal when to start operation.
 *
 *  RETURN VALUE: 
 *  The transport template
 */
/* _VMKLNX_CODECHECK_: iscsi_register_transport */
extern struct scsi_transport_template * iscsi_register_transport(
   struct iscsi_transport *transport
);

/**
 *  iscsi_unregister_transport - Unregister the Media Driver
 *  @transport: The iSCSI transport template previously registered 
 *
 *  This function is to unregister the iSCSI transport template we previously
 *  registered using the iscsi_register_transport call. It is called in the 
 *  clean up path of the Media Driver when we are trying to unload it from 
 *  the kernel.
 *
 *  RETURN VALUE: 
 *  Always zero. (Should return an error if the driver is still in use?) 
 */
/* _VMKLNX_CODECHECK_: iscsi_unregister_transport */
extern int iscsi_unregister_transport(
   struct iscsi_transport *transport
);

/**
 *  iscsi_register_host - Register the iSCSI Host Bus Adapter
 *  @hptr: The iSCSI Host Bus Adapter registered with ESX SCSI
 *  @tptr: The iSCSI transport template previously registered
 *
 *  This function is to register and associate the iSCSI Host Bus Adapter
 *  with iSCSI transport. This interface has to be called after registering the
 *  iSCSI transport and iSCSI Host Bus Adapter with the ESX SCSI Stack.
 *
 *  RETURN VALUE:
 *  0 on success otherwise an OS error
 *  
 */
/* _VMKLNX_CODECHECK_: iscsi_register_host */
extern vmk_int32 iscsi_register_host(
   struct Scsi_Host *hptr,
   struct iscsi_transport *tptr
);

/**
 *  iscsi_unregister_host - Unregister the iSCSI Host Bus Adapter from iSCSI tranport
 *  @hptr: The iSCSI Host Bus Adapter registered with ESX SCSI
 *  @tptr: The iSCSI transport template previously registered
 *
 *  This function is to unregister and remove all references to iSCSI Host Bus
 *  Adapter from iSCSI transport. It is called in the clean up path of the
 *  Media Driver when we are trying to unload it from the kernel.
 *
 *  RETURN VALUE:
 *  0 on success otherwise an OS error
 *  
 */
/* _VMKLNX_CODECHECK_: iscsi_unregister_host */
extern vmk_int32 iscsi_unregister_host(
   struct Scsi_Host *hptr,
   struct iscsi_transport *tptr
);

/**
 *  iscsi_conn_error - Inform user space of a connection error 
 *  @connection: The connection that we had an error on 
 *  @error: The iSCSI error code that was encountered: ISCSI_ERR_CONN_FAILED 
 *
 *  This up call must be made to the Mid Layer when a connection fails. IE: a 
 *  target disconnect occurs, etc. This function must then inform the 
 *  user-space process of a connection error by sending a 
 *  ISCSI_KEVENT_CONN_ERROR event packet.
 *
 *  RETURN VALUE: 
 *  NONE 
 */
/* _VMKLNX_CODECHECK_: iscsi_conn_error */
extern  void iscsi_conn_error(
   struct iscsi_cls_conn *connection,
   iSCSIErrCode_t         error
);

/**
 *  iscsi_recv_pdu -  Invoke the Mid-Layer management code handle a specific PDU 
 *  @connection: The connection the PDU arrived on 
 *  @PDU: The iSCSI PDU Header 
 *  @data: Any additional data that arrived with this PDU 
 *  @nByteOfData: The size of the additional data 
 *
 *  Called to have the Mid-Layer management code handle a specific PDU for us. 
 *  The required PDU's that must be sent to the Mid Layer are:
 *     ISCSI_OP_NOOP_IN,
 *     ISCSI_OP_ASYNC_EVENT,
 *     ISCSI_OP_TEXT_RSP
 *  The mid layer is then responsible for sending this PDU up to the 
 *  user-space daemon for processing using an ISCSI_KEVENT_RECV_PDU event 
 *  packet. Recv pdu will also need to handle asynchronous events and not 
 *  just replies to previous send_pdu packets.
 *
 *  RETURN VALUE: 
 *  OS Error. ( ENOMEM, EINVA, etc ) 
 */
/* _VMKLNX_CODECHECK_: iscsi_recv_pdu */
extern int iscsi_recv_pdu(
   struct iscsi_cls_conn *connection,
   void      *PDU,
   char                  *data,
   vmk_uint32            nByteOfData
);

/**
 * iscsi_scan_target - Perform a scsi_scan_target on the specified target
 * @dptr: Device Pointer
 * @chan: Channel
 * @id: SCSI ID
 * @lun: SCSI LUN
 * @rescan: If non-zero, update path status
 *
 * This function is called by iscsi_trans to perform the appropriate
 * target scan.
 *
 * RETURN VALUE:
 * Status Code
 */
/* _VMKLNX_CODECHECK_: iscsi_scan_target */
extern void iscsi_scan_target(
   struct device *dptr,
   uint chan,
   uint id,
   uint lun,
   int rescan
);

/**
 *  iscsi_lookup_session - Lookup an iSCSI session for a given host no,
 *                         channel id and target id combination.
 *
 *  @host_no: iSCSI Host Bust Adapter number
 *  @channel: Channel ID for the iSCSI session
 *  @id: Target ID for the iSCSI session
 *
 *  This function looks up and returns the handle to iscsi_cls_session for
 *  a given host no, channel id and target id.
 *
 *  RETURN VALUE:
 *  On success, returns Pointer to the existing iscsi_cls_session,
 *  otherwise returns NULL.
 *
 */
/* _VMKLNX_CODECHECK_: iscsi_lookup_session */
extern struct iscsi_cls_session *iscsi_lookup_session(
   int host_no,
   int channel,
   int id
);

/**
 *  iscsi_sdevice_to_session - Returns handle to iscsi_cls_session
 *                         corresponding to the scsi_device passed.
 *
 *  @sdev: Pointer to scsi_device
 *
 *  This function looks up and returns the handle to iscsi_cls_session for
 *  the given scsi_device.
 *
 *  RETURN VALUE:
 *  On success, returns Pointer to the existing iscsi_cls_session,
 *  otherwise returns NULL.
 *
 */
/* _VMKLNX_CODECHECK_: iscsi_sdevice_to_session */
extern struct iscsi_cls_session *iscsi_sdevice_to_session(
   struct scsi_device *sdev
);

/* End Middle Layer Support Prototypes */

#endif/*_ISCSI_TRANSPORT_H_*/
