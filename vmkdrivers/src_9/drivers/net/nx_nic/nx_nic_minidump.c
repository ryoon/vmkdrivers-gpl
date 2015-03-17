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

#include "unm_version.h"
#include    "unm_nic.h"

/*
 * Prototype Functions.
 */
static int nx_nic_md_cntrl(struct unm_adapter_s *adapter, ql_minidump_template_hdr_t 
				*template_hdr, ql_minidump_entry_crb_t *crbEntry);

static u32 nx_nic_md_rd_crb(struct unm_adapter_s *adapter, ql_minidump_entry_crb_t 
				*crbEntry, u32 *data_buff);

static int nx_nic_md_L1Cache(struct unm_adapter_s *adapter, ql_minidump_entry_cache_t 
				*cacheEntry, u32 *data_buff);

static int nx_nic_md_L2Cache(struct unm_adapter_s *adapter, ql_minidump_entry_cache_t 
				*cacheEntry, u32 *data_buff);

static int nx_nic_md_rdocm(struct unm_adapter_s *adapter, ql_minidump_entry_rdocm_t 
				*ocmEntry, u32 *data_buff);

static int nx_nic_md_rdmem(struct unm_adapter_s *adapter, ql_minidump_entry_rdmem_t 
				*memEntry, u64 *data_buff);

static int nx_nic_md_rdmn(struct unm_adapter_s *adapter, ql_minidump_entry_rdmem_t 
				*memEntry, u64 *data_buff);

static int nx_nic_md_rdrom(struct unm_adapter_s *adapter, ql_minidump_entry_rdrom_t 
				*romEntry, u64 *data_buff);

static u32 nx_nic_md_rdmux(struct unm_adapter_s *adapter, ql_minidump_entry_mux_t 
				*crbEntry, u32 *data_buff);

static u32 nx_nic_md_rdqueue(struct unm_adapter_s *adapter, ql_minidump_entry_queue_t 
				*queueEntry, u32 *data_buff);

static int nx_nic_md_entry_err_chk(ql_minidump_entry_t *entry, u32 esize);

/*
 * Processing the acquired minidump template.
 * template_buff ==> holds the template received from firmware
 * dump_buff ==> buffer allocated for data capture
 * buff_size ==> size of buffer allocated for data capture
 * capture_mask ==> mask selected by driver to filter entries
 */
