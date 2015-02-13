/*******************************************************************
 * This file is part of the Emulex Linux Device Driver for         *
 * Fibre Channel Host Bus Adapters.                                *
 * Copyright (C) 2004-2011 Emulex.  All rights reserved.           *
 * EMULEX and SLI are trademarks of Emulex.                        *
 * www.emulex.com                                                  *
 *                                                                 *
 * This program is free software; you can redistribute it and/or   *
 * modify it under the terms of version 2 of the GNU General       *
 * Public License as published by the Free Software Foundation.    *
 * This program is distributed in the hope that it will be useful. *
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND          *
 * WARRANTIES, INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY,  *
 * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT, ARE      *
 * DISCLAIMED, EXCEPT TO THE EXTENT THAT SUCH DISCLAIMERS ARE HELD *
 * TO BE LEGALLY INVALID.  See the GNU General Public License for  *
 * more details, a copy of which can be found in the file COPYING  *
 * included with this package.                                     *
 *******************************************************************/

typedef int (*node_filter)(struct lpfc_nodelist *, void *);

struct fc_rport;
int lpfc_issue_els_auth(struct lpfc_vport *, struct lpfc_nodelist *,
			uint8_t message_code, uint8_t *payload,
			uint32_t payload_len);
int lpfc_issue_els_auth_reject(struct lpfc_vport *vport,
			       struct lpfc_nodelist *ndlp,
			       uint8_t reason, uint8_t explanation);
void lpfc_down_link(struct lpfc_hba *, LPFC_MBOXQ_t *);
void lpfc_sli_read_link_ste(struct lpfc_hba *);
void lpfc_dump_mem(struct lpfc_hba *, LPFC_MBOXQ_t *, uint16_t, uint16_t);
void lpfc_dump_wakeup_param(struct lpfc_hba *, LPFC_MBOXQ_t *);
int lpfc_dump_static_vport(struct lpfc_hba *, LPFC_MBOXQ_t *, uint16_t);
int lpfc_dump_fcoe_param(struct lpfc_hba *, struct lpfcMboxq *);
void lpfc_read_nv(struct lpfc_hba *, LPFC_MBOXQ_t *);
void lpfc_config_async(struct lpfc_hba *, LPFC_MBOXQ_t *, uint32_t);

void lpfc_heart_beat(struct lpfc_hba *, LPFC_MBOXQ_t *);
int lpfc_read_la(struct lpfc_hba *, LPFC_MBOXQ_t *, struct lpfc_dmabuf *);
void lpfc_clear_la(struct lpfc_hba *, LPFC_MBOXQ_t *);
void lpfc_issue_clear_la(struct lpfc_hba *, struct lpfc_vport *);
void lpfc_config_link(struct lpfc_hba *, LPFC_MBOXQ_t *);
int lpfc_config_msi(struct lpfc_hba *, LPFC_MBOXQ_t *);
int lpfc_read_sparam(struct lpfc_hba *, LPFC_MBOXQ_t *, int);
void lpfc_read_config(struct lpfc_hba *, LPFC_MBOXQ_t *);
void lpfc_read_lnk_stat(struct lpfc_hba *, LPFC_MBOXQ_t *);
int lpfc_reg_rpi(struct lpfc_hba *, uint16_t, uint32_t, uint8_t *,
		 LPFC_MBOXQ_t *, uint32_t);
void lpfc_set_var(struct lpfc_hba *, LPFC_MBOXQ_t *, uint32_t, uint32_t);
void lpfc_unreg_login(struct lpfc_hba *, uint16_t, uint32_t, LPFC_MBOXQ_t *);
void lpfc_unreg_did(struct lpfc_hba *, uint16_t, uint32_t, LPFC_MBOXQ_t *);
void lpfc_sli4_unreg_all_rpis(struct lpfc_vport *);

void lpfc_reg_vpi(struct lpfc_vport *, LPFC_MBOXQ_t *);
void lpfc_register_new_vport(struct lpfc_hba *, struct lpfc_vport *,
			struct lpfc_nodelist *);
void lpfc_unreg_vpi(struct lpfc_hba *, uint16_t, LPFC_MBOXQ_t *);
void lpfc_init_link(struct lpfc_hba *, LPFC_MBOXQ_t *, uint32_t, uint32_t);
void lpfc_request_features(struct lpfc_hba *, struct lpfcMboxq *);
void lpfc_supported_pages(struct lpfcMboxq *);
void lpfc_sli4_params(struct lpfcMboxq *);
int lpfc_pc_sli4_params_get(struct lpfc_hba *, LPFC_MBOXQ_t *);

