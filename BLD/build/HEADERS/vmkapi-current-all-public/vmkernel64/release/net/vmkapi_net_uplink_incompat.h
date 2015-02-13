/* **********************************************************
 * Copyright 2006 - 2011 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Uplink                                                         */ /**
 * \addtogroup Network
 *@{
 * \addtogroup Uplink Uplink management
 *@{
 * \defgroup Incompat Uplink APIs
 *@{
 *
 * \par Uplink:
 *
 * A module may have many different functional direction and one of
 * them is to be a gateway to external network.
 * Thereby vmkernel could rely on this module to Tx/Rx packets.
 *
 * So one can imagine an uplink as a vmkernel bundle containing
 * all the handle required to interact with a module's internal network 
 * object.
 *
 * Something important to understand is that an uplink need to reflect
 * the harware services provided by the network interface is linked to.
 * Thereby if your network interface  do vlan tagging offloading a capability 
 * should be passed to vmkernel to express this service and it will be able
 * to use this capability to optimize its internal path when the got
 * corresponding uplink is going to be used.
 *
 ***********************************************************************
 */

#ifndef _VMKAPI_NET_UPLINK_INCOMPAT_H_
#define _VMKAPI_NET_UPLINK_INCOMPAT_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "net/vmkapi_net_types.h"
#include "net/vmkapi_net_pkt.h"
#include "net/vmkapi_net_pktlist.h"
#include "net/vmkapi_net_netqueue.h"
#include "net/vmkapi_net_dcb.h"
#include "net/vmkapi_net_pt.h"
#include "net/vmkapi_net_uplink.h"
#include "vds/vmkapi_vds_port.h"

/**
 * \brief Number of bytes for uplink driver info fields.
 */
#define VMK_UPLINK_DRIVER_INFO_MAX      VMK_MODULE_NAME_MAX

/**
 * \brief Number of bytes for uplink wake-on-lan strings.
 */
#define VMK_UPLINK_WOL_STRING_MAX       16

/**
 * \brief Capabilities provided by the device associated to an uplink.
 */

typedef vmk_uint64 vmk_UplinkCapabilities;

/**
 * \brief Structure containing link status information related to the device associated to an uplink.
 */

typedef struct {
   
   /** Device link state */
   vmk_LinkState linkState;
 
   /** Device link speed in Mbps */
   vmk_int32 linkSpeed;

   /** Device full duplex activated */
   vmk_Bool  fullDuplex;
} vmk_UplinkLinkInfo;

/**
 * \brief Structure containing PCI information of the device associated to an uplink.
 */

typedef struct {
   /** Device representing the uplink */
   vmk_Device device;
   
   /** Device DMA constraints */
   vmk_DMAConstraints constraints;
} vmk_UplinkDeviceInfo;

/**
 * \brief Structure containing Panic-time polling information of the device associated to an uplink.
 */

typedef struct {

   /** Interrupt vector */
   vmk_uint32     vector;

   /** Polling data to be passed to the polling function */
   void          *clientData;   
} vmk_UplinkPanicInfo;

/**
 * \brief Structure containing memory resources information related to the device associated to an uplink.
 */

typedef struct {
      
   /** Uplink I/O address */
   void                *baseAddr;
   
   /** Shared mem start */
   void                *memStart;

   /** Shared mem end */
   void                *memEnd;

   /** DMA channel */
   vmk_uint8            dma;
} vmk_UplinkMemResources;

/**
 * \brief Structure containing the information of the driver controlling the
 *        the device associated to an uplink.
 */

typedef struct {

   /** \brief String used to store the name of the driver */
   char driver[VMK_UPLINK_DRIVER_INFO_MAX];

   /** \brief String used to store the version of the driver */
   char version[VMK_UPLINK_DRIVER_INFO_MAX];

   /** \brief String used to store the firmware version of the driver */
   char firmwareVersion[VMK_UPLINK_DRIVER_INFO_MAX];

   /** \brief String used to store the name of the module managing this driver */
   char moduleInterface[VMK_UPLINK_DRIVER_INFO_MAX];
} vmk_UplinkDriverInfo;

/**
 * \brief Capabilities of wake-on-lan (wol)
 */
typedef enum {
   /** \brief wake on directed frames */
   VMK_UPLINK_WAKE_ON_PHY         =       0x01,

   /** \brief wake on unicast frame */
   VMK_UPLINK_WAKE_ON_UCAST       =       0x02,

   /** \brief wake on multicat frame */
   VMK_UPLINK_WAKE_ON_MCAST       =       0x04,

   /** \brief wake on broadcast frame */
   VMK_UPLINK_WAKE_ON_BCAST       =       0x08,

   /** \brief wake on arp */
   VMK_UPLINK_WAKE_ON_ARP         =       0x10,

   /** \brief wake up magic frame */
   VMK_UPLINK_WAKE_ON_MAGIC       =       0x20,

   /** \brief wake on magic frame */
   VMK_UPLINK_WAKE_ON_MAGICSECURE =       0x40

} vmk_UplinkWolCaps;

/**
 * \brief Structure describing the wake-on-lan features and state of an uplink
 */

typedef struct {

   /** \brief bit-flags, describing uplink supported wake-on-lan features */
   vmk_UplinkWolCaps supported;

   /** \brief bit-flags, describing uplink enabled wake-on-lan features */
   vmk_UplinkWolCaps enabled;

   /** \brief wake-on-lan secure on password */
   char secureONPassword[VMK_UPLINK_WOL_STRING_MAX];

} vmk_UplinkWolState;


/**
 * \brief Structure describing interrupt coalescing parameters of an uplink
 */
typedef struct {
   /** \brief number of milliseconds to wait for Rx, before interrupting */
   vmk_uint32             rxUsecs;

   /** \brief maximum number of (Rx) frames to wait for, before interrupting */
   vmk_uint32             rxMaxFrames;

   /** \brief number of milliseconds to wait for completed Tx, before interrupting */
   vmk_uint32             txUsecs;

   /** \brief maximum number of completed (Tx) frames to wait for, before interrupting */
   vmk_uint32             txMaxFrames;

} vmk_UplinkCoalesceParams;


