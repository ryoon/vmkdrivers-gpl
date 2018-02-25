/* **********************************************************
 * Copyright 2006-2010,2012 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * NetPoll                                                        */ /**
 * \addtogroup Network
 *@{
 * \defgroup NetPoll NetPoll
 *@{
 *
 * \par NetPoll:
 *
 * Create and access poll data structure for network devices. The net
 * poll routine polls packet from the device and pushes those packets
 * up the stack. It also performs tx completions for the device.
 *
 * The poll is initialized using vmk_NetPollCreate and takes two
 * callbacks. The first callback is used to poll packets from the
 * network device. Once the packets has been polled, the net poll
 * routine will either use the default push processing or call the
 * second callback to do custom push processing for these packets.
 * Once the poll is activated it will run while there are packets to
 * be processed.
 *
 ***********************************************************************
 */

#ifndef _VMKAPI_NET_POLL_H_
#define _VMKAPI_NET_POLL_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "net/vmkapi_net_types.h"
#include "net/vmkapi_net_pktlist.h"

/**
 * \ingroup NetPoll
 * \brief Net poll features.
 */
typedef enum {
   /**
    * \brief Use default push routine.
    */
   VMK_NETPOLL_NONE = 0,

   /**
    * \brief Use custom push routine.
    */
   VMK_NETPOLL_CUSTOM_DELIVERY_CALLBACK = 1,
} vmk_NetPollFeatures;

/**
 * \ingroup NetPoll
 * \brief Net poll state.
 */
typedef enum {
   /**
    * \brief Net poll is disabled.
    */
   VMK_NETPOLL_DISABLED = 0,

   /**
    * \brief Net poll is running or scheduled.
    */
   VMK_NETPOLL_ACTIVE = 1,
} vmk_NetPollState;


/**
 * \brief Net poll object.
 */
typedef struct vmk_NetPollInt *vmk_NetPoll;

/*
 ***********************************************************************
 * vmk_NetPollCallback --                                         */ /**
 *
 * \ingroup NetPoll
 * \brief Poll routine for the device
 *
 * \param[in] priv       Private data structure for device poll routine.
 * \param[in] budget     Maximum number of packets to be processed in
 *                       each invocation
 *
 * \retval               Whether to poll the device for more packets.
 *
 ***********************************************************************
 */

typedef vmk_Bool (*vmk_NetPollCallback) (vmk_AddrCookie priv,
                                         vmk_uint32 budget);

/*
 ***********************************************************************
 * vmk_NetPollDeliveryCallback --                                 */ /**
 *
 * \ingroup NetPoll
 * \brief Custom push routine for processing the packets received.
 *
 * \param[in] rxPktList  Packet list for the custom push routine.
 * \param[in] priv       Private data structure for device poll routine
 *
 ***********************************************************************
 */

typedef void (*vmk_NetPollDeliveryCallback) (vmk_PktList rxPktList,
                                             vmk_AddrCookie priv);

/**
 * \ingroup NetPoll
 * \brief Net poll initialization variables.
 */

typedef struct vmk_NetPollProperties {
   /**
    * \brief Device poll routine.
    */
   vmk_NetPollCallback poll;

   /**
    * \brief Custom device push routine.
    *
    * If this parameter is not NULL and
    * NETPOLL_CUSTOM_DELIVERY_Callback is set in the
    * features parameter then packets
    * will NOT be pushed up to the vmkernel
    * networking stack, they will be passed to this
    * callback instead.
    */
    vmk_NetPollDeliveryCallback deliveryCallback;

   /**
    * \brief Poll private data handler.
    */
   vmk_AddrCookie priv;

   /**
    * \brief Poll features.
    */
   vmk_NetPollFeatures features;
} vmk_NetPollProperties;

/*
 ***********************************************************************
 * vmk_NetPollActivate --                                         */ /**
 *
 * \ingroup NetPoll
 * \brief Activate the poll thread.
 *
 * The poll thread calls the device callback for polling packets.
 * These packets are pushed up the stack by the default push routine
 * or the custom callback used during the initialization. The poll
 * thread continues to push packets while the device has pending packets.
 *
 * \param[in] netPoll          Handler for the net poll.
 *
 * \retval VMK_OK              Success.
 * \retval VMK_INVALID_HANDLE  Invalid poll context.
 * \retval VMK_FAILURE         Poll could not be activated.

 ***********************************************************************
 */

VMK_ReturnStatus vmk_NetPollActivate(vmk_NetPoll netPoll);