static int
nx_nic_parse_md_template(struct unm_adapter_s *adapter) 
{
	int num_of_entries, buff_level, e_cnt;
	int end_cnt = 0;
	int rv = 0, esize;
	char *dbuff;
	int sane_start = 0, sane_end = 0;
	uint64_t cur_fn = (uint64_t)nx_nic_parse_md_template;

	void *template_buff = adapter->mdump.md_capture_buff;
	char *dump_buff = adapter->mdump.md_capture_buff + adapter->mdump.md_template_size;
	int capture_mask = adapter->mdump.md_capture_mask;

	ql_minidump_template_hdr_t *template_hdr;
	ql_minidump_entry_t *entry;
	
	NX_NIC_TRC_FN(adapter, cur_fn, 0);

	if ((capture_mask & 0x3) != 0x3) {
		dev_err(&adapter->pdev->dev, "Capture mask %02x below minimum needed "
				"for valid firmware dump\n", capture_mask);
		NX_NIC_TRC_FN(adapter, cur_fn, -EINVAL);
		return -EINVAL;
	}

	template_hdr = (ql_minidump_template_hdr_t *) template_buff;
	if (template_hdr->entry_type == TLHDR) {
		sane_start = 1;
	}

	num_of_entries = template_hdr->num_of_entries;
	entry = (ql_minidump_entry_t *) ((char *) template_buff +
				     template_hdr->first_entry_offset);

	for (e_cnt = 0, buff_level = 0; e_cnt < num_of_entries; e_cnt++) {
		/*
		 * If the capture_mask of the entry does not match capture mask,
		 * skip the entry after marking the driver_flags indicator.
		 */
		if (!(entry->hdr.entry_capture_mask & capture_mask)) {
			entry->hdr.driver_flags |= QL_DBG_SKIPPED_FLAG;
			entry = (ql_minidump_entry_t *) ((char *) entry + entry->hdr.entry_size);
			continue;
		}
		/*
		 * Decode the entry type and process it accordingly.
		 */
		switch (entry->hdr.entry_type) {
			case RDNOP:
				entry->hdr.driver_flags |= QL_DBG_SKIPPED_FLAG;
				break;
			case RDEND:
				entry->hdr.driver_flags |= QL_DBG_SKIPPED_FLAG;
				if (!sane_end) {
					end_cnt = e_cnt;
				}
				sane_end += 1;
				break;
			case CNTRL:
				if ((rv = nx_nic_md_cntrl(adapter, template_hdr, (void *) entry))) {
					entry->hdr.driver_flags |= QL_DBG_SKIPPED_FLAG;
				}
				break;
			case RDCRB:
				dbuff = dump_buff + buff_level;
				esize = nx_nic_md_rd_crb(adapter, (void *) entry, (void *) dbuff);
				if ((rv = nx_nic_md_entry_err_chk(entry, esize)) < 0) {
                    break;
                }
				buff_level += esize;
				break;
			case RDMEM:
				dbuff = dump_buff + buff_level;
				esize = nx_nic_md_rdmem(adapter, (void *) entry, (void *) dbuff);
				if ((rv = nx_nic_md_entry_err_chk(entry, esize)) < 0) {
                    break;
                }
				buff_level += esize;
				break;
			case RDMN:
				dbuff = dump_buff + buff_level;
				esize = nx_nic_md_rdmn(adapter, (void *) entry, (void *) dbuff);
				if ((rv = nx_nic_md_entry_err_chk(entry, esize)) < 0) {
                    break;
                }
				buff_level += esize;
				break;
			case BOARD:
			case RDROM:
				dbuff = dump_buff + buff_level;
				esize = nx_nic_md_rdrom(adapter, (void *) entry, (void *) dbuff);
				if ((rv = nx_nic_md_entry_err_chk(entry, esize)) < 0) {
					break;
				}
				buff_level += esize;
				break;
			case L2ITG:
			case L2DTG:
			case L2DAT:
			case L2INS:
				dbuff = dump_buff + buff_level;
				esize = nx_nic_md_L2Cache(adapter, (void *) entry, (void *) dbuff);
				if ((rv = nx_nic_md_entry_err_chk(entry, esize)) < 0) {
					break;
				}
				buff_level += esize;
				break;
			case L1DAT:
			case L1INS:
				dbuff = dump_buff + buff_level;
				esize = nx_nic_md_L1Cache(adapter, (void *) entry, (void *) dbuff);
				if ((rv = nx_nic_md_entry_err_chk(entry, esize)) < 0) {
                    break;
                }
				buff_level += esize;
				break;
			case RDOCM:
				dbuff = dump_buff + buff_level;
				esize = nx_nic_md_rdocm(adapter, (void *) entry, (void *) dbuff);
				if ((rv = nx_nic_md_entry_err_chk(entry, esize)) < 0) {
                    break;
                }
				buff_level += esize;
				break;
			case RDMUX:
				dbuff = dump_buff + buff_level;
				esize = nx_nic_md_rdmux(adapter, (void *) entry, (void *) dbuff);
				if ((rv = nx_nic_md_entry_err_chk(entry, esize)) < 0) {
                    break;
                }
				buff_level += esize;
				break;
			case QUEUE:
				dbuff = dump_buff + buff_level;
				esize = nx_nic_md_rdqueue(adapter, (void *) entry, (void *) dbuff);
				if ((rv = nx_nic_md_entry_err_chk(entry, esize)) < 0) {
                    break;
                }
				buff_level += esize;
				break;


			default:
				entry->hdr.driver_flags |= QL_DBG_SKIPPED_FLAG;
				break;
		}

		/* Next entry in the template */
		entry = (ql_minidump_entry_t *) ((char *) entry + entry->hdr.entry_size);
	}

	if (!sane_start || sane_end > 1) {
		dev_err(&adapter->pdev->dev, "Firmware minidump template configuration error.\n");
	}

	NX_NIC_TRC_FN(adapter, cur_fn, 0);
	return 0;
}

/*
 * Read CRB operation.
 */