/*
 ***********************************************************************
 * vmk_UplinkStartTx --                                           */ /**
 *
 * \brief Handler used by vmkernel to send packet through the device
 *        associated to an uplink.
 *
 * \param[in] clientData         Internal module structure for the device
 *                               associated to the uplink. This structure
 *                               is the one passed during uplink connection
 * \param[in] pktList            The set of packet needed to be sent
 *
 * \retval    VMK_OK             All the packets are being processed
 * \retval    VMK_FAILURE        If the module detects any error during
 *                               Tx process
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkStartTx)(void *clientData, 
					      vmk_PktList pktList);

/*
 ***********************************************************************
 * vmk_UplinkOpenDev --                                           */ /**
 *
 * \brief Handler used by vmkernel to open the device associated to an uplink .
 *
 * \param[in] clientData         Internal module structure for the device
 *                               associated to the uplink. This structure
 *                               is the one passed during uplink connection
 *
 * \retval    VMK_OK             Open succeeded
 * \retval    VMK_FAILURE        Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus        (*vmk_UplinkOpenDev)(void *clientData);

/*
 ***********************************************************************
 * vmk_UplinkCloseDev --                                           */ /**
 *
 * \brief Handler used by vmkernel to close the device associated to an uplink
 *
 * \param[in] clientData         Internal module structure for the device
 *                               associated to the uplink. This structure
 *                               is the one passed during uplink connection
 *
 * \retval    VMK_OK             Close succeeded
 * \retval    VMK_FAILURE        Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus        (*vmk_UplinkCloseDev)(void *clientData);

/*
 ***********************************************************************
 * vmk_UplinkPanicPoll --                                      */ /**
 *
 * \brief Handler used by vmkernel to poll for packets received by 
 *        the device associated to an uplink. Might be ignored.
 *
 * \param[in]  clientData        Internal module structure for the device
 *                               associated to the uplink. This structure
 *                               is the one passed during uplink connection
 * \param[in]  budget            Maximum work to do in the poll function.
 * \param[out] workDone          The amount of work done by the poll handler
 *
 * \retval    VMK_OK             Always
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkPanicPoll)(void *clientData,
                                                vmk_uint32 budget,
                                                vmk_int32* workDone);

/*
 ***********************************************************************
 * vmk_UplinkFlushBuffers --                                      */ /**
 *
 * \brief Handler used by vmkernel to flush the Tx/Rx buffer of 
 *        the device associated to an uplink. Might be ignored.
 *
 * \param[in] clientData         Internal module structure for the device
 *                               associated to the uplink. This structure
 *                               is the one passed during uplink connection
 *
 * \retval    VMK_OK             Always
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkFlushBuffers)(void *clientData);

/*
 ***********************************************************************
 * vmk_UplinkIoctlCB --                                           */ /**
 *
 * \brief  Handler used by vmkernel to do an ioctl call against the 
 *         device associated to an uplink.
 *         
 *
 * \param[in]  uplinkName         Name of the aimed device
 * \param[in]  cmd                Command ioctl to be issued
 * \param[in]  args               Arguments to be passed to the ioctl call
 * \param[out] result             Result value of the ioctl call
 *
 * \retval    VMK_OK              If ioctl call succeeded
 * \retval    VMK_FAILURE         Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkIoctlCB)(char *uplinkName,
                                              vmk_uint32 cmd, 
                                              void *args, 
                                              vmk_uint32 *result);

/*
 ***********************************************************************
 * vmk_UplinkBlockDev --                                          */ /**
 *
 * \brief  Handler used by vmkernel to block a device. No more traffic
 *         should go through after this call.
 *
 * \param[in]  clientData         Internal module structure for the device
 *                                associated to the uplink. This structure
 *                                is the one passed during uplink connection
 *
 * \retval    VMK_OK              If device is blocked
 * \retval    VMK_FAILURE         Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkBlockDev)(void *clientData);

/*
 ***********************************************************************
 * vmk_UplinkUnblockDev --                                        */ /**
 *
 * \brief  Handler used by vmkernel to unblock a device. Traffic should
 *         go through after this call.
 *         
 *
 * \param[in]  clientData         Internal module structure for the device
 *                                associated to the uplink. This structure
 *                                is the one passed during uplink connection
 *
 * \retval    VMK_OK              If device is unblocked
 * \retval    VMK_FAILURE         Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkUnblockDev)(void *clientData);

/*
 ***********************************************************************
 * vmk_UplinkSetLinkStatus --                                     */ /**
 *
 * \brief  Handler used by vmkernel to set the speed/duplex of a device
 *         associated with an uplink.
 *
 * \param[in]  clientData         Internal module structure for the device
 *                                associated to the uplink. This structure
 *                                is the one passed during uplink connection
 * \param[in] linkInfo            Specifies speed and duplex
 *
 * \retval    VMK_OK              If operation was successful
 * \retval    VMK_FAILURE         Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkSetLinkStatus)(void *clientData,
                                                    vmk_UplinkLinkInfo *linkInfo);

/*
 ***********************************************************************
 * vmk_UplinkResetDev --                                          */ /**
 *
 * \brief  Handler used by vmkernel to reset a device.
 *
 * \param[in]  clientData         Internal module structure for the device
 *                                associated to the uplink. This structure
 *                                is the one passed during uplink connection
 *
 * \retval    VMK_OK              If device is reset
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkResetDev)(void *clientData);


/*
 ***********************************************************************
 * vmk_UplinkGetStates --                                         */ /**
 *
 * \brief  Handler used by vmkernel to get the state of a device
 *         associated to an uplink.
 *
 * \param[in]  clientData  Internal module structure for the device
 *                         associated to the uplink. This structure
 *                         is the one passed during uplink connection
 * \param[out] state       State of the device 
 *
 * \retval    VMK_OK       If the information is properly stored
 * \retval    VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkGetStates)(void *clientData, 
						vmk_PortClientStates *states);

/*
 ***********************************************************************
 * vmk_UplinkGetMemResources --                                   */ /**
 *
 * \brief  Handler used by vmkernel to get the memory resources of a device
 *         associated to an uplink.
 *
 * \param[in]  clientData  Internal module structure for the device
 *                         associated to the uplink. This structure
 *                         is the one passed during uplink connection
 * \param[out] resources   Memory resources of the device 
 *
 * \retval    VMK_OK       If the information is properly stored
 * \retval    VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkGetMemResources)(void *clientData,
						      vmk_UplinkMemResources *resources);

/*
 ***********************************************************************
 * vmk_UplinkGetDeviceProperties --                               */ /**
 *
 * \brief  Handler used by vmkernel to get pci properties of a device
 *         associated to an uplink.
 *
 * \param[in]  clientData  Internal module structure for the device
 *                         associated to the uplink. This structure
 *                         is the one passed during uplink connection.
 * \param[out] devInfo     Device properties of the device.
 *
 * \retval    VMK_OK       If the information is properly stored
 * \retval    VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkGetDeviceProperties)(void *clientData,
						       vmk_UplinkDeviceInfo *devInfo);

/*
 ***********************************************************************
 * vmk_UplinkGetPanicInfo --                                      */ /**
 *
 * \brief  Handler used by vmkernel to get panic-time polling properties 
 *         of a device associated to an uplink.
 *
 * \param[in]  clientData  Internal module structure for the device
 *                         associated to the uplink. This structure
 *                         is the one passed during uplink connection
 * \param[out] panicInfo   Panic-time polling properties of the device
 *
 * \retval    VMK_OK       If the information is properly stored
 * \retval    VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkGetPanicInfo)(void *clientData,
                                                   vmk_UplinkPanicInfo *panicInfo);

/*
 ***********************************************************************
 * vmk_UplinkGetMACAddr --                                        */ /**
 *
 * \brief  Handler used by vmkernel to get the mac address of a device
 *         associated to an uplink.
 *
 * \param[in]  clientData Internal module structure for the device
 *                        associated to the uplink. This structure
 *                        is the one passed during uplink connection
 * \param[out] macAddr    Buffer used to store the mac address
 *
 * \retval    VMK_OK      If the mac address is properly stored
 * \retval    VMK_FAILURE Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkGetMACAddr)(void *clientData, 
						 vmk_EthAddress macAddr);

/*
 ***********************************************************************
 * vmk_UplinkGetName --                                           */ /**
 *
 * \brief  Handler used by vmkernel to get the name of the device
 *         associated to an uplink
 *
 * \param[in]  clientData Internal module structure for the device
 *                        associated to the uplink. This structure
 *                        is the one passed during uplink connection
 * \param[out] devName    Buffer used to store uplink device name.
 * \param[in]  devNameLen Length of devName buffer.
 *
 * \retval    VMK_OK              If the name is properly stored.
 * \retval    VMK_LIMIT_EXCEEDED  If the name is too long for provided
 *                                buffer.
 * \retval    VMK_FAILURE         Otherwise.
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkGetName)(void *clientData,
					      char *devName,
                                              vmk_ByteCount devNameLen);
/*
 ***********************************************************************
 * vmk_UplinkGetStats --                                          */ /**
 *
 * \brief  Handler used by vmkernel to get statistics on a device associated
 *         to an uplink.
 *
 * \param[in]  clientData  Internal module structure for the device
 *                         associated to the uplink. This structure
 *                         is the one passed during uplink connection
 * \param[out] stats       Structure used to store all the requested
 *                         information.
 *
 * \retval    VMK_OK       If the statistics are properly stored
 * \retval    VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkGetStats)(void *clientData, 
					       vmk_PortClientStats *stats);
	
/*
 ***********************************************************************
 * vmk_UplinkGetDriverInfo --                                     */ /**
 *
 * \brief  Handler used by vmkernel to get driver information of the
 *         device associated with an uplink.
 *
 * \param[in]  clientData  Internal module structure for the device
 *                         associated to the uplink. This structure
 *                         is the one passed during uplink connection
 * \param[out] driverInfo  Structure used to store all the requested
 *                         information.
 *
 * \retval    VMK_OK       If the driver information is properly stored
 * \retval    VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */
				       
