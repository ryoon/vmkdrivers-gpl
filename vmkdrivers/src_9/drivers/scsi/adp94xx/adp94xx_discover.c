/*
 * Portions Copyright 2008, 2009 VMware, Inc.
 */
#define NO_VPD_WORKAROUND
#define SMP_OVERRUN_WORKAROUND
#define SMP_UNDERRUN_WORKAROUND
/*
 * Adaptec ADP94xx SAS HBA device driver for Linux.
 *
 * Copyright (c) 2004 Adaptec Inc.
 * All rights reserved.
 *
 * Adapted by : Robert Tarte  <robt@PacificCodeWorks.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */
/*
 * The source in this file is adapted from: SASDiscoverSimulation.cpp,
 * from the SAS-1.1 draft, sas1r07.pdf, project T10/1601-D,
 * ISO/IEC 14776-151:200x.
 */
/*
 * This is an implementation of the initiator based expander discovery
 * and configuration.  Structure names used are equivalent to those
 * referenced in the SAS document.
 * Basic assumptions:
 *
 * 1. Change primitives will initiate a rediscovery/configuration sequence.
 * 2. Table locations for SASAddresses are deterministic for a specific
 * topology only, when the topology changes, the location of a SASAddress
 * in an ASIC table cannot be assumed.
 * 3. A complete discovery level occurs before the configuration of the
 * level begins, multiple passes are required as the levels of expanders
 * encountered between the initiator and the end devices is increased.
 * 4. Configuration of a single expander occurs before proceeding to
 * subsequent expanders attached.
 * 5. The Attached structure is filled in following OOB and is available
 * from the initialization routines.
 *
 * $Id: //depot/razor/linux/src/adp94xx_discover.c#73 $
 * 
 */
#define DISCOVER_DEBUG	0

#include "adp94xx_osm.h"
#include "adp94xx_inline.h"
#include "adp94xx_sata.h"
#if KDB_ENABLE
#include "linux/kdb.h"
#endif

/*
 * this defines the type of algorithm used for discover
 */
#if 0
int DiscoverAlgorithm = SAS_SIMPLE_LEVEL_DESCENT;
#else
int DiscoverAlgorithm = SAS_UNIQUE_LEVEL_DESCENT;
#endif

#define ROUTE_ENTRY(expander, i, j) \
	(expander->RouteTable + \
		((expander->num_phys * (i) * SAS_ADDR_LEN) + \
		(j) * SAS_ADDR_LEN))

#define DUMP_EXPANDER(s, expander) \
	asd_dump_expander((char *)__FUNCTION__, __LINE__, (s), expander)

#define NEW_STATE(new_state)	new_state(sm_contextp, new_state)

struct state_machine asd_DiscoverySM = {
	asd_DiscoverySM_Initialize,
	asd_DiscoverySM_StateMachine,
	asd_DiscoverySM_Finish,
	asd_DiscoverySM_Abort,
	ASD_STATE_DISCOVER_START
};

struct state_machine asd_DiscoverExpanderSM = {
	asd_DiscoverExpanderSM_Initialize,
	asd_DiscoverExpanderSM_StateMachine,
	asd_DiscoverExpanderSM_Finish,
	asd_DiscoverExpanderSM_Abort,
	ASD_STATE_REPORT_AND_DISCOVER_START
};

struct state_machine asd_DiscoverFindBoundarySM = {
	asd_DiscoverFindBoundarySM_Initialize,
	asd_DiscoverFindBoundarySM_StateMachine,
	asd_DiscoverFindBoundarySM_Finish,
	asd_DiscoverFindBoundarySM_Abort,
	ASD_STATE_FIND_BOUNDARY_START
};

struct state_machine asd_DiscoverConfigSetSM = {
	asd_DiscoverConfigSetSM_Initialize,
	asd_DiscoverConfigSetSM_StateMachine,
	asd_DiscoverConfigSetSM_Finish,
	asd_DiscoverConfigSetSM_Abort,
	ASD_STATE_CONFIG_SET_START
};

struct state_machine asd_ConfigureExpanderSM = {
	asd_ConfigureExpanderSM_Initialize,
	asd_ConfigureExpanderSM_StateMachine,
	asd_ConfigureExpanderSM_Finish,
	asd_ConfigureExpanderSM_Abort,
	ASD_STATE_CONFIG_EXPANDER_START
};

struct state_machine asd_ConfigureATA_SM = {
	asd_ConfigureATA_SM_Initialize,
	asd_ConfigureATA_SM_StateMachine,
	asd_ConfigureATA_SM_Finish,
	asd_ConfigureATA_SM_Abort,
	ASD_STATE_CONFIGURE_ATA_START
};

struct state_machine asd_InitSATA_SM = {
	asd_InitSATA_SM_Initialize,
	asd_InitSATA_SM_StateMachine,
	asd_InitSATA_SM_Finish,
	asd_InitSATA_SM_Abort,
	ASD_STATE_INIT_SATA_START
};

struct state_machine asd_SATA_SpinHoldSM = {
	asd_SATA_SpinHoldSM_Initialize,
	asd_SATA_SpinHoldSM_StateMachine,
	asd_SATA_SpinHoldSM_Finish,
	asd_SATA_SpinHoldSM_Abort,
	ASD_STATE_SATA_SPINHOLD_START
};

struct state_machine asd_InitSAS_SM = {
	asd_InitSAS_SM_Initialize,
	asd_InitSAS_SM_StateMachine,
	asd_InitSAS_SM_Finish,
	asd_InitSAS_SM_Abort,
	ASD_STATE_INIT_SAS_START
};

struct state_machine asd_InitSMP_SM = {
	asd_InitSMP_SM_Initialize,
	asd_InitSMP_SM_StateMachine,
	asd_InitSMP_SM_Finish,
	asd_InitSMP_SM_Abort,
	ASD_STATE_INIT_SMP_START
};

extern void
asd_scb_internal_done(struct asd_softc *asd, struct scb *scb,
		struct asd_done_list *dl);
extern void asd_run_device_queues(struct asd_softc *asd);
struct asd_target *asd_discover_get_target(struct state_machine_context
					*sm_contextp,
					uint8_t * dest_sas_address,
					struct list_head *old_discover_listp,
					struct list_head *found_listp,
					unsigned conn_rate,
					TRANSPORT_TYPE transport_type);

struct asd_DiscoverySM_Context;

static void asd_invalidate_targets(struct asd_softc *asd,
					struct asd_port *port);
static void asd_validate_targets_hotplug(struct asd_softc *asd,
					struct asd_port *port,
					struct asd_DiscoverySM_Context *ctx);
static void asd_validate_targets_init(struct asd_softc *asd);
static void asd_apply_conn_mask(struct asd_softc *asd,
				struct list_head *discover_list);
static int asd_discovery_queue_cmd(struct asd_softc *asd,
				struct scb *scb, struct asd_target *targ,
				struct asd_device *dev);

#if DISCOVER_DEBUG
static void asd_dump_tree(struct asd_softc *asd, struct asd_port *port);
#else
#if !defined(__VMKLNX__)
/* Comment this out to make gcc happy */
static void asd_dump_tree(struct asd_softc *asd, struct asd_port *port)
{
}
#endif /* #if !defined(__VMKLNX__) */
#endif

void asd_print_conn_rate(unsigned conn_rate, char *s)
{
	switch (conn_rate) {
	case SAS_RATE_30GBPS:
		printk(" 3.0-GBPS");
		break;
	case SAS_RATE_15GBPS:
		printk(" 1.5-GBPS");
		break;
	default:
		printk(" \?\?-GBPS");
		break;
	}
	printk("%s", s);
}

void asd_print_state(unsigned state, char *s)
{
	switch (state) {
	case ASD_STATE_DISCOVER_START:
		printk("ASD_STATE_DISCOVER_START");
		break;

	case ASD_STATE_DISCOVER_ATTACHED:
		printk("ASD_STATE_DISCOVER_ATTACHED");
		break;

	case ASD_STATE_FIND_BOUNDARY:
		printk("ASD_STATE_FIND_BOUNDARY");
		break;

	case ASD_STATE_CONFIG_BOUNDARY_SET:
		printk("ASD_STATE_CONFIG_BOUNDARY_SET");
		break;

	case ASD_STATE_CONFIG_ATTACHED_SET:
		printk("ASD_STATE_CONFIG_ATTACHED_SET");
		break;

	case ASD_STATE_FINISHED:
		printk("ASD_STATE_FINISHED");
		break;

	case ASD_STATE_SATA_SPINHOLD:
		printk("ASD_STATE_SATA_SPINHOLD");
		break;

	case ASD_STATE_INIT_SATA:
		printk("ASD_STATE_INIT_SATA");
		break;

	case ASD_STATE_INIT_SAS:
		printk("ASD_STATE_INIT_SAS");
		break;

	case ASD_STATE_INIT_SMP:
		printk("ASD_STATE_INIT_SMP");
		break;

	case ASD_STATE_FAILED:
		printk("ASD_STATE_FAILED");
		break;

	case ASD_STATE_REPORT_AND_DISCOVER_START:
		printk("ASD_STATE_REPORT_AND_DISCOVER_START");
		break;

	case ASD_STATE_ISSUE_REPORT_GENERAL:
		printk("ASD_STATE_ISSUE_REPORT_GENERAL");
		break;

	case ASD_STATE_ISSUE_DISCOVER_LOOP:
		printk("ASD_STATE_ISSUE_DISCOVER_LOOP");
		break;

	case ASD_STATE_REPORT_AND_DISCOVER_FINISHED:
		printk("ASD_STATE_REPORT_AND_DISCOVER_FINISHED");
		break;

	case ASD_STATE_REPORT_AND_DISCOVER_FAILED:
		printk("ASD_STATE_REPORT_AND_DISCOVER_FAILED");
		break;

	case ASD_STATE_FIND_BOUNDARY_START:
		printk("ASD_STATE_FIND_BOUNDARY_START");
		break;

	case ASD_STATE_FIND_BOUNDARY_LOOP:
		printk("ASD_STATE_FIND_BOUNDARY_LOOP");
		break;

	case ASD_STATE_FIND_BOUNDARY_FINISHED:
		printk("ASD_STATE_FIND_BOUNDARY_FINISHED");
		break;

	case ASD_STATE_FIND_BOUNDARY_FAILED:
		printk("ASD_STATE_FIND_BOUNDARY_FAILED");
		break;

	case ASD_STATE_CONFIG_SET_START:
		printk("ASD_STATE_CONFIG_SET_START");
		break;

	case ASD_STATE_CONFIG_SET_ISSUE_DISCOVER:
		printk("ASD_STATE_CONFIG_SET_ISSUE_DISCOVER");
		break;

	case ASD_STATE_CONFIG_SET_CONFIGURE_EXPANDER:
		printk("ASD_STATE_CONFIG_SET_CONFIGURE_EXPANDER");
		break;

	case ASD_STATE_CONFIG_SET_FINISHED:
		printk("ASD_STATE_CONFIG_SET_FINISHED");
		break;

	case ASD_STATE_CONFIG_SET_FAILED:
		printk("ASD_STATE_CONFIG_SET_FAILED");
		break;

	case ASD_STATE_CONFIG_EXPANDER_START:
		printk("ASD_STATE_CONFIG_EXPANDER_START");
		break;

	case ASD_STATE_CONFIG_EXPANDER_ROUTE:
		printk("ASD_STATE_CONFIG_EXPANDER_ROUTE");
		break;

	case ASD_STATE_CONFIG_EXPANDER_ROUTE_LOOP:
		printk("ASD_STATE_CONFIG_EXPANDER_ROUTE_LOOP");
		break;

	case ASD_STATE_CONFIG_EXPANDER_FINISHED:
		printk("ASD_STATE_CONFIG_EXPANDER_FINISHED");
		break;

	case ASD_STATE_CONFIG_EXPANDER_FAILED:
		printk("ASD_STATE_CONFIG_EXPANDER_FAILED");
		break;

	case ASD_STATE_INIT_SATA_START:
		printk("ASD_STATE_INIT_SATA_START");
		break;

	case ASD_STATE_INIT_SATA_REPORT_PHY:
		printk("ASD_STATE_INIT_SATA_REPORT_PHY");
		break;

	case ASD_STATE_INIT_SATA_IDENTIFY:
		printk("ASD_STATE_INIT_SATA_IDENTIFY");
		break;

	case ASD_STATE_INIT_SATA_CONFIGURE_FEATURES:
		printk("ASD_STATE_INIT_SATA_CONFIGURE_FEATURES");
		break;

	case ASD_STATE_INIT_SATA_FINISHED:
		printk("ASD_STATE_INIT_SATA_FINISHED");
		break;

	case ASD_STATE_INIT_SATA_FAILED:
		printk("ASD_STATE_INIT_SATA_FAILED");
		break;

	case ASD_STATE_SATA_SPINHOLD_START:
		printk("ASD_STATE_SATA_SPINHOLD_START");
		break;

	case ASD_STATE_SATA_SPINHOLD_PHY_CONTROL:
		printk("ASD_STATE_SATA_SPINHOLD_PHY_CONTROL");
		break;

	case ASD_STATE_SATA_SPINHOLD_DISCOVER:
		printk("ASD_STATE_SATA_SPINHOLD_DISCOVER");
		break;

	case ASD_STATE_SATA_SPINHOLD_FINISHED:
		printk("ASD_STATE_SATA_SPINHOLD_FINISHED");
		break;

	case ASD_STATE_SATA_SPINHOLD_FAILED:
		printk("ASD_STATE_SATA_SPINHOLD_FAILED");
		break;

	case ASD_STATE_INIT_SAS_START:
		printk("ASD_STATE_INIT_SAS_START");
		break;

	case ASD_STATE_INIT_SAS_INQUIRY:
		printk("ASD_STATE_INIT_SAS_INQUIRY");
		break;

	case ASD_STATE_INIT_SAS_GET_DEVICE_ID:
		printk("ASD_STATE_INIT_SAS_GET_DEVICE_ID");
		break;

	case ASD_STATE_INIT_SAS_GET_SERIAL_NUMBER:
		printk("ASD_STATE_INIT_SAS_GET_SERIAL_NUMBER");
		break;

	case ASD_STATE_INIT_SAS_ISSUE_REPORT_LUNS:
		printk("ASD_STATE_INIT_SAS_ISSUE_REPORT_LUNS");
		break;

	case ASD_STATE_INIT_SAS_GET_PORT_CONTROL:
		printk("ASD_STATE_INIT_SAS_GET_PORT_CONTROL");
		break;

	case ASD_STATE_INIT_SAS_FINISHED:
		printk("ASD_STATE_INIT_SAS_FINISHED");
		break;

	case ASD_STATE_INIT_SAS_FAILED:
		printk("ASD_STATE_INIT_SAS_FAILED");
		break;

	case ASD_STATE_INIT_SMP_START:
		printk("ASD_STATE_INIT_SMP_START");
		break;

	case ASD_STATE_INIT_SMP_REPORT_MANUFACTURER_INFO:
		printk("ASD_STATE_INIT_SMP_REPORT_MANUFACTURER_INFO");
		break;

	case ASD_STATE_INIT_SMP_FINISHED:
		printk("ASD_STATE_INIT_SMP_FINISHED");
		break;

	case ASD_STATE_INIT_SMP_FAILED:
		printk("ASD_STATE_INIT_SMP_FAILED");
		break;

	case ASD_STATE_CONFIGURE_ATA_START:
		printk("ASD_STATE_CONFIGURE_ATA_START");
		break;

	case ASD_STATE_CONFIGURE_ATA_FEATURES:
		printk("ASD_STATE_CONFIGURE_ATA_FEATURES");
		break;

	case ASD_STATE_CONFIGURE_ATA_FINISHED:
		printk("ASD_STATE_CONFIGURE_ATA_FINISHED");
		break;

	case ASD_STATE_CONFIGURE_ATA_FAILED:
		printk("ASD_STATE_CONFIGURE_ATA_FAILED");
		break;

	default:
		printk("[0x%04x]", state);
		break;
	}

	printk("%s", s);
}

void SM_new_state(struct state_machine_context *sm_contextp, unsigned new_state)
{
	struct state_information *state_infop;

	SETUP_STATE(sm_contextp);

	//printk("[%d]===== ", sm_contextp->state_stack_top);
	//asd_print_state(state_infop->current_state, " -> ");
	//asd_print_state(new_state, "\n");

	if ((new_state & state_infop->state_machine_p->first_state) !=
		state_infop->state_machine_p->first_state) {

		printk("illegal state 0x%x\n", new_state);
		printk("[%d]===== ", sm_contextp->state_stack_top);
		asd_print_state(state_infop->current_state, " -> ");
		asd_print_state(new_state, "\n");
	}

	state_infop->current_state = new_state;
}

DISCOVER_RESULTS
asd_run_state_machine(struct state_machine_context *sm_contextp)
{
	struct state_information *state_infop;
	DISCOVER_RESULTS results;

	SETUP_STATE(sm_contextp);

	results = state_infop->state_machine_p->state_machine(sm_contextp);
#ifdef ASD_DEBUG
//JD
	asd_print_state(state_infop->current_state, " from asd_run_state_machine\n");
//JD
	printk("results = 0x%x\n", results);
#endif
	while (results != DISCOVER_OK) {

		state_infop =
			&sm_contextp->state_stack[sm_contextp->state_stack_top];

#ifdef ASD_DEBUG
		if (results == DISCOVER_FAILED) {
			printk("State Machine Failure: ");
			asd_print_state(state_infop->current_state, "\n");
		}
#endif

		if ((results == DISCOVER_FINISHED) ||
			(results == DISCOVER_FAILED)) {

			state_infop->state_machine_p->finish(sm_contextp,
								results);

			if (sm_contextp->state_stack_top == 0) {

				//printk("nothing on stack\n");

				return DISCOVER_OK;
			}

			POP_STATE(sm_contextp);
		}

		results = state_infop->state_machine_p->
			state_machine(sm_contextp);
	}

	return results;
}

#define ASD_PUSH_STATE_MACHINE(sm_contextp, state_machine_p, arg)	\
	asd_push_state_machine(sm_contextp, #state_machine_p,		\
	state_machine_p, arg)

DISCOVER_RESULTS
asd_push_state_machine(struct state_machine_context * sm_contextp,
			char *s,
			struct state_machine * state_machine_p, void *arg)
{
	struct state_information *state_infop;
	DISCOVER_RESULTS results;

#if 0
	printk("\n\n%s:=====================================================\n",
		__FUNCTION__);
	printk("%s: %s ============================\n", __FUNCTION__, s);
	printk("%s:=====================================================\n\n\n",
		__FUNCTION__);
#endif
	sm_contextp->state_stack_top++;
	SETUP_STATE(sm_contextp);

	state_infop->current_state = state_machine_p->first_state;
	state_infop->stack_top = 0;
	state_infop->state_machine_p = state_machine_p;

	results = state_machine_p->initialize(sm_contextp, arg);

	if (results != DISCOVER_CONTINUE) {

		state_infop->state_machine_p->finish(sm_contextp, results);

		if (sm_contextp->state_stack_top == 0) {

			//printk("nothing on stack\n");

			return results;
		}

		POP_STATE(sm_contextp);
	}

	return results;
}

void asd_abort_state_machine(struct state_machine_context *sm_contextp)
{
	struct state_information *state_infop;

	SETUP_STATE(sm_contextp);

	while (sm_contextp->state_stack_top != -1) {

		state_infop =
			&sm_contextp->state_stack[sm_contextp->state_stack_top];

		state_infop->state_machine_p->abort(sm_contextp);

		POP_STATE(sm_contextp);
	}
}

void
asd_dump_expander(char *function,
		unsigned line, char *string, struct asd_target *expander)
{
	unsigned i;
	struct Discover *discover;

	printk("||||||||||| - %s  - %s:%d:\n", string, function, line);
	printk("||||||||||| - %0llx - ",
		*((uint64_t *) expander->ddb_profile.sas_addr));

	if (expander->management_type == ASD_DEVICE_END) {
		printk("\n");
		return;
	}

	printk("num_phys %d\n", expander->num_phys);

	for (i = 0; i < expander->num_phys; i++) {

		discover = &(expander->Phy[i].Result);

		printk("||||||||||| - phy %d attached to %0llx\n", i,
		       *((uint64_t *) discover->AttachedSASAddress));
	}
}

void asd_dump_expander_list(char *s, struct list_head *discover_listp)
{
	struct asd_target *target;
	struct asd_target *parent;

	printk("----- %s\n", s);

	list_for_each_entry(target, discover_listp, all_domain_targets) {

		DUMP_EXPANDER("target:", target);

		printk(" %0llx", *((uint64_t *) target->ddb_profile.sas_addr));

		asd_print_conn_rate(target->ddb_profile.conn_rate, "\n");

		for (parent = target->parent; parent != NULL;
		     parent = parent->parent) {

			printk("\t:%0llx\n",
			       *((uint64_t *) parent->ddb_profile.sas_addr));
		}
	}
	printk("-----\n");
}

void
asd_discover_wakeup_state_machine(struct state_machine_context *sm_contextp)
{
	struct discover_context *discover_contextp;

	discover_contextp = (struct discover_context *)
		sm_contextp->state_handle;

	discover_contextp->port->events |= ASD_DISCOVERY_EVENT;

	asd_wakeup_sem(&discover_contextp->asd->platform_data->discovery_sem);
}


static void
asd_discovery_ehandler_done(struct asd_softc *asd, struct scb *scb)
{
//	unsigned long flags;
	struct state_machine_context *sm_contextp;
	struct discover_context *discover_contextp;

	asd_log(ASD_DBG_ERROR, "asd_ssp_abort_scb_done ehandler is done.\n");

	sm_contextp = (struct state_machine_context *)scb->io_ctx;

	discover_contextp = (struct discover_context *)
		sm_contextp->state_handle;
	discover_contextp->resid_len = 0;
	discover_contextp->openStatus = OPEN_REJECT_BAD_DESTINATION;
	sm_contextp->wakeup_state_machine(sm_contextp);
	asd->platform_data->flags &= ~ASD_SCB_UP_EH_SEM;
}

void
asd_discovery_timeout(
u_long		val
)
{
	struct scb			*scb;
	struct state_machine_context	*sm_contextp;
	struct discover_context 	*discover_contextp;
	u_long			 flags;
	scb = (struct scb *)val;
#ifdef ASD_DEBUG
	printk("%s:%d: scb 0x%x, scb->flags 0x%x, scb->post 0x%x timeout!!!\n", __FUNCTION__, __LINE__, scb,scb->flags,scb->post);
	printk("scb->io_ctx is 0x%x.\n", scb->io_ctx);
#endif
	sm_contextp = (struct state_machine_context *)scb->io_ctx;
	if(sm_contextp==NULL)
	{
		printk( "scb->io_ctx is NULL.\n");
		return;
	}
	discover_contextp = (struct discover_context *)
		sm_contextp->state_handle;
#ifdef ASD_DEBUG
	printk( "sm_contextp->state_handle is 0x%x.\n", sm_contextp->state_handle);
#endif
	if(discover_contextp==NULL)
	{
		printk( "sm_contextp->state_handle is NULL.\n");
		return;
	}
//JD
#ifdef DEBUG_DDB
#ifdef ASD_DEBUG
#ifdef CONCURRENT_SUPPORT
		{
			u_long	lseqs_to_dump;
			u_int	lseq_id;
			int		indx;

			for(indx=0;indx< discover_contextp->asd->ddb_bitmap_size; indx++)
			{
				lseq_id = 0;
				lseqs_to_dump = discover_contextp->asd->free_ddb_bitmap[indx];

				while (lseqs_to_dump != 0) { 
					for ( ; lseq_id < (8 * sizeof(u_long)); lseq_id++) {
						if (lseqs_to_dump & (1UL << lseq_id)) {
							lseqs_to_dump &= ~(1UL << lseq_id);
							break;
						} 
					}
		/* Dump out specific LSEQ Registers state. */
					asd_hwi_dump_ssp_smp_ddb_site(discover_contextp->asd, lseq_id + (indx * 8 * sizeof(ulong)));
				}
			}
		}
		asd_hwi_dump_seq_state(discover_contextp->asd, discover_contextp->asd->hw_profile.enabled_phys);
#endif
#endif
#endif
		asd_lock(discover_contextp->asd, &flags);
//	ASD_LOCK_ASSERT(discover_contextp->asd);
/* the following will try to do a linkreset and wait for another 4 sec.
it is a workaround for Vitesse VSC7160 Phy0&SES device problem
*/
	if(discover_contextp->retry_count==0)
	{
		struct asd_target	*target;

		target = scb->platform_data->targ;
		if(target != NULL)
		{
/* workaround for Vitesse vsc7160 issue, vitesse 50001cXXXd
*/
			if( (target->ddb_profile.sas_addr[0]==0x50) &&
				(target->ddb_profile.sas_addr[1]==0x00) &&
				(target->ddb_profile.sas_addr[2]==0x1c) )
			{
					struct asd_phy		*phy;
					struct asd_port		*port;

					port = scb->platform_data->targ->src_port;
/* behind Expander
*/
					if ( (port->management_type != ASD_DEVICE_END) && (target->parent != NULL) )
					{
/* the virtual SES device, last phy
*/
						if((target->ddb_profile.sas_addr[7]&0x0f)>=target->parent->num_phys)
						{
							list_for_each_entry(phy, &port->phys_attached, links)
							{
								asd_unlock(discover_contextp->asd, &flags);
								if (asd_hwi_enable_phy(discover_contextp->asd, phy) != 0) {
			/*
			 * TODO: This shouldn't happen.
			 *       Need more thought on how to proceed.
			 */
									asd_log(ASD_DBG_ERROR, "Failed to enable phy %d.\n", phy->id);
								}
								asd_lock(discover_contextp->asd, &flags);
							}
							discover_contextp->retry_count++;
							asd_setup_scb_timer(scb, (4 * HZ), asd_discovery_timeout);
							asd_unlock(discover_contextp->asd, &flags);
							return;
						}
					}
			}
		}
	}

	scb->flags |= SCB_TIMEDOUT;
	scb->eh_post = asd_discovery_ehandler_done;
	scb->eh_state = SCB_EH_ABORT_REQ;
	list_add_tail(&scb->timedout_links, 
		&discover_contextp->asd->timedout_scbs);
	discover_contextp->asd->platform_data->flags |= ASD_SCB_UP_EH_SEM;
	asd_unlock(discover_contextp->asd, &flags);
	asd_wakeup_sem(&discover_contextp->asd->platform_data->ehandler_sem);
}

static void
asd_discovery_request_timeout(
u_long		val
)
{
	struct scb		*scb;
	void			(*timeout_func)(u_long);

	scb = (struct scb *)val;

	timeout_func = scb->post_stack[scb->post_stack_depth-1].timeout_func;

	while (timeout_func == NULL)
	{
#ifdef ASD_DEBUG
		printk( "scb 0x%x, scb->post_stack_depth 0x%x, &scb->post_stack[0] =0x%x &scb->post_stack[1] =0x%x Found one NULL! ... that's OK, check the next!\n",scb,scb->post_stack_depth, &scb->post_stack[0], &scb->post_stack[1]);
#endif
		if (scb->post_stack_depth == 0) {
			panic("post_stack underflow - scb = %p\n", scb);
			return;
		}
		scb->post_stack_depth--;

		scb->io_ctx = scb->post_stack[scb->post_stack_depth-1].io_ctx;
		scb->post = scb->post_stack[scb->post_stack_depth-1].post;
		timeout_func = 
			scb->post_stack[scb->post_stack_depth-1].timeout_func;
#ifdef ASD_DEBUG
		printk( "Next post= 0x%x, io_ctx= 0x%x, timeout_func= 0x%x\n",scb->post_stack[scb->post_stack_depth-1].post,scb->post_stack[scb->post_stack_depth-1].io_ctx,scb->post_stack[scb->post_stack_depth-1].timeout_func); 
#endif
	}

#ifdef ASD_DEBUG
	printk( "Execute timeout routine (0x%x)\n", timeout_func);
#endif

	(*timeout_func)(val);
}

