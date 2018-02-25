/* **********************************************************
 * Copyright 2007 - 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * iSCSI Externally Exported Interfaces (Incompatible)            */ /**
 * \addtogroup ISCSI ISCSI Interfaces
 * @{
 *
 * \addtogroup IscsiTransport iSCSI Transport public interfaces
 *
 * Vmkernel-specific iSCSI constants & types which are shared with
 * user-mode and driver code.
 * @{
 *
 ***********************************************************************
 */

#ifndef _VMKAPI_ISCSI_TRANSPORT_INCOMPAT_H
#define _VMKAPI_ISCSI_TRANSPORT_INCOMPAT_H

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif

#include "scsi/vmkapi_scsi_types.h"
#include "iscsi/vmkapi_iscsi_transport_compat.h"
/** \endcond never */

/** \cond never */
struct vmk_IscsiTransConnection;
/**
 * \brief iSCSI transport connection object.
 */
typedef struct vmk_IscsiTransConnection vmk_IscsiTransConnection;

struct vmk_IscsiTransSession;
/**
 * \brief iSCSI transport session object.
 */
typedef struct vmk_IscsiTransSession vmk_IscsiTransSession;

struct vmk_IscsiTransTransport;
/**
 * \brief iSCSI transport transport object.
 */
typedef struct vmk_IscsiTransTransport vmk_IscsiTransTransport;
/** \endcond never */