typedef VMK_ReturnStatus (*vmk_UplinkGetDriverInfo)(void *clientData, 
						    vmk_UplinkDriverInfo *driverInfo);

/*
 ***********************************************************************
 * vmk_UplinkGetWolState --                                       */ /**
 *
 * \brief  Handler used by vmkernel to get wake-on-lan state of
 *         device associated with an uplink.
 *
 * \param[in]  clientData  Internal module structure for the device
 *                         associated to the uplink. This structure
 *                         is the one passed during uplink connection
 * \param[out] wolState    Structure used to store all the requested
 *                         information.
 *
 * \retval    VMK_OK       If the driver information is properly stored
 * \retval    VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */
				       
typedef VMK_ReturnStatus (*vmk_UplinkGetWolState)(void *clientData, 
						  vmk_UplinkWolState *wolState);

/*
 ***********************************************************************
 * vmk_UplinkSetWolState --                                       */ /**
 *
 * \brief  Handler used by vmkernel to get wake-on-lan state of
 *         device associated with an uplink.
 *
 * \param[in]  clientData  Internal module structure for the device
 *                         associated to the uplink. This structure
 *                         is the one passed during uplink connection
 * \param[out] wolState    Structure used to store all the requested
 *                         information.
 *
 * \retval    VMK_OK       If the driver information is properly stored
 * \retval    VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */
				       
typedef VMK_ReturnStatus (*vmk_UplinkSetWolState)(void *clientData, 
						  vmk_UplinkWolState *wolState);

/*
 ***********************************************************************
 * vmk_UplinkGetCoalesceParams --                                 */ /**
 *
 * \brief  Handler used by vmkernel to get coalescing parameters of
 *         device associated with an uplink.
 *
 * \param[in]  clientData      Internal module structure for the device
 *                             associated to the uplink. This structure
 *                             is the one passed during uplink connection
 * \param[out] coalesceParams  Structure used to store all the requested
 *                             information.
 *
 * \retval    VMK_OK       If the driver information is properly stored
 * \retval    VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */
				       
typedef VMK_ReturnStatus (*vmk_UplinkGetCoalesceParams)(void *clientData, 
                                                        vmk_UplinkCoalesceParams *coalesceParams);

/*
 ***********************************************************************
 * vmk_UplinkSetCoalesceParams --                                 */ /**
 *
 * \brief  Handler used by vmkernel to set coalescing parameters of
 *         device associated with an uplink.
 *
 * \param[in]  clientData      Internal module structure for the device
 *                             associated to the uplink. This structure
 *                             is the one passed during uplink connection
 * \param[out] coalesceParams  Structure used to store all the requested
 *                             information.
 *
 * \retval    VMK_OK       If the driver information is properly stored
 * \retval    VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */
				       
typedef VMK_ReturnStatus (*vmk_UplinkSetCoalesceParams)(void *clientData, 
                                                        vmk_UplinkCoalesceParams *coalesceParams);


/**
 * \brief Default value for timeout handling before panic.
 */ 

#define VMK_UPLINK_WATCHDOG_HIT_CNT_DEFAULT 5