DISCOVER_RESULTS
asd_ssp_request(struct state_machine_context *sm_contextp,
		struct asd_target *target,
		uint8_t * command,
		unsigned command_len,
		dma_addr_t buf_busaddr, unsigned buffer_len, unsigned direction)
{
	struct asd_ssp_task_hscb *ssp_hscb;
	unsigned long flags;
	struct scb *scb;
	struct sg_element *sg;
	int error;
	struct discover_context *discover_contextp;

	discover_contextp = (struct discover_context *)
		sm_contextp->state_handle;

	asd_lock(discover_contextp->asd, &flags);
	asd_hwi_hash(target->ddb_profile.sas_addr, target->ddb_profile.hashed_sas_addr);

	/*
	 * Get an scb to use.
	 */
	if ((scb = asd_hwi_get_scb(discover_contextp->asd, 1)) == NULL) {
		// TODO - fix this

		asd_unlock(discover_contextp->asd, &flags);

		return DISCOVER_FAILED;
	}
#ifdef ASD_DEBUG
	asd_log(ASD_DBG_INFO, "scb 0x%x created\n", scb);
#endif

	scb->flags |= SCB_INTERNAL;

	scb->platform_data->targ = target;

	scb->platform_data->dev = NULL;

	list_add_tail(&scb->owner_links,
		      &discover_contextp->asd->platform_data->pending_os_scbs);

	ssp_hscb = &scb->hscb->ssp_task;

	ssp_hscb->header.opcode = SCB_INITIATE_SSP_TASK;

	asd_build_sas_header(target, ssp_hscb);

	ssp_hscb->protocol_conn_rate |= PROTOCOL_TYPE_SSP;

	ssp_hscb->data_dir_flags |= direction;

	ssp_hscb->xfer_len = asd_htole32(buffer_len);

	memcpy(ssp_hscb->cdb, command, command_len);

	memset(&ssp_hscb->cdb[command_len], 0,
		SCB_EMBEDDED_CDB_SIZE - command_len);

	sg = scb->sg_list;

	scb->platform_data->buf_busaddr = buf_busaddr;

	error = asd_sg_setup(sg, buf_busaddr, buffer_len, /*last */ 1);

	if (error != 0) {
#if defined(__VMKLNX__)
		asd_unlock(discover_contextp->asd, &flags);
#endif
		return DISCOVER_FAILED;
	}

	memcpy(ssp_hscb->sg_elements, scb->sg_list, sizeof(*sg));

	scb->sg_count = 1;

	asd_push_post_stack_timeout(discover_contextp->asd, scb,
		(void *)sm_contextp, asd_ssp_request_done,
			asd_discovery_timeout);

	scb->flags |= SCB_ACTIVE;

//JD TEST wait 4 seconds
	asd_setup_scb_timer(scb, (4 * HZ), asd_discovery_request_timeout);

	asd_hwi_post_scb(discover_contextp->asd, scb);

	asd_unlock(discover_contextp->asd, &flags);

	return DISCOVER_OK;
}

void
asd_ssp_request_done(struct asd_softc *asd,
		     struct scb *scb, struct asd_done_list *done_listp)
{
	struct state_machine_context *sm_contextp;
	struct discover_context *discover_contextp;
#ifdef ASD_DEBUG
	printk("%s:%d scb=0x%x dl=0x%x\n", __FUNCTION__, __LINE__, scb,done_listp->opcode);
#endif
//JD TEST
	del_timer_sync(&scb->platform_data->timeout);

	sm_contextp = (struct state_machine_context *)scb->io_ctx;

	discover_contextp = (struct discover_context *)
		sm_contextp->state_handle;

	asd_scb_internal_done(asd, scb, done_listp);

	discover_contextp->resid_len = 0;

	/*
	 * TODO: need better return value here
	 */
	switch (done_listp->opcode) {
	case TASK_COMP_WO_ERR:
		discover_contextp->openStatus = OPEN_ACCEPT;
		break;

	case TASK_COMP_W_UNDERRUN:
		discover_contextp->openStatus = OPEN_ACCEPT;
		discover_contextp->resid_len =
		    asd_le32toh(done_listp->stat_blk.data.res_len);
		break;

	case TASK_F_W_OPEN_REJECT:
		printk("%s:%d: reject abandon_open %x reason %x\n",
			__FUNCTION__, __LINE__,
			done_listp->stat_blk.open_reject.abandon_open,
			done_listp->stat_blk.open_reject.reason);
		break;

	case SSP_TASK_COMP_W_RESP:
		{
			union edb 		*edb;
			struct scb 		*escb;
			u_int			 edb_index;
			edb = asd_hwi_get_edb_from_dl(asd, scb, done_listp, &escb, &edb_index);
			if (edb != NULL) {
				asd_hwi_free_edb(asd, escb, edb_index);
			}
		}

	default:
		/*
		 * TODO: need better return value here
		 */
		discover_contextp->openStatus = OPEN_REJECT_BAD_DESTINATION;
		break;
	}
	sm_contextp->wakeup_state_machine(sm_contextp);
}

DISCOVER_RESULTS
asd_smp_request(struct state_machine_context *sm_contextp,
		struct asd_target *target,
		unsigned request_length, unsigned response_length)
{
	struct asd_smp_task_hscb *smp_hscb;
	unsigned long flags;
	struct scb *scb;
	struct discover_context *discover_contextp;

	discover_contextp = (struct discover_context *)
		sm_contextp->state_handle;

	asd_lock(discover_contextp->asd, &flags);

	/*
	 * Get an scb to use.
	 */
	if ((scb = asd_hwi_get_scb(discover_contextp->asd, 1)) == NULL) {
		// TODO - fix this

		asd_unlock(discover_contextp->asd, &flags);

		return DISCOVER_FAILED;
	}

	scb->flags |= SCB_INTERNAL;
	scb->platform_data->dev = NULL;
	scb->platform_data->targ = target;

	list_add_tail(&scb->owner_links,
		      &discover_contextp->asd->platform_data->pending_os_scbs);

	smp_hscb = &scb->hscb->smp_task;

	smp_hscb->header.opcode = SCB_INITIATE_SMP_TASK;
	smp_hscb->protocol_conn_rate = target->ddb_profile.conn_rate;

	smp_hscb->smp_req_busaddr = discover_contextp->SMPRequestBusAddr;
	smp_hscb->smp_req_size = request_length;

	smp_hscb->smp_req_ds = 0;
	smp_hscb->sister_scb = 0xffff;
	smp_hscb->conn_handle = target->ddb_profile.conn_handle;

	smp_hscb->smp_resp_busaddr = discover_contextp->SMPResponseBusAddr;
	smp_hscb->smp_resp_size = response_length;

	smp_hscb->smp_resp_ds = 0;

	asd_push_post_stack_timeout(discover_contextp->asd, scb,
		(void *)sm_contextp, asd_smp_request_done,
			asd_discovery_timeout);

	scb->flags |= SCB_ACTIVE;

//JD TEST
	asd_setup_scb_timer(scb, (4 * HZ), asd_discovery_request_timeout);

	asd_hwi_post_scb(discover_contextp->asd, scb);

	asd_unlock(discover_contextp->asd, &flags);

	return DISCOVER_OK;
}

void
asd_smp_request_done(struct asd_softc *asd,
		     struct scb *scb, struct asd_done_list *done_listp)
{
	struct state_machine_context *sm_contextp;
	struct discover_context *discover_contextp;

//JD TEST
	del_timer_sync(&scb->platform_data->timeout);

	sm_contextp = (struct state_machine_context *)scb->io_ctx;

	discover_contextp = (struct discover_context *)
		sm_contextp->state_handle;

	asd_scb_internal_done(asd, scb, done_listp);

#if 0
	if (done_listp->opcode != TASK_COMP_WO_ERR) {
		printk("%s:%d: opcode = 0x%x\n", __FUNCTION__, __LINE__,
		       done_listp->opcode);
	}
#endif
	discover_contextp->resid_len = 0;

	/*
	 * TODO: need better return value here
	 */
	switch (done_listp->opcode) {
	case TASK_COMP_WO_ERR:
		discover_contextp->openStatus = OPEN_ACCEPT;
		break;
#ifdef SMP_UNDERRUN_WORKAROUND
	case TASK_COMP_W_UNDERRUN:
		//printk("Ignoring UNDERRUN condition on SMP request\n");
		discover_contextp->openStatus = OPEN_ACCEPT;
		discover_contextp->resid_len =
		    asd_le32toh(done_listp->stat_blk.data.res_len);
		break;
#endif
#ifdef SMP_OVERRUN_WORKAROUND
	case TASK_COMP_W_OVERRUN:
		/*
		 * This wasn't fixed in B0, so it will be investigated more.
		 */
		//printk("Ignoring OVERRUN condition on SMP request - ");
		//printk("should be fixed in B0\n");
		discover_contextp->openStatus = OPEN_ACCEPT;
		break;
#endif
	case TASK_F_W_SMPRSP_TO:
	case TASK_F_W_SMP_XMTRCV_ERR:
		discover_contextp->openStatus = OPEN_REJECT_BAD_DESTINATION;
		break;
	case TASK_ABORTED_BY_ITNL_EXP:
		switch (done_listp->stat_blk.itnl_exp.reason) {
		case TASK_F_W_PHY_DOWN:
		case TASK_F_W_BREAK_RCVD:
		case TASK_F_W_OPEN_TO:
			discover_contextp->openStatus =
			    OPEN_REJECT_BAD_DESTINATION;
			break;

		case TASK_F_W_OPEN_REJECT:
			discover_contextp->openStatus =
			    OPEN_REJECT_BAD_DESTINATION;
#if 0
			discover_contextp->openStatus =
			    OPEN_REJECT_RATE_NOT_SUPPORTED;
			discover_contextp->openStatus =
			    OPEN_REJECT_NO_DESTINATION;
			discover_contextp->openStatus =
			    OPEN_REJECT_PATHWAY_BLOCKED;
			discover_contextp->openStatus =
			    OPEN_REJECT_PROTOCOL_NOT_SUPPORTED;
			discover_contextp->openStatus =
			    OPEN_REJECT_RESERVE_ABANDON;
			discover_contextp->openStatus =
			    OPEN_REJECT_RESERVE_CONTINUE;
			discover_contextp->openStatus =
			    OPEN_REJECT_RESERVE_INITIALIZE;
			discover_contextp->openStatus =
			    OPEN_REJECT_RESERVE_STOP;
			discover_contextp->openStatus = OPEN_REJECT_RETRY;
			discover_contextp->openStatus =
			    OPEN_REJECT_STP_RESOURCES_BUSY;
			discover_contextp->openStatus =
			    OPEN_REJECT_WRONG_DESTINATION;
#endif
			break;
		}
		break;
	case TASK_CLEARED:
		/* Aborted command. Status needs to be changed .... */
		break;
	default:
		discover_contextp->openStatus = OPEN_REJECT_BAD_DESTINATION;
		break;
	}

	sm_contextp->wakeup_state_machine(sm_contextp);
}

DISCOVER_RESULTS
asd_sata_identify_request(struct state_machine_context *sm_contextp,
			  struct asd_target *target)
{
	unsigned long flags;
	struct scb *scb;
	struct discover_context *discover_contextp;
	struct asd_target *old_target;

	discover_contextp = (struct discover_context *)
		sm_contextp->state_handle;
	asd_lock(discover_contextp->asd, &flags);

	/*
	 * Get an scb to use.
	 */
	if ((scb = asd_hwi_get_scb(discover_contextp->asd, 1)) == NULL) {
		// TODO - fix this

		asd_unlock(discover_contextp->asd, &flags);

		return DISCOVER_FAILED;
	}

	scb->flags |= SCB_INTERNAL;
	scb->platform_data->dev = NULL;

	asd_push_post_stack_timeout(discover_contextp->asd, scb,
		(void *)sm_contextp, asd_sata_identify_request_done,
		asd_discovery_timeout);

	list_add_tail(&scb->owner_links,
		      &discover_contextp->asd->platform_data->pending_os_scbs);

	if (asd_sata_identify_build(discover_contextp->asd, target, scb) != 0) {

		asd_hwi_free_scb(discover_contextp->asd, scb);

		asd_unlock(discover_contextp->asd, &flags);

		return DISCOVER_FAILED;
	}

	scb->flags |= SCB_ACTIVE;

	/*
	 * We want to use the flow control of the device queue if possible.
	 * Look through the old/new discover list for this target.
	 * If target exists, see if device exists.
	 */
	old_target = asd_find_target(discover_contextp->asd->old_discover_listp,
				     target->ddb_profile.sas_addr);

	if (old_target == NULL) {
		old_target =
		    asd_find_target(discover_contextp->asd->discover_listp,
				    target->ddb_profile.sas_addr);
	}

	/*
	 * sata devices should have only one lun, check lun 0.
	 */
	if (old_target != NULL) {

		if (old_target->devices[0] != NULL) {

			scb->platform_data->dev = target->devices[0];

			asd_unlock(discover_contextp->asd, &flags);

			if ((asd_discovery_queue_cmd(discover_contextp->asd,
						     scb, old_target,
						     old_target->devices[0]))) {

				asd_hwi_free_scb(discover_contextp->asd, scb);

				return DISCOVER_FAILED;
			}

			return DISCOVER_OK;
		}
	}

	scb->flags |= SCB_ACTIVE;

//JD TEST
	asd_setup_scb_timer(scb, (4 * HZ), asd_discovery_request_timeout);
//JD
#ifdef DEBUG_DDB
#ifdef ASD_DEBUG
#ifdef CONCURRENT_SUPPORT
		{
			u_long	lseqs_to_dump;
			u_int	lseq_id;
			int		indx;

			for(indx=0;indx< discover_contextp->asd->ddb_bitmap_size; indx++)
			{
				lseq_id = 0;
				lseqs_to_dump = discover_contextp->asd->free_ddb_bitmap[indx];

				while (lseqs_to_dump != 0) { 
					for ( ; lseq_id < (8 * sizeof(u_long)); lseq_id++) {
						if (lseqs_to_dump & (1UL << lseq_id)) {
							lseqs_to_dump &= ~(1UL << lseq_id);
							break;
						} 
					}
		/* Dump out specific LSEQ Registers state. */
					asd_hwi_dump_ssp_smp_ddb_site(discover_contextp->asd, lseq_id + (indx * 8 * sizeof(ulong)));
				}
			}
		}
#endif
#endif
#endif
	asd_hwi_post_scb(discover_contextp->asd, scb);

	asd_unlock(discover_contextp->asd, &flags);

	return DISCOVER_OK;
}

static int
asd_discovery_queue_cmd(struct asd_softc *asd,
			struct scb *scb, struct asd_target *targ,
			struct asd_device *dev)
{
	u_long				flags;
	struct asd_port			*port;

	asd_lock(asd, &flags);

	port = dev->target->src_port;

	dev->openings--;
	dev->active++;
	dev->commands_issued++;
	list_add_tail(&scb->owner_links, &asd->platform_data->pending_os_scbs);

	scb->flags |= SCB_ACTIVE;

//JD TEST
	asd_setup_scb_timer(scb, (4 * HZ), asd_discovery_request_timeout);

	asd_hwi_post_scb(asd, scb);

	asd_unlock(asd, &flags);

	return 0;
}

void
asd_sata_identify_request_done(struct asd_softc *asd,
			       struct scb *scb,
			       struct asd_done_list *done_listp)
{
	struct state_machine_context *sm_contextp;
	struct discover_context *discover_contextp;

//JD TEST
	del_timer_sync(&scb->platform_data->timeout);

	sm_contextp = (struct state_machine_context *)scb->io_ctx;

	discover_contextp = (struct discover_context *)
		sm_contextp->state_handle;

	/*
	 * If this is not NULL, scb was sent via device queue.
	 */
	if (scb->platform_data->dev) {
		scb->platform_data->dev->active--;
		scb->platform_data->dev->openings++;
		scb->platform_data->dev->commands_issued--;
	}

	asd_scb_internal_done(asd, scb, done_listp);

	discover_contextp->resid_len = 0;

	/*
	 * TODO: need better return value here
	 */
	switch (done_listp->opcode) {
	case TASK_COMP_WO_ERR:
		discover_contextp->openStatus = OPEN_ACCEPT;
		break;

	case TASK_COMP_W_UNDERRUN:
		discover_contextp->openStatus = OPEN_ACCEPT;
		discover_contextp->resid_len =
		    asd_le32toh(done_listp->stat_blk.data.res_len);
		break;

	case TASK_ABORTED_BY_ITNL_EXP:
		switch (done_listp->stat_blk.itnl_exp.reason) {
		case TASK_F_W_PHY_DOWN:
		case TASK_F_W_BREAK_RCVD:
		case TASK_F_W_OPEN_TO:
			discover_contextp->openStatus =
			    OPEN_REJECT_BAD_DESTINATION;
			break;

		case TASK_F_W_OPEN_REJECT:
			discover_contextp->openStatus =
			    OPEN_REJECT_BAD_DESTINATION;
#if 0
			discover_contextp->openStatus =
			    OPEN_REJECT_RATE_NOT_SUPPORTED;
			discover_contextp->openStatus =
			    OPEN_REJECT_NO_DESTINATION;
			discover_contextp->openStatus =
			    OPEN_REJECT_PATHWAY_BLOCKED;
			discover_contextp->openStatus =
			    OPEN_REJECT_PROTOCOL_NOT_SUPPORTED;
			discover_contextp->openStatus =
			    OPEN_REJECT_RESERVE_ABANDON;
			discover_contextp->openStatus =
			    OPEN_REJECT_RESERVE_CONTINUE;
			discover_contextp->openStatus =
			    OPEN_REJECT_RESERVE_INITIALIZE;
			discover_contextp->openStatus =
			    OPEN_REJECT_RESERVE_STOP;
			discover_contextp->openStatus = OPEN_REJECT_RETRY;
			discover_contextp->openStatus =
			    OPEN_REJECT_STP_RESOURCES_BUSY;
			discover_contextp->openStatus =
			    OPEN_REJECT_WRONG_DESTINATION;
#endif
			break;
		}
		break;
	case TASK_CLEARED:
		/* Aborted command. Status needs to be changed .... */
		break;
	default:
		discover_contextp->openStatus = OPEN_REJECT_BAD_DESTINATION;
		break;
	}

	sm_contextp->wakeup_state_machine(sm_contextp);
}

DISCOVER_RESULTS
asd_sata_configure_features(struct state_machine_context *sm_contextp,
			    struct asd_target *target,
			    uint8_t feature, uint8_t sector_count)
{
	unsigned long flags;
	struct scb *scb;
	struct discover_context *discover_contextp;

	discover_contextp = (struct discover_context *)
		sm_contextp->state_handle;

	asd_lock(discover_contextp->asd, &flags);

	/*
	 * Get an scb to use.
	 */
	if ((scb = asd_hwi_get_scb(discover_contextp->asd, 1)) == NULL) {
		// TODO - fix this

		asd_unlock(discover_contextp->asd, &flags);

		return DISCOVER_FAILED;
	}

	scb->flags |= SCB_INTERNAL;
	scb->platform_data->dev = NULL;
	scb->platform_data->targ = target;

	asd_push_post_stack_timeout(discover_contextp->asd, scb,
		(void *)sm_contextp, asd_sata_configure_features_done,
		asd_discovery_timeout);

	list_add_tail(&scb->owner_links,
		      &discover_contextp->asd->platform_data->pending_os_scbs);

	if (asd_sata_set_features_build(discover_contextp->asd, target, scb,
					feature, sector_count) != 0) {

		asd_hwi_free_scb(discover_contextp->asd, scb);

		asd_unlock(discover_contextp->asd, &flags);

		return DISCOVER_FAILED;
	}

	scb->flags |= SCB_ACTIVE;

//JD TEST
	asd_setup_scb_timer(scb, (4 * HZ), asd_discovery_request_timeout);

	asd_hwi_post_scb(discover_contextp->asd, scb);

	asd_unlock(discover_contextp->asd, &flags);

	return DISCOVER_OK;
}

void
asd_sata_configure_features_done(struct asd_softc *asd,
				 struct scb *scb,
				 struct asd_done_list *done_listp)
{
	struct state_machine_context *sm_contextp;
	struct discover_context *discover_contextp;

//JD TEST
	del_timer_sync(&scb->platform_data->timeout);

	sm_contextp = (struct state_machine_context *)scb->io_ctx;

	discover_contextp = (struct discover_context *)
		sm_contextp->state_handle;

	asd_scb_internal_done(asd, scb, done_listp);

	discover_contextp->resid_len = 0;

	/*
	 * TODO: need better return value here
	 */
	switch (done_listp->opcode) {
	case TASK_COMP_WO_ERR:
		discover_contextp->openStatus = OPEN_ACCEPT;
		break;

	case TASK_COMP_W_UNDERRUN:
		discover_contextp->openStatus = OPEN_ACCEPT;
		discover_contextp->resid_len =
		    asd_le32toh(done_listp->stat_blk.data.res_len);
		break;

	case TASK_ABORTED_BY_ITNL_EXP:
		switch (done_listp->stat_blk.itnl_exp.reason) {
		case TASK_F_W_PHY_DOWN:
		case TASK_F_W_BREAK_RCVD:
		case TASK_F_W_OPEN_TO:
			discover_contextp->openStatus =
				OPEN_REJECT_BAD_DESTINATION;
			break;

		case TASK_F_W_OPEN_REJECT:
			discover_contextp->openStatus = 
				OPEN_REJECT_BAD_DESTINATION;
			break;
		}
		break;
	default:
		discover_contextp->openStatus = OPEN_REJECT_BAD_DESTINATION;
		break;
	}

	sm_contextp->wakeup_state_machine(sm_contextp);
}

struct asd_target *asd_discover_get_target(struct state_machine_context
					   *sm_contextp,
					   uint8_t * dest_sas_address,
					   struct list_head *old_discover_listp,
					   struct list_head *found_listp,
					   unsigned conn_rate,
					   TRANSPORT_TYPE transport_type)
{
	struct asd_target *target;
	struct discover_context *discover_contextp;

	discover_contextp = (struct discover_context *)
		sm_contextp->state_handle;
	target = asd_find_target(old_discover_listp, dest_sas_address);

	if (target != NULL) {
		/*
		 * This target was previously found.
		 */
		target->flags |= ASD_TARG_RESEEN;

		target->ddb_profile.conn_rate = conn_rate;

		asd_hwi_build_ddb_site(discover_contextp->asd, target);

		/*
		 * First, take this target off of the general chain of all
		 * targets in the domain.
		 */
		list_del_init(&target->all_domain_targets);

		return target;
	}

	/*
	 * See if we haven't already talked to this device during the current
	 * discovery process.
	 */
	target = asd_find_target(found_listp, dest_sas_address);

	if (target != NULL) {
		/*
		 * First, take this target off of the general chain of all
		 * targets in the domain.
		 */
		list_del_init(&target->all_domain_targets);

		return target;
	}

	target = asd_alloc_target(discover_contextp->asd,
				  discover_contextp->port);

	/*
	 * make sure we only do this if the allocation is successful
	 */
	if (target == NULL) {
		return NULL;
	}

	target->domain = discover_contextp->asd->platform_data->
		domains[discover_contextp->port->id];

	target->ddb_profile.conn_rate = conn_rate;

	target->ddb_profile.itnl_const = ITNL_TIMEOUT_CONST;
#ifdef SEQUENCER_UPDATE
	if (transport_type == ASD_TRANSPORT_ATA) {
		target->ddb_profile.itnl_const = 1;
	}
#endif

	target->parent = NULL;

	/*
	 * Set to 1 for SSP, STP, and SMP device ports. 0 for all SATA direct
	 * attached ports.  Every device that is initialized by this routine
	 * is not a SATA direct attach.
	 */
	target->ddb_profile.open_affl = OPEN_AFFILIATION;

	memcpy(target->ddb_profile.sas_addr, dest_sas_address, SAS_ADDR_LEN);
	target->transport_type = transport_type;
	if (transport_type == ASD_TRANSPORT_STP) {
		//TODO: get SUPPORTS_AFFILIATION out of SMP request
		target->ddb_profile.open_affl |=
//		    (STP_AFFILIATION | SUPPORTS_AFFILIATION);
		    SUPPORTS_AFFILIATION;
	}
#ifdef SEQUENCER_UPDATE
#ifdef CONCURRENT_SUPPORT
	else if (transport_type != ASD_TRANSPORT_ATA) {
// not STP, nor ATA
		target->ddb_profile.open_affl |= CONCURRENT_CONNECTION_SUPPORT;
	}
#endif
#endif

	asd_hwi_hash(target->ddb_profile.sas_addr,
		     target->ddb_profile.hashed_sas_addr);

	// TODO: - we need to allocate this from LRU DDB list
	// (doesn't exist yet)
	asd_hwi_setup_ddb_site(discover_contextp->asd, target);
	return target;
}

DISCOVER_RESULTS
asd_find_subtractive_phy(struct state_machine_context * sm_contextp,
			 struct asd_target * expander,
			 uint8_t * subtractiveSASAddress,
			 uint8_t * attachedPhyIdentifier,
			 unsigned *conn_rate, uint8_t * phyIdentifier)
{
	struct Discover *discover;
	DISCOVER_RESULTS result;
	uint8_t phyCount;
	unsigned foundSubtractive;

	SAS_ZERO(subtractiveSASAddress);
	*attachedPhyIdentifier = 0;

	foundSubtractive = 0;

	/*
	 * walk through all the phys of this expander
	 */
	for (phyCount = 0; phyCount < expander->num_phys; phyCount++) {

		/*
		 * this is just a pointer helper
		 */
		discover = &(expander->Phy[phyCount].Result);

		/*
		 * look for phys with edge or fanout devices attached...
		 */
		if ((discover->RoutingAttribute != SUBTRACTIVE) ||
		    ((discover->AttachedDeviceType !=
		      EDGE_EXPANDER_DEVICE) &&
		     (discover->AttachedDeviceType !=
		      FANOUT_EXPANDER_DEVICE))) {

			continue;
		}

		/*
		 * make sure all the subtractive phys point to
		 * the same address when we are connected to an
		 * expander device
		 */
		if (SAS_ISZERO(subtractiveSASAddress)) {

			SASCPY(subtractiveSASAddress,
			       discover->AttachedSASAddress);

			*attachedPhyIdentifier =
			    discover->AttachedPhyIdentifier;

			result = DISCOVER_OK;

			*conn_rate = discover->NegotiatedPhysicalLinkRate;

			*phyIdentifier = phyCount;

			foundSubtractive = 1;
		} else if (!SAS_ISEQUAL(subtractiveSASAddress,
					discover->AttachedSASAddress)) {

			/*
			 * the addresses don't match... 
			 * problem...
			 */
			asd_log(ASD_DBG_ERROR, "\n"
				"topology error, diverging "
				"subtractive phys"
				", '%0llx' != '%0llx' \n",
				*((uint64_t *) subtractiveSASAddress),
				*((uint64_t *) discover->AttachedSASAddress));

			return DISCOVER_FAILED;
		}
	}

	if (foundSubtractive == 0) {
		return DISCOVER_FINISHED;
	}

	return DISCOVER_OK;
}

/*
 * find the table structure associated with a specific SAS address
 */
struct asd_target *asd_find_target(struct list_head *target_list,
				   uint8_t * SASAddress)
{
	struct asd_target *target;

	/*
	 * walk the list of expanders, when we find the one that matches, stop
	 */
	list_for_each_entry(target, target_list, all_domain_targets) {
		/*
		 * do the SASAdresses match
		 */
		if (SAS_ISEQUAL(target->ddb_profile.sas_addr, SASAddress)) {
			return target;
		}
	}
	return NULL;
}

/*
 * find the table structure associated with a specific target identity.
 */
struct asd_target *asd_find_target_ident(struct list_head *target_list,
					 struct asd_target *check_target)
{
	struct asd_target *target;

	/*
	 * walk the list of expanders, when we find the one that matches, stop
	 */
	list_for_each_entry(target, target_list, all_domain_targets) {

		/*
		 * Make sure we don't match ourselves.
		 */
		if (check_target == target) {
			continue;
		}

		/*
		 * It can't possibly be a match if the transport types don't
		 * match.
		 */
		if (check_target->transport_type != target->transport_type) {
			continue;
		}

		switch (target->transport_type) {
		case ASD_TRANSPORT_SSP:
			if (target->scsi_cmdset.ident_len !=
			    check_target->scsi_cmdset.ident_len) {

				continue;
			}

			if ((check_target->scsi_cmdset.ident == NULL) ||
			    (target->scsi_cmdset.ident == NULL)) {
				continue;
			}

			if (memcmp(target->scsi_cmdset.ident,
				   check_target->scsi_cmdset.ident,
				   target->scsi_cmdset.ident_len) == 0) {

				return target;
			}
			break;

		case ASD_TRANSPORT_STP:
			if (SAS_ISEQUAL(target->ddb_profile.sas_addr,
					check_target->ddb_profile.sas_addr)) {

				return target;
			}
			break;

		default:
			continue;
		}

	}

	return NULL;
}

struct asd_target *asd_find_multipath(struct asd_softc *asd,
				      struct asd_target *target)
{
	struct asd_port *port;
	unsigned port_id;
	struct asd_target *multipath_target;

	/*
	 * Check to make sure that this same device hasn't been exposed to the
	 * OS on a different port.
	 */
	for (port_id = 0; port_id < asd->hw_profile.max_ports; port_id++) {

		port = asd->port_list[port_id];

		multipath_target = asd_find_target_ident(&port->targets,
							 target);

		if (multipath_target != NULL) {
			return multipath_target;
		}
	}
	return NULL;
}

/*
 * 
 * this routine searches the subtractive phys for the upstream expander address
 *
 */
static int
asd_upstream_expander(struct asd_target *expander,
		      uint8_t * SASAddress, uint8_t * PhyIdentifier)
{
	struct Discover *discover;
	uint8_t phyCount;
	int found;

	found = 0;

	/*
	 * walk through all the phys of this expander, searching for
	 * subtractive phys return the SASAddress and PhyIdentifier for the
	 * first subtractive phy encountered, they should all be the same if
	 * they have anything attached
	 */
	for (phyCount = 0; phyCount < expander->num_phys; phyCount++) {
		/*
		 * this is just a pointer helper
		 */
		discover = &(expander->Phy[phyCount].Result);

		/*
		 * look for phys with edge or fanout devices attached...
		 */
		if ((discover->RoutingAttribute == SUBTRACTIVE) &&
		    ((discover->AttachedDeviceType == EDGE_EXPANDER_DEVICE) ||
		     (discover->AttachedDeviceType ==
		      FANOUT_EXPANDER_DEVICE))) {

			SASCPY(SASAddress, discover->AttachedSASAddress);

			*PhyIdentifier = discover->AttachedPhyIdentifier;

			found = 1;

			break;
		}
	}
	return found;
}

