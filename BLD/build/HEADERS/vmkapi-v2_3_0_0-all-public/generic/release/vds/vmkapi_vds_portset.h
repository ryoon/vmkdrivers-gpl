/***********************************************************
 * Copyright 2008 - 2014 VMware, Inc.  All rights reserved.
 ***********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * VDS                                                           */ /**
 * \addtogroup Network
 * @{
 * \defgroup VDS VDS Portset Interfaces
 *
 * The VDSwitch is an abstraction representation of multiple hosts
 * defining the same vSwitch (same name, same network policy) and
 * portgroup, which are needed to facilitate the concept of VM being
 * connected to the same network as it migrates among multiple hosts.
 *
 * Portsets are groups of ports which, together with policies for frame
 * routing, form virtual networks. Virtual nics connected to the ports
 * may forward packets to each other. The analogy is to a box (like a
 * a hub or a switch) containing some type of backplane for connecting
 * multiple ports in the physical world. If multiple hosts have virtual
 * nics connected to a VDS, each of the hosts should have a portset
 * implementation in ESX.
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_VDS_PORTSET_H_
#define _VMKAPI_VDS_PORTSET_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "net/vmkapi_net_types.h"
#include "net/vmkapi_net_eth.h"
#include "net/vmkapi_net_uplink.h"
#include "net/vmkapi_net_port.h"
#include "net/vmkapi_net_portset.h"
#include "net/vmkapi_net_queue.h"
#include "vds/vmkapi_vds_prop.h"

/**
 * \brief Flags for a VDSPortClient
 */
typedef enum {

   /** None. */
   VMK_VDS_CLIENT_FLAG_NONE    = 0x0,

   /** flags for DVS to push dvs property after port is connected. */
   VMK_VDS_CLIENT_PUSH_AFTER_CONNECT = 0x1,

} vmk_VDSPortClientFlag;


/**
 * \brief The maximum number of arguments allowed for vmk_VDSEventEx event.
 */
#define VMK_VDS_EVENTEX_ARGS_MAX_NUM       6

/**
 * \brief Handle for a vDS Command Client.
 */
typedef struct VDSCmdClient *vmk_VDSCmdClient;

/*
 ************************************************************************
 * vmk_VDSCmdClientExec --                                         */ /**
 *
 * \brief Command execution callback of a vDS command client.
 *
 * \note This callback is allowed to block.
 *
 * \param[in]     psName              Portset name
 * \param[in]     cmdName             Command name
 * \param[in]     arg                 Argument buffer
 * \param[in]     argLen              Argument length
 * \param[out]    ret                 Result buffer
 * \param[in,out] retLen              Result buffer length
 *
 * \retval      VMK_OK              Command executed successfully
 * \retval      VMK_LIMIT_EXCEEDED  Result longer than the result buffer
 * \retval      Other status        Depending on the callback
 *                                  implementation
 ************************************************************************
 */

typedef VMK_ReturnStatus (*vmk_VDSCmdClientExec)(const char *psName,
                                                 const char *cmdName,
                                                 const void *arg,
                                                 vmk_uint32 argLen,
                                                 void *ret,
                                                 vmk_uint32 *retLen);

/**
 * \brief Storage for the argument name and value when posting a vDS or
 *        port event.
 */
typedef struct vmk_VDSEventExArg {
   const char *key;
   const char *value;
} vmk_VDSEventExArg;

/**
 * \brief Contains name and arguments of an event posted to a vDS or port.
 */
typedef struct vmk_VDSEventEx {
   const char           *name;
   vmk_VDSEventExArg    args[VMK_VDS_EVENTEX_ARGS_MAX_NUM];
   vmk_uint8            argCnt;
} vmk_VDSEventEx;


/*
 ***********************************************************************
 * vmk_PortsetAcquireByVDSID --                                    *//**
 *
 * \brief Acquire a handle to a portset in the requested context.
 *
 * \nativedriversdisallowed
 *
 * A particular thread may hold at most one portset handle. By
 * extension, this means that a portset implementation may not
 * re-acquire a portset handle, and may not acquire a new portset handle
 * while in a callback where a portset handle is provided as a
 * parameter.
 *
 * Each vmkapi function that requires the caller to hold a portset
 * handle will note this requirement in its doxygen header. If a
 * particular handle context is required, it will be explicitly noted;
 * otherwise, any handle context is acceptable.
 *
 * A thread should not hold a portset handle any longer than necessary.
 * While it holds the handle, it is not blockable and may not sleep or
 * call any blocking function calls. There are several vmkapi function
 * calls that explicitly state that they cannot be called by a thread
 * holding a portset handle.
 *
 * The mutable context should not be used unless absolutely necessary,
 * as it may temporarily halt all traffic traversing the portset.
 *
 * \note The caller must not hold any spin locks while invoking
 *       this function.
 *
 * \note This function will not block.
 *
 * \param[in]  vdsID    vDS switch ID string.
 * \param[in]  context  Requested handle context.
 * \param[out] ps       Portset handle.
 *
 * \retval VMK_OK         Successfully acquired the handle.
 * \retval VMK_BAD_PARAM  Invalid arguments.
 * \retval VMK_NOT_FOUND  The vDS switch was not found.
 * \retval VMK_FAILURE    The requested portset handle cannot be
 *                        acquired in the current calling context.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortsetAcquireByVDSID(const char *vdsID,
                                           vmk_PortsetHandleContext context,
                                           vmk_Portset **ps);


/**
 * \brief Handle for a vDS Portset Client.
 */

typedef struct VDSPortsetClient *vmk_VDSPortsetClient;

/** 
 * \brief Flags passed to VDSClient Portset/Port ops.
 */