/*
 ***********************************************************************
 * vmk_IscsiCreateSessionDrvIntf--                                */ /**
 *
 * \ingroup IscsiTransport
 *
 * \brief Driver entry point interface called by iSCSI transport to
 *        create a session object.
 *
 * \note This callback may block.
 *
 * \param[in]  transport         Pointer to vmk_IscsiTransTransport.
 * \param[in]  maxCmds           Maximum concurrent tasks allowed per
 *                               session.
 * \param[in]  qDepth            Qdepth per session.
 * \param[in]  initialCmdSeqNum  Initial command sequence number to be
 *                               used for the session.
 * \param[in]  targetId          Target ID for the session.
 * \param[in]  channelId         Channel ID for the session.
 * \param[out] hostNumber        Host number used by the driver is
 *                               returned in this argument.
 * \param[out] outSession        Newly created session is returned
 *                               here.
 *
 * \return VMK_OK, if session is successfully created. All other
 *         return codes indicates failure.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_IscsiCreateSessionDrvIntf) (
   vmk_IscsiTransTransport *transport,
   vmk_uint16              maxCmds,
   vmk_uint16              qDepth,
   vmk_uint32              initialCmdSeqNum,
   vmk_uint32              targetId,
   vmk_uint32              channelId,
   vmk_uint32              *hostNumber,
   vmk_IscsiTransSession   **outSession);

/*
 ***********************************************************************
 * vmk_IscsiDestroySessionDrvIntf--                               */ /**
 *
 * \ingroup IscsiTransport
 *
 * \brief Driver entry point interface called by iSCSI transport to
 *        destroy a session object.
 *
 * \note This callback may block.
 *
 * \param[in]  sess  Pointer to session object to be destroyed.
 *
 * \return VMK_OK, if session is successfully destroyed. All other
 *         return codes indicates failure.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_IscsiDestroySessionDrvIntf) (
   vmk_IscsiTransSession   *sess);

/*
 ***********************************************************************
 * vmk_IscsiCreateConnectionDrvIntf--                             */ /**
 *
 * \ingroup IscsiTransport
 *
 * \brief Driver entry point interface called by iSCSI transport to
 *        create a connection object.
 *
 * \note This callback may block.
 *
 * \param[in]  sess              Pointer to session object for which
 *                               the new connection object to be
 *                               created.
 * \param[in]  connId            Connection ID to be used for the new
 *                               connection.
 * \param[out] outConnection     Newly created connection is returned
 *                               here.
 *
 * \return VMK_OK, if connection is successfully created. All other
 *         return codes indicates failure.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_IscsiCreateConnectionDrvIntf) (
   vmk_IscsiTransSession    *sess,
   vmk_uint32               connId,
   vmk_IscsiTransConnection **outConnection);

/*
 ***********************************************************************
 * vmk_IscsiBindConnectionDrvIntf--                               */ /**
 *
 * \ingroup IscsiTransport
 *
 * \brief Driver entry point interface called by iSCSI transport to
 *        associate a connection endpoint to a connection object.
 *
 * \note This callback may not block.
 *
 * \param[in]  sess         Pointer to session object the connection
 *                          belongs.
 * \param[in]  conn         Pointer to connection object to be bound.
 * \param[in]  socket       The "socket" or the connection endpoint to
 *                          to be associated with the connection.
 * \param[in]  isLeading    Boolean flag indicating if it is the
 *                          "leading" connection.
 *
 * \return VMK_OK, if bind operation is successfull. All other return
 *         code indicates failure.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_IscsiBindConnectionDrvIntf) (
   vmk_IscsiTransSession    *sess,
   vmk_IscsiTransConnection *conn,
   vmk_uint64               socket,
   vmk_Bool                 isLeading);

/*
 ***********************************************************************
 * vmk_IscsiStartConnectionDrvIntf--                              */ /**
 *
 * \ingroup IscsiTransport
 *
 * \brief Driver entry point interface called by iSCSI transport to
 *        enable the connection for IO.
 *
 * \note This callback may block.
 *
 * \param[in]  conn  Pointer to the connection object to be enabled.
 *
 * \return VMK_OK, if bind operation is successfull. All other return
 *         code indicates failure.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_IscsiStartConnectionDrvIntf) (
   vmk_IscsiTransConnection *conn);

/*
 ***********************************************************************
 * vmk_IscsiStopConnectionDrvIntf--                               */ /**
 *
 * \ingroup IscsiTransport
 *
 * \brief Driver entry point interface called by iSCSI transport to
 *        disable the connection for IO and dis-associate the socket
 *        from the connection.
 *
 * \note This callback may block.
 *
 * \param[in]  conn  Pointer to the connection object to be disabled.
 * \param[in]  mode  The reason for calling the interface.
 *
 * \retval  VMK_OK               If connection has successfully
 *                               completed stop operation.
 * \retval  VMK_STATUS_PENDING   If connection stop operation is
 *                               pending completion. The driver is
 *                               required to asynchrounsly call
 *                               vmk_IscsiTransportConnectionStopDone()
 *                               to complete the stop operation.
 *
 * \note All other return codes indicates failure.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_IscsiStopConnectionDrvIntf) (
   vmk_IscsiTransConnection *conn,
   vmk_IscsiStopConnMode    mode);

/*
 ***********************************************************************
 * vmk_IscsiDestroyConnectionDrvIntf--                            */ /**
 *
 * \ingroup IscsiTransport
 *
 * \brief Driver entry point interface called by iSCSI transport to
 *        destroy the connection object.
 *
 * \note This callback may block.
 *
 * \param[in]  conn  Pointer to the connection object to be destroyed.
 *
 * \return VMK_OK, if connection is destroyed successfully. All other
 *         return codes indicates failure.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_IscsiDestroyConnectionDrvIntf) (
   vmk_IscsiTransConnection *conn);

/*
 ***********************************************************************
 * vmk_IscsiGetSessionParamDrvIntf--                              */ /**
 *
 * \ingroup IscsiTransport
 *
 * \brief Driver entry point interface called by iSCSI transport to
 *        obtain session parameter values.
 *
 * \note This callback may not block.
 *
 * \param[in]  sess     Pointer to session object for which the
 *                      parameter applies.
 * \param[in]  param    The parameter to get.
 * \param[out] dataBuf  Pointer to the data buffer where the value
 *                      is to be returned.
 * \param[out] dataLen  Pointer to the data len to be filled by the
 *                      driver.
 *
 * \return VMK_OK, if the get operation is successful. All other
 *         return codes indicates failure.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_IscsiGetSessionParamDrvIntf) (
   vmk_IscsiTransSession    *sess,
   vmk_IscsiTransIscsiParam param,
   vmk_int8                 *dataBuf,
   vmk_int32                *dataLen);

/*
 ***********************************************************************
 * vmk_IscsiSetParamDrvIntf--                                     */ /**
 *
 * \ingroup IscsiTransport
 *
 * \brief Driver entry point interface called by iSCSI transport to
 *        set session/connection parameter values.
 *
 * \note This callback may not block.
 *
 * \param[in]  conn     Pointer to connection object for which the
 *                      parameter applies.
 * \param[in]  param    The parameter to set
 * \param[in]  dataBuf  Pointer to the data buffer where the value
 *                      is to be obtained from.
 * \param[in]  bufLen   Size of the dataBuf.
 *
 * \return VMK_OK, if the set operation is successful. All other
 *         return codes indicates failure.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_IscsiSetParamDrvIntf) (
   vmk_IscsiTransConnection *conn,
   vmk_IscsiTransIscsiParam param,
   vmk_int8                 *dataBuf,
   vmk_int32                bufLen);

/*
 ***********************************************************************
 * vmk_IscsiGetConnectionParamDrvIntf--                           */ /**
 *
 * \ingroup IscsiTransport
 *
 * \brief Driver entry point interface called by iSCSI transport to
 *        obtain connection parameter values.
 *
 * \note This callback may not block.
 *
 * \param[in]  conn     Pointer to connection object for which the
 *                      parameter applies.
 * \param[in]  param    The parameter to get
 * \param[in]  dataBuf  Pointer to the data buffer where the
 *                      value is to be returned.
 * \param[out] dataLen  Pointer to the data len to be filled by the
 *                      driver.
 *
 * \return VMK_OK, if the get operation is successful. All other
 *         return codes indicates failure.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_IscsiGetConnectionParamDrvIntf) (
   vmk_IscsiTransConnection *conn,
   vmk_IscsiTransIscsiParam param,
   vmk_int8                 *dataBuf,
   vmk_int32                *dataLen);

/*
 ***********************************************************************
 * vmk_IscsiGetHostParamDrvIntf--                                 */ /**
 *
 * \ingroup IscsiTransport
 *
 * \brief Driver entry point interface called by iSCSI transport to
 *        obtain host/adapter parameter values.
 *
 * \note This callback may not block.
 *
 * \param[in]  transport   Pointer to the transport object for which
 *                         the parameter applies
 * \param[in]  hostPrivate Pointer to hostPrivate object that was
 *                         registered using
 *                         vmk_IscsiTransportRegisterAdapter()
 * \param[in]  param       The parameter to get
 * \param[out] dataBuf     Pointer to the data buffer where the value
 *                         is to be returned.
 * \param[out] dataLen     Pointer to the data len to be filled by the
 *                         driver.
 *
 * \return VMK_OK, if the get operation is successful. All other
 *         return codes indicates failure.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_IscsiGetHostParamDrvIntf) (
   vmk_IscsiTransTransport *transport,
   void                    *hostPrivate,
   vmk_IscsiTransHostParam param,
   vmk_int8                *dataBuf,
   vmk_int32               *dataLen);

/*
 ***********************************************************************
 * vmk_IscsiSetHostParamDrvIntf--                                 */ /**
 *
 * \ingroup IscsiTransport
 *
 * \brief Driver entry point interface called by iSCSI transport to
 *        set host/adapter parameter values.
 *
 * \note This callback may block.
 *
 * \param[in]  transport   Pointer to the transport object for which
 *                         the parameter applies
 * \param[in]  hostPrivate Pointer to hostPrivate object that was
 *                         registered using
 *                         vmk_IscsiTransportRegisterAdapter()
 * \param[in]  param       The parameter to get
 * \param[in]  dataBuf     Pointer to the data buffer where the value
 *                         is to be returned.
 * \param[in]  bufLen      Size of the dataBuf
 *
 * \return VMK_OK, if the set operation is successful. All other
 *         return codes indicates failure.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_IscsiSetHostParamDrvIntf) (
   vmk_IscsiTransTransport *transport,
   void                    *hostPrivate,
   vmk_IscsiTransHostParam param,
   vmk_int8                *dataBuf,
   vmk_int32               bufLen);

/*
 ***********************************************************************
 * vmk_IscsiSendPduDrvIntf--                                      */ /**
 *
 * \ingroup IscsiTransport
 *
 * \brief Driver entry point interface called by iSCSI transport to
 *        send raw PDU on the connection. This may queue the PDU to
 *        be later sent asynchronously.
 *
 * \note This callback may not block.
 *
 * \param[in]  conn     Pointer to connection object using which the
 *                      PDU is to be sent.
 * \param[in]  pduHdr   PDU Header
 * \param[in]  pduData  PDU Data
 * \param[in]  dataLen  PDU Data Length
 *
 * \return VMK_OK, if driver is able to the send the PDU successfully,
 *         or if it is able to queue it successfully.
 *         All other return codes indicates failure.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_IscsiSendPduDrvIntf) (
   vmk_IscsiTransConnection *conn,
   void                     *pduHdr,
   vmk_int8                 *pduData,
   vmk_ByteCountSmall       dataLen);

/*
 ***********************************************************************
 * vmk_IscsiGetStatsDrvIntf--                                     */ /**
 *
 * \ingroup IscsiTransport
 *
 * \brief Driver entry point interface called by iSCSI transport to
 *        obtain connection statistics.
 *
 * \note This callback may not block.
 *
 * \param[in]  conn     Pointer to connection object for which the
 *                      statistics applies.
 * \param[in]  stats    Pointer to the statistics object where the
 *                      stats are to be returned.
 *
 * \return VMK_OK, if driver is provide the stats successfully.
 *         All other return codes indicates failure.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_IscsiGetStatsDrvIntf) (
   vmk_IscsiTransConnection *conn,
   vmk_IscsiTransIscsiStats *stats);

/*
 ***********************************************************************
 * vmk_IscsiSessionRecoveryTimedoutDrvIntf--                      */ /**
 *
 * \ingroup IscsiTransport
 *
 * \brief Driver entry point interface called by iSCSI transport
 *        when recovery timer expires.
 *
 *        One of the action the driver could take in this callback
 *        is to start failing the IO with VMK_NO_CONNECT and initiate
 *        the path state probe.
 *
 * \note This callback may not block.
 *
 * \param[in]  sess     Pointer to session object which has expired
 *                      the recovery timer.
 *
 ***********************************************************************
 */