/*
 * this routine determines whether a SAS address is directly attached to
 * an expander
 */
static int
asd_direct_attached(struct asd_target *expander, uint8_t * SASAddress)
{
	int direct;
	uint8_t phyCount;

	direct = 0;

	for (phyCount = 0; phyCount < expander->num_phys; phyCount++) {

		/*
		 * did we find the address attached locally
		 */
		if (*((uint64_t *) SASAddress) ==
		    *((uint64_t *) expander->Phy[phyCount].
		      Result.AttachedSASAddress)) {

			direct = 1;
			break;
		}
	}
	return direct;
}

/*
 * this routine determines whether the SAS address, can be optimized out
 * of the route table.
 *
 * expander:	the expander whose route table we are configuring.
 *
 * discover:	the response to the discovery request for the device attached
 *		to the phy of the expander that we are trying to configure
 *		into "expander's" route table.
 */
static int
asd_qualified_address(struct asd_target *expander,
		      uint8_t PhyIdentifier,
		      struct Discover *discover, uint8_t * DisableRouteEntry)
{
	int qualified;
	uint16_t routeIndex;
	uint8_t *sas_address;

	qualified = 1;

	if (DiscoverAlgorithm != SAS_UNIQUE_LEVEL_DESCENT) {
		return qualified;
	}

	/*
	 * leave in any entries that are direct routing attribute,
	 * assumes that they are slots that will be filled by end
	 * devices, if it is not direct, then filter out any empty
	 * connections, connections that match the expander we are
	 * configuring and connections that are truly direct attached
	 */
	if (!SAS_ISZERO(discover->AttachedSASAddress) &&
	    !SAS_ISEQUAL(discover->AttachedSASAddress,
			 expander->ddb_profile.sas_addr) &&
	    (!asd_direct_attached(expander, discover->AttachedSASAddress))) {

		if (discover->RoutingAttribute == DIRECT) {
			/*
			 * if this is a phy that is has a direct
			 * routing attribute then, have it consume an
			 * entry, it may be filled in at any time
			 */
		} else {
			for (routeIndex = 0; routeIndex <
			     expander->num_route_indexes; routeIndex++) {

				sas_address = ROUTE_ENTRY(expander,
							  PhyIdentifier,
							  routeIndex);

				if (SAS_ISEQUAL(sas_address,
						discover->AttachedSASAddress)) {

					qualified = 0;

					break;
				}
			}
		}
	} else if (SAS_ISZERO(discover->AttachedSASAddress)) {
		/*
		 * if a 0 address, then assume it is an
		 * empty slot that can be filled at any time, 
		 * this keeps things positionally stable for most
		 * reasonable topologies
		 */
		*DisableRouteEntry = DISABLED;
	} else {
		qualified = 0;
	}

	return qualified;
}

void
asd_add_child(struct asd_port *port,
	      struct asd_target *parent, struct asd_target *child)
{
	/*
	 * Check to make sure that this particular target hasn't already been
	 * put in the tree, or that it isn't the top of the tree.
	 */
	if ((port->tree_root == child) || (child->parent != NULL)) {
		return;
	}

	child->parent = parent;

	if (child->parent != NULL) {
		list_add_tail(&child->siblings, &parent->children);
	}
}

#define DUMP_LIST(a)		dump_list(__FUNCTION__, __LINE__, #a, a);
void
dump_list(char *function, unsigned line, char *s, struct list_head *target_list)
{
	struct asd_target *target;

	printk("%s:%d: dumping list %s\n", function, line, s);
	/*
	 * walk the list of expanders, when we find the one that matches, stop
	 */
	list_for_each_entry(target, target_list, all_domain_targets) {
		printk("%s:%d: %llx\n", __FUNCTION__, __LINE__,
		       *((uint64_t *) target->ddb_profile.sas_addr));
	}

}

DISCOVER_RESULTS
asd_configure_device(struct state_machine_context *sm_contextp,
		     struct asd_target *parentExpander,
		     struct Discover *discover,
		     struct list_head *discover_listp,
		     struct list_head *found_listp,
		     struct list_head *old_discover_listp, unsigned conn_rate)
{
	struct asd_target *target;
	COMMAND_SET_TYPE command_set_type;
	DEVICE_PROTOCOL_TYPE device_protocol_type;
	MANAGEMENT_TYPE management_type;
	TRANSPORT_TYPE transport_type;
	struct discover_context *discover_contextp;

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;

	/* LT: ignore pure initiators, virtual phys are ok */
	if (!discover->TargetBits) {
		asd_dprint("Ignoring pure initiators %llx\n",
			   be64_to_cpu(*(u64 *) discover->AttachedSASAddress));
		return DISCOVER_OK;
	}

	switch (conn_rate) {
	case RATE_UNKNOWN:
	case PHY_DISABLED:
	case PHY_FAILED:
		return DISCOVER_FAILED;

	case SPINUP_HOLD_OOB:
		printk("found SPINUP_HOLD\n");
		break;
	case GBPS_1_5:
	case GBPS_3_0:
		/*
		 * Nothing special to do.
		 */
		break;
	}

	/*
	 * This routine is only called for end devices.
	 */
	ASSERT((discover->TargetBits & SMP_TGT_PORT) == 0);

	if (discover->TargetBits & SSP_TGT_PORT) {

		command_set_type = ASD_COMMAND_SET_SCSI;
		device_protocol_type = ASD_DEVICE_PROTOCOL_SCSI;
		transport_type = ASD_TRANSPORT_SSP;
		management_type = ASD_DEVICE_END;

	} else if (discover->TargetBits & STP_TGT_PORT) {
		/* 
		 * We don't know the command set yet (could be ATAPI or ATA)
		 * We won't know until IDENTIFY / PIDENTIFY.
		 */
		command_set_type = ASD_COMMAND_SET_UNKNOWN;
		device_protocol_type = ASD_DEVICE_PROTOCOL_ATA;
		transport_type = ASD_TRANSPORT_STP;
		management_type = ASD_DEVICE_END;

	} else if (discover->TargetBits & SATA_TGT_PORT) {
		/* 
		 * We don't know the command set yet (could be ATAPI or ATA)
		 * We won't know until IDENTIFY / PIDENTIFY.
		 */
		/* 
		 * An "attached SATA host" which is "outside of the
		 * scope of this standard" 
		 * T10/1562-D Rev 5 - 10.4.3.5 pg. 340 - 7/9/2003
		 */
		command_set_type = ASD_COMMAND_SET_UNKNOWN;
		device_protocol_type = ASD_DEVICE_PROTOCOL_ATA;
		transport_type = ASD_TRANSPORT_STP;
		management_type = ASD_DEVICE_END;

	} else {
		command_set_type = ASD_COMMAND_SET_UNKNOWN;
		device_protocol_type = ASD_DEVICE_PROTOCOL_UNKNOWN;
		transport_type = ASD_TRANSPORT_UNKNOWN;
		management_type = ASD_DEVICE_UNKNOWN;
	}

	target = asd_discover_get_target(sm_contextp,
					 discover->AttachedSASAddress,
					 old_discover_listp,
					 found_listp, conn_rate,
					 transport_type);

	if (target == NULL) {

		printk("couldn't allocate target\n");

		return DISCOVER_FAILED;
	}

	target->command_set_type = command_set_type;
	target->device_protocol_type = device_protocol_type;
	target->management_type = management_type;
	target->transport_type = transport_type;

	/*
	 * Add the device to the tree.
	 */
	asd_add_child(discover_contextp->port, parentExpander, target);

	list_add_tail(&target->all_domain_targets, discover_listp);

	return DISCOVER_OK;
}

void
asd_destroy_discover_list(struct asd_softc *asd,
			  struct list_head *discover_list)
{
	struct asd_target *target;
	struct asd_target *tmp_target;

	list_for_each_entry_safe(target, tmp_target, discover_list,
				 all_domain_targets) {

		list_del_init(&target->all_domain_targets);

		asd_free_ddb(asd, target->ddb_profile.conn_handle);

		asd_free_target(asd, target);
	}
}

/* -------------------------------------------------------------------------- */

DISCOVER_RESULTS
asd_issue_discover_request(struct state_machine_context *sm_contextp,
			   struct asd_target *expander, unsigned phyIndex)
{
	DISCOVER_RESULTS results;
	struct discover_context *discover_contextp;

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;

	memset(discover_contextp->SMPRequestFrame, 0,
	       sizeof(struct SMPRequest));

	discover_contextp->SMPRequestFrame->SMPFrameType = SMP_REQUEST_FRAME;
	discover_contextp->SMPRequestFrame->Function = DISCOVER;
	discover_contextp->SMPRequestFrame->Request.Discover.PhyIdentifier =
	    phyIndex;

	/*
	 * get the discover information for each phy
	 */
	results = asd_smp_request(sm_contextp, expander,
				  sizeof(struct SMPRequestPhyInput),
				  sizeof(struct SMPResponseDiscover));

	return results;
}

DISCOVER_RESULTS
asd_issue_discover_request_post(struct state_machine_context * sm_contextp,
				struct asd_target * expander, unsigned phyIndex)
{
	struct Discover *discover;
	struct discover_context *discover_contextp;

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;

	if ((discover_contextp->openStatus != OPEN_ACCEPT) ||
	    (discover_contextp->SMPResponseFrame->FunctionResult !=
	     SMP_FUNCTION_ACCEPTED)) {

		/*
		 * if we had a problem on this  link, then don't 
		 * bother to  do anything else, production code,
		 * should be more robust...
		 */
		// asd_log(ASD_DBG_ERROR, "\n"
		printk("discover error, %02Xh at %0llx\n",
		       discover_contextp->SMPResponseFrame->FunctionResult,
		       *((uint64_t *) expander->ddb_profile.sas_addr));

		return DISCOVER_FAILED;
	}

	discover = &(expander->Phy[phyIndex].Result);

	/*
	 * copy the result into the topology table
	 */
	memcpy((void *)&(expander->Phy[phyIndex]),
	       (void *)&discover_contextp->SMPResponseFrame->Response.Discover,
	       sizeof(struct SMPResponseDiscover));

	return DISCOVER_OK;
}

/* -------------------------------------------------------------------------- */
/*
 * this function gets the report general and discover information for
 * a specific expander.  The discover process should begin at the subtractive
 * boundary and progress downstream.
 *
 * If the dest_sas_address is an expander, the expander structure is returned
 * in retExpander.
 */
DISCOVER_RESULTS
asd_issue_report_general(struct state_machine_context * sm_contextp,
			 uint8_t * dest_sas_address,
			 uint8_t conn_rate,
			 uint8_t attachedDeviceType,
			 struct list_head * old_discover_listp,
			 struct list_head * found_listp,
			 struct asd_target ** retExpander)
{
	struct asd_target *expander;
	DISCOVER_RESULTS results;
	COMMAND_SET_TYPE command_set_type;
	DEVICE_PROTOCOL_TYPE device_protocol_type;
	TRANSPORT_TYPE transport_type;
	MANAGEMENT_TYPE management_type;
	struct discover_context *discover_contextp;

	asd_dprint("Sending REPORT GENERAL\n");

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;

	expander = NULL;

	memset(discover_contextp->SMPRequestFrame, 0,
	       sizeof(struct SMPRequest));

	discover_contextp->SMPRequestFrame->SMPFrameType = SMP_REQUEST_FRAME;
	discover_contextp->SMPRequestFrame->Function = REPORT_GENERAL;

	switch (attachedDeviceType) {
	case EDGE_EXPANDER_DEVICE:
		command_set_type = ASD_COMMAND_SET_SMP;
		device_protocol_type = ASD_DEVICE_PROTOCOL_SMP;
		transport_type = ASD_TRANSPORT_SMP;
		management_type = ASD_DEVICE_EDGE_EXPANDER;
		break;

	case FANOUT_EXPANDER_DEVICE:
		management_type = ASD_DEVICE_FANOUT_EXPANDER;
		device_protocol_type = ASD_DEVICE_PROTOCOL_SMP;
		command_set_type = ASD_COMMAND_SET_SMP;
		transport_type = ASD_TRANSPORT_SMP;
		break;

	default:
		/*
		 * This should never happen.
		 */
		return DISCOVER_FAILED;
	}

	expander = asd_discover_get_target(sm_contextp, dest_sas_address,
					   old_discover_listp, found_listp,
					   conn_rate, transport_type);

	/*
	 * make sure we only do this if the allocation is successful
	 */
	if (expander == NULL) {
		return DISCOVER_FAILED;
	}

	expander->command_set_type = command_set_type;
	expander->device_protocol_type = device_protocol_type;
	expander->transport_type = transport_type;
	expander->management_type = management_type;

	/*
	 * get the report general information for the expander
	 */
	results = asd_smp_request(sm_contextp, expander,
				  sizeof(struct SMPRequestGeneralInput),
				  sizeof(struct SMPResponseReportGeneral));

	*retExpander = expander;

	return results;
}

DISCOVER_RESULTS
asd_issue_report_general_post(struct state_machine_context * sm_contextp,
			      struct asd_target * expander)
{
	uint8_t phyCount;
	struct discover_context *discover_contextp;

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;

	phyCount = 0;

	asd_dprint("----------\n");
	asd_dprint("REPORT GENERAL came back\n");

	/*
	 * the assumptions we made were exceeded, need to bump limits...
	 */
	if ((discover_contextp->openStatus != OPEN_ACCEPT) ||
	    (discover_contextp->SMPResponseFrame->FunctionResult !=
	     SMP_FUNCTION_ACCEPTED)) {
		/*
		 * if we had a problem getting report general for this expander,
		 * something is wrong, can't go any further down this path...
		 * production code, should be more robust...
		 */
		asd_log(ASD_DBG_ERROR, "\n"
			"report general error, open %02Xh result %02Xh at "
			"0x%0llx\n",
			discover_contextp->openStatus,
			discover_contextp->SMPResponseFrame->FunctionResult,
			*((uint64_t *) expander->ddb_profile.sas_addr));

		asd_free_ddb(discover_contextp->asd,
			     expander->ddb_profile.conn_handle);

		asd_free_target(discover_contextp->asd, expander);

		return DISCOVER_FAILED;
	}

	if (discover_contextp->SMPResponseFrame->Response.
	    ReportGeneral.NumberOfPhys > MAXIMUM_EXPANDER_PHYS) {

		asd_log(ASD_DBG_ERROR, "\n"
			"report general error"
			", NumberOfPhys %d exceeded limit %d on %0llx\n",
			discover_contextp->SMPResponseFrame->
			Response.ReportGeneral.NumberOfPhys,
			MAXIMUM_EXPANDER_PHYS,
			*((uint64_t *) expander->ddb_profile.sas_addr));

		asd_free_ddb(discover_contextp->asd,
			     expander->ddb_profile.conn_handle);

		asd_free_target(discover_contextp->asd, expander);

		return DISCOVER_FAILED;
	}

	expander->num_phys = discover_contextp->SMPResponseFrame->
		Response.ReportGeneral.NumberOfPhys;

	expander->num_route_indexes =
		asd_be16toh(discover_contextp->SMPResponseFrame->Response.
			ReportGeneral.ExpanderRouteIndexes);

	expander->configurable_route_table =
		discover_contextp->SMPResponseFrame->Response.ReportGeneral.
		ConfigurableRouteTable;

	if (expander->Phy != NULL) {
		asd_free_mem(expander->Phy);
	}

	expander->Phy = (struct SMPResponseDiscover *)
	    asd_alloc_mem(sizeof(struct SMPResponseDiscover) *
			  expander->num_phys, GFP_KERNEL);

	if (expander->Phy == NULL) {

		printk("unable to allocate memory\n");

		asd_free_ddb(discover_contextp->asd,
			     expander->ddb_profile.conn_handle);

		asd_free_target(discover_contextp->asd, expander);

		return DISCOVER_FAILED;
	}

	if (expander->num_route_indexes != 0) {

		if (expander->RouteTable != NULL) {
			asd_free_mem(expander->RouteTable);
		}

		expander->RouteTable =
		    (uint8_t *) asd_alloc_mem(SAS_ADDR_LEN *
					      expander->num_phys *
					      expander->num_route_indexes,
					      GFP_KERNEL);

		if (expander->route_indexes != NULL) {
			asd_free_mem(expander->route_indexes);
		}

		expander->route_indexes =
		    asd_alloc_mem(expander->num_route_indexes
				  * sizeof(uint16_t), GFP_KERNEL);

		memset(expander->route_indexes, 0,
		       expander->num_route_indexes * sizeof(uint16_t));
	}

	return DISCOVER_OK;
}

/* -------------------------------------------------------------------------- */

DISCOVER_RESULTS
asd_issue_report_manufacturer_info(struct state_machine_context *
				   sm_contextp, struct asd_target * expander)
{
	DISCOVER_RESULTS results;
	struct discover_context *discover_contextp;

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;

	memset(discover_contextp->SMPRequestFrame, 0,
	       sizeof(struct SMPRequest));

	discover_contextp->SMPRequestFrame->SMPFrameType = SMP_REQUEST_FRAME;
	discover_contextp->SMPRequestFrame->Function =
	    REPORT_MANUFACTURER_INFORMATION;

	results = asd_smp_request(sm_contextp, expander,
				  sizeof(struct SMPRequestGeneralInput),
				  sizeof(struct
					 SMPResponseReportManufacturerInfo));

	return results;
}

DISCOVER_RESULTS
asd_issue_report_manufacturer_info_post(struct state_machine_context *
					sm_contextp,
					struct asd_target * expander)
{
	struct discover_context *discover_contextp;

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;

	/*
	 * the assumptions we made were exceeded, need to bump limits...
	 */
	if ((discover_contextp->openStatus != OPEN_ACCEPT) ||
	    (discover_contextp->SMPResponseFrame->FunctionResult !=
	     SMP_FUNCTION_ACCEPTED)) {
		/*
		 * if we had a problem getting report general for this expander,
		 * something is wrong, can't go any further down this path...
		 * production code, should be more robust...
		 */
		asd_log(ASD_DBG_ERROR, "\n"
			"get manufacturer information error, "
			"open %02Xh result %02Xh at "
			"0x%0llx\n",
			discover_contextp->openStatus,
			discover_contextp->SMPResponseFrame->FunctionResult,
			*((uint64_t *) expander->ddb_profile.sas_addr));

		asd_free_ddb(discover_contextp->asd,
			     expander->ddb_profile.conn_handle);

		asd_free_target(discover_contextp->asd, expander);

		return DISCOVER_FAILED;
	}

	memcpy(&expander->smp_cmdset.manufacturer_info,
	       &discover_contextp->SMPResponseFrame->
	       Response.ReportManufacturerInfo,
	       sizeof(struct SMPResponseReportManufacturerInfo));

#if 0
	printk("%8.8s|%16.16s|%4.4s\n",
	       expander->smp_cmdset.manufacturer_info.VendorIdentification,
	       expander->smp_cmdset.manufacturer_info.ProductIdentification,
	       expander->smp_cmdset.manufacturer_info.ProductRevisionLevel);
#endif

	return DISCOVER_OK;
}

/* -------------------------------------------------------------------------- */

DISCOVER_RESULTS
asd_issue_route_config(struct state_machine_context * sm_contextp,
		       struct asd_target * expander,
		       unsigned phyIndex,
		       uint8_t disableRouteEntry, uint8_t * attachedSASAddress)
{
	uint16_t index;
	DISCOVER_RESULTS results;
	struct discover_context *discover_contextp;

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;

	index = expander->route_indexes[phyIndex];

	memset(discover_contextp->SMPRequestFrame, 0,
	       sizeof(struct SMPRequest));

	discover_contextp->SMPRequestFrame->SMPFrameType = SMP_REQUEST_FRAME;
	discover_contextp->SMPRequestFrame->Function =
	    CONFIGURE_ROUTE_INFORMATION;

	discover_contextp->SMPRequestFrame->Request.
	    ConfigureRouteInformation.ExpanderRouteIndex = asd_htobe16(index);

	discover_contextp->SMPRequestFrame->Request.
	    ConfigureRouteInformation.PhyIdentifier = phyIndex;

	discover_contextp->SMPRequestFrame->Request.
	    ConfigureRouteInformation.Configure.DisableRouteEntry =
	    disableRouteEntry;

	SASCPY(discover_contextp->SMPRequestFrame->Request.
	       ConfigureRouteInformation.Configure.RoutedSASAddress,
	       attachedSASAddress);

	/*
	 * configure the route indexes for the
	 * expander with the attached address
	 * information
	 */
	results = asd_smp_request(sm_contextp, expander,
				  sizeof(struct
					 SMPRequestConfigureRouteInformation),
				  sizeof(struct
					 SMPResponseConfigureRouteInformation));

	return results;
}

DISCOVER_RESULTS
asd_issue_route_config_post(struct state_machine_context * sm_contextp,
			    struct asd_target * expander, unsigned phyIndex)
{
	struct discover_context *discover_contextp;

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;

	/*
	 * the assumptions we made were exceeded, need to bump limits...
	 */
	if ((discover_contextp->openStatus != OPEN_ACCEPT) ||
	    (discover_contextp->SMPResponseFrame->FunctionResult !=
	     SMP_FUNCTION_ACCEPTED)) {
		/*
		 * if we had a problem getting report general for this expander,
		 * something is wrong, can't go any further down this path...
		 * production code, should be more robust...
		 */
		asd_log(ASD_DBG_ERROR, "\n"
			"route config error, open %02Xh result %02Xh at "
			"0x%0llx\n",
			discover_contextp->openStatus,
			discover_contextp->SMPResponseFrame->FunctionResult,
			*((uint64_t *) expander->ddb_profile.sas_addr));

		return DISCOVER_FAILED;
	}

	return DISCOVER_OK;
}

/* -------------------------------------------------------------------------- */

DISCOVER_RESULTS
asd_issue_report_phy_sata(struct state_machine_context * sm_contextp,
			  struct asd_target * expander, unsigned phyIndex)
{
	DISCOVER_RESULTS results;
	struct discover_context *discover_contextp;

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;

	memset(discover_contextp->SMPRequestFrame, 0,
	       sizeof(struct SMPRequest));

	discover_contextp->SMPRequestFrame->SMPFrameType = SMP_REQUEST_FRAME;
	discover_contextp->SMPRequestFrame->Function = REPORT_PHY_SATA;

	discover_contextp->SMPRequestFrame->Request.
	    ReportPhySATA.PhyIdentifier = phyIndex;

	results = asd_smp_request(sm_contextp, expander,
				  sizeof(struct SMPRequestPhyInput),
				  sizeof(struct SMPResponseReportPhySATA));

	return results;
}

DISCOVER_RESULTS
asd_issue_report_phy_sata_post(struct state_machine_context * sm_contextp,
			       struct asd_target * expander)
{
	struct discover_context *discover_contextp;

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;

	/*
	 * the assumptions we made were exceeded, need to bump limits...
	 */
	if ((discover_contextp->openStatus != OPEN_ACCEPT) ||
	    (discover_contextp->SMPResponseFrame->FunctionResult !=
	     SMP_FUNCTION_ACCEPTED)) {
		/*
		 * if we had a problem getting report general for this expander,
		 * something is wrong, can't go any further down this path...
		 * production code, should be more robust...
		 */
		asd_log(ASD_DBG_ERROR, "\n"
			"report phy SATA error, open %02Xh result %02Xh at "
			"0x%0llx\n",
			discover_contextp->openStatus,
			discover_contextp->SMPResponseFrame->FunctionResult,
			*((uint64_t *) expander->ddb_profile.sas_addr));

		return DISCOVER_FAILED;
	}

	return DISCOVER_OK;
}

COMMAND_SET_TYPE asd_sata_get_type(struct adp_dev_to_host_fis * fis)
{
	if ((fis->sector_count == 1) && (fis->lba0 == 1) &&
	    (fis->lba1 == 0x14) && (fis->lba2 == 0xeb)) {

		return ASD_COMMAND_SET_ATAPI;
	}

	return ASD_COMMAND_SET_ATA;
}

void
asd_init_sata_direct_attached(struct state_machine_context *sm_contextp,
			      struct asd_target *target)
{
	struct asd_phy *phy;
	struct adp_dev_to_host_fis *fis;
	struct discover_context *discover_contextp;

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;

	phy = list_entry(target->src_port->phys_attached.next,
			 struct asd_phy, links);

	fis = (struct adp_dev_to_host_fis *)
	    &phy->bytes_dmaed_rcvd.initial_fis_rcvd.fis[0];

	target->command_set_type = asd_sata_get_type(fis);

	memcpy((void *)&target->device_protocol.
	       ata_device_protocol.initial_fis[0],
	       (void *)fis, sizeof(struct adp_dev_to_host_fis));

	target->ddb_profile.sata_status = fis->status;

	asd_hwi_update_sata(discover_contextp->asd, target);
}

/* -------------------------------------------------------------------------- */

DISCOVER_RESULTS
asd_issue_phy_control(struct state_machine_context *sm_contextp,
		      struct asd_target *expander,
		      unsigned phyIndex, unsigned operation)
{
	DISCOVER_RESULTS results;
	struct discover_context *discover_contextp;

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;

	memset(discover_contextp->SMPRequestFrame, 0,
	       sizeof(struct SMPRequest));

	discover_contextp->SMPRequestFrame->SMPFrameType = SMP_REQUEST_FRAME;

	discover_contextp->SMPRequestFrame->Function = PHY_CONTROL;

	discover_contextp->SMPRequestFrame->Request.PhyControl.
	    PhyIdentifier = phyIndex;

	discover_contextp->SMPRequestFrame->Request.PhyControl.
	    PhyOperation = operation;

	/*
	 * get the discover information for each phy
	 */
	results = asd_smp_request(sm_contextp, expander,
				  sizeof(struct SMPRequestPhyControl),
				  sizeof(struct SMPResponsePhyControl));

	return results;
}

DISCOVER_RESULTS
asd_issue_phy_control_post(struct state_machine_context * sm_contextp,
			   struct asd_target * expander)
{
	struct discover_context *discover_contextp;

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;

	if ((discover_contextp->openStatus != OPEN_ACCEPT) ||
	    (discover_contextp->SMPResponseFrame->FunctionResult !=
	     SMP_FUNCTION_ACCEPTED)) {
		/*
		 * if we had a problem on this  link, then don't 
		 * bother to  do anything else, production code,
		 * should be more robust...
		 */
		asd_log(ASD_DBG_ERROR, "\n"
			"phy control error, %02Xh at %0llx\n",
			discover_contextp->SMPResponseFrame->FunctionResult,
			*((uint64_t *) expander->ddb_profile.sas_addr));

		return DISCOVER_FAILED;
	}

	return DISCOVER_OK;
}

/* -------------------------------------------------------------------------- */

/*
 * Standard inquiry command
 */
static uint8_t inquiry_cmd[] = {
	INQUIRY, 0x00, 0x00, 0x00, 0x00, 0x00
};

/*
 * Device ID
 */
static uint8_t inquiry_dev_id_cmd[] = {
	INQUIRY, 0x01, 0x83, 0x00, 0x00, 0x00
};

/*
 * Unit Serial Number
 */
static uint8_t inquiry_USN_cmd[] = {
	INQUIRY, 0x01, 0x80, 0x00, 0x00, 0x00
};

#define MAX_INQUIRY_LEN(a)	(((a) > 255) ? 255 : (a))

DISCOVER_RESULTS
asd_issue_inquiry(struct state_machine_context *sm_contextp,
		  struct asd_target *target,
		  uint8_t * command, unsigned command_len)
{
	struct discover_context *discover_contextp;
	unsigned xfer_len;
	DISCOVER_RESULTS results;
	uint8_t icmd[SCB_EMBEDDED_CDB_SIZE];

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;

	xfer_len = MAX_INQUIRY_LEN(discover_contextp->sas_info_len);

	memcpy(icmd, command, command_len);

	*((uint16_t *) & icmd[3]) = asd_htobe16(xfer_len);

	asd_hwi_hash(target->ddb_profile.sas_addr, target->ddb_profile.hashed_sas_addr);

	asd_dprint("issuing INQUIRY to %llx\n",
		be64_to_cpu(*((uint64_t *)target->ddb_profile.sas_addr)));
	asd_log(ASD_DBG_INFO, "hash addr is 0x%x\n", *(uint32_t *)target->ddb_profile.hashed_sas_addr);

	/* IBM-ESXS/VSC7160 rev 0.07 barfs unless we sleep here a bit.
	 */
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(HZ / 10);

	results = asd_ssp_request(sm_contextp, target, icmd, command_len,
				  discover_contextp->SASInfoBusAddr, xfer_len,
				  DATA_DIR_INBOUND);

	return results;
}

/* -------------------------------------------------------------------------- */