typedef enum {

   /** None. */
   VMK_VDS_CLIENT_OPS_FLAG_NONE    = 0x0,

   /** Ops flags for calls within an initialization. */
   VMK_VDS_CLIENT_OPS_FLAG_INIT    = 0x1,

   /** Ops flags for calls within a cleanup. */
   VMK_VDS_CLIENT_OPS_FLAG_CLEANUP = 0x2,
} vmk_VDSClientOpsFlags;


/*
 ***********************************************************************
 * vmk_VDSPortsetClientWrite --                                   */ /**
 *
 * \brief Write callback for a portset property of a VDS Portset client.
 *
 * \note This function will not block.
 *
 * \param[in]   ps             Mutable portset handle.
 * \param[in]   dataName       Data name (name of portset property)
 * \param[in]   data           Data to set for name
 * \param[in]   dataLen        Length of data
 * \param[in]   flags          Flags
 *
 * \retval      VMK_OK         Portset client accepts value written to dataName
 * \retval      Other status   Portset client does not accept change of dataName value
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_VDSPortsetClientWrite)(
   vmk_Portset *ps,
   char *dataName,
   void *data, int dataLen,
   vmk_VDSClientOpsFlags flags);


/*
 ************************************************************************
 * vmk_VDSPortsetClientBlockableWrite --                           */ /**
 *
 * \brief Write callback for a portset property of a VDS Portset client.
 *
 * This callback is invoked in a blockable context, but it should not
 * take more than a reasonable amount of time (a few seconds). This
 * callback can only be implemented on selected properties defined
 * by VMware.
 *
 * \note vDS properties may be queried from this callback, but they may
 *       not be set or cleared.
 *
 * \note This function may block.
 *
 * \param[in]   psName         Portset name.
 * \param[in]   dataName       Data name (name of portset property)
 * \param[in]   data           Data to set for name
 * \param[in]   dataLen        Length of data
 * \param[in]   flags          Flags
 *
 ************************************************************************
 */

typedef void (*vmk_VDSPortsetClientBlockableWrite)(const char *psName,
                                                   char *dataName,
                                                   void *data,
                                                   int dataLen,
                                                   vmk_VDSClientOpsFlags flags);

/*
 ***********************************************************************
 * vmk_VDSPortsetClientClear --                                   */ /**
 *
 * \brief Clear callback for a portset property of a VDS Portset client.
 *
 * \note This callback is invoked from a non-blockable context, even for
 *       blockable properties.
 *
 * \note This function will not block.
 *
 * \param[in]   ps             Mutable portset handle.
 * \param[in]   dataName       Data name (name of portset property)
 *
 * \retval      VMK_OK         Portset client accepts clear of property value
 * \retval      Other status   Portset client does not accept clear of property value
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_VDSPortsetClientClear)(vmk_Portset *ps,
                                                      char *dataName);


/**
 * \brief Argument structure for registering portset property handlers.
 */

typedef struct vmk_VDSPortsetClientOps {
   /** handler for writes to non-blockable portset properties. */
   vmk_VDSPortsetClientWrite write;

   /** handler for writes to blockable portset properties. */
   vmk_VDSPortsetClientBlockableWrite blockableWrite;

   /** handler for clearing portset properties. */
   vmk_VDSPortsetClientClear clear;
} vmk_VDSPortsetClientOps;


/*
 ***********************************************************************
 * vmk_VDSClientPortsetRegister --                                */ /**
 *
 * \brief Register VDS Portset property handlers.
 *
 * \nativedriversdisallowed
 *
 * \note At most one client may be registered for a given property (data
 *       name) on a given portset
 *
 * \note Clients for non-blockable properties must register write() and
 *       clear() callbacks. Clients for blockable properties must
 *       register blockableWrite() and clear() callbacks.
 *
 * \note We only support blockable callback for write operation of
 *       VMK_VDSPROP_HOST_PROXY_STATE property. All other callbacks
 *       for other properties cannot be registered as blockable.
 *
 * \note This function will not block.
 *
 * \param[out]  client            Ptr to handle for VDS Portset Client
 * \param[in]   psName            Portset name
 * \param[in]   dataName          Data name
 * \param[in]   ops               VDSClient portset ops to use
 * \param[in]   blockable         Whether write callback is blockable
 *
 * \retval      VMK_OK            VDSClient registered successfully
 * \retval      VMK_EXISTS        A client already exists for the given
 *                                portset & dataname
 * \retval      VMK_NO_MEMORY     Insufficient memory to perform 
 *                                registration
 * \retval      VMK_BAD_PARAM     A required callback is unspecified
 *                                (NULL)
 * \retval      VMK_NOT_SUPPORTED The specified portset client cannot be
 *                                registered as blockable
 ***********************************************************************
 */

VMK_ReturnStatus vmk_VDSClientPortsetRegister(vmk_VDSPortsetClient *client,
                                              const char *psName,
                                              const char *dataName,
                                              const vmk_VDSPortsetClientOps *ops,
                                              vmk_Bool blockable);

/*
 ***********************************************************************
 * vmk_VDSClientPortsetUnregister --                              */ /**
 *
 * \brief Unregister VDS Portset property handlers.
 *
 * \nativedriversdisallowed
 *
 * \note This function will not block.
 *
 * \param[in]   client         Handle for VDS Portset Client
 *
 * \retval      VMK_OK         VDSClient unregistered successfully
 * \retval      VMK_NOT_FOUND  The specified client does not exist
 ***********************************************************************
 */

VMK_ReturnStatus vmk_VDSClientPortsetUnregister(vmk_VDSPortsetClient client);


/**
 * \brief Handle for a VDSClient Port Client.
 */

typedef struct VDSPortClient *vmk_VDSPortClient;