/*
 ***********************************************************************
 * vmk_UplinkGetWatchdogHitCnt --                                 */ /**
 *
 * \brief  Used only if the module provides a timeout mechanism to
 *         recover from a wedged device.
 *         Handler used by vmkernel to get the timeout hit counter needed 
 *         before hitting a panic.
 *         If no panic mode is implemented you could ignore this handler.
 *
 * \param[in]  clientData  Internal module structure for the device
 *                         associated to the uplink. This structure
 *                         is the one passed during uplink connection
 * \param[out] counter     Used to store the timeout hit counter
 *
 * \retval    VMK_OK       If the information is properly stored
 * \retval    VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkGetWatchdogHitCnt)(void *clientData, 
							vmk_int16 *counter);

/*
 ***********************************************************************
 * vmk_UplinkSetWatchdogHitCnt --                                 */ /**
 *
 * \brief  Used only if the module provides a timeout mechanism to
 *         recover from a wedged device.
 *         Handler used by vmkernel to set the timeout hit counter 
 *         needed before hitting a panic.
 *         If no panic mode is implemented you could ignore this handler.
 *
 * \param[in] clientData  Internal module structure for the device
 *                        associated to the uplink. This structure
 *                        is the one passed during uplink connection
 * \param[in] counter     The timeout hit counter
 *
 * \retval    VMK_OK       If the new setting is effective
 * \retval    VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkSetWatchdogHitCnt)(void *clientData, 
							vmk_int16 counter);

/*
 ***********************************************************************
 * vmk_UplinkGetWatchdogStats --                                  */ /**
 *
 * \brief  Used only if the module provides a timeout mechanism to
 *         recover from a wedged device.
 *         Handler used by vmkernel to know the number of times the recover
 *         process (usually a reset) has been run on the device associated
 *         to an uplink. Roughly the number of times the device got wedged.
 *
 * \param[in]  clientData  Internal module structure for the device
 *                         associated to the uplink. This structure
 *                         is the one passed during uplink connection
 * \param[out] counter     The number of times the device got wedged
 *
 * \retval    VMK_OK       If the information is properly stored
 * \retval    VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkGetWatchdogStats)(void *clientData,
						       vmk_int16 *counter);

/**
 * \brief Define the different status of uplink watchdog panic mod
 */

typedef enum {
   
   /** \brief The device's watchdog panic mod is disabled */
   VMK_UPLINK_WATCHDOG_PANIC_MOD_DISABLE,

   /** \brief The device's watchdog panic mod is enabled */
   VMK_UPLINK_WATCHDOG_PANIC_MOD_ENABLE
} vmk_UplinkWatchdogPanicModState;