typedef void (*vmk_IscsiSessionRecoveryTimedoutDrvIntf) (
   vmk_IscsiTransSession    *sess);

/*
 ***********************************************************************
 * vmk_IscsiEPConnectExtendedDrvIntf--                            */ /**
 *
 * \ingroup IscsiTransport
 *
 * \brief Driver entry point interface called by iSCSI transport to
 *        establish a TCP connection for a future iSCSI session.
 *        This should just initiate the connect and not wait for it
 *        to complete. The transport will then call vmk_IscsiEPPollDrvIntf()
 *        to wait for the connect to complete.
 *
 * \note This callback may block.
 *
 * \param[in]  transport      Pointer to transport object for which
 *                            the interface applies.
 * \param[in]  destAddr       iSCSI target address to be connected to
 * \param[in]  isNonBlocking  A flag indicating if it should be
 *                            non-blocking connection establishment
 *                            or not.
 * \param[out] ep             Connection endpoint object handle to be
 *                            returned.
 * \param[in]  iscsiNetHandle Handle containing routing information.
 *
 * \retval VMK_OK             If connection is successfully
 *                            established.
 * \retval VMK_STATUS_PENDING If connection is in the process
 *                            being established. The transport will
 *                            poll using EPPoll() to determine the
 *                            connection status.
 *
 * \note   All other return codes indicates failure.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_IscsiEPConnectExtendedDrvIntf) (
   vmk_IscsiTransTransport *transport,
   void                    *destAddr,
   vmk_Bool                isNonBlocking,
   vmk_uint64              *ep,
   vmk_IscsiNetHandle      iscsiNetHandle);

/*
 ***********************************************************************
 * vmk_IscsiEPPollDrvIntf--                                       */ /**
 *
 * \ingroup IscsiTransport
 *
 * \brief Driver entry point interface called by iSCSI transport to
 *        poll for an event on the connection endpoint.
 *
 * \note This callback may block.
 *
 * \param[in]  transport      Pointer to transport object for which the
 *                            interface applies.
 * \param[in]  ep             The connection endpoint for polling.
 * \param[in]  timeoutMS      Timeout for polling.
 * \param[out] eventReply     Driver indication whether poll yields
 *                            any data or not for application.
 *
 * \return VMK_OK, if the poll operation is successfull and poll
 *         state is available in "eventReply". All other return
 *         code indicates failure.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_IscsiEPPollDrvIntf) (
   vmk_IscsiTransTransport    *transport,
   vmk_uint64                 ep,
   vmk_int32                  timeoutMS,
   vmk_IscsiTransPollStatus   *eventReply);

/*
 ***********************************************************************
 * vmk_IscsiEPDisconnectDrvIntf--                                 */ /**
 *
 * \ingroup IscsiTransport
 *
 * \brief Driver entry point interface called by iSCSI transport to
 *        disconnect an existing TCP connection.
 *
 * \note This callback may block.
 *
 * \param[in]  transport      Pointer to transport object for which
 *                            the interface applies.
 * \param[out] ep             Connection endpoint object handle to be
 *                            disconnected.
 * \return VMK_OK, if the disconnect operation is successful.
 *         All other return codes indicates failure.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_IscsiEPDisconnectDrvIntf) (
   vmk_IscsiTransTransport *transport,
   vmk_uint64              ep);

/*
 ***********************************************************************
 * vmk_IscsiTargetDiscoverDrvIntf--                               */ /**
 *
 * \ingroup IscsiTransport
 *
 * \brief Driver entry point interface called by iSCSI transport to
 *        perform discovery operation.
 *
 * \note This callback may block.
 *
 * \param[in]  transport   Pointer to transport object for which the
 *                         interface applies.
 * \param[in]  hostPrivate Pointer to the private object associated
 *                         with adapter.
 * \param[in]  discType    Type of discovery.
 * \param[in]  enable      Flag indicating whether to enable or disable the
 *                         discovery address.
 * \param[in]  discAddress The discovery target address.
 *
 * \return VMK_OK, if the given discovery address is enabled/disabled
 *         successfully. All other return codes indicates failure.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_IscsiTargetDiscoverDrvIntf) (
   vmk_IscsiTransTransport *transport,
   void                    *hostPrivate,
   vmk_IscsiTransTgtDiscvr discType,
   vmk_Bool                enable,
   void                    *discAddress);

/*
 ***********************************************************************
 * vmk_IscsiGetTransportLimitsDrvIntf--                           */ /**
 *
 * \ingroup IscsiTransport
 *
 * \brief Driver entry point interface called by iSCSI transport to
 *        obtain the transport's parameter min-max limits.
 *
 * \note This callback may not block.
 *
 * \param[in]  transport   Pointer to session object for which the
 *                         parameter applies.
 * \param[in]  param       The parameter to get
 * \param[out] paramList   Pointer to the limits object in which the
 *                         limit data is to be returned.
 * \param[in]  listMaxLen  Size of paramList
 *
 * \return VMK_OK, if driver is able to provide the "limits" values
 *         for the parameter being passed. All other return codes
 *         indicates failure.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_IscsiGetTransportLimitsDrvIntf) (
   vmk_IscsiTransTransport             *transport,
   vmk_IscsiTransIscsiParam            param,
   vmk_IscsiTransTransportParamLimits  *paramList,
   vmk_int32                           listMaxLen);

/**
 * \brief iSCSI transport connection object
 */