/*
 ***********************************************************************
 * vmk_VDSClientPortWrite --                                      */ /**
 *
 * \brief Write callback for a port property of a VDS client.
 *
 * \note Refer to the documentation for vmk_VDSPortDataSet()
 *       for a description of how this callback is used.
 *
 * \note This callback cannot block.
 *
 * \param[in]   ps             Mutable portset handle.
 * \param[in]   portID         Port ID
 * \param[in]   dataName       Data name
 * \param[in]   data           Data to set for name
 * \param[in]   dataLen        Length of data
 * \param[in]   flags          Flags
 *
 * \retval      VMK_OK         Port client accepts value change of the
 *                             data name (property) on the port
 * \retval      Other status   Port client does not accept the value change
 *                             of the data name (property) on the port
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_VDSClientPortWrite)(vmk_Portset *ps,
                                                   vmk_SwitchPortID portID,
                                                   char *dataName, void *data,
                                                   int dataLen,
                                                   vmk_VDSClientOpsFlags flags);


/*
 ***********************************************************************
 * vmk_VDSClientPortRead --                                       */ /**
 *
 * \brief Read callback for a port property of a VDS client.
 *
 * \note Refer to the documentation for vmk_VDSPortDataGet()
 *       for a description of how this callback is used.
 *
 * \note This callback cannot block.
 *
 * \param[in]   ps             Immutable portset handle.
 * \param[in]   portID         Port ID
 * \param[in]   dataName       Data name
 * \param[in]   data           Data to set for name
 * \param[in]   dataLen        Length of data
 * \param[in]   flags          Flags
 *
 * \retval      VMK_OK         Port client successfully retrieved value of
 *                             the data name (property) on the port
 * \retval      Other status   Port client could not retrieve the value
 *                             of the specified data name (property) on
 *                             the port
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_VDSClientPortRead)(vmk_Portset *ps,
                                                  vmk_SwitchPortID portID,
                                                  char *dataName, void *data,
                                                  int dataLen,
                                                  vmk_VDSClientOpsFlags flags);


/*
 ***********************************************************************
 * vmk_VDSClientPortPoll --                                       */ /**
 *
 * \brief Poll callback for a port property of a VDS client.
 *
 * \note Refer to the documentation for vmk_VDSPortDataGet()
 *       for a description of how this callback is used.
 *
 * \note This callback cannot block.
 *
 * \param[in]   ps             Immutable portset handle.
 * \param[in]   portID         Port ID
 * \param[in]   dataName       Data name
 * \param[out]  dataLenPtr     Ptr to length of data available to be read
 *
 * \retval      VMK_TRUE       If the specified data name (property)
 *                             has a new value to be read; in this case
 *                             *dataLenPtr is set to the length of
 *                             the data available to be read.
 * \retval      VMK_FALSE      If the specified data name (property)
 *                             has no new data value to be read.
 ***********************************************************************
 */

typedef vmk_Bool (*vmk_VDSClientPortPoll)(vmk_Portset *ps,
                                          vmk_SwitchPortID portID,
                                          char *dataName, int *dataLengthPtr);


/*
 ***********************************************************************
 * vmk_VDSClientPortCleanup --                                    */ /**
 *
 * \brief Cleanup callback for a port property of a VDS client.
 *
 * \note This callback is called when disabling a port, or when an
 *       enable operation fails; it should clean up and free any
 *       resources associated with the specified data name (port
 *       property) for the specified port.
 *
 * \note This callback cannot block.
 *
 * \param[in]   ps             Mutable portset handle.
 * \param[in]   portID         Port ID
 * \param[in]   dataName       Data name
 *
 * \return      None           No return value
 ***********************************************************************
 */

typedef void (*vmk_VDSClientPortCleanup)(vmk_Portset *ps,
                                         vmk_SwitchPortID portID,
                                         char *dataName);

/**
 * \brief Argument structure when registering port property handlers
 */

typedef struct vmk_VDSPortClientOps {
   /**
    * Mandatory handler for writing to port properties.
    */
   vmk_VDSClientPortWrite write;

   /**
    * Optional handler for reading port properties.
    */
   vmk_VDSClientPortRead read;

   /** 
    * Optional handler for polling port properties; must be provided if
    * a read callback is provided.
    */
   vmk_VDSClientPortPoll poll;

   /** 
    * Optional handler for cleaning up port properties.
    */
   vmk_VDSClientPortCleanup cleanup;
} vmk_VDSPortClientOps;

/*
 ***********************************************************************
 * vmk_VDSClientPortRegister --                                   */ /**
 *
 * \brief Register Port Operations for a VDS Client.
 *
 * \nativedriversdisallowed
 *
 * \note At most one client may be registered for a given property
 *       (data name) on a specified port and portset.
 *
 * \note This function will not block.
 *
 * \param[out]  client         Ptr to handle for VDS Port Client
 * \param[in]   psName         Portset name
 * \param[in]   dataName       Data name
 * \param[in]   ops            VDSClient port ops to use
 *
 * \retval      VMK_OK         VDSClient registered successfully
 * \retval      VMK_EXISTS     A client already exists for the given
 *                             portset & dataName
 * \retval      VMK_NO_MEMORY  Insufficient memory to perform
 *                             registration
 * \retval      VMK_BAD_PARAM  A required callback is unspecified (NULL)
 ***********************************************************************
 */

VMK_ReturnStatus vmk_VDSClientPortRegister(vmk_VDSPortClient *client,
                                           const char *psName,
                                           const char *dataName,
                                           const vmk_VDSPortClientOps *ops);

/*
 ***********************************************************************
 * vmk_VDSPortClientSetFlag --                                    */ /**
 *
 * \brief Set the flag for a DVSPortClient.
 *
 * \nativedriversdisallowed
 *
 * \note For now, a client can set a flag to indicate whether the client
 *       want the property to be pushed after the port is connected.
 *
 * \note This function will not block.
 *
 * \param[in]   client         The pointer which points to the handle for
 *                             the VDS Port Client.
 * \param[in]   flag           flag to be set.
 *
 * \retval      VMK_OK         Set successfully
 * \retval      VMK_BAD_PARAM  invalid parameters passed
 ***********************************************************************
 */

