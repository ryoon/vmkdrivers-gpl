/* ****************************************************************
 * Copyright 2011-2012 VMware, Inc.
 * ****************************************************************/
#ifndef _VMKLNX_NET_H
#define _VMKLNX_NET_H

/*
 * Swith Port Event Types
 */

enum swith_port_event_type {
    VMKLNX_EVENT_PORT_ENABLE  =  0x11,
    VMKLNX_EVENT_PORT_DISABLE =  0x22,
};

/* Functions used for VXLAN port update notification */

/**
 * vmklnx_netdev_vxlan_port_update_callback - handler to receive VXLAN port
 * updated event
 * @dev: pointer to net_device to receive this event
 * @port: the new VXLAN port in network byte order
 *
 * The handler used by vmklinux to notify driver when VXLAN port is updated in
 * vmkernel.
 *
 * SYNOPSIS:
 * vmklnx_netdev_vxlan_port_update_callback(dev, port)
 *
 * RETURN VALUE:
 * none
 */
typedef void (*vmklnx_netdev_vxlan_port_update_callback)(struct net_device *dev,
                                                         unsigned short port);
extern void vmklnx_netdev_set_vxlan_port_update_callback(struct net_device *dev,
                                                         vmklnx_netdev_vxlan_port_update_callback callback);
extern unsigned short vmklnx_netdev_get_vxlan_port(void);

/**
 * vmklnx_netdev_geneve_port_update_callback - handler to receive Geneve port
 * updated event
 * @dev: pointer to net_device to receive this event
 * @port: the new Geneve port in network byte order
 *
 * The handler used by vmklinux to notify driver when Geneve port is updated in
 * vmkernel.
 *
 * SYNOPSIS:
 * vmklnx_netdev_geneve_port_update_callback(dev, port)
 *
 * RETURN VALUE:
 * none
 */
typedef void (*vmklnx_netdev_geneve_port_update_callback)(struct net_device *dev,
                                                          unsigned short port);
extern void vmklnx_netdev_set_geneve_offload_params(struct net_device *dev,
                                                    vmklnx_netdev_geneve_port_update_callback callback,
                                                    unsigned int innerL7OffsetLimit,
                                                    unsigned int flags);
extern unsigned short vmklnx_netdev_get_geneve_port(void);

#endif /* _VMKLNX_NET_H */