struct lpfc_vport *lpfc_find_vport_by_did(struct lpfc_hba *, uint32_t);
void lpfc_cleanup_rcv_buffers(struct lpfc_vport *);
void lpfc_rcv_seq_check_edtov(struct lpfc_vport *);
void lpfc_cleanup_rpis(struct lpfc_vport *, int);
void lpfc_cleanup_pending_mbox(struct lpfc_vport *);
int lpfc_linkdown(struct lpfc_hba *);
void lpfc_linkdown_port(struct lpfc_vport *);
void lpfc_port_link_failure(struct lpfc_vport *);
int __lpfc_vport_delete(struct Scsi_Host *, struct lpfc_vport *);
void lpfc_mbx_cmpl_read_la(struct lpfc_hba *, LPFC_MBOXQ_t *);
void lpfc_init_vpi_cmpl(struct lpfc_hba *, LPFC_MBOXQ_t *);
void lpfc_cancel_all_vport_retry_delay_timer(struct lpfc_hba *);
void lpfc_retry_pport_discovery(struct lpfc_hba *);
void lpfc_release_rpi(struct lpfc_hba *, struct lpfc_vport *, uint16_t);

void lpfc_mbx_cmpl_clear_la(struct lpfc_hba *, LPFC_MBOXQ_t *);
void lpfc_mbx_cmpl_reg_login(struct lpfc_hba *, LPFC_MBOXQ_t *);
void lpfc_mbx_cmpl_dflt_rpi(struct lpfc_hba *, LPFC_MBOXQ_t *);
void lpfc_mbx_cmpl_fabric_reg_login(struct lpfc_hba *, LPFC_MBOXQ_t *);
void lpfc_mbx_cmpl_ns_reg_login(struct lpfc_hba *, LPFC_MBOXQ_t *);
void lpfc_mbx_cmpl_fdmi_reg_login(struct lpfc_hba *, LPFC_MBOXQ_t *);
void lpfc_mbx_cmpl_reg_vfi(struct lpfc_hba *, LPFC_MBOXQ_t *);
void lpfc_enqueue_node(struct lpfc_vport *, struct lpfc_nodelist *);
void lpfc_dequeue_node(struct lpfc_vport *, struct lpfc_nodelist *);
void lpfc_disable_node(struct lpfc_vport *, struct lpfc_nodelist *);
struct lpfc_nodelist *lpfc_enable_node(struct lpfc_vport *,
					struct lpfc_nodelist *, int);
void lpfc_nlp_set_state(struct lpfc_vport *, struct lpfc_nodelist *, int);
void lpfc_drop_node(struct lpfc_vport *, struct lpfc_nodelist *);
void lpfc_set_disctmo(struct lpfc_vport *);
int  lpfc_can_disctmo(struct lpfc_vport *);
int  lpfc_unreg_rpi(struct lpfc_vport *, struct lpfc_nodelist *);
void lpfc_unreg_all_rpis(struct lpfc_vport *);
void lpfc_unreg_hba_rpis(struct lpfc_hba *);
void lpfc_unreg_default_rpis(struct lpfc_vport *);
void lpfc_issue_reg_vpi(struct lpfc_hba *, struct lpfc_vport *);

int lpfc_check_sli_ndlp(struct lpfc_hba *, struct lpfc_sli_ring *,
			struct lpfc_iocbq *, struct lpfc_nodelist *);
void lpfc_nlp_init(struct lpfc_vport *, struct lpfc_nodelist *, uint32_t);
struct lpfc_nodelist *lpfc_nlp_get(struct lpfc_nodelist *);
int  lpfc_nlp_put(struct lpfc_nodelist *);
int  lpfc_nlp_not_used(struct lpfc_nodelist *ndlp);
struct lpfc_nodelist *lpfc_setup_disc_node(struct lpfc_vport *, uint32_t);
void lpfc_disc_list_loopmap(struct lpfc_vport *);
void lpfc_disc_start(struct lpfc_vport *);
void lpfc_disc_flush_list(struct lpfc_vport *);
void lpfc_cleanup_discovery_resources(struct lpfc_vport *);
void lpfc_cleanup(struct lpfc_vport *);
void lpfc_disc_timeout(unsigned long);

struct lpfc_nodelist *__lpfc_findnode_rpi(struct lpfc_vport *, uint16_t);
struct lpfc_nodelist *lpfc_findnode_rpi(struct lpfc_vport *, uint16_t);
struct lpfc_nodelist *lpfc_findnode_wwnn(struct lpfc_vport *,
					 struct lpfc_name *);

void lpfc_port_auth_failed(struct lpfc_nodelist *);
void lpfc_worker_wake_up(struct lpfc_hba *);
int lpfc_workq_post_event(struct lpfc_hba *, void *, void *, uint32_t);
int lpfc_do_work(void *);
int lpfc_disc_state_machine(struct lpfc_vport *, struct lpfc_nodelist *, void *,
			    uint32_t);