uint8_t report_luns_cmd[] = {
	REPORT_LUNS, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

DISCOVER_RESULTS
asd_issue_report_luns(struct state_machine_context * sm_contextp,
		      struct asd_target * target)
{
	struct discover_context *discover_contextp;
	DISCOVER_RESULTS results;
	uint8_t rl_cmd[SCB_EMBEDDED_CDB_SIZE];

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;

	memcpy(rl_cmd, report_luns_cmd, sizeof(report_luns_cmd));

	*((uint32_t *) & rl_cmd[6]) =
	    asd_htobe32(discover_contextp->sas_info_len);

	results = asd_ssp_request(sm_contextp, target,
				  rl_cmd, sizeof(report_luns_cmd),
				  discover_contextp->SASInfoBusAddr,
				  discover_contextp->sas_info_len,
				  DATA_DIR_INBOUND);

	return results;
}

/* -------------------------------------------------------------------------- */

#define CONTROL_MODE_PAGE_SIZE		0x08

uint8_t mode_sense_port_control_cmd[] = {
	MODE_SENSE, 0x00, 0x19, 0x00, 0x00, 0x00
};

DISCOVER_RESULTS
asd_issue_get_port_control(struct state_machine_context * sm_contextp,
			   struct asd_target * target)
{
	struct discover_context *discover_contextp;
	DISCOVER_RESULTS results;
	uint8_t pc_cmd[SCB_EMBEDDED_CDB_SIZE];

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;

	memcpy(pc_cmd, mode_sense_port_control_cmd,
	       sizeof(mode_sense_port_control_cmd));

	pc_cmd[4] = CONTROL_MODE_PAGE_SIZE;

	results = asd_ssp_request(sm_contextp, target,
				  pc_cmd, sizeof(mode_sense_port_control_cmd),
				  discover_contextp->SASInfoBusAddr,
				  CONTROL_MODE_PAGE_SIZE, DATA_DIR_INBOUND);

	return results;
}

/* -------------------------------------------------------------------------- */

struct asd_InitSMP_SM_Context {
	struct list_head *discover_listp;
	struct asd_target *currentTarget;
};

ASD_DISCOVERY_STATES
asd_state_init_smp_start(struct state_machine_context *sm_contextp)
{
	struct state_information *state_infop;
	struct asd_InitSMP_SM_Context *ctx;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	list_for_each_entry(ctx->currentTarget, ctx->discover_listp,
			    all_domain_targets) {

		asd_log(ASD_DBG_INFO, "%s:%d - %llx %d\n",
			__FUNCTION__, __LINE__,
			*((uint64_t *) ctx->currentTarget->
			  ddb_profile.sas_addr),
			ctx->currentTarget->ddb_profile.conn_rate);

		if (ctx->currentTarget->command_set_type == ASD_COMMAND_SET_SMP) {

			return ASD_STATE_INIT_SMP_REPORT_MANUFACTURER_INFO;
		}
	}
	return ASD_STATE_INIT_SMP_FINISHED;
}

DISCOVER_RESULTS
asd_state_init_smp_report_manufacturer_info(struct state_machine_context *
					    sm_contextp)
{
	struct state_information *state_infop;
	struct asd_InitSMP_SM_Context *ctx;
	DISCOVER_RESULTS results;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	results = asd_issue_report_manufacturer_info(sm_contextp,
						     ctx->currentTarget);

	return results;
}

ASD_DISCOVERY_STATES
asd_state_init_smp_report_manufacturer_info_post(struct
						 state_machine_context *
						 sm_contextp)
{
	struct state_information *state_infop;
	struct asd_InitSMP_SM_Context *ctx;
	struct discover_context *discover_contextp;
	DISCOVER_RESULTS results;

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;
	GET_STATE_CONTEXT(sm_contextp, ctx);

	results = asd_issue_report_manufacturer_info_post(sm_contextp,
							  ctx->currentTarget);

	if (results != DISCOVER_OK) {
		printk("could not get manufactur_info from device\n");
	}

	while (ctx->currentTarget->all_domain_targets.next
	       != ctx->discover_listp) {

		ctx->currentTarget =
		    list_entry(ctx->currentTarget->all_domain_targets.next,
			       struct asd_target, all_domain_targets);

		if (ctx->currentTarget->command_set_type == ASD_COMMAND_SET_SMP) {

			return ASD_STATE_INIT_SMP_REPORT_MANUFACTURER_INFO;
		}
	}

	return ASD_STATE_INIT_SMP_FINISHED;
}

DISCOVER_RESULTS
asd_InitSMP_SM_Initialize(struct state_machine_context * sm_contextp,
			  void *init_args)
{
	struct state_information *state_infop;
	struct asd_InitSMP_SM_Context *ctx;
	struct asd_InitSMP_SM_Arguments *args;

	NEW_CONTEXT(ctx);

	args = (struct asd_InitSMP_SM_Arguments *)init_args;

	ctx->discover_listp = args->discover_listp;

	return DISCOVER_CONTINUE;
}

DISCOVER_RESULTS
asd_InitSMP_SM_StateMachine(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_InitSMP_SM_Context *ctx;
	DISCOVER_RESULTS results;
	ASD_DISCOVERY_STATES current_state;
	ASD_DISCOVERY_STATES new_state;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	current_state = GET_CURRENT_STATE();

	switch (current_state) {
	case ASD_STATE_INIT_SMP_START:
		new_state = asd_state_init_smp_start(sm_contextp);
		break;

	case ASD_STATE_INIT_SMP_REPORT_MANUFACTURER_INFO:
		new_state =
		    asd_state_init_smp_report_manufacturer_info_post
		    (sm_contextp);
		break;

	default:
		new_state = ASD_STATE_INIT_SMP_FAILED;
		break;
	}

	SM_new_state(sm_contextp, new_state);

	switch (new_state) {

	case ASD_STATE_INIT_SMP_REPORT_MANUFACTURER_INFO:
		results =
		    asd_state_init_smp_report_manufacturer_info(sm_contextp);
		break;

	case ASD_STATE_INIT_SMP_FINISHED:
		results = DISCOVER_FINISHED;
		break;

	default:
		results = DISCOVER_FAILED;
		break;
	}

	return results;
}

void
asd_InitSMP_SM_Finish(struct state_machine_context *sm_contextp,
		      DISCOVER_RESULTS results)
{
	struct state_information *state_infop;
	struct asd_InitSMP_SM_Arguments *ctx;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	RETURN_STACK(results);
}

void asd_InitSMP_SM_Abort(struct state_machine_context *sm_contextp)
{
	struct state_information *state_infop;
	struct asd_InitSMP_SM_Arguments *ctx;
	ASD_DISCOVERY_STATES current_state;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	current_state = GET_CURRENT_STATE();

#if 0
	switch (current_state) {
	}
#endif
}

/* -------------------------------------------------------------------------- */

struct asd_InitSAS_SM_Context {

	struct list_head *discover_listp;
	struct asd_target *currentTarget;
	unsigned phyIndex;
};

ASD_DISCOVERY_STATES
asd_state_init_sas_start(struct state_machine_context *sm_contextp)
{
	struct state_information *state_infop;
	struct asd_InitSAS_SM_Context *ctx;
	struct discover_context *discover_contextp;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;

	discover_contextp->retry_count=0;

	list_for_each_entry(ctx->currentTarget, ctx->discover_listp,
			    all_domain_targets) {

		asd_log(ASD_DBG_INFO, "%s:%d - %llx %d\n",
			__FUNCTION__, __LINE__,
			*((uint64_t *) ctx->currentTarget->
			  ddb_profile.sas_addr),
			ctx->currentTarget->ddb_profile.conn_rate);

		if (ctx->currentTarget->command_set_type ==
		    ASD_COMMAND_SET_SCSI) {

			return ASD_STATE_INIT_SAS_INQUIRY;
		}
	}
	return ASD_STATE_INIT_SAS_FINISHED;
}

DISCOVER_RESULTS
asd_state_init_sas_inquiry(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_InitSAS_SM_Context *ctx;
	DISCOVER_RESULTS results;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	results = asd_issue_inquiry(sm_contextp, ctx->currentTarget,
				    inquiry_cmd, sizeof(inquiry_cmd));

	return results;
}

ASD_DISCOVERY_STATES
asd_state_init_sas_inquiry_post(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_InitSAS_SM_Context *ctx;
	unsigned inquiry_response_len;
	struct discover_context *discover_contextp;

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;
	GET_STATE_CONTEXT(sm_contextp, ctx);

	if (discover_contextp->openStatus != OPEN_ACCEPT) {
#ifdef ASD_DEBUG
		asd_log(ASD_DBG_INFO, "Inquiry failed retry_count=0x%x\n",discover_contextp->retry_count);
#endif
		if(discover_contextp->retry_count<MAX_SAS_INQUIRY_RETRY)
		{
			discover_contextp->retry_count++;
			return ASD_STATE_INIT_SAS_INQUIRY;
		}
		else
		{
			ctx->currentTarget->command_set_type = ASD_COMMAND_SET_BAD;
		}

	} else {

		inquiry_response_len =
		    MAX_INQUIRY_LEN(discover_contextp->sas_info_len) -
		    discover_contextp->resid_len;

		ctx->currentTarget->scsi_cmdset.inquiry = (uint8_t *)
		    asd_alloc_mem(inquiry_response_len, GFP_KERNEL);

		if (ctx->currentTarget->scsi_cmdset.inquiry != NULL) {

			memcpy(ctx->currentTarget->scsi_cmdset.inquiry,
			       &discover_contextp->SASInfoFrame[0],
			       inquiry_response_len);

#if 0
			printk("%8.8s | ",
			       &ctx->currentTarget->scsi_cmdset.inquiry[8]);
			printk("%16.16s | ",
			       &ctx->currentTarget->scsi_cmdset.inquiry[16]);
			printk("%4.4s\n",
			       &ctx->currentTarget->scsi_cmdset.inquiry[32]);
#endif
		}
	}
	discover_contextp->retry_count=0;

	return ASD_STATE_INIT_SAS_GET_DEVICE_ID;
}

DISCOVER_RESULTS
asd_state_init_get_device_id(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_InitSAS_SM_Context *ctx;
	DISCOVER_RESULTS results;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	results = asd_issue_inquiry(sm_contextp, ctx->currentTarget,
				    inquiry_dev_id_cmd,
				    sizeof(inquiry_dev_id_cmd));

	return results;
}

#define ASSOCIATION_SCSI_LOGICAL_UNIT		0
#define ASSOCIATION_SCSI_TARGET_PORT		1
#define ASSOCIATION_SCSI_TARGET_DEVICE		2

#define PIV_VALID				0x80
#define VPD_SAS_PROTOCOL			6

ASD_DISCOVERY_STATES
asd_state_init_get_device_id_post(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_InitSAS_SM_Context *ctx;
	uint8_t *vpd_page_header;
	uint8_t *vpd_pagep;
	unsigned page_length;
	unsigned identifier_type;
	unsigned offset;
	unsigned association;
	unsigned code_set;
	unsigned protocol;
	unsigned length;
	struct discover_context *discover_contextp;
	unsigned inquiry_response_len;
	unsigned piv_valid;
#if 0
	unsigned i;
#endif

	GET_STATE_CONTEXT(sm_contextp, ctx);

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;
	if (discover_contextp->openStatus != OPEN_ACCEPT) {
		return ASD_STATE_INIT_SAS_GET_SERIAL_NUMBER;
	}

	vpd_page_header = &discover_contextp->SASInfoFrame[0];

	inquiry_response_len =
	    MAX_INQUIRY_LEN(discover_contextp->sas_info_len) -
	    discover_contextp->resid_len;

	page_length = MIN(asd_be16toh(*((uint16_t *) & vpd_page_header[2])) + 4,
			  inquiry_response_len);

#if 0
	for (i = 0; i < page_length; i++) {
		printk("%02x ", vpd_page_header[i]);

		if (((i + 1) % 16) == 0) {
			printk("\n");
		}
	}
#endif

	vpd_pagep = &vpd_page_header[4];

	for (offset = vpd_pagep - vpd_page_header; offset < page_length;
	     vpd_pagep = vpd_pagep + length,
	     offset = vpd_pagep - vpd_page_header) {

		length = vpd_pagep[3] + 4;
		if ((offset + length) > page_length) {
			break;
		}

		identifier_type = vpd_pagep[1] & 0xf;

#if 0
		switch (identifier_type & 0xf) {
		case IDENTIFIER_TYPE_VENDOR_SPECIFIC:
			printk("IDENTIFIER_TYPE_VENDOR_SPECIFIC\n");
			break;
		case IDENTIFIER_TYPE_T10:
			printk("IDENTIFIER_TYPE_T10\n");
			break;
		case IDENTIFIER_TYPE_EIU_64:
			printk("IDENTIFIER_TYPE_EIU_64\n");
			break;
		case IDENTIFIER_TYPE_NAA:
			printk("IDENTIFIER_TYPE_NAA\n");
			break;
		case IDENTIFIER_TYPE_RELATIVE_TARGET_PORT:
			printk("IDENTIFIER_TYPE_RELATIVE_TARGET_PORT\n");
			break;
		case IDENTIFIER_TYPE_TARGET_PORT_GROUP:
			printk("IDENTIFIER_TYPE_TARGET_PORT_GROUP\n");
			break;
		case IDENTIFIER_TYPE_LOGICAL_UNIT_GROUP:
			printk("IDENTIFIER_TYPE_LOGICAL_UNIT_GROUP\n");
			break;
		case IDENTIFIER_TYPE_MD5_LOGICAL_UNIT:
			printk("IDENTIFIER_TYPE_MD5_LOGICAL_UNIT\n");
			break;
		case IDENTIFIER_TYPE_SCSI_NAME_STRING:
			printk("IDENTIFIER_TYPE_SCSI_NAME_STRING\n");
			break;
		default:
			printk("UNKNOWN\n");
			break;
		}
#endif
		if ((identifier_type & 0xf) != IDENTIFIER_TYPE_NAA) {

			continue;
		}

		protocol = (vpd_pagep[0] & 0xf0) >> 4;
		piv_valid = vpd_pagep[1] & PIV_VALID;
		code_set = vpd_pagep[0] & 0xf;
		association = (vpd_pagep[1] & 0x30) >> 4;

#if 0
		printk("Association %d | ", association);
		printk("Code Set %d | ", code_set);
		printk("PIV Valid %d | ", piv_valid);
		printk("Protocol %d\n", protocol);
#endif

		ctx->currentTarget->scsi_cmdset.ident_len = length - 4;

		switch (association) {
		case ASSOCIATION_SCSI_LOGICAL_UNIT:
			// seagate
			if (vpd_pagep[1] & PIV_VALID) {
				continue;
			}
			break;

		case ASSOCIATION_SCSI_TARGET_PORT:
		case ASSOCIATION_SCSI_TARGET_DEVICE:
			// fujitsu
			if (((piv_valid & PIV_VALID) != PIV_VALID) ||
			    (protocol != VPD_SAS_PROTOCOL)) {
				continue;
			}
			break;
		default:
			continue;

		}

		ctx->currentTarget->scsi_cmdset.ident = (uint8_t *)
		    asd_alloc_mem(ctx->currentTarget->scsi_cmdset.ident_len,
				  GFP_KERNEL);

		if (ctx->currentTarget->scsi_cmdset.ident != NULL) {

			memcpy(ctx->currentTarget->scsi_cmdset.ident,
			       &vpd_pagep[4],
			       ctx->currentTarget->scsi_cmdset.ident_len);
			break;
		}
	}

	while (ctx->currentTarget->all_domain_targets.next
	       != ctx->discover_listp) {

		ctx->currentTarget =
		    list_entry(ctx->currentTarget->all_domain_targets.next,
			       struct asd_target, all_domain_targets);

		if (ctx->currentTarget->command_set_type ==
		    ASD_COMMAND_SET_SCSI) {

			return ASD_STATE_INIT_SAS_INQUIRY;
		}
	}

	list_for_each_entry(ctx->currentTarget, ctx->discover_listp,
			    all_domain_targets) {

		asd_log(ASD_DBG_INFO, "%s:%d - %llx %d\n",
			__FUNCTION__, __LINE__,
			*((uint64_t *) ctx->currentTarget->
			  ddb_profile.sas_addr),
			ctx->currentTarget->ddb_profile.conn_rate);

		if (ctx->currentTarget->command_set_type ==
		    ASD_COMMAND_SET_SCSI) {
			return ASD_STATE_INIT_SAS_ISSUE_REPORT_LUNS;
		}
	}
	return ASD_STATE_INIT_SAS_FINISHED;
}

static const char *asd_vpd_warning = "\
Warning: INQUIRY VPD Page 80h/83h commmand can not finish. This device may\
not support INQUIRY VPD Page 83h commmand which is required by the SAS \
spec. In addition, this device may not support the optional INQUIRY VPD \
Page 80h. Devices that do not support either of these commands may \
experience file corruption and data loss.  Please upgrade your firmware.";

DISCOVER_RESULTS
asd_state_init_get_serial_number(struct state_machine_context *sm_contextp)
{
	struct state_information *state_infop;
	struct asd_InitSAS_SM_Context *ctx;
	DISCOVER_RESULTS results;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	results = asd_issue_inquiry(sm_contextp, ctx->currentTarget,
				    inquiry_USN_cmd, sizeof(inquiry_USN_cmd));

	return results;
}

ASD_DISCOVERY_STATES
asd_state_init_get_serial_number_post(struct state_machine_context *
				      sm_contextp)
{
	struct state_information *state_infop;
	struct asd_InitSAS_SM_Context *ctx;
	uint8_t *vpd_pagep;
	unsigned page_length;
	unsigned i;
	unsigned serial_number_len;
	struct discover_context *discover_contextp;
	GET_STATE_CONTEXT(sm_contextp, ctx);

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;

	if (discover_contextp->openStatus != OPEN_ACCEPT) {

		printk("%s\n", asd_vpd_warning);

#ifdef NO_VPD_WORKAROUND
		//TODO: free the ident field when the target is destroyed

		ctx->currentTarget->scsi_cmdset.ident_len = sizeof(uint64_t);

		ctx->currentTarget->scsi_cmdset.ident = (uint8_t *)
		    asd_alloc_mem(sizeof(uint64_t), GFP_KERNEL);

		if (ctx->currentTarget->scsi_cmdset.ident != NULL) {

			*(uint64_t *) ctx->currentTarget->scsi_cmdset.ident =
			    asd_be64toh(*((uint64_t *) ctx->currentTarget->
					  ddb_profile.sas_addr)) & ~0xfL;

			printk("Setting ident to %llx\n",
			       *((uint64_t *) ctx->currentTarget->
				 scsi_cmdset.ident));
		}
#endif
	} else {
		vpd_pagep = &discover_contextp->SASInfoFrame[0];

		page_length = vpd_pagep[3];

		for (i = 0; i < (page_length + 3); i++) {
			printk("%02x ", vpd_pagep[i]);

			if (((i + 1) % 16) == 0) {
				printk("\n");
			}
		}

		serial_number_len = discover_contextp->SASInfoFrame[3];

		if (serial_number_len > discover_contextp->sas_info_len) {
			serial_number_len = discover_contextp->sas_info_len;
		}

		ctx->currentTarget->scsi_cmdset.ident_len = serial_number_len;

		ctx->currentTarget->scsi_cmdset.ident = (uint8_t *)
		    asd_alloc_mem(ctx->currentTarget->scsi_cmdset.ident_len,
				  GFP_KERNEL);

		if (ctx->currentTarget->scsi_cmdset.ident != NULL) {
			memcpy(ctx->currentTarget->scsi_cmdset.ident,
			       &discover_contextp->SASInfoFrame[4],
			       ctx->currentTarget->scsi_cmdset.ident_len);
		}

		printk("serial number is %s\n",
		       ctx->currentTarget->scsi_cmdset.ident);
	}

	while (ctx->currentTarget->all_domain_targets.next
	       != ctx->discover_listp) {

		ctx->currentTarget =
		    list_entry(ctx->currentTarget->all_domain_targets.next,
			       struct asd_target, all_domain_targets);

		if (ctx->currentTarget->command_set_type ==
		    ASD_COMMAND_SET_SCSI) {

			return ASD_STATE_INIT_SAS_INQUIRY;
		}
	}

	list_for_each_entry(ctx->currentTarget, ctx->discover_listp,
			    all_domain_targets) {

		asd_log(ASD_DBG_INFO, "%s:%d - %llx %d\n",
			__FUNCTION__, __LINE__,
			*((uint64_t *) ctx->currentTarget->
			  ddb_profile.sas_addr),
			ctx->currentTarget->ddb_profile.conn_rate);

		if (ctx->currentTarget->command_set_type ==
		    ASD_COMMAND_SET_SCSI) {

			return ASD_STATE_INIT_SAS_ISSUE_REPORT_LUNS;
		}
	}

	return ASD_STATE_INIT_SAS_FINISHED;
}

DISCOVER_RESULTS
asd_state_init_sas_issue_report_luns(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_InitSAS_SM_Context *ctx;
	DISCOVER_RESULTS results;

	GET_STATE_CONTEXT(sm_contextp, ctx);
	if(ctx->currentTarget->command_set_type == ASD_COMMAND_SET_SCSI)
	{
		u8 *inquiry;
		inquiry = ctx->currentTarget->scsi_cmdset.inquiry;
#ifdef ASD_DEBUG
	asd_log(ASD_DBG_INFO, "ctx->currentTarget->ddb_profile.sas_addr=%llx, ctx->currentTarget->transport_type=0x%x, ctx->currentTarget->command_set_type 0x%x\n",
		*((uint64_t *) ctx->currentTarget->ddb_profile.sas_addr),
		ctx->currentTarget->transport_type,
		ctx->currentTarget->command_set_type);
#endif

		if (inquiry != NULL) 
		{
#ifdef ASD_DEBUG
			asd_log(ASD_DBG_INFO, "inquiry[0]=0x%x\n", inquiry[0]);
#endif
			if( ((inquiry[0] & 0xe0) == 0x60)||((inquiry[0] & 0x1f) == TYPE_ENCLOSURE) )
			{
				struct discover_context *discover_contextp;
				discover_contextp = (struct discover_context *) sm_contextp->state_handle;
				discover_contextp->openStatus = OPEN_REJECT_BAD_DESTINATION;
				sm_contextp->wakeup_state_machine(sm_contextp);

				return DISCOVER_OK;
			}
		}
	}
	results = asd_issue_report_luns(sm_contextp, ctx->currentTarget);

	return results;
}

ASD_DISCOVERY_STATES
asd_state_init_sas_issue_report_lun_post(struct state_machine_context *
					 sm_contextp)
{
	struct state_information *state_infop;
	struct asd_InitSAS_SM_Context *ctx;
	unsigned i;
	unsigned num_luns;
	uint64_t *ReportLunsFrame;
	struct discover_context *discover_contextp;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;
	if (discover_contextp->openStatus != OPEN_ACCEPT) {

		ctx->currentTarget->scsi_cmdset.num_luns = 1;

		ctx->currentTarget->scsi_cmdset.luns = (uint64_t *)
		    asd_alloc_mem(sizeof(uint64_t), GFP_KERNEL);

		memset(&ctx->currentTarget->scsi_cmdset.luns[0], 0,
		       sizeof(uint64_t));

		asd_dprint(KERN_NOTICE "adp94xx: REPORT LUNS failed\n");

		/*
		 * Report LUNS failed.
		 */
	} else {
		ReportLunsFrame = (uint64_t *) discover_contextp->SASInfoFrame;

		num_luns = asd_be32toh(ReportLunsFrame[0]) / sizeof(uint64_t);

		if (num_luns > ASD_MAX_LUNS) {
			num_luns = ASD_MAX_LUNS;
		}
		// printk("%s: found %d luns\n", __FUNCTION__, num_luns);

		ctx->currentTarget->scsi_cmdset.num_luns = num_luns;

		ctx->currentTarget->scsi_cmdset.luns = (uint64_t *)
		    asd_alloc_mem(sizeof(uint64_t) * num_luns, GFP_KERNEL);

		if (ctx->currentTarget->scsi_cmdset.luns != NULL) {
			for (i = 0; i < num_luns; i++) {
				/*
				 * The SASInfoFrame includes the length
				 * of the list as the first element.
				 */
				ctx->currentTarget->scsi_cmdset.luns[i] =
				    asd_be64toh(ReportLunsFrame[i + 1]);
			}
		} else {
			ctx->currentTarget->scsi_cmdset.num_luns = 0;
		}
	}

	while (ctx->currentTarget->all_domain_targets.next
	       != ctx->discover_listp) {

		ctx->currentTarget =
		    list_entry(ctx->currentTarget->all_domain_targets.next,
			       struct asd_target, all_domain_targets);

		if (ctx->currentTarget->command_set_type ==
		    ASD_COMMAND_SET_SCSI) {

			return ASD_STATE_INIT_SAS_ISSUE_REPORT_LUNS;
		}
	}

	list_for_each_entry(ctx->currentTarget, ctx->discover_listp,
			    all_domain_targets) {

		asd_log(ASD_DBG_INFO, "%s:%d - %llx %d\n",
			__FUNCTION__, __LINE__,
			*((uint64_t *) ctx->currentTarget->
			  ddb_profile.sas_addr),
			ctx->currentTarget->ddb_profile.conn_rate);

		if (ctx->currentTarget->command_set_type ==
		    ASD_COMMAND_SET_SCSI) {

			return ASD_STATE_INIT_SAS_GET_PORT_CONTROL;
		}
	}

	return ASD_STATE_INIT_SAS_FINISHED;
}

DISCOVER_RESULTS
asd_state_init_sas_get_port_control(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_InitSAS_SM_Context *ctx;
	DISCOVER_RESULTS results;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	results = asd_issue_get_port_control(sm_contextp, ctx->currentTarget);

	return results;
}

ASD_DISCOVERY_STATES
asd_state_init_sas_get_port_control_post(struct state_machine_context *
					 sm_contextp)
{
	struct state_information *state_infop;
	struct asd_InitSAS_SM_Context *ctx;
	uint8_t *port_control_page;
	struct discover_context *discover_contextp;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;
	if (discover_contextp->openStatus != OPEN_ACCEPT) {

		//printk("Port Control failed\n");
		/*
		 * Report LUNS failed.
		 */
	} else {
		port_control_page = (uint8_t *) discover_contextp->SASInfoFrame;

#if 0
		printk("page = 0x%x length = 0x%x protocol = 0x%x\n",
		       port_control_page[0],
		       port_control_page[1], port_control_page[2]);

		printk("I_T Nexus Loss Time = %d\n",
		       asd_be16toh(*((uint16_t *) & port_control_page[4])));
#endif
	}

	while (ctx->currentTarget->all_domain_targets.next
	       != ctx->discover_listp) {

		ctx->currentTarget =
		    list_entry(ctx->currentTarget->all_domain_targets.next,
			       struct asd_target, all_domain_targets);

		if (ctx->currentTarget->command_set_type ==
		    ASD_COMMAND_SET_SCSI) {

			return ASD_STATE_INIT_SAS_GET_PORT_CONTROL;
		}
	}

	return ASD_STATE_INIT_SAS_FINISHED;
}

DISCOVER_RESULTS
asd_InitSAS_SM_Initialize(struct state_machine_context * sm_contextp,
			  void *init_args)
{
	struct state_information *state_infop;
	struct asd_InitSAS_SM_Context *ctx;
	struct asd_InitSAS_SM_Arguments *args;

	NEW_CONTEXT(ctx);

	args = (struct asd_InitSAS_SM_Arguments *)init_args;

	/*
	 * go through the configure cycle progressively
	 * ascending to each expander starting at "newExpander"
	 */
	ctx->discover_listp = args->discover_listp;

	return DISCOVER_CONTINUE;
}

DISCOVER_RESULTS
asd_InitSAS_SM_StateMachine(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_InitSAS_SM_Context *ctx;
	DISCOVER_RESULTS results;
	ASD_DISCOVERY_STATES current_state;
	ASD_DISCOVERY_STATES new_state;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	current_state = GET_CURRENT_STATE();

	switch (current_state) {
	case ASD_STATE_INIT_SAS_START:
		new_state = asd_state_init_sas_start(sm_contextp);
		break;

	case ASD_STATE_INIT_SAS_INQUIRY:
		new_state = asd_state_init_sas_inquiry_post(sm_contextp);
		break;

	case ASD_STATE_INIT_SAS_GET_DEVICE_ID:
		new_state = asd_state_init_get_device_id_post(sm_contextp);
		break;

	case ASD_STATE_INIT_SAS_GET_SERIAL_NUMBER:
		new_state = asd_state_init_get_serial_number_post(sm_contextp);
		break;

	case ASD_STATE_INIT_SAS_ISSUE_REPORT_LUNS:
		new_state =
		    asd_state_init_sas_issue_report_lun_post(sm_contextp);
		break;

	case ASD_STATE_INIT_SAS_GET_PORT_CONTROL:
		new_state =
		    asd_state_init_sas_get_port_control_post(sm_contextp);
		break;

	default:
		new_state = ASD_STATE_INIT_SAS_FAILED;
		break;
	}

	SM_new_state(sm_contextp, new_state);

	switch (new_state) {

	case ASD_STATE_INIT_SAS_INQUIRY:
		results = asd_state_init_sas_inquiry(sm_contextp);
		break;

	case ASD_STATE_INIT_SAS_GET_DEVICE_ID:
		results = asd_state_init_get_device_id(sm_contextp);
		break;

	case ASD_STATE_INIT_SAS_GET_SERIAL_NUMBER:
		results = asd_state_init_get_serial_number(sm_contextp);
		break;

	case ASD_STATE_INIT_SAS_ISSUE_REPORT_LUNS:
		results = asd_state_init_sas_issue_report_luns(sm_contextp);
		break;

	case ASD_STATE_INIT_SAS_GET_PORT_CONTROL:
		results = asd_state_init_sas_get_port_control(sm_contextp);
		break;

	case ASD_STATE_INIT_SAS_FINISHED:
		results = DISCOVER_FINISHED;
		break;

	default:
		results = DISCOVER_FAILED;
		break;
	}

	return results;
}

void
asd_InitSAS_SM_Finish(struct state_machine_context *sm_contextp,
		      DISCOVER_RESULTS results)
{
	struct state_information *state_infop;
	struct asd_InitSAS_SM_Arguments *ctx;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	RETURN_STACK(results);
}

void asd_InitSAS_SM_Abort(struct state_machine_context *sm_contextp)
{
	struct state_information *state_infop;
	struct asd_InitSAS_SM_Arguments *ctx;
	ASD_DISCOVERY_STATES current_state;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	current_state = GET_CURRENT_STATE();

#if 0
	switch (current_state) {
	}
#endif
}

/* -------------------------------------------------------------------------- */

struct asd_SATA_SpinHoldSM_Context {
	struct discover_context *discover_contextp;
	struct list_head *discover_listp;
	struct asd_target *currentTarget;
	unsigned phyIndex;
};

ASD_DISCOVERY_STATES
asd_state_sata_spinhold_start(struct state_machine_context *sm_contextp)
{
	struct state_information *state_infop;
	struct asd_SATA_SpinHoldSM_Context *ctx;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	list_for_each_entry(ctx->currentTarget, ctx->discover_listp,
			    all_domain_targets) {

		asd_log(ASD_DBG_INFO, "%s:%d - %llx %d\n",
			__FUNCTION__, __LINE__,
			*((uint64_t *) ctx->currentTarget->
			  ddb_profile.sas_addr),
			ctx->currentTarget->ddb_profile.conn_rate);

		if ((ctx->currentTarget->transport_type ==
		     ASD_TRANSPORT_STP) &&
		    (ctx->currentTarget->ddb_profile.conn_rate ==
		     SPINUP_HOLD_OOB)) {

			return ASD_STATE_SATA_SPINHOLD_PHY_CONTROL;
		}
	}
	return ASD_STATE_SATA_SPINHOLD_FINISHED;
}

DISCOVER_RESULTS
asd_sata_spinhold_get_next_target(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_SATA_SpinHoldSM_Context *ctx;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	while (ctx->currentTarget->all_domain_targets.next
	       != ctx->discover_listp) {

		ctx->currentTarget =
		    list_entry(ctx->currentTarget->all_domain_targets.next,
			       struct asd_target, all_domain_targets);

		if ((ctx->currentTarget->transport_type ==
		     ASD_TRANSPORT_STP) &&
		    (ctx->currentTarget->ddb_profile.conn_rate ==
		     SPINUP_HOLD_OOB)) {

			return DISCOVER_OK;
		}
	}

	return DISCOVER_FINISHED;
}

DISCOVER_RESULTS
asd_state_sata_spinhold_phy_control(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_SATA_SpinHoldSM_Context *ctx;
	DISCOVER_RESULTS results;
	struct asd_target *parent;
	unsigned i;
	struct Discover *discover;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	parent = ctx->currentTarget->parent;

	/*
	 * Find the phy number of this device in the parent.
	 */
	for (i = 0; i < parent->num_phys; i++) {

		discover = &(parent->Phy[i].Result);

		if (SAS_ISEQUAL(ctx->currentTarget->ddb_profile.sas_addr,
				discover->AttachedSASAddress)) {

			break;
		}
	}

	if (i == parent->num_phys) {

		results = asd_sata_spinhold_get_next_target(sm_contextp);

		return results;
	}

	results = asd_issue_phy_control(sm_contextp, parent, i, LINK_RESET);

	return results;
}

ASD_DISCOVERY_STATES
asd_state_sata_spinhold_phy_control_post(struct state_machine_context *
					 sm_contextp)
{
	struct state_information *state_infop;
	struct asd_SATA_SpinHoldSM_Context *ctx;
	DISCOVER_RESULTS results;
	struct asd_target *parent;
	struct Discover *discover;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	results = asd_issue_phy_control_post(sm_contextp,
					     ctx->currentTarget->parent);

	if (results != DISCOVER_OK) {

		results = asd_sata_spinhold_get_next_target(sm_contextp);

		if (results != DISCOVER_FINISHED) {
			return ASD_STATE_SATA_SPINHOLD_PHY_CONTROL;
		}

		return ASD_STATE_SATA_SPINHOLD_FINISHED;
	}

	parent = ctx->currentTarget->parent;

	/*
	 * Find the phy number of the first matching device in the parent.
	 */
	for (ctx->phyIndex = 0; ctx->phyIndex < parent->num_phys;
	     ctx->phyIndex++) {

		discover = &(parent->Phy[ctx->phyIndex].Result);

		if (SAS_ISEQUAL(ctx->currentTarget->ddb_profile.sas_addr,
				discover->AttachedSASAddress)) {

			return ASD_STATE_SATA_SPINHOLD_DISCOVER;
		}
	}

	printk("Didn't find target!\n");

	results = asd_sata_spinhold_get_next_target(sm_contextp);

	if (results != DISCOVER_FINISHED) {
		return ASD_STATE_SATA_SPINHOLD_PHY_CONTROL;
	}

	return ASD_STATE_SATA_SPINHOLD_FINISHED;
}

DISCOVER_RESULTS
asd_state_sata_spinhold_discover(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_SATA_SpinHoldSM_Context *ctx;
	DISCOVER_RESULTS results;
	struct asd_target *parent;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	parent = ctx->currentTarget->parent;

	results = asd_issue_discover_request(sm_contextp,
					     parent, ctx->phyIndex);

	return results;
}

ASD_DISCOVERY_STATES
asd_state_sata_spinhold_discover_post(struct state_machine_context *
				      sm_contextp)
{
	struct state_information *state_infop;
	struct asd_SATA_SpinHoldSM_Context *ctx;
	DISCOVER_RESULTS results;
	struct asd_target *parent;
	struct Discover *discover;
	struct discover_context *discover_contextp;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;

	parent = ctx->currentTarget->parent;

	results = asd_issue_discover_request_post(sm_contextp,
						  parent, ctx->phyIndex);

	if (results != DISCOVER_OK) {
		printk("discover request failed\n");
	}

	parent = ctx->currentTarget->parent;

	discover = &(parent->Phy[ctx->phyIndex].Result);

	ctx->currentTarget->ddb_profile.conn_rate =
	    discover->NegotiatedPhysicalLinkRate;

	asd_hwi_setup_ddb_site(discover_contextp->asd, ctx->currentTarget);
	ctx->phyIndex++;

	/*
	 * Find the phy number of this device in the parent.
	 */
	for (; ctx->phyIndex < parent->num_phys; ctx->phyIndex++) {

		discover = &(parent->Phy[ctx->phyIndex].Result);

		if (SAS_ISEQUAL(ctx->currentTarget->ddb_profile.sas_addr,
				discover->AttachedSASAddress)) {

			return ASD_STATE_SATA_SPINHOLD_DISCOVER;
		}
	}

	results = asd_sata_spinhold_get_next_target(sm_contextp);

	if (results != DISCOVER_FINISHED) {
		return ASD_STATE_SATA_SPINHOLD_PHY_CONTROL;
	}

	return ASD_STATE_SATA_SPINHOLD_FINISHED;
}

DISCOVER_RESULTS
asd_SATA_SpinHoldSM_Initialize(struct state_machine_context * sm_contextp,
			       void *init_args)
{
	struct state_information *state_infop;
	struct asd_SATA_SpinHoldSM_Context *ctx;
	struct asd_SATA_SpinHoldSM_Arguments *args;

	NEW_CONTEXT(ctx);

	args = (struct asd_SATA_SpinHoldSM_Arguments *)init_args;

	/*
	 * go through the configure cycle progressively
	 * ascending to each expander starting at "newExpander"
	 */
	ctx->discover_listp = args->discover_listp;

	return DISCOVER_CONTINUE;
}

DISCOVER_RESULTS
asd_SATA_SpinHoldSM_StateMachine(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_SATA_SpinHoldSM_Context *ctx;
	DISCOVER_RESULTS results;
	ASD_DISCOVERY_STATES current_state;
	ASD_DISCOVERY_STATES new_state;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	current_state = GET_CURRENT_STATE();

	switch (current_state) {
	case ASD_STATE_SATA_SPINHOLD_START:
		new_state = asd_state_sata_spinhold_start(sm_contextp);
		break;

	case ASD_STATE_SATA_SPINHOLD_PHY_CONTROL:
		new_state =
		    asd_state_sata_spinhold_phy_control_post(sm_contextp);
		break;

	case ASD_STATE_SATA_SPINHOLD_DISCOVER:
		new_state = asd_state_sata_spinhold_discover_post(sm_contextp);
		break;

	default:
		new_state = ASD_STATE_SATA_SPINHOLD_FAILED;
		break;
	}

	SM_new_state(sm_contextp, new_state);

	switch (new_state) {
	case ASD_STATE_SATA_SPINHOLD_PHY_CONTROL:
		results = asd_state_sata_spinhold_phy_control(sm_contextp);
		break;

	case ASD_STATE_SATA_SPINHOLD_DISCOVER:
		results = asd_state_sata_spinhold_discover(sm_contextp);
		break;

	case ASD_STATE_SATA_SPINHOLD_FINISHED:
		results = DISCOVER_FINISHED;
		break;

	default:
		results = DISCOVER_FAILED;
		break;
	}

	return results;
}

void
asd_SATA_SpinHoldSM_Finish(struct state_machine_context *sm_contextp,
			   DISCOVER_RESULTS results)
{
	struct state_information *state_infop;
	struct asd_SATA_SpinHoldSM_Arguments *ctx;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	RETURN_STACK(results);
}

void asd_SATA_SpinHoldSM_Abort(struct state_machine_context *sm_contextp)
{
	struct state_information *state_infop;
	struct asd_SATA_SpinHoldSM_Arguments *ctx;
	ASD_DISCOVERY_STATES current_state;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	current_state = GET_CURRENT_STATE();

	switch (current_state) {
	case ASD_STATE_SATA_SPINHOLD_PHY_CONTROL:
		/*
		 * TODO: we need to abort the outstanding Phy Control request.
		 */
		break;

	case ASD_STATE_SATA_SPINHOLD_DISCOVER:
		/*
		 * TODO: we need to abort the outstanding Discover request.
		 */
		break;
	default:
		break;
	}
}

/* -------------------------------------------------------------------------- */

struct asd_ConfigureATA_SM_Context {
	struct asd_target *target;
	uint8_t next_feature;
	uint8_t sector_count;
};

ASD_DISCOVERY_STATES
asd_state_configure_ata_start(struct state_machine_context *sm_contextp)
{
	struct state_information *state_infop;
	struct asd_ConfigureATA_SM_Context *ctx;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	ctx->next_feature = asd_get_feature_to_enable(ctx->target,
						      &ctx->sector_count);

	if (ctx->next_feature == 0) {
		return ASD_STATE_CONFIGURE_ATA_FINISHED;
	}

	if (ctx->target->transport_type != ASD_TRANSPORT_ATA) {
		return ASD_STATE_CONFIGURE_ATA_FINISHED;
	}

	return ASD_STATE_CONFIGURE_ATA_FEATURES;
}

DISCOVER_RESULTS
asd_state_configure_ata_features(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_ConfigureATA_SM_Context *ctx;
	DISCOVER_RESULTS results;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	results = asd_sata_configure_features(sm_contextp, ctx->target,
					      ctx->next_feature,
					      ctx->sector_count);

	return results;
}

uint8_t
asd_get_feature_to_enable(struct asd_target * target, uint8_t * sector_count)
{
	unsigned features_enabled;
	unsigned features_state;
	unsigned *dma_mode_level;

	switch (target->command_set_type) {
	case ASD_COMMAND_SET_ATA:
		features_enabled = target->ata_cmdset.features_enabled;
		features_state = target->ata_cmdset.features_state;
		dma_mode_level = &target->ata_cmdset.dma_mode_level;
		break;

	case ASD_COMMAND_SET_ATAPI:
		features_enabled = target->atapi_cmdset.features_enabled;
		features_state = target->ata_cmdset.features_state;
		dma_mode_level = &target->atapi_cmdset.dma_mode_level;
		break;

	default:
		return 0;
	}

	*sector_count = 0;

	if (features_enabled & WRITE_CACHE_FEATURE_ENABLED) {
		if ((features_state & SATA_USES_WRITE_CACHE) == 0) {
			return SETFEATURES_EN_WCACHE;
		}
	} else {
		if (features_state & SATA_USES_WRITE_CACHE) {
			return SETFEATURES_DIS_WCACHE;
		}
	}

	if (features_enabled & READ_AHEAD_FEATURE_ENABLED) {
		if ((features_state & SATA_USES_READ_AHEAD) == 0) {
			return SETFEATURES_EN_RLA;
		}
	} else {
		if (features_state & SATA_USES_READ_AHEAD) {
			return SETFEATURES_DIS_RLA;
		}
	}

 	if (features_state & SATA_USES_UDMA) {
		if (features_enabled & NEEDS_XFER_SETFEATURES) {
			*sector_count = *dma_mode_level;

			return SETFEATURES_XFER;
		}
	}

	return 0;
}

ASD_DISCOVERY_STATES
asd_state_configure_ata_features_post(struct state_machine_context *
				      sm_contextp)
{
	struct state_information *state_infop;
	struct asd_ConfigureATA_SM_Context *ctx;
	unsigned *features_state;
	struct discover_context *discover_contextp;
	unsigned *features_enabled;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;

	if (discover_contextp->openStatus != OPEN_ACCEPT) {

		printk("configure failed\n");

		//TODO: we can't give up, but we need to notify the user

	} else {
		switch (ctx->target->command_set_type) {
		case ASD_COMMAND_SET_ATA:
			features_state =
			    &ctx->target->ata_cmdset.features_state;
			features_enabled =
			    &ctx->target->ata_cmdset.features_enabled;
			break;

		case ASD_COMMAND_SET_ATAPI:
			features_state =
			    &ctx->target->ata_cmdset.features_state;
			features_enabled =
			    &ctx->target->ata_cmdset.features_enabled;
			break;

		default:
			return ASD_STATE_CONFIGURE_ATA_FINISHED;
		}

		switch (ctx->next_feature) {
		case SETFEATURES_EN_WCACHE:
			*features_state |= SATA_USES_WRITE_CACHE;
			break;

		case SETFEATURES_EN_RLA:
			*features_state |= SATA_USES_READ_AHEAD;
			break;

		case SETFEATURES_DIS_WCACHE:
			*features_state &= ~SATA_USES_WRITE_CACHE;
			break;

		case SETFEATURES_DIS_RLA:
			*features_state &= ~SATA_USES_READ_AHEAD;
			break;

		case SETFEATURES_XFER:
 			*features_enabled &= ~NEEDS_XFER_SETFEATURES;
			break;
		}
	}

	ctx->next_feature = asd_get_feature_to_enable(ctx->target,
						      &ctx->sector_count);

	if (ctx->next_feature != 0) {
		return ASD_STATE_CONFIGURE_ATA_FEATURES;
	}

	return ASD_STATE_CONFIGURE_ATA_FINISHED;
}

/*
 * The ConfigureATA state machine operates on a signle target so that it can
 * be performed on an individual device to reconfigure that device after reset.
 *
 * This state machine does not need a discovery context (discover_contextp).
 */
DISCOVER_RESULTS
asd_ConfigureATA_SM_Initialize(struct state_machine_context * sm_contextp,
			       void *init_args)
{
	struct state_information *state_infop;
	struct asd_ConfigureATA_SM_Context *ctx;
	struct asd_ConfigureATA_SM_Arguments *args;

	NEW_CONTEXT(ctx);

	args = (struct asd_ConfigureATA_SM_Arguments *)init_args;

	ctx->target = args->target;

	return DISCOVER_CONTINUE;
}

DISCOVER_RESULTS
asd_ConfigureATA_SM_StateMachine(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_ConfigureATA_SM_Context *ctx;
	DISCOVER_RESULTS results;
	ASD_DISCOVERY_STATES current_state;
	ASD_DISCOVERY_STATES new_state;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	current_state = GET_CURRENT_STATE();

	switch (current_state) {
	case ASD_STATE_CONFIGURE_ATA_START:
		new_state = asd_state_configure_ata_start(sm_contextp);
		break;

	case ASD_STATE_CONFIGURE_ATA_FEATURES:
		new_state = asd_state_configure_ata_features_post(sm_contextp);
		break;

	default:
		new_state = ASD_STATE_CONFIGURE_ATA_FAILED;
		break;
	}

	SM_new_state(sm_contextp, new_state);

	switch (new_state) {
	case ASD_STATE_CONFIGURE_ATA_FEATURES:
		results = asd_state_configure_ata_features(sm_contextp);
		break;

	case ASD_STATE_CONFIGURE_ATA_FINISHED:
		results = DISCOVER_FINISHED;
		break;

	default:
		results = DISCOVER_FAILED;
		break;
	}

	return results;
}

void
asd_ConfigureATA_SM_Finish(struct state_machine_context *sm_contextp,
			   DISCOVER_RESULTS results)
{
	struct state_information *state_infop;
	struct asd_ConfigureATA_SM_Context *ctx;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	RETURN_STACK(results);
}

void asd_ConfigureATA_SM_Abort(struct state_machine_context *sm_contextp)
{
	struct state_information *state_infop;
	struct asd_ConfigureATA_SM_Context *ctx;
	ASD_DISCOVERY_STATES current_state;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	current_state = GET_CURRENT_STATE();

	switch (current_state) {
	case ASD_STATE_CONFIGURE_ATA_FEATURES:
		break;

	default:
		break;
	}
}

/* -------------------------------------------------------------------------- */

struct asd_InitSATA_SM_Context {
	struct discover_context *discover_contextp;
	struct list_head *discover_listp;
	struct asd_target *currentTarget;
};

ASD_DISCOVERY_STATES
asd_state_init_sata_start(struct state_machine_context *sm_contextp)
{
	struct state_information *state_infop;
	struct asd_InitSATA_SM_Context *ctx;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	/*
	 * Are there any devices that need a report phy?
	 */
	list_for_each_entry(ctx->currentTarget, ctx->discover_listp,
			    all_domain_targets) {

		if (ctx->currentTarget->transport_type == ASD_TRANSPORT_STP) {

			return ASD_STATE_INIT_SATA_REPORT_PHY;

		} else if (ctx->currentTarget->transport_type ==
			   ASD_TRANSPORT_ATA) {

			asd_init_sata_direct_attached(sm_contextp,
						      ctx->currentTarget);

			continue;
		}
	}

	/*
	 * If not, are there any devices that need a SATA identify or configure
	 * features?
	 */
	list_for_each_entry(ctx->currentTarget, ctx->discover_listp,
			    all_domain_targets) {

		if (ctx->currentTarget->device_protocol_type ==
		    ASD_DEVICE_PROTOCOL_ATA) {

			return ASD_STATE_INIT_SATA_IDENTIFY;
		}
	}

	return ASD_STATE_INIT_SATA_FINISHED;
}

DISCOVER_RESULTS
asd_state_init_sata_report_phy(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_InitSATA_SM_Context *ctx;
	DISCOVER_RESULTS results;
	struct asd_target *parent;
	unsigned i;
	struct Discover *discover;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	parent = ctx->currentTarget->parent;

	for (i = 0; i < parent->num_phys; i++) {

		discover = &(parent->Phy[i].Result);

		if (SAS_ISEQUAL(ctx->currentTarget->ddb_profile.sas_addr,
				discover->AttachedSASAddress)) {

			break;
		}
	}

	if (i == parent->num_phys) {

		results = DISCOVER_FAILED;

		return results;
	}

	results = asd_issue_report_phy_sata(sm_contextp, parent, i);

	return results;
}

#define NUM_FIS_DWORDS \
		(sizeof(struct adp_dev_to_host_fis) / sizeof(unsigned))

ASD_DISCOVERY_STATES
asd_init_sata_report_phy_next_target(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_InitSATA_SM_Context *ctx;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	while (ctx->currentTarget->all_domain_targets.next
	       != ctx->discover_listp) {

		ctx->currentTarget =
		    list_entry(ctx->currentTarget->all_domain_targets.next,
			       struct asd_target, all_domain_targets);

		if (ctx->currentTarget->transport_type == ASD_TRANSPORT_STP) {

			return ASD_STATE_INIT_SATA_REPORT_PHY;

		} else if (ctx->currentTarget->transport_type ==
			   ASD_TRANSPORT_ATA) {

			asd_init_sata_direct_attached(sm_contextp,
						      ctx->currentTarget);
		}
	}

	/*
	 * If there are no more STP devices to phy control, check to see if
	 * there are any devices that need IDENTIFY / PIDENTIFY.
	 */
	list_for_each_entry(ctx->currentTarget, ctx->discover_listp,
			    all_domain_targets) {

		if (ctx->currentTarget->device_protocol_type ==
		    ASD_DEVICE_PROTOCOL_ATA) {

			return ASD_STATE_INIT_SATA_IDENTIFY;
		}
	}

	return ASD_STATE_INIT_SATA_FINISHED;
}

ASD_DISCOVERY_STATES
asd_state_init_sata_report_phy_post(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_InitSATA_SM_Context *ctx;
	DISCOVER_RESULTS results;
	unsigned i;
	struct adp_dev_to_host_fis *fis;
	ASD_DISCOVERY_STATES new_state;
	struct discover_context *discover_contextp;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;

	results = asd_issue_report_phy_sata_post(sm_contextp,
						 ctx->currentTarget->parent);

	if (results != DISCOVER_OK) {

		new_state = asd_init_sata_report_phy_next_target(sm_contextp);

		return new_state;
	}

	fis = (struct adp_dev_to_host_fis *)&discover_contextp->
	    SMPResponseFrame->Response.ReportPhySATA.FIS;

	if (fis->error == FIS_DEVICE_TO_HOST) {

		for (i = 0; i < NUM_FIS_DWORDS; i++) {

			*((unsigned *)fis + i) =
			    asd_htobe32(*((unsigned *)fis + i));
		}
	}

	ctx->currentTarget->command_set_type = asd_sata_get_type(fis);

	memcpy((void *)&ctx->currentTarget->
	       device_protocol.ata_device_protocol.initial_fis[0],
	       (void *)fis, sizeof(struct adp_dev_to_host_fis));

	ctx->currentTarget->ddb_profile.sata_status = fis->status;

	asd_hwi_update_sata(discover_contextp->asd, ctx->currentTarget);

	new_state = asd_init_sata_report_phy_next_target(sm_contextp);

	return new_state;
}

DISCOVER_RESULTS
asd_state_init_sata_identify(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_InitSATA_SM_Context *ctx;
	DISCOVER_RESULTS results;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	results = asd_sata_identify_request(sm_contextp, ctx->currentTarget);

	return results;
}

ASD_DISCOVERY_STATES
asd_init_sata_identify_next_target(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_InitSATA_SM_Context *ctx;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	while (ctx->currentTarget->all_domain_targets.next
	       != ctx->discover_listp) {

		ctx->currentTarget =
		    list_entry(ctx->currentTarget->all_domain_targets.next,
			       struct asd_target, all_domain_targets);

		if (ctx->currentTarget->device_protocol_type ==
		    ASD_DEVICE_PROTOCOL_ATA) {

			return ASD_STATE_INIT_SATA_IDENTIFY;
		}
	}

	/*
	 * If there are no more ATA devices to identify, then check to see
	 * if any device need to be configured.
	 */
	list_for_each_entry(ctx->currentTarget, ctx->discover_listp,
			    all_domain_targets) {

		if (ctx->currentTarget->device_protocol_type ==
		    ASD_DEVICE_PROTOCOL_ATA) {

			return ASD_STATE_INIT_SATA_CONFIGURE_FEATURES;
		}
	}

	return ASD_STATE_INIT_SATA_FINISHED;
}

ASD_DISCOVERY_STATES
asd_state_init_sata_identify_post(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_InitSATA_SM_Context *ctx;
	ASD_DISCOVERY_STATES new_state;
	struct discover_context *discover_contextp;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;

	if (discover_contextp->openStatus != OPEN_ACCEPT) {
#ifdef ASD_DEBUG
		asd_log(ASD_DBG_INFO, "Identify failed retry_count=0x%x\n",discover_contextp->retry_count);
#endif
		if(discover_contextp->retry_count<MAX_SAS_INQUIRY_RETRY)
		{
			discover_contextp->retry_count++;
			return ASD_STATE_INIT_SATA_IDENTIFY;
		}
		else
		{
			ctx->currentTarget->command_set_type = ASD_COMMAND_SET_BAD;
		}
		//TODO: we can't give up, but we need to notify the user
	} else {
		/*
		 * Pre-compute the features that this drive supports so
		 * that we don't have to hunt them down in the hd_driveid
		 * structure.
		 */
		asd_sata_compute_support(discover_contextp->asd,
					 ctx->currentTarget);
	}

	new_state = asd_init_sata_identify_next_target(sm_contextp);

	return new_state;
}

DISCOVER_RESULTS
asd_state_init_sata_configure_features(struct state_machine_context *
				       sm_contextp)
{
	struct state_information *state_infop;
	struct asd_InitSATA_SM_Context *ctx;
	DISCOVER_RESULTS results;
	struct asd_ConfigureATA_SM_Arguments args;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	args.target = ctx->currentTarget;
#ifdef ASD_DEBUG
	asd_log(ASD_DBG_INFO, "ctx->currentTarget->ddb_profile.sas_addr=%llx, ctx->currentTarget->transport_type=0x%x, ctx->currentTarget->command_set_type 0x%x\n",
		*((uint64_t *) ctx->currentTarget->ddb_profile.sas_addr),
		ctx->currentTarget->transport_type,
		ctx->currentTarget->command_set_type);
#endif
	results = ASD_PUSH_STATE_MACHINE(sm_contextp,
					 &asd_ConfigureATA_SM, (void *)&args);

	return results;
}

ASD_DISCOVERY_STATES
asd_init_sata_configure_features_next_target(struct state_machine_context *
					     sm_contextp)
{
	struct state_information *state_infop;
	struct asd_InitSATA_SM_Context *ctx;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	while (ctx->currentTarget->all_domain_targets.next
	       != ctx->discover_listp) {

		ctx->currentTarget =
		    list_entry(ctx->currentTarget->all_domain_targets.next,
			       struct asd_target, all_domain_targets);

		if (ctx->currentTarget->device_protocol_type ==
		    ASD_DEVICE_PROTOCOL_ATA) {

			return ASD_STATE_INIT_SATA_CONFIGURE_FEATURES;
		}
	}

	return ASD_STATE_INIT_SATA_FINISHED;
}

ASD_DISCOVERY_STATES
asd_state_init_sata_configure_features_post(struct state_machine_context *
					    sm_contextp)
{
	struct state_information *state_infop;
	struct asd_InitSATA_SM_Context *ctx;
	ASD_DISCOVERY_STATES new_state;
	struct discover_context *discover_contextp;
	DISCOVER_RESULTS results;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	POP_STACK(results);

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;

	if (discover_contextp->openStatus != OPEN_ACCEPT) {

		printk("configure failed\n");

		//TODO: we can't give up, but we need to notify the user
	}

	new_state = asd_init_sata_configure_features_next_target(sm_contextp);

	return new_state;
}

DISCOVER_RESULTS
asd_InitSATA_SM_Initialize(struct state_machine_context * sm_contextp,
			   void *init_args)
{
	struct state_information *state_infop;
	struct asd_InitSATA_SM_Context *ctx;
	struct asd_InitSATA_SM_Arguments *args;

	NEW_CONTEXT(ctx);

	args = (struct asd_InitSATA_SM_Arguments *)init_args;

	/*
	 * go through the configure cycle progressively
	 * ascending to each expander starting at "newExpander"
	 */
	ctx->discover_listp = args->discover_listp;

	return DISCOVER_CONTINUE;
}

DISCOVER_RESULTS
asd_InitSATA_SM_StateMachine(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_InitSATA_SM_Context *ctx;
	DISCOVER_RESULTS results;
	ASD_DISCOVERY_STATES current_state;
	ASD_DISCOVERY_STATES new_state;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	current_state = GET_CURRENT_STATE();

	switch (current_state) {
	case ASD_STATE_INIT_SATA_START:
		new_state = asd_state_init_sata_start(sm_contextp);
		break;

	case ASD_STATE_INIT_SATA_REPORT_PHY:
		new_state = asd_state_init_sata_report_phy_post(sm_contextp);
		break;

	case ASD_STATE_INIT_SATA_IDENTIFY:
		new_state = asd_state_init_sata_identify_post(sm_contextp);
		break;

	case ASD_STATE_INIT_SATA_CONFIGURE_FEATURES:
		new_state =
		    asd_state_init_sata_configure_features_post(sm_contextp);
		break;

	default:
		new_state = ASD_STATE_INIT_SATA_FAILED;
		break;
	}

	SM_new_state(sm_contextp, new_state);

	switch (new_state) {
	case ASD_STATE_INIT_SATA_REPORT_PHY:
		results = asd_state_init_sata_report_phy(sm_contextp);
		break;

	case ASD_STATE_INIT_SATA_IDENTIFY:
		results = asd_state_init_sata_identify(sm_contextp);
		break;

	case ASD_STATE_INIT_SATA_CONFIGURE_FEATURES:
		results = asd_state_init_sata_configure_features(sm_contextp);
		break;

	case ASD_STATE_INIT_SATA_FINISHED:
		results = DISCOVER_FINISHED;
		break;

	default:
		results = DISCOVER_FAILED;
		break;
	}

	return results;
}

void
asd_InitSATA_SM_Finish(struct state_machine_context *sm_contextp,
		       DISCOVER_RESULTS results)
{
	struct state_information *state_infop;
	struct asd_InitSATA_SM_Context *ctx;
	struct discover_context *discover_contextp;
	struct asd_phy *phy;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;

	if (results == DISCOVER_FAILED) {
		phy = list_entry(discover_contextp->port->phys_attached.next,
				 struct asd_phy, links);

		printk("Phy%d: %s - discovery error\n", phy->id, __FUNCTION__);
	}

	RETURN_STACK(results);
}

void asd_InitSATA_SM_Abort(struct state_machine_context *sm_contextp)
{
	struct state_information *state_infop;
	struct asd_InitSATA_SM_Context *ctx;
	ASD_DISCOVERY_STATES current_state;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	current_state = GET_CURRENT_STATE();

	switch (current_state) {
	case ASD_STATE_INIT_SATA_REPORT_PHY:
		/*
		 * TODO: Abort Report Phy request.
		 */
		break;

	case ASD_STATE_INIT_SATA_IDENTIFY:
		/*
		 * TODO: Abort Identify request.
		 */
		break;

	default:
		break;
	}
}

/* -------------------------------------------------------------------------- */

struct asd_ConfigureExpanderSM_Context {
	struct discover_context *discover_contextp;
	struct list_head *discover_listp;
	struct asd_target *newExpander;
	struct asd_target *currentExpander;
	struct asd_target *configureExpander;
	uint8_t upstreamSASAddress[SAS_ADDR_LEN];
	uint8_t upstreamPhyIdentifier;
	unsigned phyIndex;
	unsigned routeIndex;
	uint32_t slowest_link;
};

DISCOVER_RESULTS
asd_state_config_expander_route(struct state_machine_context *sm_contextp)
{
	struct state_information *state_infop;
	struct asd_ConfigureExpanderSM_Context *ctx;
	uint8_t disableRouteEntry;
	struct Discover *discover;
	DISCOVER_RESULTS results;
	uint8_t *attached_sas_addr;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	for (; 1; ctx->currentExpander = ctx->configureExpander) {

		if (ctx->currentExpander->management_flags & DEVICE_SET_ROOT) {
			/*
			 * This is the edge of the device set, we are done.
			 */
			return DISCOVER_FINISHED;
		}
		/*
		 * move upstream from here to find the expander table to 
		 * configure with information from "attachedExpander"
		 */
		if (!asd_upstream_expander(ctx->currentExpander,
					   ctx->upstreamSASAddress,
					   &ctx->upstreamPhyIdentifier)) {

			return DISCOVER_FINISHED;
		}

		if (SAS_ISZERO(ctx->upstreamSASAddress)) {
			return DISCOVER_FINISHED;
		}

		/*
		 * get the expander associated with the upstream address
		 */
		ctx->configureExpander = asd_find_target(ctx->discover_listp,
							 ctx->
							 upstreamSASAddress);

		if (ctx->configureExpander == NULL) {
			/*
			 * If we don't have the upstream expander, something
			 * is wrong.  This should never happen.
			 */
			return DISCOVER_FAILED;
		}

		/*
		 * if we found an upstream expander, then program its route
		 * table.
		 */
		for (ctx->phyIndex = 0; ctx->phyIndex <
		     ctx->configureExpander->num_phys; ctx->phyIndex++) {

			attached_sas_addr =
			    ctx->configureExpander->
			    Phy[ctx->phyIndex].Result.AttachedSASAddress;

			if (SAS_ISEQUAL(attached_sas_addr,
					ctx->currentExpander->ddb_profile.
					sas_addr)) {

				break;
			}
		}

		if (ctx->phyIndex == ctx->configureExpander->num_phys) {

			continue;
		}

		/*
		 * assume the route entry is enabled
		 */
		disableRouteEntry = ENABLED;

		discover = NULL;

		for (ctx->routeIndex = 0;
		     ctx->routeIndex < ctx->newExpander->num_phys;
		     ctx->routeIndex++) {

			discover = &(ctx->newExpander->
				     Phy[ctx->routeIndex].Result);

			/*
			 * check to see if the address needs to be configured
			 * in the route table, this decision is based on the
			 * optimization flag
			 */
			if (asd_qualified_address(ctx->configureExpander,
						  ctx->phyIndex, discover,
						  &disableRouteEntry)) {

				break;
			}
		}

		if (ctx->routeIndex == ctx->newExpander->num_phys) {

			continue;
		}

		results = asd_issue_route_config(sm_contextp,
						 ctx->configureExpander,
						 ctx->phyIndex,
						 disableRouteEntry,
						 discover->AttachedSASAddress);

		return results;
	}
}

ASD_DISCOVERY_STATES
asd_state_config_expander_route_post(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_ConfigureExpanderSM_Context *ctx;
	struct Discover *discover;
	DISCOVER_RESULTS results;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	results = asd_issue_route_config_post(sm_contextp,
					      ctx->configureExpander,
					      ctx->phyIndex);

	if (results == DISCOVER_OK) {

		discover = &(ctx->newExpander->Phy[ctx->routeIndex].Result);

		ctx->routeIndex++;

		/*
		 * add the address to the internal copy of the
		 * route table, if successfully configured
		 */
		SASCPY(ROUTE_ENTRY(ctx->configureExpander, ctx->phyIndex,
				   ctx->configureExpander->
				   route_indexes[ctx->phyIndex]),
		       discover->AttachedSASAddress);

		ctx->configureExpander->route_indexes[ctx->phyIndex]++;

		return ASD_STATE_CONFIG_EXPANDER_ROUTE_LOOP;
	}

	return ASD_STATE_CONFIG_EXPANDER_FAILED;
}

DISCOVER_RESULTS
asd_state_config_expander_route_loop(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_ConfigureExpanderSM_Context *ctx;
	DISCOVER_RESULTS results;
	uint8_t disableRouteEntry;
	struct Discover *discover;
	uint8_t *attached_sas_addr;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	while (1) {
		/*
		 * assume the route entry is enabled
		 */
		disableRouteEntry = ENABLED;

		for (; ctx->routeIndex < ctx->newExpander->num_phys;
		     ctx->routeIndex++) {

			discover = &(ctx->newExpander->
				     Phy[ctx->routeIndex].Result);

			/*
			 * check to see if the address needs to be configured
			 * in the route table, this decision is based on the
			 * optimization flag
			 */
			if (asd_qualified_address(ctx->configureExpander,
						  ctx->phyIndex, discover,
						  &disableRouteEntry)) {

				results = asd_issue_route_config(sm_contextp,
								 ctx->
								 configureExpander,
								 ctx->phyIndex,
								 disableRouteEntry,
								 discover->
								 AttachedSASAddress);

				return results;
			}
		}

		ctx->phyIndex++;

		/*
		 * if we found an upstream expander, then program its route
		 * table.
		 */
		for (; ctx->phyIndex <
		     ctx->configureExpander->num_phys; ctx->phyIndex++) {

			attached_sas_addr = ctx->configureExpander->
			    Phy[ctx->phyIndex].Result.AttachedSASAddress;

			if (SAS_ISEQUAL(attached_sas_addr,
					ctx->currentExpander->ddb_profile.
					sas_addr)) {

				break;
			}
		}

		if (ctx->phyIndex == ctx->configureExpander->num_phys) {

			break;
		}

		ctx->routeIndex = 0;
	}

	ctx->phyIndex = 0;

	ctx->currentExpander = ctx->configureExpander;

	results = asd_state_config_expander_route(sm_contextp);

	return results;
}

DISCOVER_RESULTS
asd_ConfigureExpanderSM_Initialize(struct state_machine_context *
				   sm_contextp, void *init_args)
{
	struct state_information *state_infop;
	struct asd_ConfigureExpanderSM_Context *ctx;
	struct asd_ConfigureExpanderSM_Arguments *args;

	NEW_CONTEXT(ctx);

	args = (struct asd_ConfigureExpanderSM_Arguments *)init_args;

	/*
	 * go through the configure cycle progressively
	 * ascending to each expander starting at "newExpander"
	 */
	ctx->discover_listp = args->discover_listp;
	ctx->newExpander = args->newExpander;
	ctx->currentExpander = ctx->newExpander;
	ctx->slowest_link = 0;
	SAS_ZERO(ctx->upstreamSASAddress);

	return DISCOVER_CONTINUE;
}

DISCOVER_RESULTS
asd_ConfigureExpanderSM_StateMachine(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_ConfigureExpanderSM_Context *ctx;
	DISCOVER_RESULTS results;
	ASD_DISCOVERY_STATES current_state;
	ASD_DISCOVERY_STATES new_state;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	current_state = GET_CURRENT_STATE();

	switch (current_state) {
	case ASD_STATE_CONFIG_EXPANDER_START:
		new_state = ASD_STATE_CONFIG_EXPANDER_ROUTE;
		break;

	case ASD_STATE_CONFIG_EXPANDER_ROUTE:
		new_state = asd_state_config_expander_route_post(sm_contextp);
		break;

	case ASD_STATE_CONFIG_EXPANDER_ROUTE_LOOP:
		new_state = asd_state_config_expander_route_post(sm_contextp);
		break;

	default:
		new_state = ASD_STATE_CONFIG_EXPANDER_FAILED;
		break;
	}

	SM_new_state(sm_contextp, new_state);

	switch (new_state) {
	case ASD_STATE_CONFIG_EXPANDER_ROUTE:
		results = asd_state_config_expander_route(sm_contextp);
		break;

	case ASD_STATE_CONFIG_EXPANDER_ROUTE_LOOP:
		results = asd_state_config_expander_route_loop(sm_contextp);
		break;

	case ASD_STATE_CONFIG_EXPANDER_FINISHED:
		results = DISCOVER_FINISHED;
		break;

	default:
		results = DISCOVER_FAILED;
		break;
	}

	return results;
}

void
asd_ConfigureExpanderSM_Finish(struct state_machine_context *sm_contextp,
			       DISCOVER_RESULTS results)
{
	struct state_information *state_infop;
	struct asd_ConfigureExpanderSM_Context *ctx;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	RETURN_STACK(results);
}

void asd_ConfigureExpanderSM_Abort(struct state_machine_context *sm_contextp)
{
	struct state_information *state_infop;
	struct asd_ConfigureExpanderSM_Context *ctx;
	ASD_DISCOVERY_STATES current_state;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	current_state = GET_CURRENT_STATE();

	switch (current_state) {
	case ASD_STATE_CONFIG_EXPANDER_ROUTE:
		break;

	case ASD_STATE_CONFIG_EXPANDER_ROUTE_LOOP:
		break;

	default:
		break;
	}
}

/* -------------------------------------------------------------------------- */

struct asd_DiscoverConfigSetSM_Context {
	struct discover_context *discover_contextp;
	struct list_head *discover_listp;
	struct asd_target *currentExpander;
	struct asd_target *newExpander;
	unsigned phyIndex;
	struct list_head *found_listp;
	struct list_head *old_discover_listp;
};

DISCOVER_RESULTS asd_state_config_set_issue_discover(struct state_machine_context
						     *sm_contextp)
{
	struct state_information *state_infop;
	struct asd_DiscoverConfigSetSM_Context *ctx;
	DISCOVER_RESULTS results;
	struct Discover *currentDiscover;
	unsigned conn_rate;
	unsigned found_expander;
	struct asd_DiscoverExpanderSM_Arguments args;
	struct asd_target *target;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	currentDiscover = NULL;

	while (1) {
		for (; ctx->phyIndex !=
		     ctx->currentExpander->num_phys; ctx->phyIndex++) {

			/*
			 * this is just a pointer helper
			 */
			currentDiscover = &(ctx->currentExpander->
					    Phy[ctx->phyIndex].Result);

			if (SAS_ISZERO(currentDiscover->AttachedSASAddress)) {
				continue;
			}

			/*
			 * Vitesse:
			 */
			if (((currentDiscover->TargetBits &
			      SMP_TGT_PORT) != 0) &&
			    (currentDiscover->AttachedDeviceType ==
			     END_DEVICE)) {

				printk("%s:%d: expander %llx reports %llx as "
				       "an END_DEVICE, but SMP_TGT_PORT is "
				       "set in TargetBits\n",
				       __FUNCTION__, __LINE__,
				       *((uint64_t *) ctx->currentExpander->
					 ddb_profile.sas_addr),
				       *((uint64_t *) currentDiscover->
					 AttachedSASAddress));

				printk("%s:%d: Assuming expander\n",
				       __FUNCTION__, __LINE__);

				currentDiscover->AttachedDeviceType =
				    EDGE_EXPANDER_DEVICE;
			}

			/*
			 * look for phys with edge or fanout devices attached...
			 */
			switch (currentDiscover->AttachedDeviceType) {
			case EDGE_EXPANDER_DEVICE:
				break;

			case FANOUT_EXPANDER_DEVICE:
				break;

			case END_DEVICE:
				conn_rate = ctx->currentExpander->
				    ddb_profile.conn_rate;

				if (conn_rate > currentDiscover->
				    NegotiatedPhysicalLinkRate) {

					conn_rate = currentDiscover->
					    NegotiatedPhysicalLinkRate;
				}

				results = asd_configure_device(sm_contextp,
							       ctx->
							       currentExpander,
							       currentDiscover,
							       ctx->
							       discover_listp,
							       ctx->found_listp,
							       ctx->
							       old_discover_listp,
							       conn_rate);

				continue;

			case NO_DEVICE:
			default:
				continue;
			}

			if (currentDiscover->RoutingAttribute != TABLE) {
				continue;
			}

			/*
			 * check to see if we already have the address
			 * information in our expander list
			 */
			target = asd_find_target(ctx->discover_listp,
						 currentDiscover->
						 AttachedSASAddress);

			if (target != NULL) {
				continue;
			}

			args.sas_addr = currentDiscover->AttachedSASAddress;
			args.upstreamExpander = ctx->currentExpander;
			args.attachedDeviceType =
			    currentDiscover->AttachedDeviceType;
			args.old_discover_listp = ctx->old_discover_listp;
			args.found_listp = ctx->found_listp;
			args.conn_rate =
			    currentDiscover->NegotiatedPhysicalLinkRate;

			/*
			 * if we did not have the expander in our list, then get
			 * the information
			 */
			results = ASD_PUSH_STATE_MACHINE(sm_contextp,
							 &asd_DiscoverExpanderSM,
							 (void *)&args);

			return results;
		}

		if (ctx->phyIndex == ctx->currentExpander->num_phys) {

			found_expander = 0;

			while (ctx->currentExpander->all_domain_targets.next
			       != ctx->discover_listp) {

				ctx->currentExpander =
				    list_entry(ctx->currentExpander->
					       all_domain_targets.next,
					       struct asd_target,
					       all_domain_targets);

				if ((ctx->currentExpander->management_type ==
				     ASD_DEVICE_EDGE_EXPANDER) ||
				    (ctx->currentExpander->management_type ==
				     ASD_DEVICE_FANOUT_EXPANDER)) {

					found_expander = 1;

					break;
				}
			}

			if (found_expander == 0) {
				return DISCOVER_FINISHED;
			}

			ctx->phyIndex = 0;
		}
	}
}

ASD_DISCOVERY_STATES
asd_state_config_set_issue_discover_post(struct state_machine_context *
					 sm_contextp)
{
	struct state_information *state_infop;
	struct asd_DiscoverConfigSetSM_Context *ctx;
	struct asd_target *expander;
	DISCOVER_RESULTS results;
	struct discover_context *discover_contextp;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;

	/*
	 * The new expander is returned on the stack
	 */
	POP_STACK(results);
	POP_STACK(expander);

	ctx->phyIndex++;

	if (results != DISCOVER_FINISHED) {
		/*
		 * For some reason we couldn't talk to this expander
		 */
		printk("Discover of expander 0x%0llx failed\n",
		       *((uint64_t *) expander->ddb_profile.sas_addr));

		return ASD_STATE_CONFIG_SET_ISSUE_DISCOVER;
	}

	ctx->newExpander = expander;

	/*
	 * Add the new expander to the tree.
	 */
	asd_add_child(discover_contextp->port, ctx->currentExpander,
		      ctx->newExpander);

	list_add_tail(&ctx->newExpander->all_domain_targets,
		      ctx->discover_listp);

	if (expander->configurable_route_table == 0) {
		return ASD_STATE_CONFIG_SET_ISSUE_DISCOVER;
	}

	return ASD_STATE_CONFIG_SET_CONFIGURE_EXPANDER;
}

DISCOVER_RESULTS
asd_state_config_set_configure_expander(struct state_machine_context *
					sm_contextp)
{
	struct state_information *state_infop;
	struct asd_DiscoverConfigSetSM_Context *ctx;
	DISCOVER_RESULTS results;
	struct asd_ConfigureExpanderSM_Arguments args;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	args.discover_listp = ctx->discover_listp;
	args.newExpander = ctx->newExpander;

	results = ASD_PUSH_STATE_MACHINE(sm_contextp,
					 &asd_ConfigureExpanderSM,
					 (void *)&args);

	return results;
}

ASD_DISCOVERY_STATES
asd_state_config_set_configure_expander_post(struct state_machine_context *
					     sm_contextp)
{
	struct state_information *state_infop;
	struct asd_DiscoverConfigSetSM_Context *ctx;
	DISCOVER_RESULTS results;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	POP_STACK(results);

	if (results != DISCOVER_FINISHED) {
		return ASD_STATE_CONFIG_SET_FAILED;
	}

	return ASD_STATE_CONFIG_SET_ISSUE_DISCOVER;
}

DISCOVER_RESULTS
asd_DiscoverConfigSetSM_Initialize(struct state_machine_context *
				   sm_contextp, void *init_args)
{
	struct state_information *state_infop;
	struct asd_DiscoverConfigSetSM_Context *ctx;
	struct asd_DiscoverConfigSetSM_Arguments *args;

	NEW_CONTEXT(ctx);

	args = (struct asd_DiscoverConfigSetSM_Arguments *)init_args;

	ctx->currentExpander = args->currentExpander;
	ctx->discover_listp = args->discover_listp;
	ctx->found_listp = args->found_listp;
	ctx->old_discover_listp = args->old_discover_listp;
	ctx->phyIndex = 0;
	ctx->newExpander = NULL;

	list_del_init(&ctx->currentExpander->all_domain_targets);

	list_add_tail(&ctx->currentExpander->all_domain_targets,
		      ctx->discover_listp);

	return DISCOVER_CONTINUE;
}

DISCOVER_RESULTS
asd_DiscoverConfigSetSM_StateMachine(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_DiscoverConfigSetSM_Context *ctx;
	DISCOVER_RESULTS results;
	ASD_DISCOVERY_STATES current_state;
	ASD_DISCOVERY_STATES new_state;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	current_state = GET_CURRENT_STATE();

	switch (current_state) {
	case ASD_STATE_CONFIG_SET_START:
		new_state = ASD_STATE_CONFIG_SET_ISSUE_DISCOVER;
		break;

	case ASD_STATE_CONFIG_SET_ISSUE_DISCOVER:
		new_state =
		    asd_state_config_set_issue_discover_post(sm_contextp);
		break;

	case ASD_STATE_CONFIG_SET_CONFIGURE_EXPANDER:
		new_state =
		    asd_state_config_set_configure_expander_post(sm_contextp);
		break;

	default:
		new_state = ASD_STATE_CONFIG_SET_FAILED;
		break;
	}

	SM_new_state(sm_contextp, new_state);

	switch (new_state) {
	case ASD_STATE_CONFIG_SET_ISSUE_DISCOVER:
		results = asd_state_config_set_issue_discover(sm_contextp);
		break;

	case ASD_STATE_CONFIG_SET_FINISHED:
		results = DISCOVER_FINISHED;
		break;

	case ASD_STATE_CONFIG_SET_CONFIGURE_EXPANDER:
		results = asd_state_config_set_configure_expander(sm_contextp);
		break;

	default:
		results = DISCOVER_FAILED;
		break;
	}

	return results;
}

void
asd_DiscoverConfigSetSM_Finish(struct state_machine_context *sm_contextp,
			       DISCOVER_RESULTS results)
{
	struct state_information *state_infop;
	struct asd_DiscoverConfigSetSM_Context *ctx;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	RETURN_STACK(results);

}

void asd_DiscoverConfigSetSM_Abort(struct state_machine_context *sm_contextp)
{
	struct state_information *state_infop;
	struct asd_DiscoverConfigSetSM_Context *ctx;

	GET_STATE_CONTEXT(sm_contextp, ctx);
}

/* -------------------------------------------------------------------------- */

struct asd_DiscoverFindBoundarySM_Context {
	struct discover_context *discover_contextp;
	struct asd_target *expander;
	uint8_t upstreamSASAddress[SAS_ADDR_LEN];
	uint8_t attachedPhyIdentifier;
	uint8_t phyIdentifier;
	struct asd_target *attachedRootExpander;
	struct list_head *found_listp;
	struct list_head *old_discover_listp;
	unsigned conn_rate;
};

ASD_DISCOVERY_STATES
asd_state_find_boundary_start(struct state_machine_context *sm_contextp)
{
	struct state_information *state_infop;
	struct asd_DiscoverFindBoundarySM_Context *ctx;
	DISCOVER_RESULTS results;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	results = asd_find_subtractive_phy(sm_contextp, ctx->expander,
					   ctx->upstreamSASAddress,
					   &ctx->attachedPhyIdentifier,
					   &ctx->conn_rate,
					   &ctx->phyIdentifier);

	if ((results == DISCOVER_FINISHED) ||
	    SAS_ISZERO(ctx->upstreamSASAddress)) {

		ctx->expander->management_flags |= DEVICE_SET_ROOT;

		return ASD_STATE_FIND_BOUNDARY_FINISHED;
	}

	return ASD_STATE_FIND_BOUNDARY_LOOP;
}

DISCOVER_RESULTS
asd_state_find_boundary_loop(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_DiscoverFindBoundarySM_Context *ctx;
	struct asd_DiscoverExpanderSM_Arguments args;
	DISCOVER_RESULTS results;
	struct Discover *discover;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	/*
	 * if we have a subtractive address then go upstream
	 * to see if it is part of the edge expander device set
	 */

	discover = &(ctx->expander->Phy[ctx->phyIdentifier].Result);

	args.sas_addr = ctx->upstreamSASAddress;
	args.upstreamExpander = ctx->expander;
	args.attachedDeviceType = discover->AttachedDeviceType;
	args.old_discover_listp = ctx->old_discover_listp;
	args.found_listp = ctx->found_listp;
	args.conn_rate = ctx->conn_rate;

	results = ASD_PUSH_STATE_MACHINE(sm_contextp, &asd_DiscoverExpanderSM,
					 (void *)&args);

	return results;
}

ASD_DISCOVERY_STATES
asd_state_find_boundary_loop_post(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_DiscoverFindBoundarySM_Context *ctx;
	DISCOVER_RESULTS results;
	struct asd_target *upstreamExpander;
	struct Discover *discover;
	struct discover_context *discover_contextp;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;

	/*
	 * The new expander is returned on the stack
	 */
	POP_STACK(results);
	POP_STACK(upstreamExpander);

	if (results != DISCOVER_FINISHED) {
		return ASD_STATE_FIND_BOUNDARY_FAILED;
	}

	/*
	 * Add the new expander to the tree.
	 */
	asd_add_child(discover_contextp->port, ctx->expander, upstreamExpander);

	list_add_tail(&upstreamExpander->all_domain_targets, ctx->found_listp);

	results = asd_find_subtractive_phy(sm_contextp, upstreamExpander,
					   ctx->upstreamSASAddress,
					   &ctx->attachedPhyIdentifier,
					   &ctx->conn_rate,
					   &ctx->phyIdentifier);

	if (results == DISCOVER_FINISHED) {

		ctx->expander = upstreamExpander;

		ctx->expander->management_flags |= DEVICE_SET_ROOT;

		return ASD_STATE_FIND_BOUNDARY_FINISHED;
	}

	/*
	 * initialize the subtractive address, a zero value is not valid
	 */
	if (results == DISCOVER_FAILED) {
		/*
		 * to get here, we had to see more than one subtractive
		 * phy that connect to different SAS addresses, this is
		 * a topology error do cleanup on any memory allocated
		 * if necessary
		 */

		// TODO: fix this
		// TODO: what do we want to return here???

		return ASD_STATE_FIND_BOUNDARY_FAILED;
	}

	/*
	 * if no error, then decide if we need to go upstream or stop
	 */
	if (SAS_ISZERO(ctx->upstreamSASAddress)) {

		ctx->expander = upstreamExpander;

		return ASD_STATE_FIND_BOUNDARY_FINISHED;
	}

	/*
	 * this is just a pointer helper
	 */
	discover = &(upstreamExpander->Phy[ctx->phyIdentifier].Result);

	/*
	 * check to see if the upstream expander is connected to the 
	 * subtractive port of the previous expander, if we are then we 
	 * have two expander device sets connected together, stop here 
	 * and save the target pointer of next expander in device set.
	 */
	if (SAS_ISEQUAL(discover->AttachedSASAddress,
			upstreamExpander->parent->ddb_profile.sas_addr)) {

		ctx->attachedRootExpander = upstreamExpander;

		ctx->expander->management_flags |= DEVICE_SET_ROOT;
		ctx->attachedRootExpander->management_flags |= DEVICE_SET_ROOT;

		return ASD_STATE_FIND_BOUNDARY_FINISHED;
	}

	/*
	 * Movin' on up.
	 */
	ctx->expander = upstreamExpander;

	return ASD_STATE_FIND_BOUNDARY_LOOP;
}

DISCOVER_RESULTS
asd_DiscoverFindBoundarySM_Initialize(struct state_machine_context *
				      sm_contextp, void *init_args)
{
	struct state_information *state_infop;
	struct asd_DiscoverFindBoundarySM_Context *ctx;
	struct asd_DiscoverFindBoundarySM_Arguments *args;

	NEW_CONTEXT(ctx);

	args = (struct asd_DiscoverFindBoundarySM_Arguments *)init_args;

	ctx->expander = args->expander;
	ctx->found_listp = args->found_listp;
	ctx->old_discover_listp = args->old_discover_listp;

	SAS_ZERO(ctx->upstreamSASAddress);
	ctx->attachedPhyIdentifier = 0;
	ctx->attachedRootExpander = NULL;

	return DISCOVER_CONTINUE;
}

DISCOVER_RESULTS
asd_DiscoverFindBoundarySM_StateMachine(struct state_machine_context *
					sm_contextp)
{
	struct state_information *state_infop;
	struct asd_DiscoverFindBoundarySM_Context *ctx;
	DISCOVER_RESULTS results;
	ASD_DISCOVERY_STATES current_state;
	ASD_DISCOVERY_STATES new_state;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	current_state = GET_CURRENT_STATE();

	switch (current_state) {
	case ASD_STATE_FIND_BOUNDARY_START:
		new_state = asd_state_find_boundary_start(sm_contextp);
		break;

	case ASD_STATE_FIND_BOUNDARY_LOOP:
		new_state = asd_state_find_boundary_loop_post(sm_contextp);
		break;

	default:
		new_state = ASD_STATE_FIND_BOUNDARY_FAILED;
		break;
	}

	SM_new_state(sm_contextp, new_state);

	switch (new_state) {
	case ASD_STATE_FIND_BOUNDARY_LOOP:
		results = asd_state_find_boundary_loop(sm_contextp);
		break;

	case ASD_STATE_FIND_BOUNDARY_FINISHED:
		results = DISCOVER_FINISHED;
		break;

	default:
		results = DISCOVER_FAILED;
		break;
	}

	return results;
}

void
asd_DiscoverFindBoundarySM_Finish(struct state_machine_context *sm_contextp,
				  DISCOVER_RESULTS results)
{

	struct state_information *state_infop;
	struct asd_DiscoverFindBoundarySM_Context *ctx;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	RETURN_STACK(ctx->attachedRootExpander);
	RETURN_STACK(ctx->expander);
	RETURN_STACK(results);
}

void
asd_DiscoverFindBoundarySM_Abort(struct state_machine_context *sm_contextp)
{

	struct state_information *state_infop;
	struct asd_DiscoverFindBoundarySM_Context *ctx;

	GET_STATE_CONTEXT(sm_contextp, ctx);
}

/* -------------------------------------------------------------------------- */

struct asd_DiscoverExpanderSM_Context {
	struct discover_context *discover_contextp;
	struct asd_target *expander;
	unsigned phyIndex;
	uint8_t *sas_addr;
	unsigned attachedDeviceType;
	struct list_head *old_discover_listp;
	struct list_head *found_listp;
	struct asd_target *upstreamExpander;
	unsigned conn_rate;
};

DISCOVER_RESULTS
asd_state_issue_report_general(struct state_machine_context *sm_contextp)
{
	struct state_information *state_infop;
	struct asd_DiscoverExpanderSM_Context *ctx;
	DISCOVER_RESULTS results;
	unsigned conn_rate;
	struct discover_context *discover_contextp;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;

	/*
	 * Get the connection rate from the upstream expander.  The upstream
	 * expander is different than the "parent" expander with is based on
	 * the topology of the device set.
	 */
	if (ctx->upstreamExpander == NULL) {
		conn_rate = discover_contextp->port->conn_rate;
	} else {
		conn_rate = ctx->upstreamExpander->ddb_profile.conn_rate;
	}

	if (conn_rate > ctx->conn_rate) {
		conn_rate = ctx->conn_rate;
	}

	results = asd_issue_report_general(sm_contextp,
					   ctx->sas_addr,
					   conn_rate, ctx->attachedDeviceType,
					   ctx->old_discover_listp,
					   ctx->found_listp, &ctx->expander);

	return results;
}

ASD_DISCOVERY_STATES
asd_state_issue_report_general_post(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_DiscoverExpanderSM_Context *ctx;
	DISCOVER_RESULTS results;
	struct discover_context *discover_contextp;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;

	results = asd_issue_report_general_post(sm_contextp, ctx->expander);

	if (results != DISCOVER_OK) {

		printk("%s:%d: Report General Failed\n", __FUNCTION__,
		       __LINE__);
		return ASD_STATE_REPORT_AND_DISCOVER_FAILED;
	}

	return ASD_STATE_ISSUE_DISCOVER_LOOP;
}

DISCOVER_RESULTS
asd_state_issue_discover_loop(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_DiscoverExpanderSM_Context *ctx;
	DISCOVER_RESULTS results;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	results = asd_issue_discover_request(sm_contextp,
					     ctx->expander, ctx->phyIndex);

	return results;
}

ASD_DISCOVERY_STATES
asd_state_issue_discover_loop_post(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_DiscoverExpanderSM_Context *ctx;
	DISCOVER_RESULTS results;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	results = asd_issue_discover_request_post(sm_contextp,
						  ctx->expander, ctx->phyIndex);

	if (results != DISCOVER_OK) {
		printk("%s:%d: Discover Request Failed\n", __FUNCTION__,
		       __LINE__);

		return ASD_STATE_REPORT_AND_DISCOVER_FAILED;
	}

	ctx->phyIndex++;

	if (ctx->phyIndex == ctx->expander->num_phys) {
		return ASD_STATE_REPORT_AND_DISCOVER_FINISHED;
	}

	return ASD_STATE_ISSUE_DISCOVER_LOOP;
}

DISCOVER_RESULTS
asd_DiscoverExpanderSM_Initialize(struct state_machine_context *
				  sm_contextp, void *init_args)
{
	struct state_information *state_infop;
	struct asd_DiscoverExpanderSM_Context *ctx;
	struct asd_DiscoverExpanderSM_Arguments *args;

	NEW_CONTEXT(ctx);

	args = (struct asd_DiscoverExpanderSM_Arguments *)init_args;

	ctx->sas_addr = args->sas_addr;
	ctx->upstreamExpander = args->upstreamExpander;
	ctx->attachedDeviceType = args->attachedDeviceType;
	ctx->old_discover_listp = args->old_discover_listp;
	ctx->found_listp = args->found_listp;
	ctx->conn_rate = args->conn_rate;
	ctx->phyIndex = 0;
	ctx->expander = NULL;

	return DISCOVER_CONTINUE;
}

DISCOVER_RESULTS
asd_DiscoverExpanderSM_StateMachine(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_DiscoverExpanderSM_Context *ctx;
	DISCOVER_RESULTS results;
	ASD_DISCOVERY_STATES current_state;
	ASD_DISCOVERY_STATES new_state;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	current_state = GET_CURRENT_STATE();

	switch (current_state) {
	case ASD_STATE_REPORT_AND_DISCOVER_START:
		new_state = ASD_STATE_ISSUE_REPORT_GENERAL;
		break;

	case ASD_STATE_ISSUE_REPORT_GENERAL:
		new_state = asd_state_issue_report_general_post(sm_contextp);
		break;

	case ASD_STATE_ISSUE_DISCOVER_LOOP:
		new_state = asd_state_issue_discover_loop_post(sm_contextp);
		break;

	default:
		new_state = ASD_STATE_REPORT_AND_DISCOVER_FAILED;
		break;
	}

	SM_new_state(sm_contextp, new_state);

	switch (new_state) {
	case ASD_STATE_ISSUE_REPORT_GENERAL:
		results = asd_state_issue_report_general(sm_contextp);
		break;

	case ASD_STATE_ISSUE_DISCOVER_LOOP:
		results = asd_state_issue_discover_loop(sm_contextp);
		break;

	case ASD_STATE_REPORT_AND_DISCOVER_FINISHED:
		results = DISCOVER_FINISHED;
		break;

	default:
		results = DISCOVER_FAILED;
		break;
	}

	return results;
}

void
asd_DiscoverExpanderSM_Finish(struct state_machine_context *sm_contextp,
			      DISCOVER_RESULTS results)
{
	struct state_information *state_infop;
	struct asd_DiscoverExpanderSM_Context *ctx;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	RETURN_STACK(ctx->expander);
	RETURN_STACK(results);
}

void asd_DiscoverExpanderSM_Abort(struct state_machine_context *sm_contextp)
{
	struct state_information *state_infop;
	struct asd_DiscoverExpanderSM_Context *ctx;

	GET_STATE_CONTEXT(sm_contextp, ctx);
}

/* -------------------------------------------------------------------------- */

struct asd_DiscoverySM_Context {
	struct discover_context *discover_contextp;

	/*
	 * The expander that is directly attached to this initiator/port
	 */
	struct asd_target *connectedExpander;

	/*
	 * The expander that is at the root of this device set
	 */
	struct asd_target *thisDeviceSet;

	/*
	 * The expander that is at the root of the attached device set (if any).
	 * The attached device set is the the device set on the the other side
	 * of a subtractive-subtractive boundary.
	 */
	struct asd_target *attachedDeviceSet;

	/*
	 * The list that we discovered from thisDeviceSet.
	 */
	struct list_head discover_list;

	/*
	 * The list that we discovered from attachedDeviceSet.
	 */
	struct list_head attached_device_list;

	/*
	 * During the process of discovering expanders, we may find a
	 * device or expander that we don't know where to put at the moment.
	 */
	struct list_head found_list;

	/*
	 * The list from the previous discovery.  If there is anything left
	 * on this list at the end of discovery, the device is no longer
	 * present.
	 */
	struct list_head old_discover_list;
};

ASD_DISCOVERY_STATES
asd_state_discover_start(struct state_machine_context *sm_contextp)
{
	struct state_information *state_infop;
	struct asd_DiscoverySM_Context *ctx;
	struct asd_target *target;
	struct discover_context *discover_contextp;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;
	/*
	 * Invalidate all the targets, if any, found so far.  
	 */
	asd_invalidate_targets(discover_contextp->asd, discover_contextp->port);

	/*
	 * Move our list of formerly discovered targets to the "old discover
	 * list".  We will use this list later to figure out which devices have
	 * been removed.
	 */
	list_move_all(&ctx->old_discover_list,
		      &discover_contextp->port->targets);

	/*
	 * MDT. Saving pointers for easy access.
	 */
	discover_contextp->asd->old_discover_listp = &ctx->old_discover_list;
	discover_contextp->asd->discover_listp = &ctx->discover_list;

	if (discover_contextp->port->tree_root == NULL) {
		return ASD_STATE_FINISHED;
	}

	/*
	 * check to see if an expander is attached
	 */
	if ((discover_contextp->port->management_type !=
	     ASD_DEVICE_EDGE_EXPANDER) &&
	    (discover_contextp->port->management_type !=
	     ASD_DEVICE_FANOUT_EXPANDER)) {

		target = asd_find_target(&ctx->old_discover_list,
					 discover_contextp->port->tree_root->
					 ddb_profile.sas_addr);

		if (target != NULL) {
			/*
			 * This target was previously found.
			 */
			target->flags |= ASD_TARG_RESEEN;

			/*
			 * Take this target off of the old discover list.
			 */
			list_del_init(&target->all_domain_targets);
		}

		list_add_tail(&discover_contextp->port->tree_root->
			      all_domain_targets, &ctx->discover_list);

		/*
		 * There isn't an expander to discover, so move on to the
		 * initialization stage of the state machine.
		 */
		return ASD_STATE_SATA_SPINHOLD;
	}

	/*
	 * walk the list of targets, and re-initialize the tree links.
	 */
	list_for_each_entry(target, &ctx->old_discover_list, all_domain_targets) {

		target->parent = NULL;
		INIT_LIST_HEAD(&target->children);
		INIT_LIST_HEAD(&target->siblings);
		INIT_LIST_HEAD(&target->multipath);

		if ((target->management_type != ASD_DEVICE_EDGE_EXPANDER) &&
		    (target->management_type != ASD_DEVICE_FANOUT_EXPANDER)) {

			continue;
		}

		if (target->num_route_indexes == 0) {
			continue;
		}

		if (target->RouteTable != NULL) {
			memset(target->RouteTable, 0,
			       SAS_ADDR_LEN * target->num_phys *
			       target->num_route_indexes);
		}

		if (target->route_indexes != NULL) {
			memset(target->route_indexes, 0,
			       target->num_route_indexes * sizeof(uint16_t));
		}
	}

	/*
	 * Put our newly discovered, directly attached target in the found
	 * list.
	 */
	list_del_init(&discover_contextp->port->tree_root->all_domain_targets);

	list_add_tail(&discover_contextp->port->tree_root->all_domain_targets,
		      &ctx->found_list);

	return ASD_STATE_DISCOVER_ATTACHED;
}

DISCOVER_RESULTS
asd_state_discover_attached(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_DiscoverySM_Context *ctx;
	DISCOVER_RESULTS results;
	struct asd_DiscoverExpanderSM_Arguments args;
	uint8_t device_type;
	struct discover_context *discover_contextp;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;
	switch (discover_contextp->port->management_type) {
	case ASD_DEVICE_EDGE_EXPANDER:
		device_type = EDGE_EXPANDER_DEVICE;
		break;

	case ASD_DEVICE_FANOUT_EXPANDER:
		device_type = FANOUT_EXPANDER_DEVICE;
		break;

	default:
		/*
		 * If this isn't some kind of expander, then we shouldn't
		 * be here.
		 */
		results = DISCOVER_FAILED;
		return results;
	}

	/*
	 * There is no upstream expander from this expander, because it is
	 * the closest device to the initiator.
	 */
	args.sas_addr = discover_contextp->port->attached_sas_addr;
	args.upstreamExpander = NULL;
	args.attachedDeviceType = device_type;
	args.old_discover_listp = &ctx->old_discover_list;
	args.found_listp = &ctx->found_list;
	args.conn_rate = discover_contextp->port->conn_rate;

	results = ASD_PUSH_STATE_MACHINE(sm_contextp, &asd_DiscoverExpanderSM,
					 (void *)&args);

	return results;
}

ASD_DISCOVERY_STATES
asd_state_discover_attached_post(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_DiscoverySM_Context *ctx;
	DISCOVER_RESULTS results;
	struct asd_target *new_expander;
	struct discover_context *discover_contextp;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;
	/*
	 * The new expander is returned on the stack
	 */
	POP_STACK(results);
	POP_STACK(new_expander);

	if (results != DISCOVER_FINISHED) {
		return ASD_STATE_FAILED;
	}

	ctx->connectedExpander = new_expander;

	list_add_tail(&new_expander->all_domain_targets, &ctx->found_list);

	return ASD_STATE_FIND_BOUNDARY;
}

DISCOVER_RESULTS
asd_state_find_boundary(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_DiscoverySM_Context *ctx;
	DISCOVER_RESULTS results;
	struct asd_DiscoverFindBoundarySM_Arguments args;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	args.expander = ctx->connectedExpander;
	args.found_listp = &ctx->found_list;
	args.old_discover_listp = &ctx->old_discover_list;

	results = ASD_PUSH_STATE_MACHINE(sm_contextp,
					 &asd_DiscoverFindBoundarySM,
					 (void *)&args);

	return results;
}

ASD_DISCOVERY_STATES
asd_state_find_boundary_post(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_DiscoverySM_Context *ctx;
	DISCOVER_RESULTS results;
	struct asd_target *expander;
	struct asd_target *attachedRootExpander;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	POP_STACK(results);
	POP_STACK(expander);
	POP_STACK(attachedRootExpander);

	if (results != DISCOVER_FINISHED) {
		return ASD_STATE_FAILED;
	}

	ctx->thisDeviceSet = expander;
	ctx->attachedDeviceSet = attachedRootExpander;

	return ASD_STATE_CONFIG_BOUNDARY_SET;
}

DISCOVER_RESULTS
asd_state_config_boundary_set(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_DiscoverySM_Context *ctx;
	DISCOVER_RESULTS results;
	struct asd_DiscoverConfigSetSM_Arguments args;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	args.discover_listp = &ctx->discover_list;
	args.found_listp = &ctx->found_list;
	args.old_discover_listp = &ctx->old_discover_list;
	args.currentExpander = ctx->thisDeviceSet;

	results = ASD_PUSH_STATE_MACHINE(sm_contextp, &asd_DiscoverConfigSetSM,
					 (void *)&args);

	return results;
}

ASD_DISCOVERY_STATES
asd_state_config_boundary_set_post(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_DiscoverySM_Context *ctx;
	DISCOVER_RESULTS results;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	POP_STACK(results);

	if (results != DISCOVER_FINISHED) {
		return ASD_STATE_FAILED;
	}

	if (ctx->attachedDeviceSet == NULL) {
		return ASD_STATE_SATA_SPINHOLD;
	}

	return ASD_STATE_CONFIG_ATTACHED_SET;
}

DISCOVER_RESULTS
asd_state_config_attached_set(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_DiscoverySM_Context *ctx;
	DISCOVER_RESULTS results;
	struct asd_DiscoverConfigSetSM_Arguments args;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	args.discover_listp = &ctx->attached_device_list;
	args.found_listp = &ctx->found_list;
	args.old_discover_listp = &ctx->old_discover_list;
	args.currentExpander = ctx->attachedDeviceSet;

	results = ASD_PUSH_STATE_MACHINE(sm_contextp, &asd_DiscoverConfigSetSM,
					 (void *)&args);

	return results;
}

ASD_DISCOVERY_STATES
asd_state_config_attached_set_post(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_DiscoverySM_Context *ctx;
	DISCOVER_RESULTS results;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	POP_STACK(results);

	if (results != DISCOVER_FINISHED) {
		return ASD_STATE_FAILED;
	}

	/*
	 * put the domains together
	 */
	list_splice(&ctx->attached_device_list, ctx->discover_list.prev);

	return ASD_STATE_SATA_SPINHOLD;
}

DISCOVER_RESULTS
asd_state_sata_spinhold(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_DiscoverySM_Context *ctx;
	DISCOVER_RESULTS results;
	struct asd_SATA_SpinHoldSM_Arguments args;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	args.discover_listp = &ctx->discover_list;

	results = ASD_PUSH_STATE_MACHINE(sm_contextp, &asd_SATA_SpinHoldSM,
					 (void *)&args);

	return results;
}

ASD_DISCOVERY_STATES
asd_state_sata_spinhold_post(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_DiscoverySM_Context *ctx;
	DISCOVER_RESULTS results;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	POP_STACK(results);

	if (results != DISCOVER_FINISHED) {
		return ASD_STATE_FAILED;
	}

	return ASD_STATE_INIT_SATA;
}

DISCOVER_RESULTS asd_state_init_sata(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_DiscoverySM_Context *ctx;
	DISCOVER_RESULTS results;
	struct asd_InitSATA_SM_Arguments args;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	args.discover_listp = &ctx->discover_list;

	results = ASD_PUSH_STATE_MACHINE(sm_contextp, &asd_InitSATA_SM,
					 (void *)&args);

	return results;
}

ASD_DISCOVERY_STATES
asd_state_init_sata_post(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_DiscoverySM_Context *ctx;
	DISCOVER_RESULTS results;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	POP_STACK(results);

	if (results != DISCOVER_FINISHED) {
		//TODO: we can't give up, but we need to notify the user
	}

	return ASD_STATE_INIT_SAS;
}

DISCOVER_RESULTS asd_state_init_sas(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_DiscoverySM_Context *ctx;
	DISCOVER_RESULTS results;
	struct asd_InitSAS_SM_Arguments args;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	args.discover_listp = &ctx->discover_list;

	results = ASD_PUSH_STATE_MACHINE(sm_contextp, &asd_InitSAS_SM,
					 (void *)&args);

	return results;
}

ASD_DISCOVERY_STATES
asd_state_init_sas_post(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_DiscoverySM_Context *ctx;
	DISCOVER_RESULTS results;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	POP_STACK(results);

	if (results != DISCOVER_FINISHED) {
		return ASD_STATE_FAILED;
	}

	return ASD_STATE_INIT_SMP;
}

DISCOVER_RESULTS asd_state_init_smp(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_DiscoverySM_Context *ctx;
	DISCOVER_RESULTS results;
	struct asd_InitSMP_SM_Arguments args;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	args.discover_listp = &ctx->discover_list;

	results = ASD_PUSH_STATE_MACHINE(sm_contextp, &asd_InitSMP_SM,
					 (void *)&args);

	return results;
}

ASD_DISCOVERY_STATES
asd_state_init_smp_post(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_DiscoverySM_Context *ctx;
	DISCOVER_RESULTS results;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	POP_STACK(results);

	if (results != DISCOVER_FINISHED) {
		return ASD_STATE_FAILED;
	}

	return ASD_STATE_FINISHED;
}

DISCOVER_RESULTS
asd_DiscoverySM_Initialize(struct state_machine_context * sm_contextp,
			   void *init_args)
{
	struct state_information *state_infop;
	struct asd_DiscoverySM_Context *ctx;
	struct discover_context *discover_contextp;

	NEW_CONTEXT(ctx);

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;
	discover_contextp->retry_count=0;
	ctx->connectedExpander = NULL;
	ctx->thisDeviceSet = NULL;
	ctx->attachedDeviceSet = NULL;

	INIT_LIST_HEAD(&ctx->discover_list);
	INIT_LIST_HEAD(&ctx->attached_device_list);
	INIT_LIST_HEAD(&ctx->found_list);
	INIT_LIST_HEAD(&ctx->old_discover_list);

	return DISCOVER_CONTINUE;
}

DISCOVER_RESULTS
asd_DiscoverySM_StateMachine(struct state_machine_context * sm_contextp)
{
	struct state_information *state_infop;
	struct asd_DiscoverySM_Context *ctx;
	DISCOVER_RESULTS results;
	ASD_DISCOVERY_STATES current_state;
	ASD_DISCOVERY_STATES new_state;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	current_state = GET_CURRENT_STATE();

	switch (current_state) {
	case ASD_STATE_DISCOVER_START:
		new_state = asd_state_discover_start(sm_contextp);
		break;

	case ASD_STATE_DISCOVER_ATTACHED:
		new_state = asd_state_discover_attached_post(sm_contextp);
		break;

	case ASD_STATE_FIND_BOUNDARY:
		new_state = asd_state_find_boundary_post(sm_contextp);
		break;

	case ASD_STATE_CONFIG_BOUNDARY_SET:
		new_state = asd_state_config_boundary_set_post(sm_contextp);
		break;

	case ASD_STATE_CONFIG_ATTACHED_SET:
		new_state = asd_state_config_attached_set_post(sm_contextp);
		break;

	case ASD_STATE_SATA_SPINHOLD:
		new_state = asd_state_sata_spinhold_post(sm_contextp);
		break;

	case ASD_STATE_INIT_SATA:
		new_state = asd_state_init_sata_post(sm_contextp);
		break;

	case ASD_STATE_INIT_SAS:
		new_state = asd_state_init_sas_post(sm_contextp);
		break;

	case ASD_STATE_INIT_SMP:
		new_state = asd_state_init_smp_post(sm_contextp);
		break;

	default:
		new_state = ASD_STATE_FAILED;
		break;
	}

	SM_new_state(sm_contextp, new_state);

	switch (new_state) {
	case ASD_STATE_DISCOVER_ATTACHED:
		results = asd_state_discover_attached(sm_contextp);
		break;

	case ASD_STATE_FIND_BOUNDARY:
		results = asd_state_find_boundary(sm_contextp);
		break;

	case ASD_STATE_CONFIG_BOUNDARY_SET:
		results = asd_state_config_boundary_set(sm_contextp);
		break;

	case ASD_STATE_CONFIG_ATTACHED_SET:
		results = asd_state_config_attached_set(sm_contextp);
		break;

	case ASD_STATE_SATA_SPINHOLD:
		results = asd_state_sata_spinhold(sm_contextp);
		break;

	case ASD_STATE_INIT_SATA:
		results = asd_state_init_sata(sm_contextp);
		break;

	case ASD_STATE_INIT_SAS:
		results = asd_state_init_sas(sm_contextp);
		break;

	case ASD_STATE_INIT_SMP:
		results = asd_state_init_smp(sm_contextp);
		break;

	case ASD_STATE_FINISHED:
		results = DISCOVER_FINISHED;
		break;

	default:
		results = DISCOVER_FAILED;
		break;
	}

	return results;
}

/* ---------- SES stuff for IBM ---------- */

/* Since this may be compiled for a 2.4 kernel which does not have
 * completion variables, we have to use wait queues explicitly.
 */
static void asd_ssp_task_done(struct asd_softc *asd, struct scb *scb,
			      struct asd_done_list *dl)
{
	wait_queue_head_t *wqh;

	wqh = (wait_queue_head_t *)scb->io_ctx;

	// RST - might not need this
//JD	list_del(&scb->owner_links);

	scb->dl = dl;

	wake_up(wqh);
}

static int asd_send_ssp_task(struct asd_target *target, u8 * cmd, int cmd_len,
			       dma_addr_t dma_handle, unsigned xfer_len,
			       unsigned direction)
{
	static DECLARE_WAIT_QUEUE_HEAD(wqh);
	DECLARE_WAITQUEUE(wait, current);

	struct asd_softc *asd = target->softc;
	struct asd_ssp_task_hscb *ssp_hscb;
	unsigned long flags;
	struct scb *scb;
	struct sg_element *sg;

	int err = 0;

	asd_lock(asd, &flags);

	scb = asd_hwi_get_scb(asd, 0);
	if (!scb) {
		asd_unlock(asd, &flags);
		return -ENOMEM;
	}
	scb->flags |= SCB_INTERNAL;
	scb->platform_data->targ = target;
	scb->platform_data->dev = NULL;
	INIT_LIST_HEAD(&scb->owner_links);
	ssp_hscb = &scb->hscb->ssp_task;
	ssp_hscb->header.opcode = SCB_INITIATE_SSP_TASK;
	asd_build_sas_header(target, ssp_hscb);
	ssp_hscb->protocol_conn_rate |= PROTOCOL_TYPE_SSP;
	ssp_hscb->data_dir_flags |= direction;
	ssp_hscb->xfer_len = asd_htole32(xfer_len);
	memset(ssp_hscb->cdb, 0, sizeof(ssp_hscb->cdb));
	memcpy(ssp_hscb->cdb, cmd, cmd_len);
	sg = scb->sg_list;
	scb->platform_data->buf_busaddr = dma_handle;
	asd_sg_setup(sg, dma_handle, xfer_len, /*last */ 1);
	memcpy(ssp_hscb->sg_elements, scb->sg_list, sizeof(*sg));
	scb->sg_count = 1;
	asd_push_post_stack(asd, scb, (void *)&wqh, asd_ssp_task_done);
	scb->flags |= SCB_ACTIVE;
	scb->dl = NULL;

	add_wait_queue(&wqh, &wait);
	set_current_state(TASK_INTERRUPTIBLE);

	asd_hwi_post_scb(asd, scb);

	asd_unlock(asd, &flags);

	schedule_timeout(2 * HZ);	/* wait the task to come back or 2 seconds */

	remove_wait_queue(&wqh, &wait);

	if (!scb->dl) {
		asd_dprint("%s: no dl! something went terribly wrong!\n",
			   __FUNCTION__);
		err = -101;	/* What? */
		asd_hwi_free_scb(asd, scb);
		goto out;
	} else {
		err = -102;
		/* Ok process the status of the task */
		switch (scb->dl->opcode) {
		case TASK_COMP_WO_ERR:
			err = 0;
			break;
		case TASK_COMP_W_UNDERRUN:
			asd_dprint("task comp w underrun\n");
			break;
		case TASK_COMP_W_OVERRUN:
			asd_dprint("task comp w overrun\n");
			break;
		case SSP_TASK_COMP_W_RESP:
			asd_dprint("task comp w resp\n");
			{
				union edb 		*edb;
				struct scb 		*escb;
				u_int			edb_index;
				struct asd_done_list	*done_listp;
				done_listp=scb->dl;
				edb = asd_hwi_get_edb_from_dl(asd, scb, done_listp, &escb, &edb_index);
				if (edb != NULL) {
					asd_hwi_free_edb(asd, escb, edb_index);
				}
			}
			break;
		default:
			asd_dprint("task comp w status 0x%02x\n",
				   scb->dl->opcode);
			break;
		}
	}

	asd_lock(asd, &flags);
	asd_scb_internal_done(asd, scb, scb->dl);
	asd_unlock(asd, &flags);
      out:
	return err;
}

static int asd_get_ses_page(struct asd_target *ses_device, int ses_page,
			    u8 * buf, unsigned buf_len, dma_addr_t dma_handle)
{
	static u8 recv_diag[6] = {
		0x1c, 0x01, 0x00, 0x00, 0x00, 0x00
	};
	int err;

	memset(buf, 0, buf_len);
	recv_diag[2] = ses_page;	/* page code */
	recv_diag[3] = (buf_len & 0xFF00) >> 8;
	recv_diag[4] = (buf_len & 0x00FF);
	err = asd_send_ssp_task(ses_device, recv_diag, 6, dma_handle,
				buf_len, DATA_DIR_INBOUND);
	if (err)
		asd_dprint("couldn't get SES page: error: 0x%x\n", err);

	return err;
}

/* return negative on error, or the page length */
static int asd_read_ses_page(struct asd_target *ses_device, int page_code,
			     u8 * page, dma_addr_t dma_handle,
			     unsigned max_buffer_len)
{
	int i;
	int err;
	int alloc_len;

	err = asd_get_ses_page(ses_device, page_code, page, 4, dma_handle);
	if (err) {
		asd_dprint("couldn't get ses page 0x%x\n", page_code);
		goto out;
	}

	asd_dprint("%02x: %02x %02x %02x %02x\n", 0,
		   page[0], page[1], page[2], page[3]);

	alloc_len = (page[2] << 8) | page[3];
	alloc_len += 4;
	asd_dprint("ses sends %d bytes for page 0x%x\n", alloc_len, page_code);
	err = -ENOMEM;
	if (alloc_len <= 4) {
		asd_dprint("page 0x%x too small: %d bytes\n", page_code,
			   alloc_len);
		goto out;
	} else if (alloc_len > max_buffer_len) {
		asd_dprint("page 0x%x too big: %d bytes\n", page_code,
			   alloc_len);
		goto out;
	}
	err = asd_get_ses_page(ses_device, page_code, page, alloc_len,
			       dma_handle);
	if (err) {
		asd_dprint("couldn't get ses page 0x%x\n", page_code);
		goto out;
	}
	asd_dprint("page 0x%x:\n", page_code);
	for (i = 0; i < alloc_len; i += 4)
		asd_dprint("%02x: %02x %02x %02x %02x\n", i,
			   page[i], page[i + 1], page[i + 2], page[i + 3]);
	err = alloc_len;
      out:
	return err;
}

struct ses_order {
	int ord;		/* element order - 1 from SES pages */
	int slot;		/* SLOT %03d from page 7 */
	u8 sas_addr[SAS_ADDR_LEN];	/* device sas address from page 0xA */
};

static struct ses_order *asd_parse_page_7(const u8 * page, const int len,
					  int *num)
{
	struct ses_order *order = NULL;
	int el_desc, el;
	int total = 0, i;

	if (len < 0xC)
		goto out;
	/* skip the overall descriptor */
	el_desc = ((page[0xA] << 8) | page[0xB]) + 0xC;
	if (el_desc >= len)
		goto out;
	/* Count the elements */
	el = el_desc;		/* points to first el desc */
	do {
		int size = (page[el + 2] << 8) | page[el + 3];
		if (!size)
			break;
		el += 4 + size;
		total++;
	} while (el < len);
	asd_dprint("total elements: %d\n", total);
	order = kmalloc(sizeof(struct ses_order) * total, GFP_KERNEL);
	if (!order)
		goto out;
	el = el_desc;
	for (i = 0; i < total; i++) {
		order[i].ord = i;
		sscanf((char *)page + el + 9, "%d", &order[i].slot);
		el += 4 + ((page[el + 2] << 8) | page[el + 3]);
	}
	*num = total;
      out:
	return order;
}

static int asd_parse_page_Ah(const u8 * page, const int len,
			     struct ses_order *order, const int num_els)
{
	int err = 0;
	int el;
	int index;

	if (len < 0xC)
		goto out;

	index = 0;
	for (el = 0x8; el < len; el += 2 + page[el + 1]) {
		u8 proto = page[el];

		if (proto != 0x6)
			continue;
		if (index >= num_els)
			break;
		memcpy(order[index].sas_addr, page + el + 0x10, SAS_ADDR_LEN);

		index++;	/* count only SAS devices */
	}
      out:return err;
}

static void asd_do_actual_reorder(struct asd_target *root_target,
				  struct asd_target *ses_device,
				  const struct ses_order *order,
				  const int num_els)
{
	struct asd_target *dev;

	list_for_each_entry(dev, &root_target->children, siblings) {
		int i;

		for (i = 0; i < num_els; i++) {
			if (order[i].sas_addr[0] == 0)
				continue;
			if (memcmp(dev->ddb_profile.sas_addr,
				   order[i].sas_addr, SAS_ADDR_LEN) == 0) {
				dev->target_id = order[i].slot;
				asd_dprint("%p->target_id=%d, sas_addr:%llx\n",
					   dev, dev->target_id,
					   be64_to_cpu(*(u64 *) dev->
						       ddb_profile.sas_addr));
				break;
			}
		}
	}
}
static int asd_do_device_reorder(struct asd_target *root_target,
				 struct asd_target *ses_device)
{
	int len = 0, num_els=0, i;
	struct asd_softc *asd = ses_device->softc;
	struct pci_dev *pcidev = asd_dev_to_pdev(asd->dev);
	dma_addr_t dma_handle;
	u8 *page = pci_alloc_consistent(pcidev, PAGE_SIZE, &dma_handle);
	struct ses_order *order;

	/* read and interpret page 7 */
	len = asd_read_ses_page(ses_device, 7, page, dma_handle, PAGE_SIZE);
	if (len <= 0)
		goto out;
	order = asd_parse_page_7(page, len, &num_els);
	for (i = 0; i < num_els; i++)
		asd_dprint("ord:%d, slot:%d\n", order[i].ord, order[i].slot);

	/* read and interpret page Ah */
	len = asd_read_ses_page(ses_device, 0xA, page, dma_handle, PAGE_SIZE);
	if (len <= 0) {
		asd_dprint("reading page Ah error:%d\n", len);
		goto out_order;
	}
	i = asd_parse_page_Ah(page, len, order, num_els);
	if (i) {
		asd_dprint("couldn't parse page Ah, error:%d\n", i);
		goto out_order;
	}
	for (i = 0; i < num_els; i++)
		asd_dprint("ord:%d, slot:%d, sas addr:%llx\n",
			   order[i].ord, order[i].slot,
			   be64_to_cpu(*(u64 *) order[i].sas_addr));

	asd_do_actual_reorder(root_target, ses_device, order, num_els);

      out_order:
	kfree(order);
      out:
	pci_free_consistent(pcidev, PAGE_SIZE, page, dma_handle);
	return len;
}

static void asd_examine_ses(struct asd_softc *asd, struct asd_port *port)
{
	struct asd_target *root_target;
	struct asd_target *target;
	u8 *inquiry;

	root_target = port->tree_root;

	if (root_target == NULL) {
		return;
	}

	/* We ignore SES devices which are "target roots":
	 * Since we only care about the ordering of targets
	 * "managed" by SES devices -- meaning on the same "level"
	 * as the SES device, or at least this is how it's for IBM. */

	list_for_each_entry(target, &root_target->children, siblings) {

		if (target->command_set_type != ASD_COMMAND_SET_SCSI) {
			continue;
		}

		inquiry = target->scsi_cmdset.inquiry;

		if (inquiry == NULL) {
			continue;
		} else if ((inquiry[0] & 0x1f) == TYPE_ENCLOSURE
			   && strncmp((char *)inquiry + 8, "IBM-ESXS", 8) == 0
			   && strncmp((char *)inquiry + 16, "VSC7160", 7) == 0) {
			asd_do_device_reorder(root_target, target);
			break;
		} else if ((inquiry[0] & 0x1f) == TYPE_ENCLOSURE
			   && strncmp((char *)inquiry + 8, "ADAPTEC", 7) == 0
			   && strncmp((char *)inquiry + 16, "SANbloc", 7) == 0) {
			asd_dprint("Adaptec SANbloc!!!\n");
			asd_do_device_reorder(root_target, target);
			break;
		}
	}
}

/* ---------- end SES stuff ---------- */

void
asd_DiscoverySM_Finish(struct state_machine_context *sm_contextp,
		       DISCOVER_RESULTS results)
{
	struct state_information *state_infop;
	struct asd_DiscoverySM_Context *ctx;
	struct discover_context *discover_contextp;
	unsigned long flags;
	struct asd_phy *phy;
	uint8_t conn_mask;
	unsigned num_discovery;

	if(results == DISCOVER_FAILED) return;

	GET_STATE_CONTEXT(sm_contextp, ctx);

	discover_contextp = (struct discover_context *)
	    sm_contextp->state_handle;
	discover_contextp->retry_count=0;
	/*
	 * We don't necessarily update the mask when we start discovery.  Set
	 * it to the right value.
	 */
	conn_mask = 0;

	list_for_each_entry(phy, &discover_contextp->port->phys_attached, 
		links) {

		conn_mask |= (1 << phy->id);
	};

	discover_contextp->port->conn_mask = conn_mask;

	asd_apply_conn_mask(discover_contextp->asd, &ctx->discover_list);
	if (discover_contextp->asd->platform_data->flags & ASD_DISCOVERY_INIT) {

		// TODO: this is not MP safe, even with the lock.

		asd_lock(discover_contextp->asd, &flags);

		discover_contextp->asd->num_discovery--;

		num_discovery = discover_contextp->asd->num_discovery;

		asd_unlock(discover_contextp->asd, &flags);

		INIT_LIST_HEAD(&discover_contextp->port->targets_to_validate);

		list_move_all(&discover_contextp->port->targets_to_validate,
			      &ctx->discover_list);
#ifndef NO_SES_SUPPORT
		asd_examine_ses(discover_contextp->asd,
				discover_contextp->port);
#endif
		if (num_discovery == 0)
			asd_validate_targets_init(discover_contextp->asd);
	} else {
		/*
		 * Hot Plug.
		 */
		/*
		 * Validate any new targets that have been hot-plugged or 
		 * hot-removed.
		 */
#ifndef NO_SES_SUPPORT
		asd_examine_ses(discover_contextp->asd,
				discover_contextp->port);
#endif
		asd_validate_targets_hotplug(discover_contextp->asd,
					     discover_contextp->port, ctx);

		list_move_all(&discover_contextp->port->targets,
			      &ctx->discover_list);
	}

	discover_contextp->port->events &= ~ASD_DISCOVERY_PROCESS;

	asd_wakeup_sem(&discover_contextp->asd->platform_data->discovery_sem);
#ifdef ASD_DEBUG
	asd_dump_tree(discover_contextp->asd, discover_contextp->port);
#endif
	asd_dprint("%s: --- EXIT\n", __FUNCTION__);
}

void asd_DiscoverySM_Abort(struct state_machine_context *sm_contextp)
{
	struct state_information *state_infop;
	struct asd_DiscoverySM_Context *ctx;

	GET_STATE_CONTEXT(sm_contextp, ctx);
}

/* -------------------------------------------------------------------------- */

DISCOVER_RESULTS
asd_do_discovery(struct asd_softc *asd, struct asd_port *port)
{
	DISCOVER_RESULTS results;

	port->dc.sm_context.state_stack_top = -1;

	port->dc.asd = asd;
	port->dc.port = port;

	port->dc.sm_context.state_handle = (void *)&port->dc;
	port->dc.sm_context.wakeup_state_machine =
	    asd_discover_wakeup_state_machine;

	results = ASD_PUSH_STATE_MACHINE(&port->dc.sm_context,
		&asd_DiscoverySM, NULL);

	if (results == DISCOVER_CONTINUE) {
		results = asd_run_state_machine(&port->dc.sm_context);
	}

	return results;
}

void asd_abort_discovery(struct asd_softc *asd, struct asd_port *port)
{
	asd_abort_state_machine(&port->dc.sm_context);
}

static void
asd_invalidate_targets(struct asd_softc *asd, struct asd_port *port)
{
	struct asd_target *target;

	list_for_each_entry(target, &port->targets, all_domain_targets) {
		target->flags &= ~ASD_TARG_ONLINE;
	}
}

static void
asd_apply_conn_mask(struct asd_softc *asd, struct list_head *discover_list)
{
	struct asd_target *target;

	if (list_empty(discover_list)) {
		return;
	}

	list_for_each_entry(target, discover_list, all_domain_targets) {
		asd_hwi_build_ddb_site(asd, target);
	}
}

int asd_map_multipath(struct asd_softc *asd, struct asd_target *target)
{
	int ret;
	struct asd_target *multipath_target;
	struct asd_device *dev;
	unsigned i;

	/*
	 * Check to make sure that this same device hasn't been exposed to the
	 * OS on a different port.
	 */
	multipath_target = asd_find_multipath(asd, target);

	if (multipath_target != NULL) {
		list_add_tail(&target->multipath, &multipath_target->multipath);

		return 1;
	}

	ret = asd_map_target(asd, target);

	if (ret != 0) {
		return ret;
	}

	target->flags |= ASD_TARG_MAPPED;

	/*
	 * At this point in time, we know how many luns this device has,
	 * create the device structures for each LUN.
	 */
	switch (target->command_set_type) {
	case ASD_COMMAND_SET_SCSI:
		for (i = 0; i < target->scsi_cmdset.num_luns; i++) {
			dev = asd_alloc_device(asd, target,
					       target->src_port->id,
					       target->target_id, i);

			memcpy(dev->saslun, &target->scsi_cmdset.luns[i],
			       SAS_LUN_LEN);
		}
		break;

	case ASD_COMMAND_SET_ATA:
	case ASD_COMMAND_SET_ATAPI:
		dev = asd_alloc_device(asd, target,
				       target->src_port->id, target->target_id,
				       0);

		memset(dev->saslun, 0, SAS_LUN_LEN);
		break;

	default:
		break;
	}

	return ret;

}

void asd_mark_duplicates(struct asd_softc *asd, struct list_head *discover_list)
{
	struct asd_target *target;
	struct asd_target *tmp_target;
	struct asd_target *check_target;
	struct list_head map_list;
	unsigned found_mapped;

	INIT_LIST_HEAD(&map_list);

	/*
	 * Walk down the discovery list looking for duplicate devices.  If we
	 * find a duplicate device, chain it to the mutipath list of the primary
	 * target.  Put all of the unique devices in a list to mapped.
	 */
	while (!list_empty(discover_list)) {

		target = list_entry(discover_list->next,
				    struct asd_target, all_domain_targets);

		list_del_init(&target->all_domain_targets);

		/*
		 * We only care about presenting end devices to the OS.
		 */
		if (target->management_type != ASD_DEVICE_END) {

			list_add_tail(&target->all_domain_targets, &map_list);

			continue;
		}

		switch (target->transport_type) {
		case ASD_TRANSPORT_ATA:
			list_add_tail(&target->all_domain_targets, &map_list);

			continue;

		case ASD_TRANSPORT_STP:
		case ASD_TRANSPORT_SSP:
			break;

		default:
			list_add_tail(&target->all_domain_targets, &map_list);
			continue;
		}

		found_mapped = 0;

		/*
		 * Walk through the list marking each target that matches this
		 * target as a duplicate.
		 */
		list_for_each_entry_safe(check_target, tmp_target,
					 discover_list, all_domain_targets) {

			/*
			 * If they are not the same type of device, then
			 * they can not be duplicates.
			 */
			if (target->transport_type !=
			    check_target->transport_type) {

				continue;
			}

			switch (target->transport_type) {
			case ASD_TRANSPORT_SSP:
				/*
				 * If the lengths of the identification values
				 * don't match, then the values themselves
				 * can't match.
				 */
				if (target->scsi_cmdset.ident_len !=
				    check_target->scsi_cmdset.ident_len) {

					continue;
				}

				if ((check_target->scsi_cmdset.ident == NULL) ||
				    (target->scsi_cmdset.ident == NULL)) {
					continue;
				}

				if (memcmp(target->scsi_cmdset.ident,
					   check_target->scsi_cmdset.ident,
					   target->scsi_cmdset.ident_len) !=
				    0) {

					continue;
				}
				break;

			case ASD_TRANSPORT_STP:
				if (!SAS_ISEQUAL(target->ddb_profile.sas_addr,
						 check_target->ddb_profile.
						 sas_addr)) {

					continue;
				}

				break;

			default:
				continue;
			}

			/*
			 * We have found a duplicate device.
			 */
			list_del_init(&check_target->all_domain_targets);

			if (check_target->flags & ASD_TARG_MAPPED) {
				/*
				 * This is a device that has not been
				 * discovered before that matches a device
				 * that has already been discovered and is
				 * exposed to the OS.
				 */
				list_add_tail(&target->multipath,
					      &check_target->multipath);

				list_add_tail(&check_target->all_domain_targets,
					      &map_list);

				found_mapped = 1;

				continue;
			}

			/*
			 * This device is a multipath, so we will not map it.
			 */
			list_add_tail(&check_target->multipath,
				      &target->multipath);
		}

		if (found_mapped == 0) {
			/*
			 * We didn't find another matching target, so we
			 * will add this target to our list of devices to be
			 * mapped.
			 */
			list_add_tail(&target->all_domain_targets, &map_list);
		}
	}

	/*
	 * Walk down the list of targets to be mapped, and map them.
	 */
	list_for_each_entry_safe(target, tmp_target, &map_list,
				 all_domain_targets) {

		/*
		 * Only present end devices that didn't fail discovery
		 * to the OS.
		 */
		if ((target->management_type != ASD_DEVICE_END) || 
		    (target->command_set_type == ASD_COMMAND_SET_BAD)) {
			continue;
		}

		if (target->flags & ASD_TARG_MAPPED) {
			continue;
		}

		target->flags |= ASD_TARG_NEEDS_MAP;

	}

	list_move_all(discover_list, &map_list);
}

void
asd_remap_device(struct asd_softc *asd,
		 struct asd_target *target, struct asd_target *multipath_target)
{
	unsigned i;
	struct asd_domain *dm;

	if ((target->flags & ASD_TARG_MAPPED) == 0) {
		return;
	}

	multipath_target->flags |= ASD_TARG_MAPPED;

	target->flags &= ~ASD_TARG_MAPPED;

	dm = target->domain;

	if (multipath_target->target_id != ASD_MAX_TARGET_IDS) {
		/* The target ID was assigned in the discover code,
		 * possibly via querying an SES device.
		 */
		if (dm->targets[multipath_target->target_id] == NULL) {
			dm->targets[multipath_target->target_id] =
			    multipath_target;
		} else if (dm->targets[multipath_target->target_id]
			   != multipath_target) {
			asd_dprint("oops: target with id %d already exists\n",
				   multipath_target->target_id);
			return;
		}
	} else {
#ifndef NO_SES_SUPPORT
		for (i = 128; i < ASD_MAX_TARGET_IDS; i++) {
#else
		for (i = 0; i < (ASD_MAX_TARGET_IDS - 128); i++) {
#endif
			if (dm->targets[i] != target)
				continue;
			dm->targets[i] = multipath_target;
			multipath_target->target_id = i;
			break;
		}
	}

	multipath_target->domain = dm;
	multipath_target->refcount = target->refcount;

	target->domain = NULL;

	for (i = 0; i < ASD_MAX_LUNS; i++) {

		if (target->devices[i] == NULL) {
			continue;
		}

		multipath_target->devices[i] = target->devices[i];
		multipath_target->devices[i]->target = multipath_target;
#ifdef MULTIPATH_IO
		multipath_target->devices[i]->current_target = multipath_target;
#endif
	}
}

/*
 * At this point, all targets are either on their port->target list or on
 * the context discover_list.  There can be more than one discovery going on
 * simulataneously, but each one will call asd_validate_targets when it 
 * completes.
 */
static void asd_validate_targets_init(struct asd_softc *asd)
{
	struct asd_target *target;
	struct asd_target *tmp_target;
	struct asd_port *port;
	unsigned port_id;
	struct list_head map_list;
#ifdef OCM_REMAP
	unsigned i;
	struct asd_phy *phy;
	struct asd_unit_element_format *punit_elm;
	struct asd_uid_lu_naa_wwn *punit_id_lun;
	struct asd_uid_lu_sata_model_num *punit_id_dir_sata;
#endif

	INIT_LIST_HEAD(&map_list);

	/*
	 * This code has been turned off in favor of using labels or UUIDS to
	 * determine drive to filesystem mapping.
	 */
#ifdef OCM_REMAP
	/*
	 * Do one pass through the OCM structure that was passed by the BIOS.
	 * The devices will go at the head of the "map_list".  Since these
	 * devices are going on the list first, they will also be the exported
	 * targets if we find duplicates later.
	 */
	for (i = 0; i < asd->num_unit_elements; i++) {

		punit_elm = &asd->unit_elements[i];

		switch (punit_elm->type) {
		case UNELEM_LU_SATA_MOD_SERIAL_NUM:
			punit_id_dir_sata = (struct asd_uid_lu_sata_model_num *)
			    punit_elm->id;

#if 0
			printk("%s:%d: UNIT_ELEMENT_DIRECT_ATTACHED_SATA "
			       "phy ID %d\n", __FUNCTION__, __LINE__,
			       punit_id_dir_sata->phy_id);
#endif

			port = NULL;
			phy = NULL;

			for (port_id = 0; port_id < asd->hw_profile.max_ports;
			     port_id++) {

				port = asd->port_list[port_id];

				list_for_each_entry(phy,
						    &port->phys_attached,
						    links) {

					if (phy->id ==
					    punit_id_dir_sata->phy_id) {
						break;
					}
				}
			}
			if (port_id != asd->hw_profile.max_ports) {
				if (list_empty(&port->targets_to_validate)) {
					target =
					    list_entry(port->
						       targets_to_validate.next,
						       struct asd_target,
						       all_domain_targets);

					target->flags |= ASD_TARG_ONLINE;

					list_del_init(&target->
						      all_domain_targets);

					list_add_tail(&target->
						      all_domain_targets,
						      &map_list);

					target->flags |= ASD_TARG_MAP_BOOT;
				} else {
					printk("Warning: no devices on target "
					       "list for phy %d\n", phy->id);
				}
			} else {
				printk("Warning: could not find direct phy ID "
				       "matching %d\n",
				       punit_id_dir_sata->phy_id);
			}
			break;
		case UNELEM_LU_NAA_WWN:
			punit_id_lun = (struct asd_uid_lu_naa_wwn *)
			    punit_elm->id;

#if 0
			printk("%s:%d: UNIT_ELEMENT_LOGICAL_UNIT %llx\n",
			       __FUNCTION__, __LINE__,
			       asd_be64toh(*((uint64_t *) punit_id_lun->
					     sas_address)));
#endif

			target = NULL;

			for (port_id = 0; port_id < asd->hw_profile.max_ports;
			     port_id++) {

				port = asd->port_list[port_id];

				target =
				    asd_find_target(&port->targets_to_validate,
						    punit_id_lun->sas_address);

				if (target != NULL) {
					break;
				}
			}

			if (target != NULL) {
				target->flags |= ASD_TARG_ONLINE;

				list_del_init(&target->all_domain_targets);

				list_add_tail(&target->all_domain_targets,
					      &map_list);

				target->flags |= ASD_TARG_MAP_BOOT;
			} else {
				printk("Warning: Couldn't find "
				       "UNIT_ELEMENT_LOGICAL_UNIT %llx\n",
				       asd_be64toh(*((uint64_t *) punit_id_lun->
						     sas_address)));
			}

			break;
		case UNELEM_VOLUME_SET_DDF_GUID:
			printk("%s:%d: UNIT_ELEMENT_VOLUME_SET\n",
			       __FUNCTION__, __LINE__);
			break;

		case UNELEM_LU_SGPIO:
			printk("%s:%d: UNELEM_LU_SGPIO\n",
			       __FUNCTION__, __LINE__);
			break;

		case UNELEM_LU_SAS_TOPOLOGY:
			printk("%s:%d: UNELEM_LU_SAS_TOPOLOGY\n",
			       __FUNCTION__, __LINE__);
			break;
		}

	}
#endif

	/*
	 * Now go through the devices that are left and put them on our list
	 * of potential devices to be exported to the OS.
	 */
	for (port_id = 0; port_id < asd->hw_profile.max_ports; port_id++) {

		port = asd->port_list[port_id];

		list_for_each_entry_safe(target, tmp_target,
					 &port->targets_to_validate,
					 all_domain_targets) {

			target->flags |= ASD_TARG_ONLINE;

			if (target->management_type == ASD_DEVICE_END) {
				target->flags |= ASD_TARG_NEEDS_MAP;
			}
//JD
			else
			{
				asd_log(ASD_DBG_INFO, "port_id (0x%x) target->management_type (0x%x) is not end_device\n",port_id,target->management_type);
			}
			list_del_init(&target->all_domain_targets);

			list_add_tail(&target->all_domain_targets, &map_list);
		}
	}

	asd_mark_duplicates(asd, &map_list);

	list_for_each_entry_safe(target, tmp_target,
				 &map_list, all_domain_targets) {

		list_del_init(&target->all_domain_targets);

		if ((target->flags & ASD_TARG_NEEDS_MAP) != 0) {

			target->flags &= ~ASD_TARG_NEEDS_MAP;

			if (asd_map_multipath(asd, target) != 0) {
				// TODO: fix potential leak

				continue;
			}
		}

		list_add_tail(&target->all_domain_targets,
			      &target->src_port->targets);
	}

	return;
}

static void
asd_validate_targets_hotplug(struct asd_softc *asd,
			     struct asd_port *port,
			     struct asd_DiscoverySM_Context *ctx)
{
	struct asd_target *target;
	struct asd_target *tmp_target;
	struct asd_target *multipath_target;

	INIT_LIST_HEAD(&port->targets_to_validate);

	/*
	 * Go through the old discover list and see if any devices that 
	 * have been removed.  If this is our first time through the discovery
	 * process, then there will be nothing on the old_discover_list.  The
	 * old_discover_list has all of the devices that were seen in a
	 * previous discover, but are no longer present.
	 */
	list_for_each_entry_safe(target, tmp_target,
				 &ctx->old_discover_list, all_domain_targets) {

		list_del_init(&target->all_domain_targets);

		list_add_tail(&target->validate_links,
			      &port->targets_to_validate);

		/* 
		 * The target was previously seen and now has gone missing.
		 */
		target->flags |= ASD_TARG_HOT_REMOVED;

		multipath_target = asd_find_target_ident(&ctx->discover_list,
							 target);

		if (multipath_target != NULL) {

			/*
			 * If there were multiple paths to the same device,
			 * then don't report this device as removed;
			 */
			asd_remap_device(asd, target, multipath_target);

			continue;
		}

		multipath_target = asd_find_multipath(asd, target);

		if (multipath_target != NULL) {

			/*
			 * If there were multiple paths to the same device,
			 * then don't report this device as removed;
			 */
			asd_remap_device(asd, target, multipath_target);
		}

		if (!list_empty(&target->multipath)) {
			multipath_target = list_entry(target->multipath.next,
						      struct asd_target,
						      multipath);

			asd_remap_device(asd, target, multipath_target);

			/*
			 * This device wasn't in the target list because it
			 * was a multipath.  Add it to the list.
			 */
			list_add_tail(&multipath_target->all_domain_targets,
				      &multipath_target->src_port->targets);

			target->flags &= ~ASD_TARG_MAPPED;
		}
	}

	asd_mark_duplicates(asd, &ctx->discover_list);

	list_for_each_entry_safe(target, tmp_target, &ctx->discover_list,
				 all_domain_targets) {

		if (((target->flags & ASD_TARG_ONLINE) == 0) &&
		    (target->flags & ASD_TARG_RESEEN)) {

			/*
			 * Previously seen target.
			 * Set target state to ONLINE.
			 */
			target->flags &= ~ASD_TARG_RESEEN;
			target->flags |= ASD_TARG_ONLINE;

			continue;
		}

		if ((target->flags & ASD_TARG_NEEDS_MAP) == 0) {
			/*
			 * If the device is not an end device, then
			 * ASD_TARG_NEEDS_MAP will never be set.
			 */
			continue;
		}

		target->flags &= ~ASD_TARG_NEEDS_MAP;

		if (asd_map_multipath(asd, target) == 0) {
			/*
			 * New target found.
			 */
			target->flags |= ASD_TARG_HOT_ADDED;

			list_add_tail(&target->validate_links,
				      &port->targets_to_validate);
		} else {
			/*
			 * The target was not mapped.  It might have been
			 * already mapped, so don't include it in our list
			 * of new targets.
			 */
			list_del_init(&target->all_domain_targets);
		}
	}

	INIT_LIST_HEAD(&ctx->old_discover_list);

	/*
	 * Set the flag that targets validation is required.
	 */
	port->events |= ASD_VALIDATION_REQ;
}

/* -------------------------------------------------------------------------- */

#if DISCOVER_DEBUG
void asd_debug_indent(unsigned indent)
{
	unsigned i;

	for (i = 0; i < indent; i++) {
		asd_print("|  ");
	}
}
static void
asd_dump_tree_internal(struct asd_softc *asd,
		       struct asd_port *port,
		       struct asd_target *target,
		       unsigned indent, unsigned phy_num)
{
	struct Discover *discover;
	struct asd_target *child_target;
	unsigned i;

	asd_debug_indent(indent);

	asd_print("+ Phy %d:", phy_num);

	asd_print(" Addr: %0llx - ",
		  asd_be64toh(*((uint64_t *) target->ddb_profile.sas_addr)));

	asd_print_conn_rate(target->ddb_profile.conn_rate, "|");

	switch (target->command_set_type) {
	case ASD_COMMAND_SET_SCSI:
		asd_print("SCSI|");
		break;
	case ASD_COMMAND_SET_ATA:
		asd_print("ATA|");
		break;
	case ASD_COMMAND_SET_ATAPI:
		asd_print("ATAPI|");
		break;
	case ASD_COMMAND_SET_SMP:
		asd_print("SMP|");
		break;
	default:
		asd_print("Unknown (%u)|", (unsigned)target->command_set_type);
		break;
	}
	switch (target->device_protocol_type) {
	case ASD_DEVICE_PROTOCOL_SCSI:
		asd_print("SCSI|");
		break;
	case ASD_DEVICE_PROTOCOL_ATA:
		asd_print("ATA|");
		break;
	case ASD_DEVICE_PROTOCOL_SMP:
		asd_print("SMP|");
		break;
	default:
		asd_print("Unknown (%u)|",
			  (unsigned)target->device_protocol_type);
		break;
	}

	switch (target->transport_type) {
	case ASD_TRANSPORT_SSP:
		asd_print("SSP|");
		break;
	case ASD_TRANSPORT_SMP:
		asd_print("SMP|");
		break;
	case ASD_TRANSPORT_STP:
		asd_print("STP|");
		break;
	case ASD_TRANSPORT_ATA:
		asd_print("ATA|");
		break;
	default:
		asd_print("Unknown (%u)|", (unsigned)target->transport_type);
		break;
	}

	switch (target->management_type) {
	case ASD_DEVICE_END:
		asd_print("END_DEVICE");
		break;
	case ASD_DEVICE_FANOUT_EXPANDER:
		asd_print("FANOUT_DEVICE");
		break;
	case ASD_DEVICE_EDGE_EXPANDER:
		asd_print("EXPANDER_DEVICE");
		break;
	default:
		asd_print("Unknown (%u)", (unsigned)target->management_type);
		break;
	}

	asd_print("\n");

	if (target->transport_type != ASD_TRANSPORT_SMP) {
		return;
	}

	asd_debug_indent(indent);

	asd_print("| Number of Phys: %d\n", target->num_phys);

	discover = NULL;

	indent++;

	list_for_each_entry(child_target, &target->children, siblings) {

		for (i = 0; i < target->num_phys; i++) {

			discover = &(target->Phy[i].Result);

			if (SAS_ISEQUAL(child_target->ddb_profile.sas_addr,
					discover->AttachedSASAddress)) {
				break;
			}
		}

		if (i == target->num_phys) {
			continue;
		}

		asd_dump_tree_internal(asd, port, child_target, indent, i);
	}
}

static void asd_dump_tree(struct asd_softc *asd, struct asd_port *port)
{
	struct asd_target *target;
	struct state_information *state_infop;

	SETUP_STATE(&port->dc.sm_context);

	if (!list_empty(&port->targets)) {

		target = port->tree_root;

		asd_dump_tree_internal(asd, port, target, 1, 0);
	}
}

#if KDB_ENABLE
extern struct list_head asd_hbas;

void asd_dump_sas_state(void)
{
	unsigned i;
	struct asd_softc *asd;

	printk("%s:%d --\n", __FUNCTION__, __LINE__);

	list_for_each_entry(asd, &asd_hbas, link) {
		for (i = 0; i < asd->hw_profile.max_ports; i++) {
			if (asd->port_list[i] != NULL) {
				asd_dump_tree(asd, asd->port_list[i]);
			}
		}
	}
}

void asd_kdb_init(void)
{
	kdb_register("sas", (void *)asd_dump_sas_state, "",
		     "Dumps the SAS state", 0);
}

void asd_kdb_exit(void)
{
	kdb_unregister("sas");
}
#endif /* KDB_ENABLE */
#endif /* DISCOVER_DEBUG */