/*
 ***********************************************************************
 * vmk_UplinkGetWatchdogPanicModState --                          */ /**
 *
 * \brief  Used only if the module provides a timeout mechanism to
 *         recover from a wedged device.
 *         Handler used by vmkernel to know if the timeout panic mod
 *         is enabled or not.
 *
 * \param[in]  clientData  Internal module structure for the device
 *                         associated to the uplink. This structure
 *                         is the one passed during uplink connection
 * \param[out] status      Status of the watchdog panic mod
 *
 * \retval    VMK_OK       If the information is properly stored
 * \retval    VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkGetWatchdogPanicModState)(void *clientData, 
							       vmk_UplinkWatchdogPanicModState *state);

/*
 ***********************************************************************
 * vmk_UplinkSetWatchdogPanicModState --                          */ /**
 *
 * \brief  Used only if the module provides a timeout mechanism to
 *         recover from a wedged device.
 *         Handler used by vmkernel to enable or disable the timeout 
 *         panic mod. Set panic mod could be useful for debugging as it
 *         is possible to get a coredump at this point.
 *
 * \param[in] clientData  Internal module structure for the device
 *                        associated to the uplink. This structure
 *                        is the one passed during uplink connection
 * \param[in] enable      Tne status of the watchdog panic mod
 *
 * \retval    VMK_OK       If the new panic mod is effective
 * \retval    VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkSetWatchdogPanicModState)(void *clientData, 
							       vmk_UplinkWatchdogPanicModState state);

/*
 ***********************************************************************
 * vmk_UplinkSetMTU --                                            */ /**
 *
 * \brief  Handler used by vmkernel to set up the mtu of the
 *         device associated with an uplink.
 *
 * \param[in] clientData  Internal module structure for the device
 *                        associated to the uplink. This structure
 *                        is the one passed during uplink connection
 * \param[in] mtu         The mtu to be set up
 *
 * \retval    VMK_OK       If the mtu setting is effective
 * \retval    VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkSetMTU) (void *clientData, 
					      vmk_uint32 mtu);

/*
 ***********************************************************************
 * vmk_UplinkGetMTU --                                            */ /**
 *
 * \brief  Handler used by vmkernel to retrieve the mtu of the
 *         device associated with an uplink.
 *
 * \param[in]  clientData  Internal module structure for the device
 *                         associated to the uplink. This structure
 *                         is the one passed during uplink connection
 * \param[out] mtu         Used to stored the current mtu
 *
 * \retval    VMK_OK       If the information is properly stored
 * \retval    VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkGetMTU) (void *clientData,
					      vmk_uint32 *mtu);

/*
 ***********************************************************************
 * vmk_UplinkVlanSetupHw --                                       */ /**
 *
 * \brief Handler used by vmkernel to activate vlan and add vid for the
 *        device associated to an uplink.
 *
 * \param[in] clientData         Internal module structure for the device
 *                               associated to the uplink. This structure
 *                               is the one passed during uplink connection
 * \param[in] enable             Initialize hw vlan functionality
 * \param[in] bitmap             A bitmap of permitted vlan id's.
 *
 * \retval    VMK_OK             If vlan (de)activation succeeded
 * \retval    VMK_FAILURE        Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkVlanSetupHw)(void *clientData, 
                                                  vmk_Bool enable,
						  void *bitmap);

/*
 ***********************************************************************
 * vmk_UplinkVlanRemoveHw --                                      */ /**
 *
 * \brief  Handler used by vmkernel to delete vlan ids and deactivate
 *         hw vlan for the device associated to an uplink.
 *
 * \param[in] clientData         Internal module structure for the device
 *                               associated to the uplink. This structure
 *                               is the one passed during uplink connection
 * \param[in] disable            Deactivate hw vlan completely
 * \param[in] bitmap             A bitmap of permitted vlan id's.
 *
 * \retval    VMK_OK             If vlan update succeeded
 * \retval    VMK_FAILURE        Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkVlanRemoveHw)(void *clientData, 
						   vmk_Bool disable,
                                                   void *bitmap);

/*
 ***********************************************************************
 * vmk_UplinkNetqueueOpFunc --                                    */ /**
 *
 * \brief  Handler used by vmkernel to issue netqueue control operations
 *
 * \param[in] clientData         Internal module structure for the device
 *                               associated to the uplink. This structure
 *                               is the one passed during uplink connection
 * \param[in] op                 Netqueue operation
 * \param[in] opArgs             Arguments to Netqueue operation
 *
 * \retval    VMK_OK             Operation succeeded
 * \retval    VMK_FAILURE        Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkNetqueueOpFunc)(void *clientData, 
						     vmk_NetqueueOp op, 
						     void *opArgs);

typedef VMK_ReturnStatus (*vmk_UplinkNetqueueXmit)(void *, 
						   vmk_NetqueueQueueID, 
						   vmk_PktList);
/*
 ***********************************************************************
 * vmk_UplinkPTOpFunc --                                          */ /**
 *
 * \brief  The routine to dispatch PT management operations to 
 *         driver exported callbacks
 *
 * \param[in] clientData         Used to identify a VF 
 * \param[in] op                 The operation
 * \param[in] args               The optional arguments for the operation
 *
 * \retval    VMK_OK             If the operation succeeds
 * \retval    VMK_FAILURE        Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkPTOpFunc)(void *clientData, 
                                               vmk_NetPTOP op, 
                                               void *args);

/*
 ***********************************************************************
 * vmk_UplinkIsDCBEnabled --                                      */ /**
 *
 * \brief  The routine to check whether DCB support is enabled on
 *         the device.
 *
 * \param[in]  clientData  Internal module structure for the device
 *                         associated to the uplink.
 * \param[out] enabled     Used to store the DCB state of the device.
 * \param[out] version     Used to store the DCB version supported by
 *                         the device.
 *
 * \retval    VMK_OK       If operation succeeds.
 * \retval    VMK_FAILURE  If the operation fails or if the device
 *                         is not DCB capable.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkIsDCBEnabled) (void *clientData,
                                                    vmk_Bool *enabled,
                                                    vmk_DCBVersion *version);

/*
 ***********************************************************************
 * vmk_UplinkEnableDCB --                                         */ /**
 *
 * \brief  The routine to enable DCB support on the device.
 *
 * \note   It should only be called from the DCB daemon that does
 *         DCB negotiation on behalf of this device.
 *
 * \note   vmk_UplinkDCBApplySettings() needs to be called after
 *         this call to guaranttee the changes will be flushed
 *         onto the device.
 *
 * \param[in] clientData   Internal module structure for the device
 *                         associated to the uplink.
 *
 * \retval    VMK_OK       If operation succeeds.
 * \retval    VMK_FAILURE  Otherwise.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkEnableDCB) (void *clientData);

/*
 ***********************************************************************
 * vmk_UplinkDisableDCB --                                        */ /**
 *
 * \brief  The routine to disable DCB support on the device.
 *
 * \note   It should only be called from the DCB daemon that does
 *         DCB negotiation on behalf of this device.
 *
 * \note   vmk_UplinkDCBApplySettings() needs to be called after
 *         this call to guaranttee the changes will be flushed
 *         onto the device.
 *
 * \param[in] clientData   Internal module structure for the device
 *                         associated to the uplink.
 *
 * \retval    VMK_OK       If operation succeeds.
 * \retval    VMK_FAILURE  Otherwise.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDisableDCB) (void *clientData);

/*
 ***********************************************************************
 * vmk_UplinkDCBGetNumTCs --                                      */ /**
 *
 * \brief  The routine to retrieve Traffic Classes information
 *         from the device.
 *
 * \param[in]  clientData  Internal module structure for the device
 *                         associated to the uplink.
 * \param[out] numTCs      Used to store the Traffic Class information.
 *
 * \retval    VMK_OK       If operation succeeds.
 * \retval    VMK_FAILURE  Otherwise.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBGetNumTCs) (void *clientData,
                                                    vmk_DCBNumTCs *numTCs);

/*
 ***********************************************************************
 * vmk_UplinkDCBGetPriorityGroup --                               */ /**
 *
 * \brief  The routine to retrieve DCB Priority Group settings from
 *         the device.
 *
 * \param[in]  clientData   Internal module structure for the device
 *                          associated to the uplink.
 * \param[out] pg           Used to stored the current PG setting.
 *
 * \retval    VMK_OK       If operation succeeds.
 * \retval    VMK_FAILURE  Otherwise.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBGetPriorityGroup) (void *clientData,
                                                  vmk_DCBPriorityGroup *pg);

/*
 ***********************************************************************
 * vmk_UplinkDCBSetPriorityGroup --                               */ /**
 *
 * \brief  The routine to pushdown DCB Priority Group settings to
 *         the device.
 *
 * \note   It should only be called from the DCB daemon that does
 *         DCB negotiation on behalf of this device.
 *
 * \note   vmk_UplinkDCBApplySettings() needs to be called after
 *         this call to guaranttee the changes will be flushed
 *         onto the device.
 *
 * \param[in] clientData   Internal module structure for the device
 *                         associated to the uplink.
 * \param[in] pg           The Priority Group to be set up.
 *
 * \retval    VMK_OK       If operation succeeds.
 * \retval    VMK_FAILURE  Otherwise.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBSetPriorityGroup) (void *clientData,
                                                  vmk_DCBPriorityGroup *pg);

/*
 ***********************************************************************
 * vmk_UplinkDCBGetPFCCfg --                                      */ /**
 *
 * \brief  The routine to retrieve Priority-based Flow Control
 *         configurations from the device.
 *
 * \param[in]  clientData   Internal module structure for the device
 *                          associated to the uplink.
 * \param[out] pfcCfg       Used to stored the current PFC configuration.
 *
 * \retval    VMK_OK       If operation succeeds.
 * \retval    VMK_FAILURE  Otherwise.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBGetPFCCfg) (void *clientData,
                              vmk_DCBPriorityFlowControlCfg *pfcCfg);

/*
 ***********************************************************************
 * vmk_UplinkDCBSetPFCCfg --                                      */ /**
 *
 * \brief  The routine to pushdown Priority-based Flow Control
 *         configurations to the device.
 *
 * \note   It should only be called from the DCB daemon that does
 *         DCB negotiation on behalf of this device.
 *
 * \note   vmk_UplinkDCBApplySettings() needs to be called after
 *         this call to guaranttee the changes will be flushed
 *         onto the device.
 *
 * \param[in] clientData   Internal module structure for the device
 *                         associated to the uplink.
 * \param[in] pfcCfg       The PFC configuration to be set.
 *
 * \retval    VMK_OK       If operation succeeds.
 * \retval    VMK_FAILURE  Otherwise.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBSetPFCCfg) (void *clientData,
                               vmk_DCBPriorityFlowControlCfg *pfcCfg);

/*
 ***********************************************************************
 * vmk_UplinkDCBIsPFCEnabled --                                   */ /**
 *
 * \brief  The routine to check whether Priority-based Flow Control
 *         support is enabled on the device.
 *
 * \param[in]  clientData   Internal module structure for the device
 *                          associated to the uplink.
 * \param[out] enabled      Used to stored the current PFC support state.
 *
 * \retval    VMK_OK       If operation succeeds.
 * \retval    VMK_FAILURE  Otherwise.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBIsPFCEnabled) (void *clientData,
                                                       vmk_Bool *enabled);

/*
 ***********************************************************************
 * vmk_UplinkDCBEnablePFC --                                      */ /**
 *
 * \brief  The routine to enable Priority-based Flow Control support
 *         on the device.
 *
 * \note   It should only be called from the DCB daemon that does
 *         DCB negotiation on behalf of this device.
 *
 * \note   PFC configurations must be setup correctly before enabling
 *         PFC support on the device.
 *
 * \note   vmk_UplinkDCBApplySettings() needs to be called after
 *         this call to guaranttee the changes will be flushed
 *         onto the device.
 *
 * \param[in] clientData   Internal module structure for the device
 *                         associated to the uplink.
 *
 * \retval    VMK_OK       If operation succeeds.
 * \retval    VMK_FAILURE  Otherwise.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBEnablePFC) (void *clientData);

/*
 ***********************************************************************
 * vmk_UplinkDCBDisablePFC --                                     */ /**
 *
 * \brief  The routine to disable Priority-based Flow Control
 *         support on the device.
 *
 * \note   It should only be called from the DCB daemon that does
 *         DCB negotiation on behalf of this device.
 *
 * \note   vmk_UplinkDCBApplySettings() needs to be called after
 *         this call to guaranttee the changes will be flushed
 *         onto the device.
 *
 * \param[in] clientData   Internal module structure for the device
 *                         associated to the uplink.
 *
 * \retval    VMK_OK       If operation succeeds.
 * \retval    VMK_FAILURE  Otherwise.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBDisablePFC) (void *clientData);

/*
 ***********************************************************************
 * vmk_UplinkDCBGetApplications --                                */ /**
 *
 * \brief  The routine to retrieve all DCB Application Protocols
 *         settings from the device.
 *
 * \param[in]  clientData  Internal module structure for the device
 *                         associated to the uplink.
 * \param[out] apps        Used to store the DCB Applications
 *                         settings of the device.
 *
 * \retval    VMK_OK       If operation succeeds.
 * \retval    VMK_FAILURE  Otherwise.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBGetApplications) (void *clientData,
                                                vmk_DCBApplications *apps);

/*
 ***********************************************************************
 * vmk_UplinkDCBSetApplication --                                 */ /**
 *
 * \brief  The routine to pushdown DCB Application Protocol
 *         settings to the device.
 *
 * \note   It should only be called from the DCB daemon that does
 *         DCB negotiation on behalf of this device.
 *
 * \note   vmk_UplinkDCBApplySettings() needs to be called after
 *         this call to guaranttee the changes will be flushed
 *         onto the device.
 *
 * \param[in] clientData   Internal module structure for the device
 *                         associated to the uplink.
 * \param[in] app          DCB Application Protocol setting of the
 *                         device.
 *
 * \retval    VMK_OK       If operation succeeds.
 * \retval    VMK_FAILURE  Otherwise.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBSetApplication) (void *clientData,
                                                 vmk_DCBApplication *app);

/*
 ***********************************************************************
 * vmk_UplinkDCBGetCapabilities --                                */ /**
 *
 * \brief  The routine to retrieve DCB capabilities information
 *         from the device.
 *
 * \param[in]  clientData  Internal module structure for the device
 *                         associated to the uplink.
 * \param[out] caps        Used to store the DCB capabilities
 *                         information of the device.
 *
 * \retval    VMK_OK       If operation succeeds.
 * \retval    VMK_FAILURE  Otherwise.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBGetCapabilities) (void *clientData,
                                                vmk_DCBCapabilities *caps);

/*
 ***********************************************************************
 * vmk_UplinkDCBApplySettings --                                  */ /**
 *
 * \brief  The routine to flush out all pending DCB configuration
 *         changes on the device.
 *
 * \note   It should only be called from the DCB daemon that does
 *         DCB negotiation on behalf of this device. DCB daemon
 *         calls this routine after all DCB parameters are negotiated
 *         and pushed down to the driver.
 *
 * \param[in] clientData   Internal module structure for the device
 *                         associated to the uplink.
 *
 * \retval    VMK_OK       If operation succeeds.
 * \retval    VMK_FAILURE  Otherwise.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBApplySettings) (void *clientData);

/*
 ***********************************************************************
 * vmk_UplinkDCBGetSettings --                                    */ /**
 *
 * \brief  The routine to retrieve all DCB settings from the device.
 *
 * \param[in]  clientData  Internal module structure for the device
 *                         associated to the uplink.
 * \param[out] dcb         Used to store the DCB configurations of
 *                         the device.
 *
 * \retval    VMK_OK       If operation succeeds.
 * \retval    VMK_FAILURE  Otherwise.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBGetSettings) (void *clientData,
                                                      vmk_DCBConfig *dcb);

/**
 * \brief Structure used to have access to the properties of the
 *        device associated to an uplink. 
 */

