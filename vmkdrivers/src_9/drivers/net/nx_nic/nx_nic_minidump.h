/*
 * Copyright (C) 2003 - 2009 NetXen, Inc.
 * Copyright (C) 2009 - QLogic Corporation.
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
 * MA  02111-1307, USA.
 *
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.
 *
 */
#ifndef __QL_MINIDUMP_H__
#define __QL_MINIDUMP_H__
int nx_nic_collect_minidump(struct unm_adapter_s *adapter);
u32 nx_nic_get_capture_size(struct unm_adapter_s *adapter);
u32 nx_nic_check_template_checksum(struct unm_adapter_s *adapter);

#define NX_NIC_MD_MIN_CAPTURE_MASK    		0x03
#define NX_NIC_MD_MAX_CAPTURE_MASK    		0xFF

#define NX_NIC_MD_DEFAULT_CAPTURE_MASK    	0x7F
#define NX_NIC_MINIDUMP_IOCTL_COPY_SIZE     4096

/*
 * Entry Type Defines
 */
#define RDNOP                   0
#define RDCRB                   1
#define RDMUX                   2
#define QUEUE                   3
#define BOARD                   4
#define RDSRE                   5
#define RDOCM                   6
#define PREGS                   7
#define L1DTG                   8
#define L1ITG                   9
#define CACHE                  10

#define L1DAT                  11
#define L1INS                  12
#define RDSTK                  13
#define RDCON                  14

#define L2DTG                  21
#define L2ITG                  22
#define L2DAT                  23
#define L2INS                  24
#define RDOC3                  25

#define MEMBK                  32

#define RDROM                  71
#define RDMEM                  72
#define RDMN                   73

#define INFOR                  81
#define CNTRL                  98

#define TLHDR                  99
#define RDEND                  255

#define PRIMQ                  103
#define SQG2Q                  104
#define SQG3Q                  105

/*
 * Opcodes for Control Entries.
 * These Flags are bit fields.
 */
#define QL_DBG_OPCODE_WR        0x01
#define QL_DBG_OPCODE_RW        0x02
#define QL_DBG_OPCODE_AND       0x04
#define QL_DBG_OPCODE_OR       	0x08
#define QL_DBG_OPCODE_POLL      0x10
#define QL_DBG_OPCODE_RDSTATE   0x20
#define QL_DBG_OPCODE_WRSTATE   0x40
#define QL_DBG_OPCODE_MDSTATE   0x80

/*
 * Template Header
 * Parts of the template header can be modified by the driver.
 * These include the saved_state_array, capture_debug_level, driver_timestamp
 * The driver_info_wordX is used to add info about the drivers environment.
 * It is important that drivers add identication and system info in these fields.
 */

#define QL_DBG_STATE_ARRAY_LEN        16
#define QL_DBG_CAP_SIZE_ARRAY_LEN     8
#define QL_DBG_RSVD_ARRAY_LEN         8


typedef struct ql_minidump_template_hdr_s {
	u32 entry_type;
	u32 first_entry_offset;
	u32 size_of_template;
	u32 capture_debug_level;

	u32 num_of_entries;
	u32 version;
	u32 driver_timestamp;
	u32 checksum;

	u32 driver_capture_mask;
	u32 driver_info_word2;
	u32 driver_info_word3;
	u32 driver_info_word4;

	u32 saved_state_array[QL_DBG_STATE_ARRAY_LEN];
	u32 capture_size_array[QL_DBG_CAP_SIZE_ARRAY_LEN];

	/*  markers_array used to capture some special locations on board */
	u32 markers_array[QL_DBG_RSVD_ARRAY_LEN];
	u32 num_of_free_entries;	/* For internal use */
	u32 free_entry_offset;	/* For internal use */
	u32 total_table_size;	/*  For internal use */
	u32 bkup_table_offset;	/*  For internal use */
} ql_minidump_template_hdr_t;

/*
 * Common Entry Header:  Common to All Entry Types
 */
/*
 * Driver Flags
 */
#define QL_DBG_SKIPPED_FLAG     0x80	/*  driver skipped this entry  */
#define QL_DBG_SIZE_ERR_FLAG    0x40	/*  entry size vs capture size mismatch */

/*
 * Driver Code is for driver to write some info about the entry.
 * Currently not used.
 */

typedef struct ql_minidump_entry_hdr_s {
	u32 entry_type;
	u32 entry_size;
	u32 entry_capture_size;
	union {
		struct {
			u8 entry_capture_mask;
			u8 entry_code;
			u8 driver_code;
			u8 driver_flags;
		};
		u32 entry_ctrl_word;
	};
} ql_minidump_entry_hdr_t;

/*
 * Generic Entry Including Header
 */
typedef struct ql_minidump_entry_s {
	ql_minidump_entry_hdr_t hdr;

	u32 entry_data00;
	u32 entry_data01;
	u32 entry_data02;
	u32 entry_data03;

	u32 entry_data04;
	u32 entry_data05;
	u32 entry_data06;
	u32 entry_data07;
} ql_minidump_entry_t;