void lpfc_do_scr_ns_plogi(struct lpfc_hba *, struct lpfc_vport *);
int lpfc_check_sparm(struct lpfc_vport *, struct lpfc_nodelist *,
		     struct serv_parm *, uint32_t, int);
int lpfc_els_abort(struct lpfc_hba *, struct lpfc_nodelist *);
void lpfc_more_plogi(struct lpfc_vport *);
void lpfc_more_adisc(struct lpfc_vport *);
void lpfc_end_rscn(struct lpfc_vport *);
int lpfc_els_chk_latt(struct lpfc_vport *);
struct lpfc_iocbq *lpfc_prep_els_iocb(struct lpfc_vport *, uint8_t, uint16_t,
				      uint8_t, struct lpfc_nodelist *, uint32_t,
				      uint32_t);
int lpfc_els_abort_flogi(struct lpfc_hba *);
int lpfc_initial_flogi(struct lpfc_vport *);
int lpfc_initial_fdisc(struct lpfc_vport *);
int lpfc_issue_els_plogi(struct lpfc_vport *, uint32_t, uint8_t);
int lpfc_issue_els_prli(struct lpfc_vport *, struct lpfc_nodelist *, uint8_t);
int lpfc_issue_els_adisc(struct lpfc_vport *, struct lpfc_nodelist *, uint8_t);
int lpfc_issue_els_logo(struct lpfc_vport *, struct lpfc_nodelist *, uint8_t);
int lpfc_issue_els_npiv_logo(struct lpfc_vport *, struct lpfc_nodelist *);
int lpfc_issue_els_scr(struct lpfc_vport *, uint32_t, uint8_t);
int lpfc_issue_fabric_reglogin(struct lpfc_vport *);
int lpfc_els_free_iocb(struct lpfc_hba *, struct lpfc_iocbq *);
int lpfc_ct_free_iocb(struct lpfc_hba *, struct lpfc_iocbq *);
int lpfc_els_rsp_acc(struct lpfc_vport *, uint32_t, struct lpfc_iocbq *,
		     struct lpfc_nodelist *, LPFC_MBOXQ_t *);
int lpfc_els_rsp_reject(struct lpfc_vport *, uint32_t, struct lpfc_iocbq *,
			struct lpfc_nodelist *, LPFC_MBOXQ_t *);
int lpfc_els_rsp_adisc_acc(struct lpfc_vport *, struct lpfc_iocbq *,
			   struct lpfc_nodelist *);
int lpfc_els_rsp_prli_acc(struct lpfc_vport *, struct lpfc_iocbq *,
			  struct lpfc_nodelist *);
void lpfc_cancel_retry_delay_tmo(struct lpfc_vport *, struct lpfc_nodelist *);
void lpfc_els_retry_delay(unsigned long);
void lpfc_els_retry_delay_handler(struct lpfc_nodelist *);
void lpfc_reauth_node(unsigned long);
void lpfc_reauthentication_handler(struct lpfc_nodelist *);
void lpfc_els_unsol_event(struct lpfc_hba *, struct lpfc_sli_ring *,
			  struct lpfc_iocbq *);
int lpfc_els_handle_rscn(struct lpfc_vport *);
void lpfc_els_flush_rscn(struct lpfc_vport *);
int lpfc_rscn_payload_check(struct lpfc_vport *, uint32_t);
void lpfc_els_flush_all_cmd(struct lpfc_hba *);
void lpfc_els_flush_cmd(struct lpfc_vport *);
int lpfc_els_disc_adisc(struct lpfc_vport *);
int lpfc_els_disc_plogi(struct lpfc_vport *);
void lpfc_els_timeout(unsigned long);
void lpfc_els_timeout_handler(struct lpfc_vport *);
void lpfc_hb_timeout_handler(struct lpfc_hba *);

void lpfc_ct_unsol_event(struct lpfc_hba *, struct lpfc_sli_ring *,
			 struct lpfc_iocbq *);
void lpfc_sli4_ct_abort_unsol_event(struct lpfc_hba *, struct lpfc_sli_ring *,
				    struct lpfc_iocbq *);
int lpfc_ns_cmd(struct lpfc_vport *, int, uint8_t, uint32_t);
int lpfc_fdmi_cmd(struct lpfc_vport *, struct lpfc_nodelist *, int);
void lpfc_fdmi_tmo(unsigned long);
void lpfc_fdmi_timeout_handler(struct lpfc_vport *);