typedef struct {

   /** Handler used to retrieve the state of the device */
   vmk_UplinkGetStates          getStates;

   /** Handler used to retrieve memory resources of the device */
   vmk_UplinkGetMemResources    getMemResources;

   /** Handler used to retrieve device properties of the device */
   vmk_UplinkGetDeviceProperties getDeviceProperties;

   /** Handler used to retrieve panic-time polling properties of the device */
   vmk_UplinkGetPanicInfo       getPanicInfo;

   /** Handler used to retrieve the MAC address of the device */
   vmk_UplinkGetMACAddr         getMACAddr;

   /** Handler used to retrieve the name of the device */
   vmk_UplinkGetName            getName;

   /** Handler used to retrieve the statistics of the device */
   vmk_UplinkGetStats           getStats;

   /** Handler used to retrieve the driver information of the device */
   vmk_UplinkGetDriverInfo      getDriverInfo;

   /** Handler used to retrieve the wake-on-lan state of the device */
   vmk_UplinkGetWolState        getWolState;

   /** Handler used to set the wake-on-lan state of the device */
   vmk_UplinkGetWolState        setWolState;

   /** Handler used to get colesing state of the device */
   vmk_UplinkGetCoalesceParams  getCoalesceParams;

   /** Handler used to set colesing state of the device */
   vmk_UplinkSetCoalesceParams  setCoalesceParams;
} vmk_UplinkPropFunctions;

/** 
 * \brief Structure used to have access to the timeout properties of the
 *        device associated to an uplink.
 *        If the module does not provide a timeout mechanism, this information
 *        can be ignored.
 */

typedef struct {

   /** Handler used to retrieve the number of times the device handles timeout before hitting a panic */
   vmk_UplinkGetWatchdogHitCnt         getHitCnt;

   /** Handler used to set the number of times the device handles timeout before hitting a panic */
   vmk_UplinkSetWatchdogHitCnt         setHitCnt;

   /** Handler used to retrieve the global number of times the device hit a timeout */
   vmk_UplinkGetWatchdogStats          getStats;

   /** Handler used to retrieve the timeout panic mod's status for the device */
   vmk_UplinkGetWatchdogPanicModState  getPanicMod;

   /** Handler used to set the timeout panic mod's status for the device */
   vmk_UplinkSetWatchdogPanicModState  setPanicMod;
} vmk_UplinkWatchdogFunctions;