/*
 ***********************************************************************
 * vmk_NetPollDeliveryCallbackSet --                              */ /**
 *
 * \ingroup NetPoll
 * \brief Set new delivery callback for net poll handler.
 *
 * Net poll routine will use the new callbacks.
 *
 * \param[in] netPoll          Handler for the net poll.
 * \param[in] init             New properties for the poll handler.
 *
 * \retval VMK_OK              Success.
 * \retval VMK_INVALID_HANDLE  Invalid poll context.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_NetPollDeliveryCallbackSet(vmk_NetPoll netPoll,
                                                vmk_NetPollProperties *init);

/*
 ***********************************************************************
 * vmk_NetPollDeliveryCallbackClear --                            */ /**
 *
 * \ingroup NetPoll
 * \brief Clear new delivery callback for net poll handler.
 *
 * Net poll routine will use the new callbacks.
 *
 * \param[in] netPoll          Handler for the net poll.
 *
 * \retval VMK_OK              Success.
 * \retval VMK_INVALID_HANDLE  Invalid poll context.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_NetPollDeliveryCallbackClear(vmk_NetPoll netPoll);


/*
 ***********************************************************************
 * vmk_NetPollCheckState --                                       */ /**
 *
 * \ingroup NetPoll
 * \brief Get the poll state.
 *
 * The state indicates whether the poll is active or suspended.
 *
 * \param[in] netPoll          Handler for the net poll.
 * \param[out] state           The state of the poll.
 *
 * \retval VMK_OK              Success.
 * \retval VMK_INVALID_HANDLE  Invalid poll context.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_NetPollCheckState(vmk_NetPoll netPoll,
                                       vmk_NetPollState *state);

/*
 ***********************************************************************
 * vmk_NetPollDestroy --                                          */ /**
 *
 * \ingroup NetPoll
 * \brief Destroy the poll thread.
 *
 * Destroys the netpoll synchronously by waiting for any active netpoll
 * context to complete processing packets and issueing poll callbacks
 *
 * \param[in] netPoll     Handler for the net poll.
 *
 ***********************************************************************
 */

void vmk_NetPollDestroy(vmk_NetPoll netPoll);

/*
 ************************************************************************
 * vmk_NetPollGetCurrent --                                        */ /**
 *
 * \ingroup NetPoll
 * \brief Get the current executing net poll context.
 *
 * \param[out] netPoll          Handler for the poll.
 *
 * \retval VMK_OK               Success.
 * \retval VMK_INVALID_HANDLE   Invalid poll context.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_NetPollGetCurrent(vmk_NetPoll *netPoll);

/*
 ************************************************************************
 * vmk_NetPollGetPrivate --                                        */ /**
 *
 * \ingroup NetPoll
 * \brief Return the private data registered for the device callback.
 *
 * \param[in] netPoll      Handler for the net poll.
 *
 * \return                 Void address registered with the
 *                         device callback.
 *
 ***********************************************************************
 */

vmk_AddrCookie vmk_NetPollGetPrivate(vmk_NetPoll netPoll);