int lpfc_config_port_prep(struct lpfc_hba *);
int lpfc_config_port_post(struct lpfc_hba *);
int lpfc_hba_down_prep(struct lpfc_hba *);
int lpfc_hba_down_post(struct lpfc_hba *);
void lpfc_hba_init(struct lpfc_hba *, uint32_t *);
int lpfc_post_buffer(struct lpfc_hba *, struct lpfc_sli_ring *, int, int);
void lpfc_decode_firmware_rev(struct lpfc_hba *, char *, int);
int lpfc_online(struct lpfc_hba *);
void lpfc_unblock_mgmt_io(struct lpfc_hba *);
void lpfc_offline_prep(struct lpfc_hba *);
void lpfc_offline(struct lpfc_hba *);
void lpfc_reset_hba(struct lpfc_hba *);

int lpfc_sli_setup(struct lpfc_hba *);
int lpfc_sli_queue_setup(struct lpfc_hba *);
int lpfc_sli_set_dma_length(struct lpfc_hba *, uint32_t);
int lpfc_sli4_hba_setup(struct lpfc_hba *);
uint16_t lpfc_sli4_fcf_record_get(struct lpfc_hba *, struct fcf_record *,
				  uint16_t);
void lpfc_handle_eratt(struct lpfc_hba *);
void lpfc_handle_latt(struct lpfc_hba *);
#if !defined(__VMKLNX__) && LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
irqreturn_t lpfc_sli_intr_handler(int, void *, struct pt_regs *);
irqreturn_t lpfc_sli_sp_intr_handler(int, void *, struct pt_regs *);
irqreturn_t lpfc_sli_fp_intr_handler(int, void *, struct pt_regs *);
irqreturn_t lpfc_sli4_intr_handler(int, void *, struct pt_regs *);
irqreturn_t lpfc_sli4_sp_intr_handler(int, void *, struct pt_regs *);
irqreturn_t lpfc_sli4_fp_intr_handler(int, void *, struct pt_regs *);
#else
irqreturn_t lpfc_sli_intr_handler(int, void *);
irqreturn_t lpfc_sli_sp_intr_handler(int, void *);
irqreturn_t lpfc_sli_fp_intr_handler(int, void *);
irqreturn_t lpfc_sli4_intr_handler(int, void *);
irqreturn_t lpfc_sli4_sp_intr_handler(int, void *);
irqreturn_t lpfc_sli4_fp_intr_handler(int, void *);
#endif

void lpfc_read_rev(struct lpfc_hba *, LPFC_MBOXQ_t *);
void lpfc_sli4_swap_str(struct lpfc_hba *, LPFC_MBOXQ_t *);
void lpfc_config_ring(struct lpfc_hba *, int, LPFC_MBOXQ_t *);
void lpfc_config_port(struct lpfc_hba *, LPFC_MBOXQ_t *);
void lpfc_kill_board(struct lpfc_hba *, LPFC_MBOXQ_t *);
void lpfc_mbox_put(struct lpfc_hba *, LPFC_MBOXQ_t *);
LPFC_MBOXQ_t *lpfc_mbox_get(struct lpfc_hba *);
void __lpfc_mbox_cmpl_put(struct lpfc_hba *, LPFC_MBOXQ_t *);
void lpfc_mbox_cmpl_put(struct lpfc_hba *, LPFC_MBOXQ_t *);
int lpfc_mbox_cmd_check(struct lpfc_hba *, LPFC_MBOXQ_t *);
int lpfc_mbox_dev_check(struct lpfc_hba *);
int lpfc_mbox_tmo_val(struct lpfc_hba *, int);
void lpfc_init_vfi(struct lpfcMboxq *, struct lpfc_vport *);
void lpfc_reg_vfi(struct lpfcMboxq *, struct lpfc_vport *, dma_addr_t);
void lpfc_init_vpi(struct lpfc_hba *, struct lpfcMboxq *, uint16_t);
void lpfc_unreg_vfi(struct lpfcMboxq *, struct lpfc_vport *);
void lpfc_reg_fcfi(struct lpfc_hba *, struct lpfcMboxq *);
void lpfc_unreg_fcfi(struct lpfcMboxq *, uint16_t);
void lpfc_resume_rpi(struct lpfcMboxq *, struct lpfc_nodelist *);
int lpfc_check_pending_fcoe_event(struct lpfc_hba *, uint8_t);
void lpfc_issue_init_vpi(struct lpfc_vport *);

void lpfc_config_hbq(struct lpfc_hba *, uint32_t, struct lpfc_hbq_init *,
	uint32_t , LPFC_MBOXQ_t *);