typedef struct {
   /** Netqueue control operation entry point */
   vmk_UplinkNetqueueOpFunc               netqOpFunc;

   /** Netqueue packet transmit function (obsolete) */
   vmk_UplinkNetqueueXmit                 netqXmit;
} vmk_UplinkNetqueueFunctions;

typedef struct {
   /** dispatch routine for PT management operations */
   vmk_UplinkPTOpFunc                     ptOpFunc;
} vmk_UplinkPTFunctions;

typedef struct {

   /** Handler used to setup vlan hardware context and add vid */
   vmk_UplinkVlanSetupHw            setupVlan;

   /** Handler used to delete vlan id and deactivate hw for the device */
   vmk_UplinkVlanRemoveHw           removeVlan;
} vmk_UplinkVlanFunctions;

typedef struct {

   /** Handler used to retrieve the MTU of the device */
   vmk_UplinkGetMTU                 getMTU;

   /** Handler used to set the MTU of the device */
   vmk_UplinkSetMTU                 setMTU;
} vmk_UplinkMtuFunctions;

typedef struct {

   /** Handler used to Tx a packet immediately through the device */
   vmk_UplinkStartTx                startTxImmediate;

   /** Handler used to set up the resources of the device */
   vmk_UplinkOpenDev                open;

   /** Handler used to release the resources of the device */
   vmk_UplinkCloseDev               close;

   /** Handler used to poll device for a Rx packet */
   vmk_UplinkPanicPoll              panicPoll;

   /** Handler used to flush the Rx/Tx buffers of the device */
   vmk_UplinkFlushBuffers           flushRxBuffers;

   /** Handler used to issue an ioctl command to the device */
   vmk_UplinkIoctlCB                ioctl;

   /** Handler used to set the device as blocked */
   vmk_UplinkBlockDev               block;

   /** Handler used to set the device as unblocked */
   vmk_UplinkUnblockDev             unblock;

   /** Handler used to change link speed and duplex */
   vmk_UplinkSetLinkStatus          setLinkStatus;

   /** Handler used to reset a device */
   vmk_UplinkResetDev               reset;
} vmk_UplinkCoreFunctions;

typedef struct {

   /** Handler used to check whether DCB is enabled on the deivce */
   vmk_UplinkIsDCBEnabled           isDCBEnabled;

   /** Handler used to enable DCB support on the device */
   vmk_UplinkEnableDCB              enableDCB;

   /** Handler used to disable DCB support on the device */
   vmk_UplinkDisableDCB             disableDCB;

   /** Handler used to retrieve Traffic Classes information from the device */
   vmk_UplinkDCBGetNumTCs           getNumTCs;

   /** Handler used to retrieve Priority Group information from the device */
   vmk_UplinkDCBGetPriorityGroup    getPG;

   /** Handler used to push down Priority Group settings to the device */
   vmk_UplinkDCBSetPriorityGroup    setPG;

   /**
    * Handler used to retrieve Priority-based Flow Control configurations
    * from the device
    */
   vmk_UplinkDCBGetPFCCfg           getPFCCfg;

   /**
    * Handler used to pushdown Priority-based Flow Control configurations
    * to the device
    */
   vmk_UplinkDCBSetPFCCfg           setPFCCfg;

   /**
    * Handler used to check whether Priority-based Flow Control support is
    * enabled on the device
    */
   vmk_UplinkDCBIsPFCEnabled        isPFCEnabled;

   /** Handler used to enable Priority-based Flow Control on the device */
   vmk_UplinkDCBEnablePFC           enablePFC;

   /** Handler used to disable Priority-based Flow Control on the device */
   vmk_UplinkDCBDisablePFC          disablePFC;

   /**
    * Handler used to retrieve all DCB Application Protocols settings
    * from the device
    */
   vmk_UplinkDCBGetApplications     getApps;

   /**
    * Handler used to pushdown DCB Application Protocol settings to the device
    */
   vmk_UplinkDCBSetApplication      setApp;

   /** Handler used to retrieve DCB capabilities information from the device */
   vmk_UplinkDCBGetCapabilities     getCaps;

   /**
    * Handler used to flush all pending DCB configuration changes to the device
    */
   vmk_UplinkDCBApplySettings       applySettings;

   /** Handler used to retrieve all DCB settings from the device */
   vmk_UplinkDCBGetSettings         getSettings;
} vmk_UplinkDCBFunctions;

/**
 * \brief Structure passed to vmkernel in order to interact with the device
 *        associated to an uplink.
 */

typedef struct vmk_UplinkFunctions {

   /** Set of functions giving access to the core services of the device */
   vmk_UplinkCoreFunctions          coreFns;

   /** Set of functions giving access to the vlan services of the device */
   vmk_UplinkVlanFunctions          vlanFns;

   /** Set of functions giving access to the MTU services of the device*/
   vmk_UplinkMtuFunctions           mtuFns;

   /** Set of functions giving access to the properties/statistics of the device */
   vmk_UplinkPropFunctions          propFns;

   /** Set of functions giving access to the watchdog management of the device */
   vmk_UplinkWatchdogFunctions      watchdogFns;

   /** Set of functions giving access to the netqueue services of the device */
   vmk_UplinkNetqueueFunctions      netqueueFns;

   /** Set of functions giving access to the PT services of the device */
   vmk_UplinkPTFunctions            ptFns;

   /** Set of functions giving access to the DCB services of the device */
   vmk_UplinkDCBFunctions           dcbFns;

} vmk_UplinkFunctions;

/**
 * \brief Uplink flags for misc. info.
 */
typedef enum {
   /** \brief hidden from management apps */
   VMK_UPLINK_FLAG_HIDDEN         =       0x01,

   /** This will be set if physical device is being registered as pseudo-dev */
   VMK_UPLINK_FLAG_PSEUDO_REG     =       0x02,

} vmk_UplinkFlags;

/**
 * \brief Structure containing all the required information to bind a 
 *        device to an uplink.
 */

typedef struct {

   /** Name of the freshly connected device */
   char                  *devName;

   /** Internal module structure for this network device */
   void                  *clientData;

   /** Module identifier of the caller module */
   vmk_ModuleID          moduleID;

   /** Functions used by vmkernel to interact with module network services */
   vmk_UplinkFunctions   *functions;

   /** Capabilities populated to vmkernel level for this particular uplink */
   vmk_UplinkCapabilities cap;

   /** Data misc. flags for the uplink */
   vmk_UplinkFlags       flags;
} vmk_UplinkConnectInfo;

