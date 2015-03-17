/* ****************************************************************
 * Copyright 2008-2013 VMware, Inc.
 * ****************************************************************/

/*
 * XXX - This header must be kept in sync with the file
 *       bora/lib/vmkctl/include/storage/FcoeIoctl.h.
 */

#ifndef _CNA_IOCTL_H_
#define _CNA_IOCTL_H_

#include "vmkapi.h"

#define CNA_NAME                               "cna"

/*
 * Each ioctl has a code, result field, and enough space to carry result.
 * Keep this the size of the largest payload structure (fcoe_cna_list).
 */
#define MAX_FCOE_IOCTL_CMD_SIZE                 1028

/*
 * Usage of Ioctl commands
 * 7            6-4             3-0 
 * X            XXX             XXXX
 * DCB/FCOE     Request/Resp    Command
 */
#define CNA_IOCTL_DCB_MASK                      0x10000000

/*
 * FCOE Command codes - Sent down by the application
 */
#define FCOE_IOCTL_CREATE_FC_CONTROLLER         0x0001
#define FCOE_IOCTL_DISCOVER_FCF                 0x0002
#define FCOE_IOCTL_FCF_LOGOUT                   0x0004
#define FCOE_IOCTL_DESTROY_FC_CONTROLLER        0x0008
#define FCOE_IOCTL_GET_CNA_VMNICS               0x0010
#define FCOE_IOCTL_GET_CNA_INFO                 0x0020
#define FCOE_IOCTL_SET_CNA_VN2VN_MODE           0x0040
#define FCOE_IOCTL_UNSET_CNA_VN2VN_MODE         0x0080
#define FCOE_IOCTL_GET_FC_CONTROLLER            0x0200
#define FCOE_IOCTL_DCB_WAKEUP                   0x0400
#define FCOE_IOCTL_DCB_HWASSIST                 0x0800

/*
 * FCOE response codes sent up by the driver
 */
#define FCOE_IOCTL_RESPONSE_CODE                0x00010000
#define FCOE_IOCTL_NO_CONTROLLER                0x00020000

/*
 * FCOE Ioctl error codes
 */
#define FCOE_IOCTL_SUCCESS                      0x0
#define FCOE_IOCTL_GENERIC_FAILURE              0x1
#define FCOE_IOCTL_ERROR_DEVICE_EXIST           0x2
#define FCOE_IOCTL_WAKEUP_LINK_DOWN             0x3

/*
 * DCB Command codes - Sent down by DCB daemon
 * Not in use as of today (replaced by VSI)
 */

/*
 * DCB response codes sent up by the driver
 */
#define DCB_IOCTL_RESPONSE_CODE                0x10010000

/*
 * DCB Ioctl error codes
 */
#define DCB_IOCTL_SUCCESS                      0x0
#define DCB_IOCTL_GENERIC_FAILURE              0x1
#define DCB_IOCTL_NO_CONTROLLER_FAILURE        0x2

/*
 * Total number of CNA adapters and the driver name string
 * should fit into ioctl data.
 */
#define CNA_DRIVER_NAME                         32
#define VMKLNX_MAX_CNA                          32
#define MAX_PRIORITY_COUNT                      8
#define MAX_BWGROUP_COUNT                       8
#define MAX_TC_COUNT                            8

#define CNA_NAME                               "cna"
#define FCOE_DEV_NAME                 "/vmfs/devices/char/vmkdriver/" CNA_NAME

/*
 * FCOE Ioctl delivery mechanism
 */
struct fcoe_ioctl_pkt {
   vmk_uint32 cmd;
   vmk_uint32 status;
   vmk_uint32 cmd_len;
   char       data[MAX_FCOE_IOCTL_CMD_SIZE];
};

/*
 * FCOE Controller
 */
struct fcoe_cntlr {
   vmk_uint32 priority;
   vmk_uint32 vlan[128]; // Vlan mapping
   char vmnic_name[CNA_DRIVER_NAME];
   vmk_uint8 mac_address[6];
};