struct hbq_dmabuf *lpfc_els_hbq_alloc(struct lpfc_hba *);
void lpfc_els_hbq_free(struct lpfc_hba *, struct hbq_dmabuf *);
struct hbq_dmabuf *lpfc_sli4_rb_alloc(struct lpfc_hba *);
void lpfc_sli4_rb_free(struct lpfc_hba *, struct hbq_dmabuf *);
void lpfc_sli4_build_dflt_fcf_record(struct lpfc_hba *, struct fcf_record *,
			uint16_t);
void lpfc_unregister_fcf(struct lpfc_hba *);
void lpfc_unregister_fcf_rescan(struct lpfc_hba *);
void lpfc_unregister_unused_fcf(struct lpfc_hba *);
int lpfc_sli4_redisc_fcf_table(struct lpfc_hba *);
void lpfc_fcf_redisc_wait_start_timer(struct lpfc_hba *);
void lpfc_sli4_fcf_dead_failthrough(struct lpfc_hba *);
uint16_t lpfc_sli4_fcf_rr_next_index_get(struct lpfc_hba *);
int lpfc_sli4_fcf_rr_index_set(struct lpfc_hba *, uint16_t);
void lpfc_sli4_fcf_rr_index_clear(struct lpfc_hba *, uint16_t);
int lpfc_sli4_fcf_rr_next_proc(struct lpfc_vport *, uint16_t);

int lpfc_mem_alloc(struct lpfc_hba *, int align);
void lpfc_mem_free(struct lpfc_hba *);
void lpfc_mem_free_all(struct lpfc_hba *);
void lpfc_stop_vport_timers(struct lpfc_vport *);

void lpfc_poll_timeout(unsigned long ptr);
void lpfc_poll_start_timer(struct lpfc_hba *);
void lpfc_poll_eratt(unsigned long);
int
lpfc_sli_handle_fast_ring_event(struct lpfc_hba *,
			struct lpfc_sli_ring *, uint32_t);

struct lpfc_iocbq * lpfc_sli_get_iocbq(struct lpfc_hba *);
void lpfc_sli_release_iocbq(struct lpfc_hba *, struct lpfc_iocbq *);
uint16_t lpfc_sli_next_iotag(struct lpfc_hba *, struct lpfc_iocbq *);
void lpfc_sli_cancel_iocbs(struct lpfc_hba *, struct list_head *, uint32_t,
			   uint32_t);
void lpfc_sli_wake_mbox_wait(struct lpfc_hba *, LPFC_MBOXQ_t *);

void lpfc_reset_barrier(struct lpfc_hba * phba);
int lpfc_sli_brdready(struct lpfc_hba *, uint32_t);
int lpfc_sli_brdkill(struct lpfc_hba *);
int lpfc_sli_brdreset(struct lpfc_hba *);
int lpfc_sli_brdrestart(struct lpfc_hba *);
int lpfc_sli_hba_setup(struct lpfc_hba *);
int lpfc_sli_config_port(struct lpfc_hba *, int);
int lpfc_sli_host_down(struct lpfc_vport *);
int lpfc_sli_hba_down(struct lpfc_hba *);
int lpfc_sli_issue_mbox(struct lpfc_hba *, LPFC_MBOXQ_t *, uint32_t);
int lpfc_sli_handle_mb_event(struct lpfc_hba *);
void lpfc_sli_mbox_sys_shutdown(struct lpfc_hba *);
int lpfc_sli_check_eratt(struct lpfc_hba *);
void lpfc_sli_handle_slow_ring_event(struct lpfc_hba *,
				    struct lpfc_sli_ring *, uint32_t);
void lpfc_sli4_handle_received_buffer(struct lpfc_hba *, struct hbq_dmabuf *);
void lpfc_sli_def_mbox_cmpl(struct lpfc_hba *, LPFC_MBOXQ_t *);
int lpfc_sli_issue_iocb(struct lpfc_hba *, uint32_t,
			struct lpfc_iocbq *, uint32_t);
void lpfc_sli_pcimem_bcopy(void *, void *, uint32_t);
void lpfc_sli_bemem_bcopy(void *, void *, uint32_t);
void lpfc_sli_abort_iocb_ring(struct lpfc_hba *, struct lpfc_sli_ring *);
void lpfc_sli_hba_iocb_abort(struct lpfc_hba *);
void lpfc_sli_flush_fcp_rings(struct lpfc_hba *);
int lpfc_sli_ringpostbuf_put(struct lpfc_hba *, struct lpfc_sli_ring *,
			     struct lpfc_dmabuf *);
struct lpfc_dmabuf *lpfc_sli_ringpostbuf_get(struct lpfc_hba *,
					     struct lpfc_sli_ring *,
					     dma_addr_t);