VMK_ReturnStatus vmk_VDSPortClientSetFlag(vmk_VDSPortClient client,
                                          vmk_VDSPortClientFlag flag);

/*
 ***********************************************************************
 * vmk_VDSClientPortUnregister --                                 */ /**
 *
 * \brief Unregister a VDS Port Client.
 *
 * \nativedriversdisallowed
 *
 * \note This function will not block.
 *
 * \param[in]   client         Handle for VDS Port Client
 *
 * \retval      VMK_OK         VDSClient unregistered successfully
 * \retval      VMK_NOT_FOUND  The specified client does not exist
 ***********************************************************************
 */

VMK_ReturnStatus vmk_VDSClientPortUnregister(vmk_VDSPortClient client);

/*
 ***********************************************************************
 * vmk_VDSHostPropLookup --                                       */ /**
 *
 * \brief Lookup a host property by name.
 *
 * \nativedriversdisallowed
 *
 * \note The caller must not hold any locks or portset handles.
 *
 * \note This function may block.
 *
 * \param[in]   psName         Portset name.
 * \param[in]   dataName       Name of data item to look up
 * \param[out]  dataValue      Pointer to block of data to associated
 *                             with the name
 * \param[out]  dataLength     Length of the block at dataValue
 *
 * \retval      VMK_OK         Success; in this case *entryp is updated
 * \retval      VMK_NOT_FOUND  The switch or specified property cannot
 *                             be found.
 * \retval      VMK_BAD_PARAM  The provided psName, dataName, dataValue
 *                             or dataLength is invalid
 ***********************************************************************
 */

VMK_ReturnStatus vmk_VDSHostPropLookup(const char *psName,
                                       const char *dataName,
                                       void **dataValue,
                                       vmk_uint32 *dataLength);


/*
 ***********************************************************************
 * vmk_VDSHostPropSet --                                          */ /**
 *
 * \brief Set a host property (name, value) pair.
 *
 * \nativedriversdisallowed
 *
 * The property value to be set must not be NULL; dataLength should
 * not be zero; dataName should not be empty. To clean up property
 * value, use vmk_VDSHostPropClear.
 *
 * \note This operation will call the VDS portset client's
 *       vmk_VDSPortsetClientWrite() callback to set the value of the
 *       property. If this succeeds, the value written will also be
 *       saved in the cache. 
 * 
 * \note The caller must not hold any locks or portset handles.
 *
 * \note This function may block.
 *
 * \param[in]   psName         Portset name.
 * \param[in]   dataName       Name of the data item to set
 * \param[in]   dataValue      Pointer to block of data to associated
 *                             with the name
 * \param[in]   dataLength     Length of the block at dataValue
 *
 * \retval      VMK_OK         Success; the host property (name, value)
 *                             pair has been updated
 * \retval      VMK_NOT_FOUND  The switch cannot be found.
 * \retval      VMK_BAD_PARAM  The provided psName, dataName, dataValue
 *                             or dataLength is invalid
 * \retval      VMK_NO_MEMORY  Insufficient memory for storing the
 *                             (name, value) pair
 * \retval      Other status   vmk_VDSPortsetClientWrite callback failed
 ***********************************************************************
 */
VMK_ReturnStatus vmk_VDSHostPropSet(const char *psName,
                                    const char *dataName,
                                    const void *dataValue,
                                    vmk_uint32 dataLength);

/*
 ***********************************************************************
 * vmk_VDSHostPropClear --                                        */ /**
 *
 * \brief Clear host property with specified data name
 *
 * \nativedriversdisallowed
 *
 * \note This operation will call the VDS portset client's
 *       vmk_VDSPortsetClientClear() callback to clear the value of the
 *       property. If succeeds, the portset client for this property
 *       will be removed.
 *
 * \note The caller must not hold any locks or portset handles.
 *
 * \note This function may block.
 *
 * \param[in]   psName         Portset name.
 * \param[in]   dataName       Name of the data item to clear
 *
 * \retval      VMK_OK         Success; the host property with dataName
 *                             has been cleared
 * \retval      VMK_NOT_FOUND  The switch or named data cannot be found.
 * \retval      VMK_BAD_PARAM  The provided psName or dataName is
 *                             invalid
 ***********************************************************************
 */

VMK_ReturnStatus vmk_VDSHostPropClear(const char *psName,
                                      const char *dataName);

/*
 ***********************************************************************
 * vmk_VDSHostStatusSet --                                         *//**
 *
 * \brief Set VDS host status and display string.
 * 
 * \nativedriversdisallowed
 *
 * The display string will be copied inside the function, and the
 * copied string will be propogated to Virtual Center in events.
 *
 * \note The caller must not hold any locks or portset handles.
 *
 * \note This function may block.
 *
 * \param[in]   psName           Portset name.
 * \param[in]   vdsStatus        VDS Host status to be set
 * \param[in]   displayStr       Detailed status string less than
 *                               256 characters 
 *
 * \retval      VMK_OK            Success; the host status has been updated
 * \retval      VMK_NOT_FOUND     VDS is not found
 * \retval      VMK_NAME_TOO_LONG displayStr is longer than 256 characters
 * \retval      VMK_FAILURE       Failure; host status has not been updated
 * \retval      VMK_NO_MEMORY     No memory
 * \retval      VMK_BAD_PARAM     The provided psName or displayStr is
 *                                invalid
 ***********************************************************************
 */