static u32
nx_nic_md_rd_crb(struct unm_adapter_s *adapter, ql_minidump_entry_crb_t 
							*crbEntry, u32 *data_buff)
{
	int loop_cnt;
	u32 op_count, addr, stride, value;

	addr = crbEntry->addr;
	op_count = crbEntry->op_count;
	stride = crbEntry->addr_stride;

	for (loop_cnt = 0; loop_cnt < op_count; loop_cnt++) {
		value = unm_nic_hw_read_bar_reg_2M(adapter, addr);
		*data_buff++ = addr;
		*data_buff++ = value;
		addr = addr + stride;
	}

	return (loop_cnt * (2 * sizeof(u32)));
}

/*
 * Handling crb control entries.
 */
static int
nx_nic_md_cntrl(struct unm_adapter_s *adapter, ql_minidump_template_hdr_t 
			*template_hdr, ql_minidump_entry_crb_t *crbEntry)
{
	int loop_cnt, i;
    u32 op_count, stride;
	volatile u32 opcode, read_value, addr;
	int rv = 0, timeout_flag;
	unsigned long timeout, timeout_jiffies;

	addr = crbEntry->addr;
    op_count = crbEntry->op_count;
    stride = crbEntry->addr_stride;

	for (loop_cnt = 0; loop_cnt < op_count; loop_cnt++) {
		for (i = 0; i < sizeof(crbEntry->opcode) * 8; i++) {
			opcode = (crbEntry->opcode & (0x1 << i));

			if (opcode) {
				switch (opcode) {
					case QL_DBG_OPCODE_WR:
						unm_nic_hw_write_bar_reg_2M(adapter, addr, crbEntry->value_1);
						break;
					case QL_DBG_OPCODE_RW:
						read_value = unm_nic_hw_read_bar_reg_2M(adapter, addr);
						unm_nic_hw_write_bar_reg_2M(adapter, addr, read_value);
						break;
					case QL_DBG_OPCODE_AND:
						read_value = unm_nic_hw_read_bar_reg_2M(adapter, addr);
						read_value &= crbEntry->value_2;
						unm_nic_hw_write_bar_reg_2M(adapter, addr, read_value);
						break;
					case QL_DBG_OPCODE_OR:
						read_value = unm_nic_hw_read_bar_reg_2M(adapter, addr);
						read_value |= crbEntry->value_3;
						unm_nic_hw_write_bar_reg_2M(adapter, addr, read_value);
						break;
					case QL_DBG_OPCODE_POLL:
						timeout = crbEntry->poll_timeout;
						read_value = unm_nic_hw_read_bar_reg_2M(adapter, addr);

						timeout_jiffies = msecs_to_jiffies(timeout) + jiffies;

						for (timeout_flag = 0; !timeout_flag
								&& ((read_value & crbEntry->value_2) !=
									crbEntry->value_1);) {

							if (time_after(jiffies, timeout_jiffies)){
								timeout_flag = 1;
							}
							read_value = unm_nic_hw_read_bar_reg_2M(adapter, addr);
						}

						if (timeout_flag) {
							dev_err(&adapter->pdev->dev, "%s : Timeout in poll_crb control "
										"operation.\n",	__FUNCTION__);
							return -1;
						}
						break;
					case QL_DBG_OPCODE_RDSTATE:
						/*
						 * Decide which address to use.
						 */
						if (crbEntry->state_index_a) {
							addr = template_hdr->saved_state_array[crbEntry->state_index_a];
						}

						read_value = unm_nic_hw_read_bar_reg_2M(adapter, addr);
						template_hdr->saved_state_array[crbEntry->state_index_v] = read_value;
						break;
					case QL_DBG_OPCODE_WRSTATE:
						/*
						 * Decide which value to use.
						 */
						if (crbEntry->state_index_v) {
							read_value = template_hdr->saved_state_array[crbEntry->state_index_v];
						} else {
							read_value =  crbEntry->value_1;
						}

						/*
						 * Decide which address to use.
						 */
						if (crbEntry->state_index_a) {
							addr = template_hdr->saved_state_array[crbEntry->state_index_a];
						}

						unm_nic_hw_write_bar_reg_2M(adapter, addr, read_value);
						break;
					case QL_DBG_OPCODE_MDSTATE:
						/*  Read value from saved state using index */
						read_value = template_hdr->saved_state_array[crbEntry->state_index_v];

						read_value <<= crbEntry->shl;	/*  Shift left operation */
						read_value >>= crbEntry->shr;	/*  Shift right operation */

						if (crbEntry->value_2) {		/*  check if AND mask is provided */
							read_value &= crbEntry->value_2;	/*  */
						}

						read_value |= crbEntry->value_3;	/*  OR operation */
						read_value += crbEntry->value_1;	/*  increment operation */

						/* Write value back to state area. */
						template_hdr->saved_state_array[crbEntry->state_index_v] = read_value;

						break;
					default:
						rv = 1;
						break;
				} //end switch-case(opcode)
			}
		} //end of opcode bit mask loop.
		addr = addr + stride;
	} //end of op_count loop.

	return (rv);
}