uint32_t lpfc_sli_get_buffer_tag(struct lpfc_hba *);
struct lpfc_dmabuf * lpfc_sli_ring_taggedbuf_get(struct lpfc_hba *,
			struct lpfc_sli_ring *, uint32_t );

int lpfc_sli_hbq_count(void);
int lpfc_sli_hbqbuf_add_hbqs(struct lpfc_hba *, uint32_t);
void lpfc_sli_hbqbuf_free_all(struct lpfc_hba *);
int lpfc_sli_hbq_size(void);
int lpfc_sli_issue_abort_iotag(struct lpfc_hba *, struct lpfc_sli_ring *,
			       struct lpfc_iocbq *);
int lpfc_sli_sum_iocb(struct lpfc_vport *, uint16_t, uint64_t, lpfc_ctx_cmd,
			unsigned long, uint16_t);
int lpfc_sli_abort_iocb(struct lpfc_vport *, struct lpfc_sli_ring *, uint16_t,
			uint64_t, lpfc_ctx_cmd, uint16_t flag);

void lpfc_mbox_timeout(unsigned long);
void lpfc_mbox_timeout_handler(struct lpfc_hba *);

struct lpfc_nodelist *__lpfc_find_node(struct lpfc_vport *, node_filter,
				       void *);
struct lpfc_nodelist *lpfc_find_node(struct lpfc_vport *, node_filter, void *);
struct lpfc_nodelist *lpfc_findnode_did(struct lpfc_vport *, uint32_t);
struct lpfc_nodelist *lpfc_findnode_wwpn(struct lpfc_vport *,
					 struct lpfc_name *);

bool lpfc_is_unmapped_node_logged_in(struct lpfc_vport *, uint32_t);
int lpfc_sli_issue_mbox_wait(struct lpfc_hba *, LPFC_MBOXQ_t *, uint32_t);

int lpfc_sli_issue_iocb_wait(struct lpfc_hba *, uint32_t,
                             struct lpfc_iocbq *, struct lpfc_iocbq *,
                             uint32_t);
void lpfc_sli_abort_fcp_cmpl(struct lpfc_hba *, struct lpfc_iocbq *,
			     struct lpfc_iocbq *);

void lpfc_sli_free_hbq(struct lpfc_hba *, struct hbq_dmabuf *);

void *lpfc_mbuf_alloc(struct lpfc_hba *, int, dma_addr_t *);
void __lpfc_mbuf_free(struct lpfc_hba *, void *, dma_addr_t);
void lpfc_mbuf_free(struct lpfc_hba *, void *, dma_addr_t);

void lpfc_in_buf_free(struct lpfc_hba *, struct lpfc_dmabuf *);
/* Function prototypes. */
const char* lpfc_info(struct Scsi_Host *);
int lpfc_hba_put_event(struct lpfc_hba *, uint32_t, uint32_t,
			uint32_t, uint32_t, uint32_t);
int lpfc_scan_finished(struct Scsi_Host *, unsigned long);

int lpfc_init_api_table_setup(struct lpfc_hba *, uint8_t);
int lpfc_sli_api_table_setup(struct lpfc_hba *, uint8_t);
int lpfc_scsi_api_table_setup(struct lpfc_hba *, uint8_t);
int lpfc_mbox_api_table_setup(struct lpfc_hba *, uint8_t);
int lpfc_api_table_setup(struct lpfc_hba *, uint8_t);

void lpfc_get_cfgparam(struct lpfc_hba *);
void lpfc_get_vport_cfgparam(struct lpfc_vport *);
int lpfc_alloc_sysfs_attr(struct lpfc_vport *);
void lpfc_free_sysfs_attr(struct lpfc_vport *);
extern struct class_device_attribute *lpfc_hba_attrs[];
extern struct class_device_attribute *lpfc_vport_attrs[];
extern struct scsi_host_template lpfc_template;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 21)
extern struct class_device_attribute *lpfc_hba_attrs_no_npiv[];
#endif
extern struct scsi_host_template lpfc_vport_template;
extern struct fc_function_template lpfc_transport_functions;
extern struct fc_function_template lpfc_vport_transport_functions;

#if !defined(__VMKLNX__)
extern int lpfc_sli_mode;
#endif

int lpfc_get_enable_npiv(void);

extern char *lpfc_exclude_hba;

int  lpfc_vport_symbolic_node_name(struct lpfc_vport *, char *, size_t);
int  lpfc_vport_symbolic_port_name(struct lpfc_vport *, char *, size_t);
void lpfc_terminate_rport_io(struct fc_rport *);
void lpfc_dev_loss_tmo_callbk(struct fc_rport *rport);