VMK_ReturnStatus vmk_VDSHostStatusSet(const char *psName,
                                      vmk_VDSHostStatus vdsStatus,
                                      const char *displayStr);


/*
 ***********************************************************************
 * vmk_VDSPortDataGet --                                          */ /**
 *
 * \brief Get the value part of a specified port data (name, value) pair.
 *
 * \nativedriversdisallowed
 *
 * This operation will call the VDS Port Client's
 * vmk_VDSClientPortPoll() callback to find out if there is new data to
 * be read.  If so, the client's vmk_VDSClientPortRead() callback will
 * be called to obtain the new data.  This new data will also be saved
 * in a cache.  If the poll operation reports that new data is not
 * available, then the previously cached value will be returned.
 *
 * \note The caller must hold a mutable handle for the portset.
 *
 * \note Port data returned can be used safely only when mutable portset
 *       handle is held by caller.
 *
 * \note This function may block.
 *
 * \param[in]   portID         Numeric ID of a virtual port.
 * \param[in]   dataName       Name of port data.
 * \param[out]  dataValue      Buffer for value to associated
 *                             with the name.
 * \param[out]  dataLength     Length of data returned.
 *
 *
 * \retval      VMK_OK         Success; dataValuePtr and DataLengthPtr
 *                             updated
 * \retval      VMK_BAD_PARAM  The provided dataName, dataValue, or
 *                             dataLength is invalid
 * \retval      VMK_NOT_FOUND  The specified port cannot be found or
 *                             the port is not associated with vds port;
 *                             the named data cannot be found.
 * \retval      Other status   vmk_VDSClientPortPoll or
 *                             vmk_VDSClientPortRead callback failed
 ***********************************************************************
 */

VMK_ReturnStatus vmk_VDSPortDataGet(vmk_SwitchPortID portID,
                                    const char *dataName,
                                    void **dataValue,
                                    vmk_uint32 *dataLength);


/*
 ***********************************************************************
 * vmk_VDSPortDataSet --                                          */ /**
 *
 * \brief Set a port data (name, value) pair.
 *
 * \nativedriversdisallowed
 *
 * This operation will call the VDS Port Client's
 * vmk_VDSClientPortWrite() callback to set the value
 * of the property.  If this succeeds, the value written
 * will also be saved in the cache.  See the description
 * of vmk_VDSPortDataGet() and vmk_VDSPortDataClear().
 *
 * \note The caller must hold a mutable handle for the portset.
 *
 * \note This function may block.
 *
 * \param[in]   portID         Numeric ID of a virtual port.
 * \param[in]   dataName       Name of port data
 * \param[in]   dataValue      Value to associate with the name
 * \param[in]   dataLength     Length of data
 *
 * \retval      VMK_OK         Success
 * \retval      VMK_BAD_PARAM  The provided dataName, dataValue, or
 *                             dataLength is invalid
 * \retval      VMK_NOT_FOUND  The specified port cannot be found or
 *                             port is not associated with vds port
 * \retval      VMK_FAILURE    No value provided to associated with
 *                             the name
 * \retval      VMK_NO_MEMORY  Insufficient memory for storing the
 *                             (name, value) pair
 * \retval      Other status   vmk_VDSClientPortWrite callback failed
 ***********************************************************************
 */

VMK_ReturnStatus vmk_VDSPortDataSet(vmk_SwitchPortID portID,
                                    const char *dataName,
                                    const void *dataValue,
                                    vmk_uint32 dataLength);

/*
 ***********************************************************************
 * vmk_VDSPortDataClear --                                        */ /**
 *
 * \brief Set a port data (name, value) pair.
 *
 * \nativedriversdisallowed
 *
 * This operation will call the VDS Port Client's
 * vmk_VDSClientPortCleanup() callback to clean up the property.
 * If this succeeds, the property data will be removed from cache,
 * and port client will also be removed.
 *
 * \note The caller must hold a mutable handle for the portset.
 *
 * \note This function may block.
 *
 * \param[in]   portID         Numeric ID of a virtual port.
 * \param[in]   dataName       Name of port data
 *
 * \retval      VMK_OK         Success
 * \retval      VMK_BAD_PARAM  The provided dataName is invalid
 * \retval      VMK_NOT_FOUND  The specified port or named data cannot
 *                             be found.
 * \retval      VMK_FAILURE    vmk_VDSClientPortCleanup callback returns
 *                             failure or other failure conditions
 * \retval      Other status   vmk_VDSClientPortCleanup callback failed
 ***********************************************************************
 */
VMK_ReturnStatus vmk_VDSPortDataClear(vmk_SwitchPortID portID,
                                      const char *dataName);

/*
 ***********************************************************************
 * vmk_VDSLocalCachePortDataSet --                                */ /**
 *
 * \brief Set port data in vDS host local cache. 
 *
 * \nativedriversdisallowed
 *
 * This API should be called during port reserve or connect 
 * time (i.e. in portset callbacks) in order to seed port property
 * value.
 *
 * \note This operation will not call property client (i.e.
 *       vmk_VDSClientPortWrite()).
 *
 * \note The caller must hold a mutable handle for the portset.
 *
 * \note The function will not block.
 *
 * \param[in]   portID         Numeric ID of a virtual port.
 * \param[in]   dataName       Name of port data
 * \param[in]   dataValue      Value to associate with the name
 * \param[in]   dataLength     Length of data
 *
 * \retval  VMK_OK                         Success
 * \retval  VMK_BAD_PARAM                  The provided dataName,
 *                                         dataValue, or dataLength is
 *                                         invalid
 * \retval  VMK_NOT_FOUND                  The specified port cannot be
 *                                         found or port is not
 *                                         associated with vds port
 * \retval  VMK_PORTSET_HANDLE_NOT_MUTABLE The caller did not hold a
 *                                         mutable handle
 * \retval  VMK_NO_MEMORY                  Insufficient memory for
 *                                         storing the (name, value) pair
 ***********************************************************************
 */