struct vmk_IscsiTransConnection {
   /**
    * \brief Object Signature
    *
    * Created and assigned by iSCSI transport.
    */
   vmk_uint64 magic;

   /**
    * \brief Last Rx time in jiffies
    *
    * To be updated by media drivers every time it has a successful
    * Rx. This is used by iSCSI transport to determine if connection
    * probe is necessary or not.
    */
   vmk_uint64 *lastRxTimeJiffies;

   /**
    * \brief Driver's Private Object
    *
    * This is private use for the Media Driver.  If connDataSize in
    * transport template is non-zero vmk_IscsiTransCreateConnection()
    * will allocate and save the address in this member field. If
    * connDataSize is zero, it is up to the driver to manage this
    * member.
    */
   void *ddData;
};

/**
 * \brief iSCSI transport session object
 */
struct vmk_IscsiTransSession {
   /**
    * \brief Object Signature
    *
    * Created and assigned by iSCSI transport.
    */
   vmk_uint64 magic;

   /**
    * \brief Session Recovery Timeout Value
    *
    * For iSCSI transport managed drivers, the timeout value is set by
    * the management application. For other drivers, the driver need to
    * update the value after session creation.
    *
    */
   vmk_int32 *recoveryTimeout;

   /**
    * \brief Driver's Private Object
    *
    * This is private use for the Media Driver.  If sessionDataSize in
    * transport template is non-zero vmk_IscsiTransCreateSession() /
    * vmk_IscsiTransAllocSession will allocate and save the address
    * in this member field. If sessionDataSize is zero, it is up
    * to the driver to manage this member.
    */
   void *ddData;
};

/**
 * \brief iSCSI transport's transport object
 */