/*
 ***********************************************************************
 * vmk_UplinkUpdateLinkState --                                   */ /**
 *
 * \brief Update link status information related to a specified uplink
 *        with a bundle containing the information.
 *
 * \param[in] uplink   Uplink aimed
 * \param[in] linkInfo Structure containing link information
 *
 * \retval    VMK_OK   Always
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkUpdateLinkState(vmk_Uplink uplink,
					   vmk_UplinkLinkInfo *linkInfo);

/*
 ***********************************************************************
 * vmk_UplinkWatchdogTimeoutHit --                                */ /**
 *
 * \brief Notify vmkernel that a watchdog timeout has occurred.
 *
 * \note If an uplink driver has a watchdog for the transmit queue of the device,
 *       the driver should notify vmkernel when a timeout occurs. Vmkernel may use this
 *       information to determine the reliability of a particular uplink.
 *
 * \param[in]  uplink    Uplink aimed
 *
 * \retval     VMK_OK    Always
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkWatchdogTimeoutHit(vmk_Uplink uplink);

/*
 ***********************************************************************
 * vmk_UplinkRegister --                                          */ /**
 *
 * \brief Notify vmkernel that an uplink has been connected.
 *
 * \note This function create the bond between vmkernel uplink and 
 *       a module internal structure.
 *       Through this connection vmkernel will be able to manage
 *       Rx/Tx and other operations on module network services.
 *
 * \param[out] uplink         Address of the new uplink
 * \param[in]  connectInfo    Information passed to vmkernel to bind an 
 *                            uplink to a module internal NIC representation
 *
 * \retval     VMK_OK        if succeed
 * \retval     VMK_FAILURE   otherwise
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkRegister(vmk_Uplink *uplink,
                                    vmk_UplinkConnectInfo *connectInfo);

/*
 ***********************************************************************
 * vmk_UplinkUnregister                                           */ /**
 *
 * \brief Destroys the corresponding uplink
 *
 * \note Once called, the uplink handle is no longer valid.
 *
 * \param[in]  uplink        Uplink to unregister
 *
 * \retval     VMK_OK        if succeed
 * \retval     VMK_FAILURE   otherwise
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkUnregister(vmk_Uplink uplink);

/*
 ***********************************************************************
 * vmk_UplinkOpen --                                              */ /**
 *
 * \brief Open the device associated with the uplink
 *
 * \note This function needs to be called if the device associated with
 *       the uplink is not a PCI device. For PCI device, 
 *       vmk_PCIDoPostInsert() should be called instead.
 *
 * \param[in]  uplink        Uplink to be opened
 *
 * \retval     VMK_OK        if succeed
 * \retval     VMK_FAILURE   otherwise
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkOpen(vmk_Uplink uplink);

/*
 ***********************************************************************
 * vmk_UplinkClose --                                             */ /**
 *
 * \brief Close the device associated with the uplink and disconnect the
 *        uplink from the network services
 *
 * \note This function needs to be called if the device associated with
 *       the uplink is not a PCI device. For PCI device,
 *       vmk_PCIDoPreRemove() should be called instead.
 *
 * \param[in]  uplink        Uplink to be closed
 *
 * \retval     VMK_OK        if succeed
 * \retval     VMK_FAILURE   otherwise
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkClose(vmk_Uplink uplink);

/*
 ***********************************************************************
 * vmk_UplinkIsNameAvailable --                                   */ /**
 *
 * \brief Check if a name is already used by an uplink in vmkernel.
 *
 * \param[in]  uplinkName     Name of the uplink
 *
 * \retval     VMK_TRUE       the uplink name is available
 * \retval     VMK_FALSE      otherwise
 *
 ***********************************************************************
 */

vmk_Bool vmk_UplinkIsNameAvailable(char *uplinkName);

/*
 ***********************************************************************
 * vmk_UplinkWorldletSet --                                       */ /**
 *
 * \brief Associate worldlet with an uplink.
 *
 * \param[in]  uplink   Uplink aimed
 * \param[in]  worldlet Worldlet to associate with this uplink
 *
 * \retval     VMK_OK        if succeed
 * \retval     VMK_FAILURE   otherwise
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkWorldletSet(vmk_Uplink uplink,
                                       void *worldlet);

/*
 ***********************************************************************
 * vmk_UplinkCapabilitySet --                                     */ /**
 *
 * \brief Enable/Disable a capability for an uplink.
 *
 * \param[in] uplinkCaps    Capabilities to be modified
 * \param[in] cap           Capability to be enabled/disabled
 * \param[in] enable        true => enable, false => disable
 *
 * \retval    VMK_OK        If capability is valid
 * \retval    VMK_NOT_FOUND Otherwise
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkCapabilitySet(vmk_UplinkCapabilities *uplinkCaps, 
					 vmk_PortClientCaps cap, 
					 vmk_Bool enable);

/*
 ***********************************************************************
 * vmk_UplinkCapabilityGet --                                     */ /**
 *
 * \brief Retrieve status of a capability for an uplink.
 *
 * \param[in]  uplinkCaps    Capabilities to be modified
 * \param[in]  cap           Capability to be checked
 * \param[out] status        true => enabled, false => disabled
 *
 * \retval     VMK_OK        If capability is valid
 * \retval     VMK_NOT_FOUND Otherwise
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkCapabilityGet(vmk_UplinkCapabilities *uplinkCaps, 
					 vmk_PortClientCaps cap, 
					 vmk_Bool *status);

/*
 ***********************************************************************
 * vmk_UplinkQueueStop --                                         */ /**
 *
 * \brief Notify stack of uplink queue stop
 *
 * \param[in]  uplink        Uplink aimed
 * \param[in]  qid           Queue ID
 *
 * \retval     VMK_OK
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkQueueStop(vmk_Uplink uplink,
                                     vmk_NetqueueQueueID qid);

/*
 ***********************************************************************
 * vmk_UplinkQueueStart --                                        */ /**
 *
 * \brief Notify stack of uplink queue (re)start
 *
 * \param[in]  uplink        Uplink aimed
 * \param[in]  qid           Queue ID
 *
 * \retval     VMK_OK
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkQueueStart(vmk_Uplink uplink,
                                      vmk_NetqueueQueueID qid);

/*
 ***********************************************************************
 * vmk_PktQueueForRxProcess --                                    */ /**
 *
 * \brief Queue a specified packet coming from an uplink for Rx process.
 *
 * \param[in] pkt       Target packet
 * \param[in] uplink    Uplink where the packet came from
 *
 ***********************************************************************
 */
extern
void vmk_PktQueueForRxProcess(vmk_PktHandle *pkt,
                              vmk_Uplink uplink);

/*
 ***********************************************************************
 * vmk_PktListRxProcess --                                        */ /**
 *
 * \brief Process a list of packets from an uplink.
 *
 * \param[in] pktList   Set of packets to process.
 * \param[in] uplink    Uplink from where the packets came from.
 *
 ***********************************************************************
 */
void vmk_PktListRxProcess(vmk_PktList pktList,
                          vmk_Uplink uplink);


#endif /* _VMKAPI_NET_UPLINK_INCOMPAT_H_ */
/** @} */
/** @} */
/** @} */