/*
 *  Read CRB entry header
 */
typedef struct ql_minidump_entry_crb_s {
	ql_minidump_entry_hdr_t h;

	u32 addr;
	union {
		struct {
			u8 addr_stride;
			u8 state_index_a;
			u16 poll_timeout;
		};
		u32 addr_cntrl;
	};

	u32 data_size;
	u32 op_count;

	union {
		struct {
			u8 opcode;
			u8 state_index_v;
			u8 shl;
			u8 shr;
		};
		u32 control_value;
	};

	u32 value_1;
	u32 value_2;
	u32 value_3;
} ql_minidump_entry_crb_t;

/*
 *
 */
typedef struct ql_minidump_entry_cache_s {
	ql_minidump_entry_hdr_t h;

	u32 tag_reg_addr;
	union {
		struct {
			u16 tag_value_stride;
			u16 init_tag_value;
		};
		u32 select_addr_cntrl;
	};

	u32 data_size;
	u32 op_count;

	u32 control_addr;
	union {
		struct {
			u16 write_value;
			u8 poll_mask;
			u8 poll_wait;
		};
		u32 control_value;
	};

	u32 read_addr;
	union {
		struct {
			u8 read_addr_stride;
			u8 read_addr_cnt;
			u16 rsvd_1;
		};
		u32 read_addr_cntrl;
	};
} ql_minidump_entry_cache_t;

typedef struct ql_minidump_entry_rdocm_s {
	ql_minidump_entry_hdr_t h;

	u32 rsvd_0;
	union {
		struct {
			u32 rsvd_1;
		};
		u32 select_addr_cntrl;
	};

	u32 data_size;
	u32 op_count;

	u32 rsvd_2;
	u32 rsvd_3;

	u32 read_addr;
	union {
		struct {
			u32 read_addr_stride;
		};
		u32 read_addr_cntrl;
	};
} ql_minidump_entry_rdocm_t;

typedef struct ql_minidump_entry_rdmem_s {
	ql_minidump_entry_hdr_t h;

	union {
		struct {
			u32 select_addr_reg;
		};
		u32 rsvd_0;
	};

	union {
		struct {
			u8 addr_stride;
			u8 addr_cnt;
			u16 data_size;
		};
		u32 rsvd_1;
	};

	union {
		struct {
			u32 op_count;
		};
		u32 rsvd_2;
	};

	union {
		struct {
			u32 read_addr_reg;
		};
		u32 rsvd_3;
	};

	union {
		struct {
			u32 cntrl_addr_reg;
		};
		u32 rsvd_4;
	};

	union {
		struct {
			u8 wr_byte0;
			u8 wr_byte1;
			u8 poll_mask;
			u8 poll_cnt;
		};
		u32 rsvd_5;
	};

	u32 read_addr;
	u32 read_data_size;
} ql_minidump_entry_rdmem_t;

typedef struct ql_minidump_entry_rdrom_s {
	ql_minidump_entry_hdr_t h;

	union {
		struct {
			u32 select_addr_reg;
		};
		u32 rsvd_0;
	};

	union {
		struct {
			u8 addr_stride;
			u8 addr_cnt;
			u16 data_size;
		};
		u32 rsvd_1;
	};

	union {
		struct {
			u32 op_count;
		};
		u32 rsvd_2;
	};

	union {
		struct {
			u32 read_addr_reg;
		};
		u32 rsvd_3;
	};

	union {
		struct {
			u32 write_mask;
		};
		u32 rsvd_4;
	};

	union {
		struct {
			u32 read_mask;
		};
		u32 rsvd_5;
	};

	u32 read_addr;
	u32 read_data_size;
} ql_minidump_entry_rdrom_t;

typedef struct ql_minidump_entry_mux_s {
	ql_minidump_entry_hdr_t h;

	u32 select_addr;
	union {
		struct {
			u32 rsvd_0;
		};
		u32 select_addr_cntrl;
	};

	u32 data_size;
	u32 op_count;

	u32 select_value;
	u32 select_value_stride;

	u32 read_addr;
	u32 rsvd_1;

} ql_minidump_entry_mux_t;

/*
 *
 */
typedef struct ql_minidump_entry_queue_s {
	ql_minidump_entry_hdr_t h;

	u32 select_addr;
	union {
		struct {
			u16 queue_id_stride;
			u16 rsvd_0;
		};
		u32 select_addr_cntrl;
	};

	u32 data_size;
	u32 op_count;

	u32 rsvd_1;
	u32 rsvd_2;

	u32 read_addr;
	union {
		struct {
			u8 read_addr_stride;
			u8 read_addr_cnt;
			u16 rsvd_3;
		};
		u32 read_addr_cntrl;
	};

} ql_minidump_entry_queue_t;

#endif