struct vmk_IscsiTransTransport {
   /**
    * \brief iSCSI Transport API version
    *
    * The media driver is responsible to fill in the version of the
    * API. The media driver must assign VMK_ISCSI_TRANSPORT_REVISION
    * into this field. The iSCSI transport only accepts transport
    * registration if the version is the current transport API version
    * that is being managed. If the version does not match,
    * vmk_IscsiTransRegisterRansport() will fail with VMK_BAD_PARAM.
    */
   vmk_uint32 revision;

   /**
    * \brief Object signature
    *
    * Created and assigned by iSCSI transport.
    */
   vmk_uint64 magic;

   /**
    * \brief Name of the transport
    *
    * Every transport registration must with be unique address with
    * unique name.
    */
   vmk_uint8 name[VMK_ISCSI_MAX_TRANSPORT_NAME_SZ];

   /**
    * \brief Display Description of the transport
    *
    * Every transport registration must provide a human readable
    * description of the transport.
    */
   char description[VMK_ISCSI_MAX_TRANSPORT_DESC_SZ];

   /**
    * \brief Media driver's private data area
    *
    * iSCSI transport does not manage this member.
    */
   void *ddData;

   /**
    * \brief Transport driver's capabilities
    *
    * As documented in vmk_IscsiTransCapabilities
    */
   vmk_IscsiTransCapabilitiesProperty capsProperty;

   /**
    * \brief Connection & Session Supported parameter mask
    *
    * Controls get / set of particular parameter.
    */
   vmk_IscsiTransIscsiParamProperty iscsiParamProperty;

   /**
    * \brief Supported adapter parameter mask
    *
    * This controls the param types that will be get/set on the
    * host/adapter.
    */
   vmk_IscsiTransHostParamProperty hostParamProperty;

   /**
    * \brief Connection data size
    *
    * Size of private memory to be allocated when connection is
    * created. The allocated memory address is saved in ddData of
    * connection object.
    */
   vmk_int32 connDataSize;

   /**
    * \brief Session data size
    *
    * Size of private memory to be allocated when session is
    * created. The allocated memory address is saved in ddData of
    * session object.
    */
   vmk_int32 sessionDataSize;

   /**
    * \brief Maximum number of LUNs supported.
    *
    */
   vmk_int32 maxLun;

   /**
    * \brief Maximum number of connections per session supported.
    *
    */
   vmk_int32 maxConn;

   /**
    * \brief Maximum SCSI command length (CDB length)
    *
    */
   vmk_int32 maxCmdLen;

   /**
    * \brief Creates and initializes the session object
    *
    */
   vmk_IscsiCreateSessionDrvIntf createSessionPersistent;

   /**
    * \brief Delete and free the session object
    *
    */
   vmk_IscsiDestroySessionDrvIntf destroySession;

   /**
    * \brief Create a connection class object
    *
    */
   vmk_IscsiCreateConnectionDrvIntf createConnection;

   /**
    * \brief Bind a socket created in user space to a session.
    *
    */
   vmk_IscsiBindConnectionDrvIntf bindConnection;

   /**
    * \brief Enable connection for IO ( Connect )
    *
    */
   vmk_IscsiStartConnectionDrvIntf startConnection;

   /**
    * \brief Stop IO on the connection ( Disconnect )
    *
    */
   vmk_IscsiStopConnectionDrvIntf stopConnection;

   /**
    * \brief Destroy a connection
    *
    */
   vmk_IscsiDestroyConnectionDrvIntf destroyConnection;

   /**
    * \brief Retrieve session parameters from Media Driver
    *
    */
   vmk_IscsiGetSessionParamDrvIntf getSessionParam;

   /**
    * \brief Set connection/session parameters from Media Driver
    *
    */
   vmk_IscsiSetParamDrvIntf setParam;

   /**
    * \brief Retrieve connection parameters from Media Driver
    *
    */
   vmk_IscsiGetConnectionParamDrvIntf getConnectionParam;

   /**
    * \brief Retrieve host configuration parameters from Media Driver
    *
    */
   vmk_IscsiGetHostParamDrvIntf getHostParam;

   /**
    * \brief Set a host configuration parameters for a Media Driver
    *
    */
   vmk_IscsiSetHostParamDrvIntf setHostParam;

   /**
    * \brief Send a data PDU to a target.
    *
    */
   vmk_IscsiSendPduDrvIntf sendPdu;

   /**
    * \brief Retrieve per connection statistics from the Media Driver
    *
    */
   vmk_IscsiGetStatsDrvIntf getStats;

   /**
    * \brief Recovery timer expiry callback
    *
    */
   vmk_IscsiSessionRecoveryTimedoutDrvIntf sessionRecoveryTimedout;

   /**
    * \brief Connect to the specified destination through a media driver.
    *
    */
   vmk_IscsiEPConnectExtendedDrvIntf EPConnectExtended;

   /**
    * \brief Poll for an event from the Media Driver
    *
    */
   vmk_IscsiEPPollDrvIntf EPPoll;

   /**
    * \brief Disconnect the socket channel though the Media Driver
    *
    */
   vmk_IscsiEPDisconnectDrvIntf EPDisconnect;

   /**
    * \brief Perform a target discovery on a specific ip address
    *
    */
   vmk_IscsiTargetDiscoverDrvIntf targetDiscover;

   /**
    * \brief Get driver's lower and upper limits for the parameters
    *
    */
   vmk_IscsiGetTransportLimitsDrvIntf getTransportLimits;
};

