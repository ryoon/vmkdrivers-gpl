/*
 * Copyright (C) 2003 - 2009 NetXen, Inc.
 * All rights reserved.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 * 
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.
 * 
 * Contact Information:
 * licensing@netxen.com
 * NetXen, Inc.
 * 18922 Forge Drive
 * Cupertino, CA 95014
 */
#if !defined(_NX_LICENSE_H_)
#define _NX_LICENSE_H_

#include <message.h>
#include <nic_cmn.h>

#define LICENSE_H2C_MAJOR        UNM_MSGQ_SQM_TYPE1
#define LICENSE_H2C_MINOR        PEGOS_PEGNET_CORE
 
 /*These defines can be changed later*/
#define NX_MAX_NUM_CAPABILITIES    8
#define	NX_FINGERPRINT_MAXLENGTH  1024
#define	NX_LICENSE_MAXLENGTH  1024 
#define	NX_OPTION_MAXLENGTH  1024 
#define	NX_DOMAIN_MAXLENGTH  32
#define NX_EQUAL_TO "%3D"
#define NX_SEMI_COLON "%3B"

/*Following type of error values can be returned from card to host */
enum {
        NX_FW_LICENSE_VALID = 0,
        NX_BUFFER_OVERFLOW ,
        NX_ENCRYPT_FAILS ,
        NX_DECRYPT_FAILS ,
        NX_ZERO_DMA_LENGTH,
	NX_DOMAIN_NOT_SET,
        NX_FW_LICENSE_INVALID ,
        NX_FW_LICENSE_DISABLED ,
        NX_FW_LICENSE_EXPIRED
};


/*Ioctl Type :- NX_LIC_GET_FINGER_PRINT*/

/*
 * USER --> DRIVER
 *
 * argument for ioctl.
 */

typedef struct {
        char *req_finger_print;
        __uint64_t req_len;
		__uint64_t req_time;
}nx_finger_print_ioctl_t;

/* Data structure to contain HW fingerprint */
typedef struct {
        __uint64_t len;
        char data[NX_FINGERPRINT_MAXLENGTH];
} PREALIGN(512) nx_finger_print_t POSTALIGN(512);

/*
 * HOST --> CARD
 *
 * Request to DMA the hardware fingerprint to a certain location.
 */
typedef struct {
        host_peg_msg_hdr_t      hdr;
        __uint64_t              dma_to_addr; /*Physical address for dma*/
	__uint64_t		nx_time;
        __uint32_t              dma_size;
        __uint16_t              ring_ctx;
        __uint16_t              rsvd;       /* 64 bit alignment */
} nx_get_finger_print_request_t;

/*
 * CARD --> HOST
 *
 * Sent in response to get_finger_print_request.
 */

typedef struct {
        host_peg_msg_hdr_t      hdr;
        __uint32_t              ret_code;
        __uint32_t              rsvd;           /* 64 bit alignment */
} nx_get_finger_print_response_t;

/*Ioctl Type :- NX_LIC_INSTALL_LICENSE*/
/*
 * USER --> HOST DRIVER
 *
 */

typedef struct {
        char *data;
        __uint64_t data_len;
}nx_install_license_ioctl_t;

/* Data structure to contain license recieved from license server */
typedef struct {
        __uint64_t len;
        char data[NX_LICENSE_MAXLENGTH];
} PREALIGN(512) nx_install_license_t POSTALIGN(512);

/*
 * HOST --> CARD
 *
 * Sending Request to install the license.
 */
typedef struct {
        host_peg_msg_hdr_t      hdr;
        __uint64_t              dma_to_addr; /*Physical address for dma*/
        __uint32_t              dma_size; /*Size of netxen_install_license_t*/
        __uint16_t              ring_ctx;
        __uint16_t              rsvd;       /* 64 bit alignment */
} nx_install_license_request_t;

/*
 * CARD --> HOST
 *
 * Sent in response to get_install_license_request.
 */

typedef struct {
        host_peg_msg_hdr_t      hdr;
        __uint32_t              ret_code;
        __uint32_t              rsvd;           /* 64 bit alignment */
} nx_install_license_response_t;


/*Ioctl Type :- NX_LIC_GET_LICENSE_CAPABILITIES*/

/*
 * USER --> DRIVER
 *
 * argument for ioctl.
 */

typedef struct {
        char *req_license_capabilities;
        __uint64_t req_len;
}nx_license_capabilities_ioctl_t;

/* These are some of the predefined capabilites. It will act as index in nx_capabilities_arr */
enum {
        IS_NX_CHIMNEY_ENABLED = 0,
        IS_NX_RDMA_ENABLED
};

enum {
	NX_CHIMNEY_CAPABLE = 0x1 ,
	NX_RDMA_CAPABLE = 0x2 ,
};

/* Data structure to contain license capabilities */
typedef struct {
        __uint64_t len;
        __uint64_t arr[NX_MAX_NUM_CAPABILITIES];
} PREALIGN(512) nx_license_capabilities_t POSTALIGN(512);

/*
 * HOST --> CARD
 *
 * Sending Request to get license capability.
 */

typedef struct {
        host_peg_msg_hdr_t      hdr;
        __uint64_t              dma_to_addr; /*Physical address for dma*/
        __uint32_t              dma_size;
        __uint16_t              ring_ctx;
        __uint16_t              rsvd;       /* 64 bit alignment */
} nx_get_license_capability_request_t;

/*
 * CARD --> HOST
 *
 * Sent in response to get_license_capability_request.
 */
typedef struct {
        host_peg_msg_hdr_t      hdr;
        __uint32_t              ret_code;
        __uint32_t              rsvd;           /* 64 bit alignment */
} nx_get_license_capability_response_t;


/*
 * CARD --> HOST
 *
 * Sent in response to get_license_capability_request.
 */
typedef struct {
        host_peg_msg_hdr_t      hdr;
        __uint32_t              ret_code;
        __uint32_t              rsvd;           /* 64 bit alignment */
} nx_license_status_response_t;
#endif /* NX_LICENSE */