VMK_ReturnStatus vmk_VDSLocalCachePortDataSet(vmk_SwitchPortID portID,
                                              const char *dataName,
                                              const void *dataValue,
                                              vmk_uint32 dataLength);

/*
 ***********************************************************************
 * vmk_VDSLocalCachePortDataGet --                                */ /**
 *
 * \brief Get port data value from vDS host local cache.
 *
 * \nativedriversdisallowed
 *
 * \note This operation will not call property client (i.e.
 *       vmk_VDSClientPortRead()). 
 *
 * \note The caller must hold a mutable handle for the portset.
 *
 * \note The function will not block.
 *
 * \param[in]   portID         Numeric ID of a virtual port.
 * \param[in]   dataName       Name of port data
 * \param[in]   dataValue      Value to associate with the name
 * \param[in]   dataLength     Length of data
 *
 * \retval  VMK_OK                          Success
 * \retval  VMK_NOT_FOUND                   The specified port or named
 *                                          data cannot be found.
 * \retval  VMK_BAD_PARAM                   The provided dataName,
 *                                          dataValue, or dataLength is
 *                                          invalid
 * \retval  VMK_PORTSET_HANDLE_NOT_MUTABLE  The caller did not hold a
 *                                          mutable handle
 ***********************************************************************
 */
VMK_ReturnStatus vmk_VDSLocalCachePortDataGet(vmk_SwitchPortID portID,
                                              const char *dataName,
                                              void **dataValue,
                                              vmk_uint32 *dataLength);