/*
 ***********************************************************************
 * vmk_IscsiTransportCreateSessionDone--                          */ /**
 *
 * \ingroup IscsiTransport
 *
 * \brief Inform completion of session creation.
 *
 *        This interface is called from the media drivers when it has
 *        completed creating a session or an existing session has
 *        gone into the "Logged In" in state.
 *
 * \note This function will not block.
 *
 * \param[in]  conn  Transport connection object.
 *
 * \retval VMK_OK
 * \retval VMK_NO_MEMORY
 * \retval VMK_BAD_PARAM   Not a valid connection object.
 * \retval VMK_FAILURE     Any other failure.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportCreateSessionDone(
   vmk_IscsiTransConnection   *conn);

/*
 ***********************************************************************
 * vmk_IscsiTransportDestroySessionDone--                         */ /**
 *
 * \ingroup IscsiTransport
 * \brief  Inform completion of session termination.
 *
 *         This interface is called from the media drivers to inform
 *         that a particular session was removed by the media driver.
 *
 * \note This function will not block.
 *
 * \param[in]  conn  Transport connection object.
 *
 * \retval VMK_OK
 * \retval VMK_NO_MEMORY
 * \retval VMK_BAD_PARAM   Not a valid connection object.
 * \retval VMK_FAILURE     Any other failure.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportDestroySessionDone(
   vmk_IscsiTransConnection   *conn);

/*
 ***********************************************************************
 * vmk_IscsiTransportConnectionStartDone--                        */ /**
 *
 * \ingroup IscsiTransport
 * \brief  Inform completion of connection start.
 *
 *         This interface is called from the media drivers when it has
 *         completed the connection start operation. The connection is
 *         ready for IO operations.
 *
 * \note This function will not block.
 *
 * \param[in]  conn  Transport connection object.
 * \param[in]  error Error code for the operation.
 *
 * \retval VMK_OK
 * \retval VMK_NO_MEMORY
 * \retval VMK_BAD_PARAM   Not a valid connection object.
 * \retval VMK_FAILURE     Any other failure.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportConnectionStartDone(
   vmk_IscsiTransConnection   *conn,
   VMK_ReturnStatus           error);

/*
 ***********************************************************************
 * vmk_IscsiTransportConnectionStopDone--                         */ /**
 *
 * \ingroup IscsiTransport
 * \brief  Inform completion of connection stop.
 *
 *         This interface is called from the media drivers when it has
 *         completed the connection stop operation. As part of the
 *         connection stop, the drivers are required to clean-up
 *         active/pending IO's.
 *
 * \note This function will not block.
 *
 * \param[in]  conn  Transport connection object.
 * \param[in]  error Error code for the operation.
 *
 * \retval VMK_OK
 * \retval VMK_NO_MEMORY
 * \retval VMK_BAD_PARAM   Not a valid connection object.
 * \retval VMK_FAILURE     Any other failure.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportConnectionStopDone(
   vmk_IscsiTransConnection   *conn,
   VMK_ReturnStatus           error);

/*
 ***********************************************************************
 * vmk_IscsiTransportReportConnectionError--                      */ /**
 *
 * \ingroup IscsiTransport
 * \brief  Report an error on the connection.
 *
 *         This interface is called by media drivers when a connection
 *         fails. IE: a target disconnect occurs, network error, etc.
 *         Following reporting of this error, the iSCSI transport will
 *         start error recovery procedure on the connection.
 *
 * \note This function will not block.
 *
 * \param[in]  conn  Transport connection object.
 * \param[in]  error Error code that caused the failure.
 *
 * \retval VMK_OK
 * \retval VMK_NO_MEMORY
 * \retval VMK_BAD_PARAM   Not a valid connection object.
 * \retval VMK_FAILURE     Any other failure.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportReportConnectionError(
   vmk_IscsiTransConnection      *conn,
   vmk_IscsiTransIscsiErrorCode  error);

/*
 ***********************************************************************
 * vmk_IscsiTransportSendReceivedPdu--                            */ /**
 *
 * \ingroup IscsiTransport
 * \brief   Send a received iSCSI control PDU up to iSCSI transport.
 *
 *          Called by media drivers to have the iSCSI transport handle
 *          a specific PDU.  Only following PDU's are valid:
 *             Login Response,
 *             Logout Response,
 *             Nop In,
 *             Async Event,
 *             Text Response.
 *
 * \note This function will not block.
 *
 * \param[in]  conn     Transport connection object.
 * \param[in]  pduHdr   Pointer to buffer containing iSCSI PDU header.
 * \param[in]  pduData  Pointer to buffer containing iSCSI PDU data.
 *                      (can be NULL if no data).
 * \param[in]  dataLen  Data length if pduData is not NULL.
 *
 * \retval VMK_OK
 * \retval VMK_NO_MEMORY
 * \retval VMK_BAD_PARAM   Not a valid connection object.
 * \retval VMK_FAILURE     Any other failure.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportSendReceivedPdu(
   vmk_IscsiTransConnection   *conn,
   void                       *pduHdr,
   vmk_uint8                  *pduData,
   vmk_ByteCountSmall         dataLen);

/*
 ***********************************************************************
 * vmk_IscsiTransportRegisterAdapter--                            */ /**
 *
 * \ingroup IscsiTransport
 * \brief  Register the iSCSI adapter with iSCSI transport.
 *
 *         Register an adapter with the iSCSI transport so for
 *         attaching management interfaces.
 *
 * \note This function may block.
 *
 * \param[in]  vmkAdapter  The adapter to attach our management adapter.
 * \param[in]  transport   The transport template that will be used to
 *                         call into the media driver for this adapter.
 *
 * \retval VMK_OK
 * \retval VMK_EXISTS
 * \retval VMK_NO_MEMORY
 * \retval VMK_BAD_PARAM   Not a valid transport/adapter object.
 * \retval VMK_FAILURE     Any other failure.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportRegisterAdapter(
   vmk_ScsiAdapter         *vmkAdapter, 
   vmk_IscsiTransTransport *transport
   );

/*
 ***********************************************************************
 * vmk_IscsiTransportUnregisterAdapter--                          */ /**
 *
 * \ingroup IscsiTransport
 * \brief   Release this adapter freeing it's management interfaces.
 *
 * \note This function may block.
 *
 * \param[in]  vmkAdapter  The adapter to release.
 *
 * \retval VMK_OK
 * \retval VMK_BUSY
 * \retval VMK_BAD_PARAM   Not a valid adapter object.
 * \retval VMK_FAILURE     Any other failure.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportUnregisterAdapter(
   vmk_ScsiAdapter *vmkAdapter);

/*
 ***********************************************************************
 * vmk_IscsiTransportAllocSession--                               */ /**
 *
 * \ingroup IscsiTransport
 * \brief Allocates iSCSI session object.
 *
 *        Allocates and initialize the class object that represents a
 *        session.
 *
 * \note This function may block.
 *
 * \param[in]   hostPrivate   A private object to be associated with
 *                            the session.
 * \param[in]   transport     Pointer to transport object.
 * \param[out]  session       The pointer to the newly allocated session.
 *
 * \retval VMK_OK
 * \retval VMK_NO_MEMORY
 * \retval VMK_BAD_PARAM   Not a valid transport object.
 * \retval VMK_FAILURE     Any other failure.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportAllocSession(
   void                    *hostPrivate, 
   vmk_IscsiTransTransport *transport,
   vmk_IscsiTransSession   **session);

/*
 ***********************************************************************
 * vmk_IscsiTransportAddSession--                                 */ /**
 *
 * \ingroup IscsiTransport
 * \brief  Associate host, target and channel Ids for the session.
 *
 *         Also activates the session for further operations. Following
 *         completion of this interface, the storage stack can perform
 *         scanning operations on the session.
 *
 * \note This function may block.
 *
 * \param[in]   session    A session previously created with
 *                         vmk_IscsiTransportAllocSession().
 * \param[in]   hostNo     The host ID to be asssociated for this
 *                         session.
 * \param[in]   targetId   The target ID to be asssociated for this
 *                         session.
 * \param[out]  channelId  The channel ID to be asssociated for this
 *                         session.
 *
 * \retval VMK_OK
 * \retval VMK_BAD_PARAM   Given session object is invalid.
 * \retval VMK_FAILURE     Any other failure.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportAddSession(
   vmk_IscsiTransSession   *session,
   vmk_uint32              hostNo,
   vmk_uint32              targetId,
   vmk_uint32              channelId);

/*
 ***********************************************************************
 * vmk_IscsiTransportCreateSession--                              */ /**
 *
 * \ingroup IscsiTransport
 * \brief Creates and activates a new session.
 *
 *        This interface allocates a new iSCSI transport session
 *        object and activates it.
 *
 * \note This function may block.
 *
 * \param[in]   hostPrivate   Driver's private data to be associated
 *                            with the session object.
 * \param[in]   transport     Transport object to be associated with the
 *                            session object.
 * \param[in]   hostNo        The host ID to be asssociated for this
 *                            session.
 * \param[in]   targetId      The target ID to be asssociated for this
 *                            session.
 * \param[in]   channelId     The channel ID to be asssociated for this
 *                            session.
 * \param[out]  session       The pointer to the newly allocated session.
 *
 * \retval VMK_OK
 * \retval VMK_NO_MEMORY
 * \retval VMK_EXISTS      Already an object with same targetId and
 *                         channelId exists for the same transport.
 * \retval VMK_BAD_PARAM   Not a valid session object.
 * \retval VMK_FAILURE     Any other failure.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportCreateSession(
   void                    *hostPrivate,
   vmk_IscsiTransTransport *transport,
   vmk_uint32              hostNo,
   vmk_uint32              targetId,
   vmk_uint32              channelId,
   vmk_IscsiTransSession   **session);

/*
 ***********************************************************************
 * vmk_IscsiTransportRemoveSession--                              */ /**
 *
 * \ingroup IscsiTransport
 * \brief Removes a session object from iSCSI transport's active list.
 *
 * \note This function may block.
 *
 * \param[in]  session     The session to be removed.
 *
 * \retval VMK_OK
 * \retval VMK_BUSY        There are other resources attached to the
 *                         object, can't be removed.
 * \retval VMK_BAD_PARAM   Not a valid session object.
 * \retval VMK_FAILURE     Any other failure.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportRemoveSession(
   vmk_IscsiTransSession   *session);

/*
 ***********************************************************************
 * vmk_IscsiTransportFreeSession--                                */ /**
 *
 * \ingroup IscsiTransport
 * \brief  Free the memory associated with session object.
 *
 * \note This function may block.
 *
 * \param[in]  session     The session to be freed.
 *
 * \retval VMK_OK
 * \retval VMK_BUSY        There are other resources attached to the
 *                         object, can't be freed.
 * \retval VMK_BAD_PARAM   Not a valid session object.
 * \retval VMK_FAILURE     Any other failure.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportFreeSession(
   vmk_IscsiTransSession   *session);

/*
 ***********************************************************************
 * vmk_IscsiTransportDestroySession--                             */ /**
 *
 * \ingroup IscsiTransport
 * \brief Remove and free the session object.
 *
 * \note This function may block.
 *
 * \param[in]  session     The session to be destroyed.
 *
 * \retval VMK_OK
 * \retval VMK_BUSY        There are other resources attached to the
 *                         object, can't be freed.
 * \retval VMK_BAD_PARAM   Not a valid session object.
 * \retval VMK_FAILURE     Any other failure.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportDestroySession(
   vmk_IscsiTransSession   *session);

/*
 ***********************************************************************
 * vmk_IscsiTransportCreateConnection--                           */ /**
 *
 * \ingroup IscsiTransport
 * \brief Create a new connection object for the specified session.
 *
 * \note This function may block.
 *
 * \param[in]  session     The session for which the new connection to
 *                         be created.
 * \param[in]  connId      The connection ID to be associated with the
 *                         new connection.
 * \param[out] conn        Newly created connection object.
 *
 * \retval VMK_OK
 * \retval VMK_NO_MEMORY
 * \retval VMK_BAD_PARAM   Not a valid session object.
 * \retval VMK_FAILURE     Any other failure.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportCreateConnection(
   vmk_IscsiTransSession      *session,
   vmk_uint32                 connId,
   vmk_IscsiTransConnection   **conn);

/*
 ***********************************************************************
 * vmk_IscsiTransportDestroyConnection--                          */ /**
 *
 * \ingroup IscsiTransport
 * \brief    Destroy the connection object previously created.
 *
 * \note This function may block.
 *
 * \param[in] conn  The connection to be destroyed.
 *
 * \retval VMK_OK
 * \retval VMK_BUSY        There are other resources attached to the
 *                         object, object can't be destroyed.
 * \retval VMK_BAD_PARAM   Not a valid session object.
 * \retval VMK_FAILURE     Any other failure.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportDestroyConnection(
   vmk_IscsiTransConnection   *conn);

/*
 ***********************************************************************
 * vmk_IscsiTransportStopSessionRecoveryTimer--                   */ /**
 *
 * \ingroup IscsiTransport
 * \brief    Disarm the recovery timer that was started earlier.
 *
 * \note This function will not block.
 *
 * \param[in] sess  The session object for which the timer is to
 *                  be stopped.
 *
 ***********************************************************************
 */