/*
 * Handle L2 Cache.
 */
static int
nx_nic_md_L2Cache(struct unm_adapter_s *adapter, ql_minidump_entry_cache_t 
						*cacheEntry, u32 *data_buff)
{
	int i, k, timeout_flag = 0;
	int loop_cnt;

	u32 read_value;
	u32 addr, read_addr, cntrl_addr, tag_reg_addr;
	u32 tag_value, read_cnt;
	volatile u8 cntl_value_w, cntl_value_r;
	unsigned long timeout, timeout_jiffies;

	loop_cnt = cacheEntry->op_count;
	read_addr = cacheEntry->read_addr;
	cntrl_addr = cacheEntry->control_addr;
	cntl_value_w = (u32) cacheEntry->write_value;

	tag_reg_addr = cacheEntry->tag_reg_addr;

	tag_value = cacheEntry->init_tag_value;
	read_cnt = cacheEntry->read_addr_cnt;

	for (i = 0; i < loop_cnt; i++) {
		unm_nic_hw_write_bar_reg_2M(adapter, tag_reg_addr, tag_value);
		if (cntl_value_w) {
			unm_nic_hw_write_bar_reg_2M(adapter, cntrl_addr, (u32)cntl_value_w);
		}

		if (cacheEntry->poll_mask) {
			timeout = cacheEntry->poll_wait;

			cntl_value_r = (u8) unm_nic_hw_read_bar_reg_2M(adapter, cntrl_addr);
			timeout_jiffies = msecs_to_jiffies(timeout) + jiffies;

			for (timeout_flag = 0; !timeout_flag && 
					((cntl_value_r & cacheEntry->poll_mask) != 0);) {

				if (time_after(jiffies, timeout_jiffies)) {
					timeout_flag = 1;
				}
				cntl_value_r = (u8) unm_nic_hw_read_bar_reg_2M(adapter, cntrl_addr);
			}

			if (timeout_flag) {
				dev_err(&adapter->pdev->dev, "Timeout in processing L2 Tag poll.\n");
				return (-1);
			}
		}

		addr = read_addr;
		for (k = 0; k < read_cnt; k++) {
			read_value = unm_nic_hw_read_bar_reg_2M(adapter, addr);
			*data_buff++ = read_value;
			addr += cacheEntry->read_addr_stride;
		}

		tag_value += cacheEntry->tag_value_stride;
	}

	return (read_cnt * loop_cnt * sizeof(read_value));
}

/*
 * Handle L1 Cache.
 */
static int
nx_nic_md_L1Cache(struct unm_adapter_s *adapter, ql_minidump_entry_cache_t 
						*cacheEntry, u32 *data_buff)
{
	int i, k, loop_cnt;

	u32 read_value;
	u32 addr, read_addr, cntrl_addr, tag_reg_addr;
	u32 tag_value, read_cnt;
	volatile u8 cntl_value_w;

	loop_cnt = cacheEntry->op_count;
	read_addr = cacheEntry->read_addr;
	cntrl_addr = cacheEntry->control_addr;
	cntl_value_w = (u32) cacheEntry->write_value;

	tag_reg_addr = cacheEntry->tag_reg_addr;

	tag_value = cacheEntry->init_tag_value;
	read_cnt = cacheEntry->read_addr_cnt;

	for (i = 0; i < loop_cnt; i++) {
		unm_nic_hw_write_bar_reg_2M(adapter, tag_reg_addr, tag_value);
		unm_nic_hw_write_bar_reg_2M(adapter, cntrl_addr, (u32) cntl_value_w);

		addr = read_addr;
		for (k = 0; k < read_cnt; k++) {
			read_value = unm_nic_hw_read_bar_reg_2M(adapter, addr);
			*data_buff++ = read_value;
			addr += cacheEntry->read_addr_stride;
		}

		tag_value += cacheEntry->tag_value_stride;
	}

	return (read_cnt * loop_cnt * sizeof(read_value));
}