/*
 ***********************************************************************
 * vmk_PortLookupVDSPortAlias --                                  */ /**
 *
 * \brief Retrieves the vDS port alias for the specified ephemeral port.
 *
 * \nativedriversdisallowed
 *
 * \note The port alias string is constant for the lifetime of the 
 *       ephemeral port.
 *
 * \note If the vDS port alias is not configured, then this function
 *       will succeed and the string returned in vdsName will be
 *       "unknown".
 *
 * \note The caller must provide a buffer that is
 *       VMK_VDS_PORT_ALIAS_MAX_LEN bytes in length.
 *
 * \note The caller must hold a mutable handle for the portset
 *       associated with the specified portID.
 *
 * \note The function will not block.
 *
 * \param[in]  portID        Numeric ID of a virtual port.
 * \param[out] vdsPortAlias  The buffer for the NULL-terminated vDS
 *                           port alias string.
 *
 * \retval VMK_OK                          Success.
 * \retval VMK_BAD_PARAM                   An argument is NULL.
 * \retval VMK_NOT_FOUND                   The vDS port or vDS port alias
 *                                         was not found
 * \retval VMK_PORTSET_HANDLE_NOT_MUTABLE  The caller did not hold a
 *                                         mutable handle
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortLookupVDSPortAlias(vmk_SwitchPortID portID,
                                            char *vdsPortAlias);


/*
 ***********************************************************************
 * vmk_PortLookupVDSPortID --                                     */ /**
 *
 * \brief Retrieves the vDS port ID for the specified ephemeral port.
 *
 * \nativedriversdisallowed
 *
 * \note The port ID string is constant for the lifetime of the
 *       ephemeral port.
 *
 * \note The caller must provide a buffer with length longer or equal
 *       to VMK_VDS_PORT_ID_MAX_LEN bytes.
 *
 * \note The caller must hold a mutable handle for the portset
 *       associated with the specified portID.
 *
 * \note The function will not block.
 *
 * \param[in]   portID        Numeric ID of a virtual port.
 * \param[out]  vdsPortID     The buffer for the NULL-terminated vDS
 *                            port ID string.
 * \param[in]   vdsPortIDLen  vdsPortID buffer length.
 *
 * \retval VMK_OK                          Success.
 * \retval VMK_BAD_PARAM                   An argument is NULL.
 * \retval VMK_NOT_FOUND                   The vDS port or vDS port ID
 *                                         was not found
 * \retval VMK_PORTSET_HANDLE_NOT_MUTABLE  The caller did not hold a
 *                                         mutable handle
 * \retval VMK_BUF_TOO_SMALL               The buffer is too small.
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortLookupVDSPortID(vmk_SwitchPortID portID,
                                         char *vdsPortID,
                                         vmk_uint32 vdsPortIDLen);


/*
 ***********************************************************************
 * vmk_PortsetLookupVDSID --                                      */ /**
 *
 * \brief Retrieves the vDS switch ID for the specified portset.
 *
 * \nativedriversdisallowed
 *
 * \note The vDS switch ID is constant for the lifetime of the portset.
 *
 * \note The caller must provide a buffer that is VMK_VDS_ID_MAX_LEN
 *       bytes in length.
 *
 * \note The function will not block.
 *
 * \param[in]  ps     Mutable portset handle.
 * \param[out] vdsID  The buffer for the NULL-terminated vDS ID string.
 *
 * \retval VMK_OK         Success.
 * \retval VMK_BAD_PARAM  An argument is NULL.
 * \retval VMK_NOT_FOUND  There is no vDS associated with this portset
 *                        or vDS switch ID was not found 
 * \retval VMK_FAILURE    The caller did not hold a immutable/mutable
 *                        handle
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortsetLookupVDSID(vmk_Portset *ps,
                                        char *vdsID);

/*
 ***********************************************************************
 * vmk_PortsetLookupVDSName --                                    */ /**
 *
 * \brief Retrieves the vDS switch name for the specified portset.
 *
 * \nativedriversdisallowed
 *
 * \note The vDS switch name is constant for the lifetime of the
 *       portset.
 *
 * \note If the vDS switch name is not configured, then this function
 *       will succeed and the string returned in vdsName will be
 *       "unknown".
 *
 * \note The caller must provide a buffer that is VMK_VDS_NAME_MAX_LEN
 *       bytes in length.
 *
 * \note The function will not block.
 *
 * \param[in]  ps       Mutable portset handle.
 * \param[out] vdsName  The buffer for the NULL-terminated vDS name 
 *                      string.
 *
 * \retval VMK_OK                          Success.
 * \retval VMK_BAD_PARAM                   An argument is NULL.
 * \retval VMK_NOT_FOUND                   There is no vDS associated
 *                                         with this portset or vDS switch
 *                                         name was not found
 * \retval VMK_PORTSET_HANDLE_NOT_MUTABLE  The caller did not hold a
 *                                         mutable handle
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortsetLookupVDSName(vmk_Portset *ps,
                                          char *vdsName);

/*
 ***********************************************************************
 * vmk_PortsetLookupVDSClassName --                               */ /**
 *
 * \brief Retrieves the vDS switch class name for the specified portset.
 *
 * \nativedriversdisallowed
 *
 * \note The vDS class name is constant for the lifetime of the portset.
 *
 * \note The caller provide a buffer that is VMK_VDS_CLASS_NAME_MAX_LEN
 *       bytes in length.
 *
 * \note The function will not block.
 *
 * \param[in]  ps            Portset handle.
 * \param[out] vdsClassName  The buffer for the NULL-terminated vDS
 *                           class name string.
 *
 * \retval VMK_OK                          Success.
 * \retval VMK_BAD_PARAM                   An argument is NULL.
 * \retval VMK_NOT_FOUND                   There is no vDS associated
 *                                         with this portset or vDS
 *                                         switch class name was not
 *                                         found   
 * \retval VMK_PORTSET_HANDLE_NOT_MUTABLE  The caller did not hold a
 *                                         mutable handle
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortsetLookupVDSClassName(vmk_Portset *ps,
                                               char *vdsClassName);

/*
 ***********************************************************************
 * vmk_PortsetLookupVDSVersion --                                 */ /**
 *
 * \brief Retrieves the vDS version for the specified portset.
 *
 * \nativedriversdisallowed
 *
 * \note The vDS version is constant for the lifetime of the portset.
 *
 * \note The function will not block.
 *
 * \param[in]  ps          Portset handle.
 * \param[out] vdsVersion  The version for the specified portset.
 *
 * \retval VMK_OK              Success.
 * \retval VMK_BAD_PARAM       An argument is NULL.
 * \retval VMK_NOT_FOUND       There is no vDS associated with this
 *                             portset.
 * \retval VMK_INVALID_HANDLE  The caller did not hold an immutable
 *                             or mutable handle.
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortsetLookupVDSVersion(vmk_Portset *ps,
                                             vmk_revnum *vdsVersion);

/*
 ***********************************************************************
 * vmk_PortFindByVDSPortID --                                     */ /**
 *
 * \brief Retrieves the numeric port identifier for the ephemeral port.
 *
 * \nativedriversdisallowed
 *
 * \note The caller must not hold any locks or portset handles.
 *
 * \note The function will not block.
 *
 * \param[in]   vdsID          VDS switch ID string.
 * \param[in]   vdsPortID      VDS port ID string.
 * \param[out]  portID         Numeric ID of a virtual port.
 *
 * \retval      VMK_OK         Success.
 * \retval      VMK_BAD_PARAM  One or more parameters are NULL.
 * \retval      VMK_NOT_FOUND  The specified vdsPort could not be found,
 *                             or is not associated with an ephemeral
 *                             port.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortFindByVDSPortID(const char *vdsID,
                                         const char *vdsPortID,
                                         vmk_SwitchPortID *portID);


/*
 ***********************************************************************
 * vmk_PortsetFindByVDSID --                                      */ /**
 *
 * \brief Retrieves the name of the portset.
 *
 * \nativedriversdisallowed
 *
 * \note The caller must provide a buffer that is VMK_PORTSET_NAME_MAX
 *       bytes in length.
 *
 * \note The caller must not hold any locks or portset handles.
 *
 * \note The function will not block.
 * 
 * \param[in]  vdsID   VDS switch ID string.
 * \param[out] psName  Buffer for the portset name string.
 *
 * \retval      VMK_OK         Success.
 * \retval      VMK_BAD_PARAM  One or more parameters are NULL.
 * \retval      VMK_NOT_FOUND  The specified vds could not be found.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortsetFindByVDSID(const char *vdsID,
                                        char *psName);


/*
 ***********************************************************************
 * vmk_VDSClientCmdRegister --                                    */ /**
 *
 * \brief Register vDS command execution handler.
 *
 * \nativedriversdisallowed
 *
 * \note At most one client may be registered for a given command name.
 *
 * This operation allows a module to register a command callback into vDS.
 * The callback is identified by a string format command name.
 *
 * \note Vendors command names follow conventions defined as
 *       "com.<vendor>...."
 *
 * \note This function will not block.
 *
 * \param[out]  client            Ptr to handle for vDS Command Client
 * \param[in]   psName            Portset name
 * \param[in]   cmdName           Command name
 * \param[in]   callback          Command callback
 *
 * \retval      VMK_OK            Command client registered successfully
 * \retval      VMK_EXISTS        A client already exists for the given
 *                                portset & command name
 * \retval      VMK_NO_MEMORY     Insufficient memory to perform
 *                                registration
 * \retval      VMK_BAD_PARAM     A required callback is unspecified
 *                                (NULL)
 ***********************************************************************
 */

VMK_ReturnStatus vmk_VDSClientCmdRegister(vmk_VDSCmdClient *client,
                                          const char *psName,
                                          const char *cmdName,
                                          vmk_VDSCmdClientExec callback);