void
vmk_IscsiTransportStopSessionRecoveryTimer(
   vmk_IscsiTransSession   *sess);

/*
 ***********************************************************************
 * vmk_IscsiTransportStartSessionRecoveryTimer--                  */ /**
 *
 * \ingroup IscsiTransport
 * \brief    Schedule the recovery timer for the session.
 *
 * \note This function will not block.
 *
 * \param[in] sess   The session object for which the timer is to
 *                   be started.
 *
 * \retval VMK_OK
 * \retval VMK_NO_RESOURCES
 * \retval VMK_FAILURE     Any other failure.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportStartSessionRecoveryTimer(
   vmk_IscsiTransSession   *sess);

/*
 ***********************************************************************
 * vmk_IscsiTransportRegisterTransport--                          */ /**
 *
 * \ingroup IscsiTransport
 * \brief    Register media driver transport interface with the iSCSI
 *           transport.
 *
 * \note This function may block.
 *
 * \param[in]  transport   Transport object to be registered.
 *
 * \retval VMK_OK
 * \retval VMK_NO_MEMORY
 * \retval VMK_BAD_PARAM   Not a valid transport object.
 * \retval VMK_FAILURE     Any other failure.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportRegisterTransport(
   vmk_IscsiTransTransport *transport);

/*
 ***********************************************************************
 * vmk_IscsiTransportUnregisterTransport--                        */ /**
 *
 * \ingroup IscsiTransport
 * \brief    Unregister the media driver transport interface.
 *
 * \note This function may block.
 *
 * \param[in]  transport   Transport object to be registered.
 *
 * \retval VMK_OK
 * \retval VMK_BUSY        There are still some resources using this
 *                         transport object.
 * \retval VMK_BAD_PARAM   Not a valid transport object.
 * \retval VMK_FAILURE     Any other failure.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportUnregisterTransport(
  vmk_IscsiTransTransport  *transport);

/*
 ***********************************************************************
 * vmk_IscsiTransportLookupSession--                              */ /**
 *
 * \ingroup IscsiTransport
 * \brief    Return session associated with Host, Channel and Target ID.
 *
 * \note This function may block.
 *
 * \param[in]  hostNum     Host ID.
 * \param[in]  channelId   Channel ID.
 * \param[in]  targetId    Target ID.
 * \param[out] session     If found, pointer to session object is
 *                         returned in this
 *
 * \retval VMK_OK          Found a session for given Host, Channel and
 *                         Target ID.
 * \retval VMK_NOT_FOUND   Unable to find a session for given Host
 *                         Channel and target ID.
 * \retval VMK_FAILURE     Any other failure.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_IscsiTransportLookupSession(
   vmk_int32               hostNum,
   vmk_int32               channelId,
   vmk_int32               targetId,
   vmk_IscsiTransSession   **session);

#endif /* _VMKAPI_ISCSI_TRANSPORT_INCOMPAT_H */
/** @} */
/** @} */