/*
 ***********************************************************************
 * vmk_NetPollCreate --                                           */ /**
 *
 * \brief Create a net poll
 *
 * \param[in]  pollInit    Poll initialization parameters.
 * \param[in]  id          Vmkernel service account.
 * \param[in]  moduleID    module ID
 * \param[out] poll        Handler for the net poll.
 *
 * \retval VMK_OK          Success.
 * \retval VMK_NO_MEMORY   Cannot allocate net poll resource.
 * \retval VMK_FAILURE     Poll initialization failed.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_NetPollCreate(vmk_NetPollProperties *pollInit,
                                   vmk_ServiceAcctID id,
                                   vmk_ModuleID moduleID,
                                   vmk_NetPoll *poll);

/*
 ***********************************************************************
 * vmk_NetPollRxPktQueue --                                       */ /**
 *
 * \ingroup NetPoll
 * \brief Queue packets received from the NIC.
 *
 * \pre The poll should be in active state.
 *
 * \param[in] netPoll           Handler for the net poll.
 * \param[in] pkt               Packet to be queued.
 *
 * \retval VMK_OK               Success.
 * \retval VMK_INVALID_HANDLE   Invalid poll handle.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_NetPollRxPktQueue(vmk_NetPoll netPoll,
                                       vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_NetPollQueueCompPkt --                                     */ /**
 *
 * \ingroup NetPoll
 * \brief Queue packets for tx completion or queue received packets that
 *        need to be released on error.
 *
 * \pre The poll should be in active state.
 *
 * \param[in] netPoll           Handler for the net poll.
 * \param[in] pkt               Packet to be queued.
 *
 * \retval VMK_OK               Success.
 * \retval VMK_INVALID_HANDLE   Invalid poll handle.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_NetPollQueueCompPkt(vmk_NetPoll netPoll,
                                         vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_NetPollInterruptSet --                                     */ /**
 *
 * \ingroup NetPoll
 * \brief Set the associated interrupt cookie with the poll.
 *
 * Vmkernel can have an interrupt cookie be associated with the network
 * poll so that the interrupt handler can be affinitized with the poll
 * routine.
 *
 * \param[in] netPoll          Handler for the net poll.
 * \param[in] intrCookie       Interrupt cookie.
 *
 * \retval VMK_OK              Success.
 * \retval VMK_INVALID_HANDLE  Invalid Poll handle.
 * \retval VMK_BAD_PARAM       If invalid interrupt cookie specified or
 *                             cookie could not be controlled.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_NetPollInterruptSet(vmk_NetPoll netPoll,
                                         vmk_IntrCookie intrCookie);

/*
 ***********************************************************************
 * vmk_NetPollInterruptUnSet --                                   */ /**
 *
 * \ingroup NetPoll
 * \brief Clear the interrupt cookie registered with the poll.
 *
 * \param[in] netPoll          Handler for the net poll.
 *
 * \retval VMK_OK              Success.
 * \retval VMK_INVALID_HANDLE  Invalid Poll handle.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_NetPollInterruptUnSet(vmk_NetPoll netPoll);


/*
 ***********************************************************************
 * vmk_NetPollRegisterUplink --                                   */ /**
 *
 * \brief Register the uplink for the poll.
 *
 * Each poll requires a uplink handle if they are to use the poll
 * callback. If a netpoll handle is exposed in uplink's shared data, it
 * will be registered to the corresponding uplink port automatically
 * during uplink device creation. For those not exposed in the shared
 * data, calling this vmkapi in uplink's start IO callback is required
 * to perform the registration. However if the device provides its own
 * custom poll callback, no registration for uplink is required.
 *
 * A poll can have just one uplink be registered. Adding another uplink
 * will return error. However an uplink can be shared by multiple
 * net polls. But the uplink can have only one default poll registered.
 * The default poll is registered using the defaultPoll during the
 * initialization process. Once a default poll is registered it cannot
 * be overwritten.
 *
 * \param[in] netPoll       Handler for the net poll.
 * \param[in] uplink        Uplink from where the packets came from.
 * \param[in] name          Descriptive name for the poll.
 * \param[in] defaultPoll   Whether the net poll is default poll for
 *                          the NIC.
 *
 * \retval VMK_OK           Success.
 * \retval VMK_BAD_PARAM    Invalid uplink assignment.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_NetPollRegisterUplink(vmk_NetPoll netPoll,
                                           vmk_Uplink uplink,
                                           vmk_Name name,
                                           vmk_Bool defaultPoll);

/*
 ***********************************************************************
 * vmk_NetPollUnregisterUplink --                                 */ /**
 *
 * \brief Unregister the uplink for the poll.
 *
 * \param[in] netPoll       Handler for the net poll.
 *
 * \retval VMK_OK           Success.
 * \retval VMK_BAD_PARAM    Invalid uplink assignment.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_NetPollUnregisterUplink(vmk_NetPoll netPoll);


/*
 ************************************************************************
 * vmk_NetPollFlushRx --                                         */ /**
 *
 * \ingroup NetPoll
 * \brief Process packets stored in the net poll list.
 *
 * \param[in] netPoll        Handler for the net poll.
 *
 ***********************************************************************
 */

void vmk_NetPollFlushRx(vmk_NetPoll netPoll);

/*
 ***********************************************************************
 * vmk_NetPollRxPktListQueue --                                   */ /**
 *
 * \ingroup NetPoll
 * \brief Queue a packet list into the poll context
 *
 * \param[in]   netPoll     Handler for netpoll
 * \param[in]   pktList     packet list to be queued.
 *
 * \retval     VMK_OK       If queueing was successful
 * \retval     VMK_FAILURE  Otherwise.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_NetPollRxPktListQueue(vmk_NetPoll netPoll,
                                           vmk_PktList pktList);

/*
 ***********************************************************************
 * vmk_NetPollCompPktListQueue --                                 */ /**
 *
 * \ingroup NetPoll
 * \brief Queue a packet list for completion into the poll context
 *
 * \param[in]   netPoll     Handler for netpoll
 * \param[in]   pktList     packet list to be queued.
 *
 * \retval     VMK_OK       if queueing pkt list for completion was successful
 * \retval     VMK_FAILURE  Otherwise.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_NetPollCompPktListQueue(vmk_NetPoll netPoll,
                                             vmk_PktList pktList);

/*
 ***********************************************************************
 * vmk_NetPollDisable --                                          */ /**
 *
 * \ingroup     NetPoll
 * \brief       Disable NetPoll
 *
 * Disables the netpoll synchronously by coordinating with the netpoll
 * world/worldet.
 *
 * \param[in]   netPoll     Handler for netpoll
 *
 * \retval      None
 *
 ***********************************************************************
 */
void vmk_NetPollDisable(vmk_NetPoll netPoll);

/*
 ***********************************************************************
 * vmk_NetPollEnable --                                          */ /**
 *
 * \ingroup     NetPoll
 * \brief       Enable NetPoll
 *
 * Enables the netpoll and wakes up any waiting world/worldlet.
 *
 * \param[in]   netPoll     Handler for netpoll
 *
 * \retval      None
 *
 ***********************************************************************
 */
void vmk_NetPollEnable(vmk_NetPoll netPoll);

#endif /* _VMKAPI_NET_POLL_H_ */
/** @} */
/** @} */