/*
 ***********************************************************************
 * vmk_VDSClientCmdUnregister --                                  */ /**
 *
 * \brief Unregister vDS Command handler.
 *
 * \nativedriversdisallowed
 *
 * \note This function will not block.
 *
 * \param[in]   client         Handle for vDS Command Client
 *
 * \retval      VMK_OK         Command Client unregistered successfully
 * \retval      VMK_NOT_FOUND  The specified client does not exist
 * \retval      VMK_BUSY       The client is being used.
 ***********************************************************************
 */

VMK_ReturnStatus vmk_VDSClientCmdUnregister(vmk_VDSCmdClient client);

/*
 ***********************************************************************
 * vmk_VDSEventExPost --                                          */ /**
 *
 * \brief Post a vDS level vmk_VDSEventEx to hostd or other vDS
 *        event consumers. vmk_VDSEventEx with customized name and
 *        arguments will be posted to the management plane. Such events
 *        will be displayed by vCenter if the relevant event description
 *        file is registered via the VIM API.
 *
 * \nativedriversdisallowed
 *
 * \note The caller must hold an immutable handle for the portset.
 *
 * \note Modules should control the rate of event posting to avoid
 *       sending too many events in a short time period. vDS framework
 *       will throttle repeated vDS EventExs sent from the same vDS
 *       within a second.
 *
 * \note This function will copy the strings in the input vmk_VDSEventEx,
 *       Caller should free these strings if they are allocated from heap.
 *
 * \note This function will not block.
 *
 * \param[in]  ps                   Immutable portset handle
 * \param[in]  event                Event name and arguments
 *
 * \retval     VMK_OK               Event posted successfully
 * \retval     VMK_FAILURE          The caller did not hold an
 *                                  immutable portset handle
 * \retval     VMK_BAD_PARAM        Invalid parameter, if ps and event
 *                                  are NULL, or event argument number
 *                                  exceeds the max value.
 * \retval     VMK_NO_MEMORY        Out of Memory
 * \retval     VMK_LIMIT_EXCEEDED   The total length of event name and
 *                                  event arguments is too long.
 * \retval     VMK_EXISTS           A same eventEx is being posted.
 ***********************************************************************
 */

VMK_ReturnStatus vmk_VDSEventExPost(vmk_Portset *ps,
                                    vmk_VDSEventEx *event);
/*
 ***********************************************************************
 * vmk_VDSPortEventExPost --                                      */ /**
 *
 * \brief Post a port level vmk_VDSEventEx to hostd or other vDS
 *        event consumers. vmk_VDSEventEx with customized name and
 *        arguments will be posted to the management plane. Such events
 *        will be displayed by vCenter if the relevant event description
 *        file is registered via the VIM API.
 *
 * \nativedriversdisallowed
 *
 * \note The caller must hold an immutable handle for the portset
 *       that the port is connected to.
 *
 * \note Modules should control the rate of event posting to avoid
 *       sending too many events in a short time period. vDS framework
 *       will throttle repeated vDS port EventExs sent from the same
 *       dvport within a second.
 *
 * \note This function will copy the strings in the input vmk_VDSEventEx,
 *       Caller should free these strings if they are allocated from heap.
 *
 * \note This function will not block.
 *
 * \param[in]  portID               Numeric ID of a virtual port
 * \param[in]  event                Event name and arguments
 *
 * \retval     VMK_OK               Event posted successfully
 * \retval     VMK_FAILURE          The caller did not hold an
 *                                  immutable portset handle
 * \retval     VMK_BAD_PARAM        Invalid parameter, if event is NULL,
 *                                  portID is invalid or event argument
 *                                  number exceeds the max value.
 * \retval     VMK_NO_MEMORY        Out of Memory
 * \retval     VMK_NOT_FOUND        The portset/port does not exist
 * \retval     VMK_LIMIT_EXCEEDED   The total length of event name and
 *                                  event arguments are too long.
 * \retval     VMK_EXISTS           A same eventEx is being posted.
 ***********************************************************************
 */

VMK_ReturnStatus vmk_VDSPortEventExPost(vmk_SwitchPortID portID,
                                        vmk_VDSEventEx *event);

/*
 ***********************************************************************
 * vmk_VDSPortsetDataGet --                                       */ /**
 *
 * \brief Get the value of a specified portset data (name, value) pair.
 *
 * \nativedriversdisallowed
 *
 * \note The caller must hold a mutable handle for the portset.
 *
 * \note Portset data returned can be used safely only when the mutable
 *       portset handle remains held by caller.
 *
 * \note The function will not block.
 *
 * \param[in]   ps             Mutable portset Handle.
 * \param[in]   dataName       Name of portset data.
 * \param[out]  dataValue      Buffer for value to associated with
 *                             the name.
 * \param[out]  dataLength     Length of data returned.
 * \param[in]   isHostProp     Whether the data is host property.
 *
 * \retval VMK_OK                          Success; dataValue and
 *                                         DataLength updated.
 * \retval VMK_BAD_PARAM                   One of the provided ps,
 *                                         dataName, dataValue, or
 *                                         dataLength is invalid.
 * \retval VMK_NOT_FOUND                   There is no vDS associated
 *                                         with this portset, or the
 *                                         data was not found.
 * \retval VMK_PORTSET_HANDLE_NOT_MUTABLE  The caller did not hold a
 *                                         mutable handle
 ***********************************************************************
 */

VMK_ReturnStatus vmk_VDSPortsetDataGet(vmk_Portset *ps,
                                       const char *dataName,
                                       vmk_AddrCookie *dataValue,
                                       vmk_uint32 *dataLength,
                                       vmk_Bool isHostProp);

#endif
/** @} */
/** @} */