/*
 * Reading OCM memory
 */
static int
nx_nic_md_rdocm(struct unm_adapter_s *adapter, ql_minidump_entry_rdocm_t 
					*ocmEntry, u32 *data_buff)
{
	int i, loop_cnt;
	u32 addr, value;

	addr = ocmEntry->read_addr;
	loop_cnt = ocmEntry->op_count; 

	for (i = 0; i < loop_cnt; i++) {
		value = unm_nic_hw_read_bar_reg_2M(adapter, addr);  
		*data_buff++ = value;

		addr += ocmEntry->read_addr_stride;
	}

	return (i * sizeof(u32));
}

/*
 * Read memory
 */
static int
nx_nic_md_rdmem(struct unm_adapter_s *adapter, ql_minidump_entry_rdmem_t 
						*memEntry, u64 *data_buff)
{
	u64 addr, value = 0;
	int i = 0, loop_cnt;

	addr = (u64) memEntry->read_addr;
	loop_cnt = memEntry->read_data_size;	/* This is size in bytes */
	loop_cnt /= sizeof(value);

	for (i = 0; i < loop_cnt; i++) {
		if (unm_nic_pci_mem_read_md(adapter, addr, &value, sizeof(value), RDMEM)) {
			goto out;
		}

		*data_buff++ = value;
		addr += sizeof(value);
	}

out:
	return (i * sizeof(value));
}

/*
 * Read MN
 */
static int
nx_nic_md_rdmn(struct unm_adapter_s *adapter, ql_minidump_entry_rdmem_t 
						*memEntry, u64 *data_buff)
{
	u64 addr, value = 0;
	int i = 0, loop_cnt;

	addr = (u64) memEntry->read_addr;
	loop_cnt = memEntry->read_data_size;	/* This is size in bytes */
	loop_cnt /= sizeof(value);

	for (i = 0; i < loop_cnt; i++) {
		if (unm_nic_pci_mem_read_md(adapter, addr, &value, sizeof(value), RDMN)) {
			goto out;
		}

		*data_buff++ = value;
		addr += sizeof(value);
	}

out:
	return (i * sizeof(value));
}
/*
 * Read ROM
 */
static int
nx_nic_md_rdrom(struct unm_adapter_s *adapter, ql_minidump_entry_rdrom_t 
					*romEntry, u64 *data_buff)
{
	int size = 0;
	u32 addr;

	addr = romEntry->read_addr;
	size = romEntry->read_data_size;	/* This is size in bytes */

	if (nx_nic_rom_fast_read_words(adapter, addr, (u8*) data_buff, size)) {
		return -EIO;
	}

	return (size);
}

/*
 * Read MUX data
 */
static u32
nx_nic_md_rdmux(struct unm_adapter_s *adapter, ql_minidump_entry_mux_t 
					*muxEntry, u32 *data_buff)
{
	int loop_cnt = 0;
	u32 read_value, sel_value;
	u32 read_addr, select_addr;

	read_addr = muxEntry->read_addr;
	sel_value = muxEntry->select_value;
	select_addr = muxEntry->select_addr;

	for (loop_cnt = 0; loop_cnt < muxEntry->op_count; loop_cnt++) {
		unm_nic_hw_write_bar_reg_2M(adapter, select_addr, sel_value);
		read_value = unm_nic_hw_read_bar_reg_2M(adapter, read_addr);

		*data_buff++ = sel_value;
		*data_buff++ = read_value;

		sel_value += muxEntry->select_value_stride;
	}

	return (loop_cnt * (2 * sizeof(u32)));
}