/*
 * CNA info structure used to communicate default values
 * and settability to upper layers.
 */
struct fcoe_cna_info {
   // Input argument
   char vmnic_name[CNA_DRIVER_NAME];

   // Default parameter values (output)
   vmk_uint32 default_priority;
   vmk_uint32 default_vlan[128]; // Vlan mapping
   vmk_uint8  default_mac_address[6];

   // Flags which indicate whether a parameter is settable (output)
   vmk_Bool priority_settable;
   vmk_Bool vlan_settable;
   vmk_Bool mac_address_settable;
};
 
/*
 * List of valid vmnics
 */
struct fcoe_cna_list {
   int cna_count;
   char vmnic_name[VMKLNX_MAX_CNA][CNA_DRIVER_NAME];
};

/*
 * Check if DCB negotiation is done in the adapter firmware
 */
struct fcoe_dcb_mode {
   char vmnic_name[CNA_DRIVER_NAME];

   vmk_Bool dcb_hw_assist;
};

/************************************
 * DCB related messages
 ***********************************/
/*
 * State of DCB hardware
 */
struct dcb_state {
   char vmnic_name[CNA_DRIVER_NAME];
   vmk_uint32 state;
};

/*
 * PFC configuration.
 */
struct dcb_pfc_info {
   char vmnic_name[CNA_DRIVER_NAME];
   vmk_Bool pfc_enable[MAX_PRIORITY_COUNT];
};

/*
 * PFC set state
 */
struct dcb_pfc_state {
   char vmnic_name[CNA_DRIVER_NAME];
   vmk_Bool set_state;
};

typedef enum {
        dcb_priority_none       = 0x0000,
        dcb_priority_group,
        dcb_priority_link,
} dcb_priority_type;

/*
 * Priority Group attributes.
 */
struct dcb_pg_attrib {
   /*
    * Maps PGID : Link Bandwidth %.
    */
   vmk_uint8 pg_bw_percent[MAX_BWGROUP_COUNT];

   /*
    * Maps TC : UP bitmap.
    *
    * A set bit X in tc_to_up_map[Y] indicates
    * that UP X belongs to TC Y.
    */
   vmk_uint8 tc_to_up_map[MAX_TC_COUNT];

   /*
    * Maps TC : PGID.
    */
   vmk_uint8 tc_to_pgid_map[MAX_TC_COUNT];

   /*
    * Maps TC : Sub-PG Bandwidth %.
    */
   vmk_uint8 tc_sub_pg_bw_percent[MAX_TC_COUNT];

   /*
    * Maps TC : Link priority type.
    */
   vmk_uint8 tc_to_link_prio_type[MAX_TC_COUNT]; 
};

/*
 * Priority Group settings.
 */
struct dcb_pg_info {
   char vmnic_name[CNA_DRIVER_NAME];
   vmk_uint8 num_tcs;
   struct dcb_pg_attrib tx;
   struct dcb_pg_attrib rx;
};

/*
 * Set HW information
 */
struct dcb_hw_info {
   char vmnic_name[CNA_DRIVER_NAME];
   vmk_int32 hwState;
};

/*
 * Get DCB capabilities
 */
struct dcb_cap_info {
   char vmnic_name[CNA_DRIVER_NAME];
   vmk_uint8 pg;
   vmk_uint8 pfc;
   vmk_uint8 bcn;
   vmk_uint8 up2tc_map;
   vmk_uint8 gsp;
   vmk_uint8 total_traffic_classes;
   vmk_uint8 pfc_traffic_classes;
};

/*
 * Get DCB TCS info
 */
struct dcb_num_tcs {
   char vmnic_name[CNA_DRIVER_NAME];
   vmk_uint8 pg_tcs;
   vmk_uint8 pfc_tcs;
};

#endif /*_CNA_IOCTL_H_ */