struct lpfc_vport *lpfc_create_port(struct lpfc_hba *, int, struct device *);
#ifdef fc_host_vports
int  lpfc_vport_disable(struct fc_vport *fc_vport, bool disable);
#endif
int lpfc_mbx_unreg_vpi(struct lpfc_vport *);
void destroy_port(struct lpfc_vport *);
int lpfc_get_instance(void);
int lpfc_get_shost_instance(void);
void lpfc_host_attrib_init(struct Scsi_Host *);

int lpfc_selective_reset(struct lpfc_hba *);
int lpfc_security_wait(struct lpfc_hba *);
int  lpfc_get_security_enabled(struct Scsi_Host *);
void lpfc_security_service_online(struct Scsi_Host *);
void lpfc_security_service_offline(struct Scsi_Host *);
void lpfc_security_config(struct Scsi_Host *, int status, void *);
int lpfc_security_config_wait(struct lpfc_vport *vport);
void lpfc_dhchap_make_challenge(struct Scsi_Host *, int , void *, uint32_t);
void lpfc_dhchap_make_response(struct Scsi_Host *, int , void *, uint32_t);
void lpfc_dhchap_authenticate(struct Scsi_Host *, int , void *, uint32_t);
int lpfc_start_node_authentication(struct lpfc_nodelist *);
int lpfc_get_auth_config(struct lpfc_nodelist *, struct lpfc_name *);
void lpfc_start_discovery(struct lpfc_vport *vport);
void lpfc_start_authentication(struct lpfc_vport *, struct lpfc_nodelist *);
int lpfc_rcv_nl_msg(struct Scsi_Host *, void *, uint32_t, uint32_t);

extern void lpfc_debugfs_initialize(struct lpfc_vport *);
extern void lpfc_debugfs_terminate(struct lpfc_vport *);
extern void lpfc_debugfs_disc_trc(struct lpfc_vport *, int, char *, uint32_t,
	uint32_t, uint32_t);
extern void lpfc_debugfs_slow_ring_trc(struct lpfc_hba *, char *, uint32_t,
	uint32_t, uint32_t);
extern struct lpfc_hbq_init *lpfc_hbq_defs[];

extern uint8_t lpfc_security_service_state;
extern spinlock_t fc_security_user_lock;
extern struct list_head fc_security_user_list;
extern int fc_service_state;
#ifndef SCSI_NL_SHOST_VENDOR
extern struct notifier_block lpfc_fc_netlink_notifier;
extern struct sock *fc_nl_sock;
void lpfc_fc_nl_rcv(struct sock *sk, int len);
int lpfc_fc_nl_rcv_nl_event(struct notifier_block *, unsigned long , void *);
#else
void lpfc_rcv_nl_event(struct notifier_block *, unsigned long , void *);
#endif

/* Interface exported by fabric iocb scheduler */
void lpfc_fabric_abort_nport(struct lpfc_nodelist *);
void lpfc_fabric_abort_hba(struct lpfc_hba *);
void lpfc_fabric_abort_flogi(struct lpfc_hba *);
void lpfc_fabric_block_timeout(unsigned long);
void lpfc_unblock_fabric_iocbs(struct lpfc_hba *);
void lpfc_rampdown_queue_depth(struct lpfc_hba *);
void lpfc_ramp_down_queue_handler(struct lpfc_hba *);
void lpfc_ramp_up_queue_handler(struct lpfc_hba *);
int lpfc_change_queue_depth(struct scsi_device *sdev, int qdepth, int reason);

void lpfc_scsi_dev_block(struct lpfc_hba *);
void lpfc_scsi_dev_rescan(struct lpfc_hba *);

void
lpfc_send_els_failure_event(struct lpfc_hba *, struct lpfc_iocbq *,
				struct lpfc_iocbq *);
void lpfc_board_errevt_to_mgmt(struct lpfc_hba *);
struct lpfc_fast_path_event *lpfc_alloc_fast_evt(struct lpfc_hba *);
void lpfc_free_fast_evt(struct lpfc_hba *, struct lpfc_fast_path_event *);
void lpfc_create_static_vport(struct lpfc_hba *);
void lpfc_stop_hba_timers(struct lpfc_hba *);
void lpfc_stop_port(struct lpfc_hba *);
void __lpfc_sli4_stop_fcf_redisc_wait_timer(struct lpfc_hba *);
void lpfc_sli4_stop_fcf_redisc_wait_timer(struct lpfc_hba *);
void lpfc_parse_fcoe_conf(struct lpfc_hba *, uint8_t *, uint32_t);
int lpfc_parse_vpd(struct lpfc_hba *, uint8_t *, int);
void lpfc_start_fdiscs(struct lpfc_hba *phba);
struct lpfc_vport *lpfc_find_vport_by_vpid(struct lpfc_hba *, uint16_t);
struct lpfc_sglq *__lpfc_get_active_sglq(struct lpfc_hba *, uint16_t);
int lpfc_get_hba_info(struct lpfc_hba *phba, uint32_t *, uint32_t *, uint32_t *,
                      uint32_t *, uint32_t *, uint32_t *);