/*
 * Handling Queue State Reads.
 */
static u32
nx_nic_md_rdqueue(struct unm_adapter_s *adapter, ql_minidump_entry_queue_t 
						*queueEntry, u32 *data_buff)
{
	int loop_cnt, k;
	u32 read_value;
	u32 read_addr, read_stride, select_addr;
	u32 queue_id, read_cnt;

	read_cnt = queueEntry->read_addr_cnt;
	read_stride = queueEntry->read_addr_stride;
	select_addr = queueEntry->select_addr;

	for (loop_cnt = 0, queue_id = 0; loop_cnt < queueEntry->op_count;
				 loop_cnt++) {
		unm_nic_hw_write_bar_reg_2M(adapter, select_addr, queue_id);
		read_addr = queueEntry->read_addr;

		for (k = 0; k < read_cnt; k++) {
			read_value = unm_nic_hw_read_bar_reg_2M(adapter, read_addr);
			*data_buff++ = read_value;
			read_addr += read_stride;
		}

		queue_id += queueEntry->queue_id_stride;
	}

	return (loop_cnt * (read_cnt * sizeof(read_value)));
}

/*
 * We catch an error where driver does not read
 * as much data as we expect from the entry.
 */
static int
nx_nic_md_entry_err_chk(ql_minidump_entry_t *entry, u32 esize)
{
	if (esize < 0) {
		entry->hdr.driver_flags |= QL_DBG_SKIPPED_FLAG;
		return esize;
	}

	if (esize != entry->hdr.entry_capture_size) {
		entry->hdr.entry_capture_size = esize;
		entry->hdr.driver_flags |= QL_DBG_SIZE_ERR_FLAG;
	}

	return 0;
}

u32 nx_nic_check_template_checksum(struct unm_adapter_s *adapter)
{
	u64 sum =  0 ;
	u32 *buff = adapter->mdump.md_template;
    int count =  adapter->mdump.md_template_size/sizeof(uint32_t) ;

    while(count-- > 0) {
        sum += *buff++ ;
    }

    while(sum >> 32) {
        sum = (sum & 0xFFFFFFFF) +  (sum >> 32) ;
    }

    return (~sum) ;
}

u32 nx_nic_get_capture_size(struct unm_adapter_s *adapter)
{
	ql_minidump_template_hdr_t *hdr;
	int i, k, data_size = 0;
	u32 capture_mask;

	hdr = (ql_minidump_template_hdr_t *) adapter->mdump.md_template;

    capture_mask = adapter->mdump.md_capture_mask;

    for (i = 0x2, k = 1; (i & 0xFF); i <<= 1, k++) {
        if (i & capture_mask) {
            data_size += hdr->capture_size_array[k];
        }
    }	

	return data_size;
}

int nx_nic_collect_minidump(struct unm_adapter_s *adapter)
{
	int ret = 0;
	ql_minidump_template_hdr_t *hdr;
	struct timespec val;
	uint64_t cur_fn = (uint64_t)nx_nic_collect_minidump;

	memcpy(adapter->mdump.md_capture_buff, adapter->mdump.md_template, 
				adapter->mdump.md_template_size);
	if ((ret = nx_nic_parse_md_template(adapter))) {
		NX_NIC_TRC_FN(adapter, cur_fn, ret);
		return ret;
	}
	
	hdr = (ql_minidump_template_hdr_t *) adapter->mdump.md_capture_buff;

	hdr->driver_capture_mask = adapter->mdump.md_capture_mask;

	jiffies_to_timespec(jiffies, &val);
	hdr->driver_timestamp = (u32) val.tv_sec;

	hdr->driver_info_word2 = 0x00455358;			/* ESX */
	hdr->driver_info_word3 = 0x00342E2A;			/* 4.* */

	hdr->driver_info_word4 = NX_NIC_VERSION_CODE(_UNM_NIC_LINUX_MAJOR, _UNM_NIC_LINUX_MINOR, 
							_UNM_NIC_LINUX_MINOR);
	NX_NIC_TRC_FN(adapter, cur_fn, ret);
	return ret;
}