int lpfc_board_mode_get(struct lpfc_hba *, uint32_t *);
int
lpfc_board_mode_set(struct lpfc_hba *, uint32_t, uint32_t *);
int __dfc_cmd_data_free(struct lpfc_hba *, struct lpfc_dmabufext *);
void lpfc_sli4_eq_clr_intr(struct lpfc_queue *);
void lpfc_sli4_check_fp_eq(struct lpfc_hba *, uint32_t);
void lpfc_set_clock();
#define ScsiResult(host_code, scsi_code) (((host_code) << 16) | scsi_code)
#define HBA_EVENT_RSCN                   5
#define HBA_EVENT_LINK_UP                2
#define HBA_EVENT_LINK_DOWN              3

int lpfc_log_verbose_set(struct lpfc_vport *, uint32_t);

#define CFG_LP  CFG_SLI3 /* Used for all LightPulse instances (SLI-1,2,3) */

/*
 * Returns TRUE if phba is an FCoE instance
 */
static inline bool
lpfc_is_fcoe(struct lpfc_hba *phba)
{
	return (phba->hba_flag & HBA_FCOE_SUPPORT) ? true : false;
}

/*
 * Returns TRUE if phba is an FC instance
 */
static inline bool
lpfc_is_fc(struct lpfc_hba *phba)
{
	return (phba->hba_flag & HBA_FCOE_SUPPORT) ? false : true;
}

/*
 * Returns TRUE if phba is an SLI4 instance
 */
static inline bool
lpfc_is_sli4(struct lpfc_hba *phba)
{
	return (phba->sli_rev == LPFC_SLI_REV4) ? true : false;
}


/*
 * Returns TRUE if phba is LightPulse instance.
 * That would be SLI-1, 2, or 3.
 */
static inline bool
lpfc_is_lp(struct lpfc_hba *phba)
{
	return (phba->sli_rev < LPFC_SLI_REV4) ? true : false;
}

extern uint32_t sd_test;
void dfc_inject_sdevent(struct lpfc_hba *,
			struct lpfc_iocbq *,
			struct lpfc_iocbq *);
void dfc_inject_qdepth_down(struct lpfc_hba *, IOCB_t *);
void dfc_inject_qdepth_down_s4(struct lpfc_hba *, struct lpfc_wcqe_complete *);
int lpfc_ioctl_set_sd_test(struct lpfc_hba *, struct lpfcCmdInput *);
int lpfc_ioctl_sd_event(struct lpfc_hba *, struct lpfcCmdInput *);
int lpfc_ioctl_node_stat(struct lpfc_hba *, struct lpfcCmdInput *, void *);

/*
 * The random32 apis are back ported from upstream 2.6.33rc7 kernel.
 * The apis are available to SLES11 or later, and RHEL6 or later
 * distributions.
 */
uint32_t random32(struct lpfc_hba *);
void srandom32(struct lpfc_hba *, uint32_t);
int random32_init(struct lpfc_hba *);
int random32_reseed(struct lpfc_hba *);

void __lpfc_sli_ringtx_put(struct lpfc_hba *, struct lpfc_sli_ring *,
	struct lpfc_iocbq *);
struct lpfc_iocbq *lpfc_sli_ringtx_get(struct lpfc_hba *,
	struct lpfc_sli_ring *);
int __lpfc_sli_issue_iocb(struct lpfc_hba *, uint32_t,
	struct lpfc_iocbq *, uint32_t);
uint32_t lpfc_drain_txq(struct lpfc_hba *);
void lpfc_clr_rrq_active(struct lpfc_hba *, uint16_t, struct lpfc_node_rrq *);
int lpfc_test_rrq_active(struct lpfc_hba *, struct lpfc_nodelist *, uint16_t);
void lpfc_handle_rrq_active(struct lpfc_hba *);
int lpfc_send_rrq(struct lpfc_hba *, struct lpfc_node_rrq *);
int lpfc_set_rrq_active(struct lpfc_hba *, struct lpfc_nodelist *,
	uint16_t, uint16_t, uint16_t);
void lpfc_cleanup_wt_rrqs(struct lpfc_hba *);
void lpfc_cleanup_vports_rrqs(struct lpfc_vport *, struct lpfc_nodelist *);
struct lpfc_node_rrq *lpfc_get_active_rrq(struct lpfc_vport *, uint16_t,
	uint32_t);

