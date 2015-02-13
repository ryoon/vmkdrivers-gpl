/*
 * Adaptec ADP94xx SAS HBA device driver for Linux.
 * Functions for interfacing with Sequencers.
 *
 * Written by : David Chaw <david_chaw@adaptec.com>
 *   
 * Copyright (c) 2004 Adaptec Inc.
 * All rights reserved.
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
 *
 * $Id: //depot/razor/linux/src/adp94xx_seq.c#67 $
 * 
 */	

#include "adp94xx_osm.h"
#include "adp94xx_inline.h"
#include "adp94xx_seq.h"
#if KDB_ENABLE
#include "linux/kdb.h"
#endif

/*
 * Wrappers for which particular version of sequencer code and interrupt
 * vectors  to use.
 */
#define ASD_USE_A1_CODE(softc)		\
	(softc->hw_profile.rev_id == AIC9410_DEV_REV_A1 ? 1: 0)

#define ASD_SEQ_VER(seq, ver)	(seq##ver)

#define ASD_INT_VEC(vec, ver)	(vec##ver)
	
#ifdef ASD_DEBUG
/*
 * Registers dump state definitions (for debug purpose).
 */
static void	asd_hwi_dump_cseq_state(struct asd_softc *asd);
static void	asd_hwi_dump_lseq_state(struct asd_softc *asd, u_int lseq_id);

typedef struct lseq_cio_reqs {
	uint8_t		name[24];
	uint16_t	offset;
	uint16_t	width;
	uint16_t	mode;
} lseq_cio_regs_t;

#define MD(x)	(1 << x)

static lseq_cio_regs_t	LSEQmCIOREGS[] =
{
   {"LmMODEPTR"     ,0x00, 8, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmALTMODE"     ,0x01, 8, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmFLAG"        ,0x04, 8, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmARP2INTCTL"  ,0x05, 8, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmPRGMCNT"     ,0x08,16, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmARP2HALTCODE",0x15, 8, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmCURRADDR"    ,0x16,16, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmLASTADDR"    ,0x18,16, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmNXTLADDR"    ,0x1A,16, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmMnSCBPTR"    ,0x20,16, MD(0)|MD(1)|MD(2)|MD(3)},
   {"LmMnDDBPTR"    ,0x22,16, MD(0)|MD(1)|MD(2)|MD(3)},
   {"LmREQMBX"      ,0x30,32, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmRSPMBX"      ,0x34,32, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmMnINT"       ,0x38,32, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmMnINTEN"     ,0x3C,32, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmXMTPRIMD"    ,0x40,32, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmXMTPRIMCS"   ,0x44, 8, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmCONSTAT"     ,0x45, 8, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmMnDMAERRS"   ,0x46, 8, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmMnSGDMAERRS" ,0x47, 8, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmMnSASALIGN"  ,0x48, 8, MD(1)},
   {"LmMnSTPALIGN"  ,0x49, 8, MD(1)},
   {"LmALIGNMODE"   ,0x4B, 8, MD(1)},
   {"LmMnEXPRCVCNT" ,0x4C,32, MD(0)},
   {"LmMnXMTCNT"    ,0x4C,32, MD(1)},
   {"LmMnCURRTAG"   ,0x54,16, MD(0)},
   {"LmMnPREVTAG"   ,0x56,16, MD(0)},
   {"LmMnACKOFS"    ,0x58, 8, MD(1)},
   {"LmMnXFRLVL"    ,0x59, 8, MD(0)|MD(1)},
   {"LmMnSGDMACTL"  ,0x5A, 8, MD(0)|MD(1)},
   {"LmMnSGDMASTAT" ,0x5B, 8, MD(0)|MD(1)},
   {"LmMnDDMACTL"   ,0x5C, 8, MD(0)|MD(1)},
   {"LmMnDDMASTAT"  ,0x5D, 8, MD(0)|MD(1)},
   {"LmMnDDMAMODE"  ,0x5E,16, MD(0)|MD(1)},
   {"LmMnPIPECTL"   ,0x61, 8, MD(0)|MD(1)},
   {"LmMnACTSCB"    ,0x62,16, MD(0)|MD(1)},
   {"LmMnSGBHADR"   ,0x64, 8, MD(0)|MD(1)},
   {"LmMnSGBADR"    ,0x65, 8, MD(0)|MD(1)},
   {"LmMnSGDCNT"    ,0x66, 8, MD(0)|MD(1)},
   {"LmMnSGDMADR"   ,0x68,32, MD(0)|MD(1)},
   {"LmMnSGDMADR"   ,0x6C,32, MD(0)|MD(1)},
   {"LmMnXFRCNT"    ,0x70,32, MD(0)|MD(1)},
   {"LmMnXMTCRC"    ,0x74,32, MD(1)},
   {"LmCURRTAG"     ,0x74,16, MD(0)},
   {"LmPREVTAG"     ,0x76,16, MD(0)},
   {"LmDPSEL"       ,0x7B, 8, MD(0)|MD(1)},
   {"LmDPTHSTAT"    ,0x7C, 8, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmMnHOLDLVL"   ,0x7D, 8, MD(0)},
   {"LmMnSATAFS"    ,0x7E, 8, MD(1)},
   {"LmMnCMPLTSTAT" ,0x7F, 8, MD(0)|MD(1)},
   {"LmPRMSTAT0"    ,0x80,32, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmPRMSTAT1"    ,0x84,32, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmGPRMINT"     ,0x88, 8, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmMnCURRSCB"   ,0x8A,16, MD(0)},
   {"LmPRMICODE"    ,0x8C,32, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmMnRCVCNT"    ,0x90,16, MD(0)},
   {"LmMnBUFSTAT"   ,0x92,16, MD(0)},
   {"LmMnXMTHDRSIZE",0x92, 8, MD(1)},
   {"LmMnXMTSIZE"   ,0x93, 8, MD(1)},
   {"LmMnTGTXFRCNT" ,0x94,32, MD(0)},
   {"LmMnEXPROFS"   ,0x98,32, MD(0)},
   {"LmMnXMTROFS"   ,0x98,32, MD(1)},
   {"LmMnRCVROFS"   ,0x9C,32, MD(0)},
   {"LmCONCTL"      ,0xA0,16, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmBITLTIMER"   ,0xA2,16, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmWWNLOW"      ,0xA8,32, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmWWNHIGH"     ,0xAC,32, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmMnFRMERR"    ,0xB0,32, MD(0)},
   {"LmMnFRMERREN"  ,0xB4,32, MD(0)},
   {"LmAWTIMER"     ,0xB8,16, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmAWTCTL"      ,0xBA, 8, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmMnHDRCMPS"   ,0xC0,32, MD(0)},
   {"LmMnXMTSTAT"   ,0xC4, 8, MD(1)},
   {"LmHWTSTATEN"   ,0xC5, 8, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmMnRRDYRC"    ,0xC6, 8, MD(0)},
   {"LmMnRRDYTC"    ,0xC6, 8, MD(1)},
   {"LmHWTSTAT"     ,0xC7, 8, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmMnDATABUFADR",0xC8,16, MD(0)|MD(1)},
   {"LmDWSSTATUS"   ,0xCB, 8, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmMnACTSTAT"   ,0xCE,16, MD(0)|MD(1)},
   {"LmMnREQSCB"    ,0xD2,16, MD(0)|MD(1)},
   {"LmXXXPRIM"     ,0xD4,32, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmRCVASTAT"    ,0xD9, 8, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmINTDIS1"     ,0xDA, 8, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmPSTORESEL"   ,0xDB, 8, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmPSTORE"      ,0xDC,32, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmPRIMSTAT0EN" ,0xE0,32, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmPRIMSTAT1EN" ,0xE4,32, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"LmDONETCTL"    ,0xF2,16, MD(0)|MD(1)|MD(2)|MD(3)|MD(4)|MD(5)|MD(6)|MD(7)},
   {"", 0, 0, 0 }	/* Last entry should be NULL. */
}; 

static lseq_cio_regs_t	LSEQmOOBREGS[] =
{
   {"OOB_BFLTR"        ,0x100, 8, MD(5)},
   {"OOB_INIT_MIN"     ,0x102,16, MD(5)},
   {"OOB_INIT_MAX"     ,0x104,16, MD(5)},
   {"OOB_INIT_NEG"     ,0x106,16, MD(5)},
   {"OOB_SAS_MIN"      ,0x108,16, MD(5)},
   {"OOB_SAS_MAX"      ,0x10A,16, MD(5)},
   {"OOB_SAS_NEG"      ,0x10C,16, MD(5)},
   {"OOB_WAKE_MIN"     ,0x10E,16, MD(5)},
   {"OOB_WAKE_MAX"     ,0x110,16, MD(5)},
   {"OOB_WAKE_NEG"     ,0x112,16, MD(5)},
   {"OOB_IDLE_MAX"     ,0x114,16, MD(5)},
   {"OOB_BURST_MAX"    ,0x116,16, MD(5)},
   {"OOB_XMIT_BURST"   ,0x118, 8, MD(5)},
   {"OOB_SEND_PAIRS"   ,0x119, 8, MD(5)},
   {"OOB_INIT_IDLE"    ,0x11A, 8, MD(5)},
   {"OOB_INIT_NEGO"    ,0x11C, 8, MD(5)},
   {"OOB_SAS_IDLE"     ,0x11E, 8, MD(5)},
   {"OOB_SAS_NEGO"     ,0x120, 8, MD(5)},
   {"OOB_WAKE_IDLE"    ,0x122, 8, MD(5)},
   {"OOB_WAKE_NEGO"    ,0x124, 8, MD(5)},
   {"OOB_DATA_KBITS"   ,0x126, 8, MD(5)},
   {"OOB_BURST_DATA"   ,0x128,32, MD(5)},
   {"OOB_ALIGN_0_DATA" ,0x12C,32, MD(5)},
   {"OOB_ALIGN_1_DATA" ,0x130,32, MD(5)},
   {"OOB_SYNC_DATA"    ,0x134,32, MD(5)},
   {"OOB_D10_2_DATA"   ,0x138,32, MD(5)},
   {"OOB_PHY_RST_CNT"  ,0x13C,32, MD(5)},
   {"OOB_SIG_GEN"      ,0x140, 8, MD(5)},
   {"OOB_XMIT"         ,0x141, 8, MD(5)},
   {"FUNCTION_MAKS"    ,0x142, 8, MD(5)},
   {"OOB_MODE"         ,0x143, 8, MD(5)},
   {"CURRENT_STATUS"   ,0x144, 8, MD(5)},
   {"SPEED_MASK"       ,0x145, 8, MD(5)},
   {"PRIM_COUNT"       ,0x146, 8, MD(5)},
   {"OOB_SIGNALS"      ,0x148, 8, MD(5)},
   {"OOB_DATA_DET"     ,0x149, 8, MD(5)},
   {"OOB_TIME_OUT"     ,0x14C, 8, MD(5)},
   {"OOB_TIMER_ENABLE" ,0x14D, 8, MD(5)},
   {"OOB_STATUS"       ,0x14E, 8, MD(5)},
   {"HOT_PLUG_DELAY"   ,0x150, 8, MD(5)},
   {"RCD_DELAY"        ,0x151, 8, MD(5)},
   {"COMSAS_TIMER"     ,0x152, 8, MD(5)},
   {"SNTT_DELAY"       ,0x153, 8, MD(5)},
   {"SPD_CHNG_DELAY"   ,0x154, 8, MD(5)},
   {"SNLT_DELAY"       ,0x155, 8, MD(5)},
   {"SNWT_DELAY"       ,0x156, 8, MD(5)},
   {"ALIGN_DELAY"      ,0x157, 8, MD(5)},
   {"INT_ENABLE_0"     ,0x158, 8, MD(5)},
   {"INT_ENABLE_1"     ,0x159, 8, MD(5)},
   {"INT_ENABLE_2"     ,0x15A, 8, MD(5)},
   {"INT_ENABLE_3"     ,0x15B, 8, MD(5)},
   {"OOB_TEST_REG"     ,0x15C, 8, MD(5)},
   {"PHY_CONTROL_0"    ,0x160, 8, MD(5)},
   {"PHY_CONTROL_1"    ,0x161, 8, MD(5)},
   {"PHY_CONTROL_2"    ,0x162, 8, MD(5)},
   {"PHY_CONTROL_3"    ,0x163, 8, MD(5)},
   {"PHY_OOB_CAL_TX"   ,0x164, 8, MD(5)},
   {"PHY_OOB_CAL_RX"   ,0x165, 8, MD(5)},
   {"OOB_PHY_CAL_TX"   ,0x166, 8, MD(5)},
   {"OOB_PHY_CAL_RX"   ,0x167, 8, MD(5)},
   {"PHY_CONTROL_4"    ,0x168, 8, MD(5)},
   {"PHY_TEST"         ,0x169, 8, MD(5)},
   {"PHY_PWR_CTL"      ,0x16A, 8, MD(5)},
   {"PHY_PWR_DELAY"    ,0x16B, 8, MD(5)},
   {"OOB_SM_CON"       ,0x16C, 8, MD(5)},
   {"ADDR_TRAP_1"      ,0x16D, 8, MD(5)},
   {"ADDR_NEXT_1"      ,0x16E, 8, MD(5)},
   {"NEXT_ST_1"        ,0x16F, 8, MD(5)},
   {"OOB_SM_STATE"     ,0x170, 8, MD(5)},
   {"ADDR_TRAP_2"      ,0x171, 8, MD(5)},
   {"ADDR_NEXT_2"      ,0x172, 8, MD(5)},
   {"NEXT_ST_2"        ,0x173, 8, MD(5)},
   {"", 0, 0, 0 }	/* Last entry should be NULL. */
};
#endif /* ASD_DEBUG */

/* Local functions' prototypes */
static int	asd_hwi_verify_seqs(struct asd_softc *asd, uint8_t *code, 
				    uint32_t code_size, uint8_t lseq_mask);
static void	asd_hwi_init_cseq_scratch(struct asd_softc *asd);
static void	asd_hwi_init_cseq_mip(struct asd_softc *asd);
static void	asd_hwi_init_cseq_mdp(struct asd_softc *asd);
static void	asd_hwi_init_cseq_cio(struct asd_softc *asd);
static void 	asd_hwi_post_init_cseq(struct asd_softc *asd);
static void	asd_hwi_init_scb_sites(struct asd_softc *asd);
static void	asd_hwi_init_lseq_scratch(struct asd_softc *asd);
static void	asd_hwi_init_lseq_mip(struct asd_softc *asd, u_int link_num);
static void	asd_hwi_init_lseq_mdp(struct asd_softc *asd, u_int link_num);
static void	asd_hwi_init_lseq_cio(struct asd_softc *asd, u_int link_num);
static inline void
		asd_swap_with_next_hscb(struct asd_softc *asd, struct scb *scb);
      

/* Sequencer misc. utilities. */
static inline void asd_hwi_set_scbptr(struct asd_softc *asd, uint16_t val);
static inline void asd_hwi_set_scbsite_byte(struct asd_softc *asd,
					    uint16_t site_offset, uint8_t val);
static inline void asd_hwi_set_scbsite_word(struct asd_softc *asd,
					    uint16_t site_offset, uint16_t val);
static inline void asd_hwi_set_scbsite_dword(struct asd_softc *asd,
					    uint16_t site_offset, uint32_t val);
static inline uint8_t asd_hwi_get_scbsite_byte(struct asd_softc *asd, 
					       uint16_t site_offset);
static inline uint16_t asd_hwi_get_scbsite_word(struct asd_softc *asd, 
					        uint16_t site_offset);
static inline uint32_t asd_hwi_get_scbsite_dword(struct asd_softc *asd, 
						 uint16_t site_offset);

/* 
 * Function:
 *	asd_hwi_set_scbptr()
 *
 * Description:
 *      Program the SCBPTR. 
 *	SCBPTR is in Mode 15 of CSEQ CIO Bus Registers. 
 */
static inline void
asd_hwi_set_scbptr(struct asd_softc *asd, uint16_t val)
{
	asd_hwi_swb_write_word(asd, CSEQm_CIO_REG(15, MnSCBPTR), val);
}

/* 
 * Function:
 *	asd_hwi_set_ddbptr()
 *
 * Description:
 *      Program the DDBPTR. 
 *	DDBPTR is in Mode 15 of CSEQ CIO Bus Registers. 
 */
//static inline void
void
asd_hwi_set_ddbptr(struct asd_softc *asd, uint16_t val)
{
	asd_hwi_swb_write_word(asd, CSEQm_CIO_REG(15, MnDDBPTR), val);
}

/* 
 * Function:
 *	asd_hwi_set_scbsite_byte()
 *
 * Description:
 *      Write an 8-bits value to the SCBSITE starting from the site_offset. 
 *	SCBSITE is in Mode 15 of CSEQ CIO Bus Registers. 
 */
static inline void
asd_hwi_set_scbsite_byte(struct asd_softc *asd, uint16_t site_offset, 
			 uint8_t val)
{
	asd_hwi_swb_write_byte(asd,
			       CSEQm_CIO_REG(15, (MnSCB_SITE + site_offset)),
			       val);
}

/* 
 * Function:
 *	asd_hwi_set_scbsite_word()
 *
 * Description:
 *      Write a 16-bits value to the SCBSITE starting from the site_offset. 
 *	SCBSITE is in Mode 15 of CSEQ CIO Bus Registers. 
 */
static inline void
asd_hwi_set_scbsite_word(struct asd_softc *asd, uint16_t site_offset, 
			 uint16_t val)
{
	asd_hwi_swb_write_word(asd,
			       CSEQm_CIO_REG(15, (MnSCB_SITE + site_offset)),
			       val);
}

/* 
 * Function:
 *	asd_hwi_set_scbsite_dword()
 *
 * Description:
 *      Write a 32-bits value to the SCBSITE starting from the site_offset. 
 *	SCBSITE is in Mode 15 of CSEQ CIO Bus Registers. 
 */
static inline void
asd_hwi_set_scbsite_dword(struct asd_softc *asd, uint16_t site_offset, 
			  uint32_t val)
{
	asd_hwi_swb_write_dword(asd,
				CSEQm_CIO_REG(15, (MnSCB_SITE + site_offset)),
				val);
}

/* 
 * Function:
 *	asd_hwi_get_scbsite_byte()
 *
 * Description:
 *      Read an 8-bits value from the SCBSITE starting from the site_offset. 
 *	SCBSITE is in Mode 15 of CSEQ CIO Bus Registers. 
 */
static inline uint8_t
asd_hwi_get_scbsite_byte(struct asd_softc *asd, uint16_t site_offset)
{
	return ((uint8_t) asd_hwi_swb_read_byte(
				asd,
				CSEQm_CIO_REG(15, (MnSCB_SITE + site_offset))));
}

/* 
 * Function:
 *	asd_hwi_get_scbsite_word()
 *
 * Description:
 *      Read a 16-bits value from the SCBSITE starting from the site_offset. 
 *	SCBSITE is in Mode 15 of CSEQ CIO Bus Registers. 
 */
static inline uint16_t
asd_hwi_get_scbsite_word(struct asd_softc *asd, uint16_t site_offset)
{
	return ((uint16_t) asd_hwi_swb_read_word(
				asd,
				CSEQm_CIO_REG(15, (MnSCB_SITE + site_offset))));
}

/* 
 * Function:
 *	asd_hwi_get_scbsite_dword()
 *
 * Description:
 *      Read a 32-bits value from the SCBSITE starting from the site_offset. 
 *	SCBSITE is in Mode 15 of CSEQ CIO Bus Registers. 
 */
static inline uint32_t
asd_hwi_get_scbsite_dword(struct asd_softc *asd, uint16_t site_offset)
{
	return ((uint32_t) asd_hwi_swb_read_dword(
				asd,
				CSEQm_CIO_REG(15, (MnSCB_SITE + site_offset))));
}

/* 
 * Function:
 *	asd_hwi_set_ddbsite_byte()
 *
 * Description:
 *      Write an 8-bits value to the DDBSITE starting from the site_offset. 
 *	DDBSITE is in Mode 15 of CSEQ CIO Bus Registers. 
 */
void asd_hwi_set_ddbsite_byte(struct asd_softc *asd, uint16_t site_offset, 
			      uint8_t val)
{
	asd_hwi_swb_write_byte(asd,
			       CSEQm_CIO_REG(15, (MnDDB_SITE + site_offset)),
			       val);
}

/* 
 * Function:
 *	asd_hwi_set_ddbsite_word()
 *
 * Description:
 *      Write a 16-bits value to the DDBSITE starting from the site_offset. 
 *	DDBSITE is in Mode 15 of CSEQ CIO Bus Registers. 
 */
void asd_hwi_set_ddbsite_word(struct asd_softc *asd, uint16_t site_offset, 
			      uint16_t val)
{
	asd_hwi_swb_write_word(asd,
			       CSEQm_CIO_REG(15, (MnDDB_SITE + site_offset)),
			       val);
}

/* 
 * Function:
 *	asd_hwi_set_ddbsite_dword()
 *
 * Description:
 *      Write a 32-bits value to the DDBSITE starting from the site_offset. 
 *	DDBSITE is in Mode 15 of CSEQ CIO Bus Registers. 
 */
void asd_hwi_set_ddbsite_dword(struct asd_softc *asd, uint16_t site_offset, 
			       uint32_t val)
{
	asd_hwi_swb_write_dword(asd,
				CSEQm_CIO_REG(15, (MnDDB_SITE + site_offset)),
				val);
}

/* 
 * Function:
 *	asd_hwi_get_ddbsite_byte()
 *
 * Description:
 *      Read an 8-bits value from the DDBSITE starting from the site_offset. 
 *	DDBSITE is in Mode 15 of CSEQ CIO Bus Registers. 
 */
uint8_t asd_hwi_get_ddbsite_byte(struct asd_softc *asd, uint16_t site_offset)
{
	return ((uint8_t) asd_hwi_swb_read_byte(
				asd,
			      	CSEQm_CIO_REG(15, (MnDDB_SITE + site_offset))));
}

/* 
 * Function:
 *	asd_hwi_get_ddbsite_word()
 *
 * Description:
 *      Read a 16-bits value from the DDBSITE starting from the site_offset. 
 *	DDBSITE is in Mode 15 of CSEQ CIO Bus Registers. 
 */
uint16_t asd_hwi_get_ddbsite_word(struct asd_softc *asd, uint16_t site_offset)
{
	return ((uint16_t) asd_hwi_swb_read_word(
				asd,
			      	CSEQm_CIO_REG(15, (MnDDB_SITE + site_offset))));
}

/* 
 * Function:
 *	asd_hwi_get_ddbsite_dword()
 *
 * Description:
 *      Read a 32-bits value from the DDBSITE starting from the site_offset. 
 *	DDBSITE is in Mode 15 of CSEQ CIO Bus Registers. 
 */
uint32_t asd_hwi_get_ddbsite_dword(struct asd_softc *asd, uint16_t site_offset)
{
	return ((uint32_t) asd_hwi_swb_read_dword(
				asd,
			      	CSEQm_CIO_REG(15, (MnDDB_SITE + site_offset))));
}

/* 
 * Function:
 *	asd_hwi_pause_cseq()
 *
 * Description:
 *      Pause the Central Sequencer. 
 */
int
asd_hwi_pause_cseq(struct asd_softc *asd)
{
	uint32_t	arp2ctl_reg;
	uint32_t	arp2ctl;
	uint32_t	timer_tick;

	timer_tick = ASD_REG_TIMEOUT_TICK;
	arp2ctl_reg = (uint32_t) CARP2CTL;
	arp2ctl = asd_hwi_swb_read_dword(asd, arp2ctl_reg);
	
	/* Check if the CSEQ is paused. */
	if (!(arp2ctl & PAUSED)) {
		/* CSEQ is running. Pause it. */
		asd_hwi_swb_write_dword(asd, arp2ctl_reg, (arp2ctl | EPAUSE));
		/* Verify that the CSEQ is paused. */
		do { 
			arp2ctl = asd_hwi_swb_read_dword(asd, arp2ctl_reg);
			if (!(arp2ctl & PAUSED)) {
				timer_tick--;		
				asd_delay(ASD_DELAY_COUNT);	
			} else
				break;
		} while (timer_tick != 0);					
	}		 		
	if (timer_tick == 0) {
		asd_log(ASD_DBG_ERROR, "Timeout expired when pausing CSEQ.\n");
		ASD_DUMP_REG(CARP2CTL);
		return (-1);
	}

	return (0);
}

/* 
 * Function:
 *	asd_hwi_unpause_cseq()
 *
 * Description:
 *      Unpause the Central Sequencer. 
 */
int
asd_hwi_unpause_cseq(struct asd_softc *asd)
{
	uint32_t	arp2ctl;
	uint32_t	timer_tick;

	timer_tick = ASD_REG_TIMEOUT_TICK;
	arp2ctl = asd_hwi_swb_read_dword(asd, CARP2CTL);

	/* Check if the CSEQ is paused. */
	if (arp2ctl & PAUSED) {
		/* CSEQ is currently paused. Unpause it. */
		asd_hwi_swb_write_dword(asd, CARP2CTL, (arp2ctl & ~EPAUSE));
		/* Verify that the CSEQ is unpaused. */
		do { 
			arp2ctl = asd_hwi_swb_read_dword(asd, CARP2CTL);
			if (arp2ctl & PAUSED) {
				timer_tick--;		
				asd_delay(ASD_DELAY_COUNT);	
			} else
				break;
		} while (timer_tick != 0);	
	}
	if (timer_tick == 0) {
		asd_log(ASD_DBG_ERROR, "Timeout expired when unpausing "
					"CSEQ.\n");
		ASD_DUMP_REG(CARP2CTL);
		return (-1);
	}

	return (0);
}

/* 
 * Function:
 *	asd_hwi_pause_lseq()
 *
 * Description:
 *      Pause the requested Link Sequencer(s). 
 */
int	
asd_hwi_pause_lseq(struct asd_softc *asd, uint8_t lseq_mask)
{
	uint32_t	arp2ctl;
	uint32_t	timer_tick;
	uint8_t		temp_lseq_mask;	
	uint8_t		phy_id;

	phy_id = 0;
	temp_lseq_mask = lseq_mask;
	timer_tick = ASD_REG_TIMEOUT_TICK;	
	while (temp_lseq_mask) {
		do {
			if (temp_lseq_mask & (1 << phy_id))  
				break;
			else 
				phy_id++;		
		} while (phy_id < asd->hw_profile.max_phys);
		
		arp2ctl = asd_hwi_swb_read_dword(asd, LmARP2CTL(phy_id));
		
		/* Check if the LSEQ is paused. */
		if (!(arp2ctl & PAUSED)) {
			/*
		 	 * LSEQ is running. Pause it.
		 	 */
			asd_hwi_swb_write_dword(asd, LmARP2CTL(phy_id), 
						(arp2ctl | EPAUSE));
			/* Verify that the LSEQ is paused. */
			do { 
				arp2ctl = asd_hwi_swb_read_dword(asd, 
							LmARP2CTL(phy_id));
				if (!(arp2ctl & PAUSED)) {
					timer_tick--;		
					asd_delay(ASD_DELAY_COUNT);	
				} else
					break;
			} while (timer_tick != 0);				
		}
		if (timer_tick == 0) {
			asd_log(ASD_DBG_ERROR, "Timeout expired when pausing "
				"LSEQ %d.\n", phy_id);
			return (-1);
		} 	
		temp_lseq_mask &= (~(1 << phy_id));		
		phy_id++;
	}

	return (0);						 
}

/* 
 * Function:
 *	asd_hwi_unpause_lseq()
 *
 * Description:
 *      Unpause the requested Link Sequencer(s). 
 */
int
asd_hwi_unpause_lseq(struct asd_softc *asd, uint8_t lseq_mask)
{
	uint32_t	arp2ctl;
	uint32_t	timer_tick;
	uint8_t		temp_lseq_mask;	
	uint8_t		phy_id;
	
	phy_id = 0;
	temp_lseq_mask = lseq_mask;
	timer_tick = ASD_REG_TIMEOUT_TICK;	
	while (temp_lseq_mask) {
		do {
			if (temp_lseq_mask & (1 << phy_id))  
				break;
			else 
				phy_id++;		
		} while (phy_id < asd->hw_profile.max_phys);
		
		arp2ctl = asd_hwi_swb_read_dword(asd, LmARP2CTL(phy_id));	

		/* Check if the LSEQ is paused. */
		if (arp2ctl & PAUSED) {
			/* Unpause the LSEQ. */
			asd_hwi_swb_write_dword(asd, LmARP2CTL(phy_id), 
					       (arp2ctl & ~EPAUSE));
			do { 
				arp2ctl = asd_hwi_swb_read_dword(asd, 
							LmARP2CTL(phy_id));
				if (arp2ctl & PAUSED) {
					timer_tick--;		
					asd_delay(ASD_DELAY_COUNT);	
				} else
					break;
			} while (timer_tick != 0);	

		}
		if (timer_tick == 0) {
			asd_log(ASD_DBG_ERROR, "Timeout expired when unpausing "
					"LSEQ %d.\n", phy_id);
			return (-1);
		}
		temp_lseq_mask &= (~(1 << phy_id));		
		phy_id++;
	}

	return (0);
}

/* 
 * Function:
 *	asd_hwi_download_seqs()
 *
 * Description:
 *      Setup the Central and Link Sequencers.
 *	Download the sequencers microcode.  
 */
int	
asd_hwi_download_seqs(struct asd_softc *asd)
{
	int 	error;

#define ASD_SET_SEQ_VER(asd, seq)	\
	(ASD_USE_A1_CODE(asd) == 1 ? ASD_SEQ_VER(seq, a1) :		\
	 ASD_SEQ_VER(seq, b0))

#define ASD_SEQ_SIZE(asd, seq)		\
	(ASD_USE_A1_CODE(asd) == 1 ? sizeof(ASD_SEQ_VER(seq, a1)) :	\
	 sizeof(ASD_SEQ_VER(seq, b0)))

	/* Download the Central Sequencer code. */
	error = ASD_HWI_DOWNLOAD_SEQS(asd, ASD_SET_SEQ_VER(asd, Cs), 
				      ASD_SEQ_SIZE(asd, Cs),
				      0);
	if (error != 0) {
		asd_log(ASD_DBG_ERROR, "CSEQ download failed.\n");
		goto exit;
	}
	
	/* 
	 * Download the Link Sequencers code. All of the Link Sequencers 
	 * microcode can be downloaded at the same time.
	 */
	error = ASD_HWI_DOWNLOAD_SEQS(asd, ASD_SET_SEQ_VER(asd, Ls), 
				      ASD_SEQ_SIZE(asd, Ls),
				      asd->hw_profile.enabled_phys); 
	if (error != 0) {
		uint8_t		i;
		
		/*
		 * If we failed to load the LSEQ all at a time. 
		 * Try to load them one at a time.
		 */		 
		for (i = 0; i < asd->hw_profile.max_phys; i++) {
			error = ASD_HWI_DOWNLOAD_SEQS(
					asd,
					ASD_SET_SEQ_VER(asd, Ls),
					ASD_SEQ_SIZE(asd, Ls),
				      	(1 << i));
			if (error != 0)
				break;

			asd->phy_list[i]->state = ASD_PHY_OFFLINE;
		}
		if (error) {
			asd_log(ASD_DBG_ERROR, "LSEQs download failed.\n");
			goto exit;
		}
	}

exit:
	return (error);		  
}

#if ASD_DMA_DOWNLOAD_SEQS
/* 
 * Function:
 *	asd_hwi_dma_load_seqs()
 *
 * Description:
 *	Download the sequencers using Host Overlay DMA mode.  
 */
int
asd_hwi_dma_load_seqs(struct asd_softc *asd, uint8_t *code,
		      uint32_t code_size, uint8_t lseq_mask)
{
	struct map_node	 buf_map;	
	bus_dma_tag_t	 buf_dmat;
	uint32_t	 buf_size;
	uint32_t	 comstaten_val;
	uint32_t	 instr_size;
	uint32_t	 timer_tick;
	uint16_t	 ovlyaddr;
	uint16_t	 ovlydmactl_lo;
	uint16_t	 ovlydmactl_hi;	
	uint8_t		*instr;	
	uint8_t		 nseg;
	u_int		 i;
	int		 error;
	
	error = 0;
	if (code_size % 4) {
		/*
		 * For PIO mode, sequencer code size must be multiple of dword.
		 */
		asd_log(ASD_DBG_ERROR, "SEQ code size (%d) not multiple of "
			"dword. \n", code_size);
		return (-1);	
	}
	
	/* Save the Interrupt Mask register value. */
	comstaten_val = asd_read_dword(asd, COMSTATEN);
	/* 
	 * Disable Device Communication Status interrupt and clear any 
	 * pending interrupts.
	 */
	asd_write_dword(asd, COMSTATEN, 0x0);
	asd_write_dword(asd, COMSTAT, COMSTAT_MASK);
		
	/* Disable CHIM interrupt and clear any pending interrupts. */
	asd_write_dword(asd, CHIMINTEN, RST_CHIMINTEN);
	asd_write_dword(asd, CHIMINT, CHIMINT_MASK);
	
	/* 
	 * Check the limit of HW DMA transfer size.
	 * Limit the Overlay DMA transter size to code_size at a time.
	 */ 
	if ((asd->hw_profile.max_scbs * ASD_SCB_SIZE) >= code_size)
		buf_size = code_size;
	else
		buf_size = (asd->hw_profile.max_scbs * ASD_SCB_SIZE);	

	/* Allocate a dma tag for buffer to dma-ing the instruction codes. */
	if (asd_dma_tag_create(asd, 4, buf_size, GFP_ATOMIC, &buf_dmat) != 0)
		return (-ENOMEM);

	if (asd_dmamem_alloc(asd, buf_dmat, (void **) &buf_map.vaddr,
			     GFP_ATOMIC, &buf_map.dmamap,
			     &buf_map.busaddr) != 0) {
		asd_dma_tag_destroy(asd, buf_dmat);
		return (-ENOMEM);
	}

	instr = (uint8_t *) buf_map.vaddr;
	
	/* Calculate number of DMA segments needed to transfer the code. */
	nseg = (code_size + buf_size - 1) / buf_size;
	instr_size = buf_size;
	ovlydmactl_lo = ovlydmactl_hi = 0;
	ovlyaddr = 0;
	
	asd_log(ASD_DBG_INFO, "Downloading %s (Overlay DMA Mode) ...\n", 
		(lseq_mask == 0 ? "CSEQ" : "LSEQ"));
	
	for (i = 0; i < nseg; ) {
		memcpy(instr, &code[ovlyaddr], instr_size);
		
		/* Program the DMA download size. */
		asd_write_dword(asd, OVLYDMACNT, instr_size);
		/* 
		 * Program the 64-bit DMA address, OVLYDMAADDR register is 
		 * 64-bit. 
		 */
		asd_write_dword(asd, OVLYDMAADR0, 
				ASD_GET_PADR(buf_map.busaddr));
		asd_write_dword(asd, OVLYDMAADR1,
				ASD_GET_PUADR(buf_map.busaddr));
		
		/* lseq_mask tells us which sequencer(s) the code is for. */
		if (lseq_mask == 0)
			ovlydmactl_lo = (uint16_t) OVLYCSEQ;
		else
			ovlydmactl_lo = (uint16_t) (((uint16_t)lseq_mask) << 8);
		/* 
		 * Program the OVLYADR. It increments for each dword 
		 * instruction overlaid. 
		 */
		ovlydmactl_hi = (ovlyaddr / 4);
		/*
		 * Reset the Overlay DMA counter and buffer pointers to zero.
		 * Also, enabled the Overlay DMA engine.
		 */
	       ovlydmactl_lo |= (RESETOVLYDMA | STARTOVLYDMA | OVLYHALTERR);
		/* 
		 * Start the DMA. We need to set the higher two bytes of 
		 * OVLYDMACTL register before programming the lower two bytes 
		 * as the lowest byte of OVLYDMACTL register contains 
		 * STARTOVLYDMA bit which once is written, will start the DMA.
		 */
		asd_write_word(asd, (OVLYDMACTL+2), ovlydmactl_hi);
		asd_write_word(asd, OVLYDMACTL, ovlydmactl_lo);		
			       
	       	timer_tick = ASD_REG_TIMEOUT_TICK;
		do {
			/*
			 * Check if the Overlay DMA is still active. 
			 * It will get reset once the transfer is done.
			 */
			if (asd_read_dword(asd, OVLYDMACTL) & OVLYDMAACT) {
				timer_tick--;		
				asd_delay(ASD_DELAY_COUNT);
			} else
				break;
		} while (timer_tick != 0);
		
		/*
		 * Check if the DMA transfer has completed successfully.
		 */
		if ((timer_tick != 0) && 
		    (asd_read_dword(asd, COMSTAT) & OVLYDMADONE) &&
		    (!(asd_read_dword(asd, COMSTAT) & OVLYERR)) &&
		    (!(asd_read_dword(asd, CHIMINT) & DEVEXCEPT_MASK))) {
			/* DMA transfer completed successfully. */
			ovlyaddr += instr_size;
			/*
			 * Sanity check when we doing the last segment, 
			 * make sure that we only transfer the remaing code
			 * size.
			 */ 
			if (++i == (nseg - 1))
				instr_size = code_size - (i * instr_size);
		} else {
			/* DMA transfer failed. */
			asd_log(ASD_DBG_ERROR, "%s download failed.\n",
				(lseq_mask == 0 ? "CSEQ" : "LSEQ"));
			error = -1;
			goto exit;
		}
	} 

	/* Restore the Interrupt Mask. */
	asd_write_dword(asd, COMSTATEN, comstaten_val);	

	/* Verify that the sequencer is downloaded properly. */
	asd_log(ASD_DBG_INFO, "Verifying %s ...\n", 
		(lseq_mask == 0 ? "CSEQ" : "LSEQ"));
	if (asd_hwi_verify_seqs(asd, code, code_size, lseq_mask) != 0) {
		asd_log(ASD_DBG_ERROR, "%s verify failed.\n",
			(lseq_mask == 0 ? "CSEQ" : "LSEQ"));
		error = -1;
		goto exit;
	}
	asd_log(ASD_DBG_INFO, "%s verified successfully.\n", 
		(lseq_mask == 0 ? "CSEQ" : "LSEQ"));

exit:
	asd_free_dma_mem(asd, buf_dmat, &buf_map);
	
	return (error);
}

#else

/* 
 * Function:
 *	asd_hwi_pio_load_seqs()
 *
 * Description:
 *	Download the sequencers using PIO mode.  
 */
int
asd_hwi_pio_load_seqs(struct asd_softc *asd, uint8_t*code,
		      uint32_t code_size, uint8_t lseq_mask)
{
	uint32_t	reg_val;
	uint32_t	instr;
	uint32_t	i;
	
	if (code_size % 4) {
		/*
		 * For PIO mode, sequencer code size must be multiple of dword.
		 */
		asd_log(ASD_DBG_ERROR, "SEQ code size (%d) not multiple of "
			"dword. \n", code_size);	
		return (-1);
	}
	/* Set to PIO Mode */
	reg_val = PIOCMODE;
		
	/* lseq_mask tells us which sequencer(s) the code is for. */
	if (lseq_mask != 0)
		reg_val |= (uint32_t) (((uint16_t) lseq_mask) << 8);
	else
		reg_val |= OVLYCSEQ;

	/*
	 * Progam the sequencer RAM address, which sequencer(s) to load and the
	 * download mode.
	 */	
	/* Program the download size. */
	asd_write_dword(asd, OVLYDMACNT, code_size);
	asd_write_dword(asd, OVLYDMACTL, reg_val); 	
	
	asd_log(ASD_DBG_INFO, "Downloading %s (PIO Mode) ...\n", 
		(lseq_mask == 0 ? "CSEQ" : "LSEQ"));
	/* Download the instr 4 bytes a time. */
	for (i = 0; i < (code_size/4); i++) {
		instr = *(uint32_t *) &code[i*4];
		/* The sequencer is little-endian. */
		instr = asd_htole32(instr);
		asd_write_dword(asd, SPIODATA, instr);	
	}
	
	/*
	 * TBRV : Check Device Exception Error ??
	 */
	 
	/* Clear the PIO mode bit and enabled Overlay Halt Error. */
	reg_val = (reg_val & ~PIOCMODE) | OVLYHALTERR;
	asd_write_dword(asd, OVLYDMACTL, reg_val);
	
	/* Verify that the sequencer is downloaded properly. */
	asd_log(ASD_DBG_INFO,
		"Verifying %s ...\n", (lseq_mask == 0 ? "CSEQ" : "LSEQ"));
	if (asd_hwi_verify_seqs(asd, code, code_size, lseq_mask) != 0) {
		asd_log(ASD_DBG_ERROR, "%s verify failed.\n",
			(lseq_mask == 0 ? "CSEQ" : "LSEQ"));
		return (-1);
	}
	asd_log(ASD_DBG_INFO, "%s verified successfully.\n", 
		(lseq_mask == 0 ? "CSEQ" : "LSEQ"));

	return (0);
}

#endif

/* 
 * Function:
 *	asd_hwi_verify_seqs()
 *
 * Description:
 *	Verify the downloaded sequencer.  
 */
static int
asd_hwi_verify_seqs(struct asd_softc *asd, uint8_t *code, 
		    uint32_t code_size, uint8_t lseq_mask)
{
	int		error;
	uint32_t	base_addr;
	uint32_t	page_offset;
	uint32_t	instr;
	uint32_t	i;
	uint8_t		temp_lseq_mask;	
	uint8_t		phy_id;

	error = 0;
	phy_id = 0;
	temp_lseq_mask = lseq_mask;	
	do {	
		if (temp_lseq_mask == 0) {
			/* Get CSEQ Instruction RAM base addr. */
			base_addr = CSEQ_RAM_REG_BASE_ADR;
		} else {
			for ( ; phy_id < asd->hw_profile.max_phys; phy_id++) {
				if (temp_lseq_mask & (1 << phy_id)) {
					temp_lseq_mask &= ~(1 << phy_id);
					break;
				}
			}
			
			/* Get the LmSEQ Instruction RAM base addr. */
			base_addr = (uint32_t) LmSEQRAM(phy_id);;
			/*
			 * Set the LmSEQ Instruction Memory Page to 0.
			 * LmSEQRAM is mapped 4KB in internal memory space.
			 */
			asd_hwi_swb_write_dword(asd, LmBISTCTL1(phy_id), 0);
		}	
		
		page_offset = 0;
		for (i = 0; i < (code_size/4); i++) {
			if ((base_addr != CSEQ_RAM_REG_BASE_ADR) && (i > 0) &&
			    ((i % 1024) == 0)) {
				/*
			 	 * For LSEQ, we need to adjust the LmSEQ 
				 * Instruction Memory page to the next 4KB page
				 * once we past the page boundary.
				 */
				asd_hwi_swb_write_dword(asd, 
							LmBISTCTL1(phy_id), 
							((i / 1024) <<
							 LmRAMPAGE_LSHIFT));
				page_offset = 0;		 
			}				  
		
			/* 
		 	 * Compare dword at a time since Instruction RAM page is
		 	 * dword accessible read only.
		 	 */ 
			instr = asd_htole32(*(uint32_t *)&code[i*4]);
			if (instr != asd_hwi_swb_read_dword(asd, 
					base_addr + page_offset)) {
				/* Code doesn't match. */
				error = -1;
				break;
			}
			page_offset += 4;
		}
		/* Done verifing the sequencer(s). */
		if (temp_lseq_mask == 0)
			break;
	} while (error == 0);

	return (error);
}

/* 
 * Function:
 *	asd_hwi_setup_seqs()
 *
 * Description:
 *	Setup and initialize Central and Link sequencers.  
 */
void
asd_hwi_setup_seqs(struct asd_softc *asd)
{
	int 		link_num;
	uint8_t		enabled_phys;

#define ASD_SET_INT_VEC(asd, vec)	\
	(ASD_USE_A1_CODE(asd) == 1 ? ASD_INT_VEC(vec, A1) :	\
	 ASD_INT_VEC(vec, B0))

	/* Initialize CSEQ Scratch RAM registers. */
	asd_hwi_init_cseq_scratch(asd);
	
	/* Initialize LmSEQ Scratch RAM registers. */
	asd_hwi_init_lseq_scratch(asd);

	/* Initialize SCB sites. */
	asd_hwi_init_scb_sites(asd);

	/* Initialize CSEQ CIO registers. */
	asd_hwi_init_cseq_cio(asd);
		
	/* Initialize LmSEQ CIO registers. */
	link_num = 0;
	enabled_phys = asd->hw_profile.enabled_phys;

	while (enabled_phys != 0) { 
		for ( ; link_num < asd->hw_profile.max_phys; link_num++) {
			if (enabled_phys & (1 << link_num)) {
				enabled_phys &= ~(1 << link_num);
				break;
			} 
		}
		
		asd_hwi_init_lseq_cio(asd, link_num);
	}

	asd_hwi_post_init_cseq(asd);

	//asd_hwi_dump_seq_raw(asd);
}

/* 
 * Function:
 *	asd_hwi_cseq_init_scratch()
 *
 * Description:
 *	Setup and initialize Central sequencers. Initialiaze the mode 
 *	independent and dependent scratch page to the default settings.
 */
static void
asd_hwi_init_cseq_scratch(struct asd_softc *asd)
{
	unsigned	i;

	/* Reset SCBPRO count register. */
	asd_write_dword(asd, SCBPRO, asd->qinfifonext);

	/*
	 * Clear out memory
	 */
	for (i = 0 ; i < CMAPPEDSCR_LEN ; i+=4)
		asd_hwi_swb_write_dword(asd, CMAPPEDSCR + i, 0x0000);

	/* Initialize CSEQ Mode Independent Page. */
	asd_hwi_init_cseq_mip(asd);
	/* Initialize CSEQ Mode Dependent Page. */
	asd_hwi_init_cseq_mdp(asd);
}

/* 
 * Function:
 *	asd_hwi_init_cseq_mip()
 *
 * Description:
 *	Initialize CSEQ Mode Independent Pages 4-7.	 
 */
static void
asd_hwi_init_cseq_mip(struct asd_softc *asd)
{
	uint8_t		free_scb_mask;
	u_int		i;
#ifdef SEQUENCER_UPDATE
	uint8_t		val;
	unsigned	num_bits;
#endif
	
	/*
	 * -------------------------------------
	 * CSEQ Mode Independent , page 4 setup.
	 * -------------------------------------
	 */
	asd_hwi_swb_write_word(asd, CSEQ_Q_EXE_HEAD, 0xFFFF);
	asd_hwi_swb_write_word(asd, CSEQ_Q_EXE_TAIL, 0xFFFF);
	asd_hwi_swb_write_word(asd, CSEQ_Q_DONE_HEAD, 0xFFFF);
	asd_hwi_swb_write_word(asd, CSEQ_Q_DONE_TAIL, 0xFFFF);
	asd_hwi_swb_write_word(asd, CSEQ_Q_SEND_HEAD, 0xFFFF);
	asd_hwi_swb_write_word(asd, CSEQ_Q_SEND_TAIL, 0xFFFF);
	asd_hwi_swb_write_word(asd, CSEQ_Q_DMA2CHIM_HEAD, 0xFFFF);
	asd_hwi_swb_write_word(asd, CSEQ_Q_DMA2CHIM_TAIL, 0xFFFF);
	asd_hwi_swb_write_word(asd, CSEQ_Q_COPY_HEAD, 0xFFFF);
	asd_hwi_swb_write_word(asd, CSEQ_Q_COPY_TAIL, 0xFFFF);
	asd_hwi_swb_write_word(asd, CSEQ_REG0, 0x0);
	asd_hwi_swb_write_word(asd, CSEQ_REG1, 0x0);
	asd_hwi_swb_write_word(asd, CSEQ_REG2, 0x0);
	asd_hwi_swb_write_word(asd, CSEQ_REG3, 0x0);
	asd_hwi_swb_write_byte(asd, CSEQ_LINK_CTL_Q_MAP, 0x0);
#ifdef SEQUENCER_UPDATE
	val = asd_hwi_swb_read_byte(asd, CCONEXIST);

	for (i = 0, num_bits = 0 ; i < 8 ; i++) {
		if ((val & (1 << i)) != 0) {
			num_bits++;
		}
	}

	asd_hwi_swb_write_byte(asd, CSEQ_MAX_CSEQ_MODE, 
		(num_bits << 4) | num_bits);
#else
	asd_hwi_swb_write_byte(asd, CSEQ_SCRATCH_FLAGS, 0x0);
#endif

	/*
	 * -------------------------------------
	 * CSEQ Mode Independent , page 5 setup.
	 * -------------------------------------
	 */
#ifdef SEQUENCER_UPDATE
	asd_hwi_swb_write_dword(asd, CSEQ_EST_NEXUS_REQ_QUEUE, 0);
	asd_hwi_swb_write_dword(asd, CSEQ_EST_NEXUS_REQ_QUEUE + 4, 0);
	asd_hwi_swb_write_dword(asd, CSEQ_EST_NEXUS_REQ_COUNT, 0);
	asd_hwi_swb_write_dword(asd, CSEQ_EST_NEXUS_REQ_COUNT + 4, 0);
	asd_hwi_swb_write_word(asd, CSEQ_EST_NEXUS_HEAD, 0xffff);
	asd_hwi_swb_write_word(asd, CSEQ_EST_NEXUS_TAIL, 0xffff);
	asd_hwi_swb_write_word(asd, CSEQ_NEED_EST_NEXUS_SCB, 0);
	asd_hwi_swb_write_byte(asd, CSEQ_EST_NEXUS_REQ_HEAD, 0);
	asd_hwi_swb_write_byte(asd, CSEQ_EST_NEXUS_REQ_TAIL, 0);
	asd_hwi_swb_write_byte(asd, CSEQ_EST_NEXUS_SCB_OFFSET, 0);
#else
	/* Calculate the free scb mask. */
	free_scb_mask = (uint8_t) ((~(((asd->hw_profile.max_scbs * 
					ASD_SCB_SIZE) / 128) - 1)) >> 8);

	asd_hwi_swb_write_byte(asd, CSEQ_FREE_SCB_MASK, free_scb_mask);
			       		       
	/* 
	 * Fill BUILTIN_FREE_SCB_HEAD with the first scb no. and 
	 * BUILTIN_FREE_SCB_TAIL with the last scb no.
	 */
	asd_hwi_swb_write_word(asd, CSEQ_BUILTIN_FREE_SCB_HEAD, 0x0);
	asd_hwi_swb_write_word(asd, CSEQ_BUILTIN_FREE_SCB_TAIL,
			      ((((ASD_MAX_SCB_SITES-1) & 0xFF) == 0xFF) ?
			      (ASD_MAX_SCB_SITES-2) : (ASD_MAX_SCB_SITES-1))); 
		
	/* Extended SCB sites are not being used now. */
	asd_hwi_swb_write_word(asd, CSEQ_EXTNDED_FREE_SCB_HEAD, 0xFFFF);
	asd_hwi_swb_write_word(asd, CSEQ_EXTNDED_FREE_SCB_TAIL, 0xFFFF);
#endif

	/*
	 * -------------------------------------
	 * CSEQ Mode Independent , page 6 setup.
	 * -------------------------------------
	 */
	asd_hwi_swb_write_word(asd, CSEQ_INT_ROUT_RET_ADDR0, 0x0);
	asd_hwi_swb_write_word(asd, CSEQ_INT_ROUT_RET_ADDR1, 0x0);	
	asd_hwi_swb_write_word(asd, CSEQ_INT_ROUT_SCBPTR, 0x0);
	asd_hwi_swb_write_byte(asd, CSEQ_INT_ROUT_MODE, 0x0);
	asd_hwi_swb_write_byte(asd, CSEQ_ISR_SCRATCH_FLAGS, 0x0);
	asd_hwi_swb_write_word(asd, CSEQ_ISR_SAVE_SINDEX, 0x0);
	asd_hwi_swb_write_word(asd, CSEQ_ISR_SAVE_DINDEX, 0x0);
#ifndef SEQUENCER_UPDATE
	asd_hwi_swb_write_word(asd, CSEQ_SLS_SAVE_ACCUM, 0x0);
	asd_hwi_swb_write_word(asd, CSEQ_SLS_SAVE_SINDEX, 0x0);
#endif
#ifdef SEQUENCER_UPDATE
	asd_hwi_swb_write_word(asd, CSEQ_Q_MONIRTT_HEAD, 0xffff);
	asd_hwi_swb_write_word(asd, CSEQ_Q_MONIRTT_TAIL, 0xffff);

	/* Calculate the free scb mask. */
#ifdef EXTENDED_SCB
   free_scb_mask = (uint8_t) ((~((( (asd->hw_profile.max_scbs - ASD_EXTENDED_SCB_NUMBER) * 
					ASD_SCB_SIZE) / 128) - 1)) >> 8);
#else
   free_scb_mask = (uint8_t) ((~(((asd->hw_profile.max_scbs * 
					ASD_SCB_SIZE) / 128) - 1)) >> 8);
#endif
	asd_hwi_swb_write_byte(asd, CSEQ_FREE_SCB_MASK, free_scb_mask);
			       		       
	/* 
	 * Fill BUILTIN_FREE_SCB_HEAD with the first scb no. and 
	 * BUILTIN_FREE_SCB_TAIL with the last scb no.
	 */
	/*
	 * We have to skip 0x00 - 0x1f
	 */
#ifdef SATA_SKIP_FIX
	asd_hwi_swb_write_word(asd, CSEQ_BUILTIN_FREE_SCB_HEAD, 0x20);
#else
	asd_hwi_swb_write_word(asd, CSEQ_BUILTIN_FREE_SCB_HEAD, 0x0);
#endif
	asd_hwi_swb_write_word(asd, CSEQ_BUILTIN_FREE_SCB_TAIL,
			      ((((ASD_MAX_SCB_SITES-1) & 0xFF) == 0xFF) ?
			      (ASD_MAX_SCB_SITES-2) : (ASD_MAX_SCB_SITES-1))); 

#ifndef EXTENDED_SCB		
	/* Extended SCB sites are not being used now. */
	asd_hwi_swb_write_word(asd, CSEQ_EXTNDED_FREE_SCB_HEAD, 0xFFFF);
	asd_hwi_swb_write_word(asd, CSEQ_EXTNDED_FREE_SCB_TAIL, 0xFFFF);
#else //EXTENDED_SCB
	/* Extended SCB sites are not being used now. */

#ifdef SATA_SKIP_FIX
	asd_hwi_swb_write_word(asd, CSEQ_EXTNDED_FREE_SCB_HEAD, (uint16_t)ASD_MAX_SCB_SITES + 0x20);
#else //!SATA_SKIP_FIX
	asd_hwi_swb_write_word(asd, CSEQ_EXTNDED_FREE_SCB_HEAD, (uint16_t)ASD_MAX_SCB_SITES );
#endif //ifdef SATA_SKIP_FIX

	asd_hwi_swb_write_word(asd, CSEQ_EXTNDED_FREE_SCB_TAIL, (uint16_t)(asd->hw_profile.max_scbs - 1));
#endif //ifndef EXTENDED_SCB

#endif 
	
	/*
	 * -------------------------------------
	 * CSEQ Mode Independent , page 7 setup.
	 * -------------------------------------
	 */
	for (i = 0; i < 8; i = i+4) {
		asd_hwi_swb_write_dword(asd,
				       (CSEQ_EMPTY_REQ_QUEUE + i),
					0x0);
		asd_hwi_swb_write_dword(asd,
				       (CSEQ_EMPTY_REQ_COUNT + i),
					0x0);	
	}
	/* Initialize Q_EMPTY_HEAD and Q_EMPTY_TAIL. */
	asd_hwi_swb_write_word(asd, CSEQ_Q_EMPTY_HEAD, 0xFFFF);
	asd_hwi_swb_write_word(asd, CSEQ_Q_EMPTY_TAIL, 0xFFFF);
	asd_hwi_swb_write_word(asd, CSEQ_NEED_EMPTY_SCB, 0x0);
	asd_hwi_swb_write_byte(asd, CSEQ_EMPTY_REQ_HEAD, 0x0);
	asd_hwi_swb_write_byte(asd, CSEQ_EMPTY_REQ_TAIL, 0x0);
	asd_hwi_swb_write_byte(asd, CSEQ_EMPTY_SCB_OFFSET, 0x0);
	asd_hwi_swb_write_word(asd, CSEQ_PRIMITIVE_DATA, 0x0);
	asd_hwi_swb_write_dword(asd, CSEQ_TIMEOUT_CONSTANT, 0x0);

#ifdef EXTENDED_SCB
//Todo 128 byte alignment
	{
		uint64_t dmabusaddr;
		dmabusaddr = (asd->ext_scb_map.busaddr + ASD_SCB_SIZE - 1)&(uint64_t)(-1 * ASD_SCB_SIZE);
#ifdef ASD_DEBUG
		asd_print("Extended SCB busaddr:0x%Lx, dmabusaddr:0x%Lx\n", asd->ext_scb_map.busaddr, dmabusaddr);
#endif
		asd_hwi_swb_write_dword(asd, (CMAPPEDSCR + CMDCTXBASE), ASD_GET_PADR(dmabusaddr));
		asd_hwi_swb_write_dword(asd, (CMAPPEDSCR + CMDCTXBASE + 4), ASD_GET_PUADR(dmabusaddr));
	//	CTXDOMAIN ?
	}
#endif
	
}

/* 
 * Function:
 *	asd_hwi_init_cseq_mdp()
 *
 * Description:
 *	Initialize CSEQ Mode Dependent Pages.	 
 */
static void	
asd_hwi_init_cseq_mdp(struct asd_softc *asd)
{
	u_int	i;
	u_int	mode_offset; 

	mode_offset = CSEQ_PAGE_SIZE * 2;
	
	/* CSEQ Mode Dependent 0-7, page 0 setup. */
	for (i = 0; i < 8; i++) {
		asd_hwi_swb_write_word(asd, ((i * mode_offset) + 
				       CSEQ_LRM_SAVE_SINDEX), 0x0);
		asd_hwi_swb_write_word(asd, ((i * mode_offset) + 
				       CSEQ_LRM_SAVE_SCBPTR), 0x0);
		asd_hwi_swb_write_word(asd, ((i * mode_offset) + 
				       CSEQ_Q_LINK_HEAD), 0xFFFF);
		asd_hwi_swb_write_word(asd, ((i * mode_offset) +
				       CSEQ_Q_LINK_TAIL), 0xFFFF);
	}
	
	/* CSEQ Mode Dependent 0-7, page 1 and 2 shall be ignored. */

	/* CSEQ Mode Dependent 8, page 0 setup. */
	asd_hwi_swb_write_word(asd, CSEQ_RET_ADDR, 0xFFFF);
	asd_hwi_swb_write_word(asd, CSEQ_RET_SCBPTR, 0x0);
	asd_hwi_swb_write_word(asd, CSEQ_SAVE_SCBPTR, 0x0);
	//asd_hwi_swb_write_word(asd, CSEQ_EMPTY_TRANS_CTX, 0x0);
	asd_hwi_swb_write_dword(asd, CSEQ_EMPTY_TRANS_CTX, 0x0);
	asd_hwi_swb_write_word(asd, CSEQ_RESP_LEN, 0x0);
	asd_hwi_swb_write_word(asd, CSEQ_TMF_SCBPTR, 0x0);
	asd_hwi_swb_write_word(asd, CSEQ_GLOBAL_PREV_SCB, 0x0);
#ifdef SEQUENCER_UPDATE
	asd_hwi_swb_write_word(asd, CSEQ_CLEAR_LU_HEAD, 0x0);
	asd_hwi_swb_write_word(asd, CSEQ_TMF_OPCODE, 0x0);
	asd_hwi_swb_write_word(asd, CSEQ_SCRATCH_FLAGS, 0x0);
	asd_hwi_swb_write_word(asd, CSEQ_HSS_SITE, 0x0);
#else
	asd_hwi_swb_write_word(asd, CSEQ_GLOBAL_HEAD, 0x0);
	asd_hwi_swb_write_byte(asd, CSEQ_TMF_OPCODE, 0x0);
	asd_hwi_swb_write_word(asd, CSEQ_CLEAR_LU_HEAD, 0x0);
#endif
	asd_hwi_swb_write_word(asd, CSEQ_FIRST_INV_SCB_SITE,
			       ASD_MAX_SCB_SITES);
	asd_hwi_swb_write_word(asd, CSEQ_FIRST_INV_DDB_SITE, ASD_MAX_DDBS);

	/* CSEQ Mode Dependent 8, page 1 setup. */
	asd_hwi_swb_write_dword(asd, CSEQ_LUN_TO_CLEAR, 0x0);
	asd_hwi_swb_write_dword(asd, (CSEQ_LUN_TO_CLEAR + 4), 0x0);
#ifndef SEQUENCER_UPDATE
	asd_hwi_swb_write_dword(asd, CSEQ_LUN_TO_CHECK, 0x0);
	asd_hwi_swb_write_dword(asd, (CSEQ_LUN_TO_CHECK + 4), 0x0);
#endif

	/* CSEQ Mode Dependent 8, page 2 setup. */
	/* Advertise the first SCB site address to the sequencer. */
	asd_hwi_set_hw_addr(asd, CSEQ_Q_NEW_POINTER,
			    asd->next_queued_hscb_busaddr);

	/* Advertise the first Done List address to the sequencer.*/
	asd_hwi_set_hw_addr(asd, CSEQ_Q_DONE_BASE,
			    asd->shared_data_map.busaddr);

	/* 
	 * Initialize the Q_DONE_POINTER with the least significant bytes of 
	 * the first Done List address.
	 */
	asd_hwi_swb_write_dword(asd, CSEQ_Q_DONE_POINTER,
				ASD_GET_PADR(asd->shared_data_map.busaddr));

	asd_hwi_swb_write_byte(asd, CSEQ_Q_DONE_PASS, ASD_QDONE_PASS_DEF);
	
	/* CSEQ Mode Dependent 8, page 3 shall be ignored. */
}

/* 
 * Function:
 *	asd_hwi_init_cseq_cio()
 *
 * Description:
 *	Initialize CSEQ CIO Registers.	 
 */
static void
asd_hwi_init_cseq_cio(struct asd_softc *asd)
{
	uint8_t		dl_bits;
	u_int		i;
	
	/* Enabled ARP2HALTC (ARP2 Halted from Halt Code Write). */
#ifdef SEQUENCER_UPDATE
	asd_hwi_swb_write_byte(asd, CARP2INTEN, 
		EN_ARP2HALTC | EN_ARP2ILLOPC | EN_ARP2PERR | EN_ARP2CIOPERR);

	asd_hwi_swb_write_dword(asd, CARP2BREAKADR01, 0x0fff0fff);
	asd_hwi_swb_write_dword(asd, CARP2BREAKADR23, 0x0fff0fff);
#else
	asd_hwi_swb_write_byte(asd, CARP2INTEN, EN_ARP2HALTC);
#endif

	/* Initialize CSEQ Scratch Page to 0x04. */
	asd_hwi_swb_write_byte(asd, CSCRATCHPAGE, 0x04);
	
	/* Initialize CSEQ Mode[0-8] Dependent registers. */	
	for (i = 0; i < 9; i++) 
		/* Initialize Scratch Page to 0. */
		asd_hwi_swb_write_byte(asd, CMnSCRATCHPAGE(i), 0x0);
	
	/* 
	 * CSEQCOMINTEN, CSEQDLCTL, and CSEQDLOFFS are in Mode 8.
	 */
	asd_hwi_swb_write_byte(asd, CSEQCOMINTEN, 0x0);
	
	/*
	 * Get the DONELISTSIZE bits by calculate number of DL entry 
	 * available and set the value to the DoneList Control register.
	 */
	dl_bits = ffs(asd->dl_wrap_mask + 1);
	
	/* Minimum amount of done lists is 4. Reduce dl_bits by 3. */
	dl_bits -= 3;
	asd_hwi_swb_write_byte(asd, CSEQDLCTL, 
		((asd_hwi_swb_read_byte(asd, CSEQDLCTL) & 
		 ~DONELISTSIZE_MASK) | dl_bits));
	
	asd_hwi_swb_write_byte(asd, CSEQDLOFFS, 0);
	asd_hwi_swb_write_byte(asd, (CSEQDLOFFS+1), 0);

	/* Reset the Producer mailbox. */
	asd_write_dword(asd, SCBPRO, 0x0);				

	/* Intialize CSEQ Mode 11 Interrupt Vectors. */
	asd_hwi_swb_write_word(asd, CM11INTVEC0, 
			      ((ASD_SET_INT_VEC(asd, CSEQ_INT_VEC0)) / 4));
	asd_hwi_swb_write_word(asd, CM11INTVEC1,
			      ((ASD_SET_INT_VEC(asd, CSEQ_INT_VEC1)) / 4));
	asd_hwi_swb_write_word(asd, CM11INTVEC2,
			      ((ASD_SET_INT_VEC(asd, CSEQ_INT_VEC2)) / 4));

	/* Reset the ARP2 Program Count. */
	asd_hwi_swb_write_word(asd, CPRGMCNT, (CSEQ_IDLE_LOOP_ENTRY / 4));

	for (i = 0; i < 8; i++) {
		/* Intialize Mode n Link m Interrupt Enable. */
		asd_hwi_swb_write_dword(asd, CMnINTEN(i), EN_CMnRSPMBXF);
		/* Initialize Mode n Request Mailbox. */
		asd_hwi_swb_write_dword(asd, CMnREQMBX(i), 0x0);	
	}

	/* Reset the Consumer mailbox. */
	asd_hwi_swb_write_dword(asd, CSEQCON, 0x0);
}

/* 
 * Function:
 *	asd_hwi_init_scb_sites()
 *
 * Description:
 *	Initialize HW SCB sites.	 
 */
static void
asd_hwi_init_scb_sites(struct asd_softc *asd)
{
	uint16_t	site_no;
	uint16_t	next_site_no;
	u_int		i;

#ifndef EXTENDED_SCB
	for (site_no = 0; site_no < ASD_MAX_SCB_SITES; site_no++) {
#else
	for (site_no = 0; site_no < (ASD_MAX_SCB_SITES + ASD_EXTENDED_SCB_NUMBER); site_no++) {
#endif
		/* 
		 * Adjust to the SCB site that we want to access in command
		 * context memory.
		 */	 
		asd_hwi_set_scbptr(asd, site_no);

		/* Initialize all fields in the SCB site to 0. */
		for (i = 0; i < ASD_SCB_SIZE; i += 4)
			asd_hwi_set_scbsite_dword(asd, i, 0x0);
		/* Initialize SCB Site Opcode field to invalid. */
		asd_hwi_set_scbsite_byte(asd,offsetof(struct hscb_header, opcode),
				       0xFF);

		/* Initialize SCB Site Flags field to mean a response
		 * frame has been received.  This means inadvertent
		 * frames received to be dropped. */
		asd_hwi_set_scbsite_byte(asd,0x49, 0x01);


		/*
		 * Workaround needed by SEQ to fix a SATA issue is to skip
	      	 * including scb sites that ended with FFh in the sequencer
		 * free list.
		 */
		if ((site_no & 0xF0FF) == 0x00FF)
			continue;
#ifdef SATA_SKIP_FIX
		/*
		 * Another work around. Skip the first 32 SCBs in each
		 * group of 256.
		 */
		if ((site_no & 0xF0FF) <= 0x001F)
			continue;
#endif
		
		/*
		 * For every SCB site, we need to initialize the following 
		 * fields: Q_NEXT, SCB_OPCODE, SCB_FLAGS, and SG Element Flag.
		 */

		next_site_no = ((((site_no+1) & 0xF0FF) == 0x00FF) ?
				  (site_no+2) : (site_no+1));
#ifdef SATA_SKIP_FIX
		if ((next_site_no & 0xF0FF) == 0x0000)
			next_site_no = next_site_no + 0x20;
#endif
		/* 
	 	 * Set the Q_NEXT of last usable SCB site to 0xFFFF.
	 	 */
#ifndef EXTENDED_SCB
		if (next_site_no >= ASD_MAX_SCB_SITES)
#else
		if ( (next_site_no >= (ASD_MAX_SCB_SITES + ASD_EXTENDED_SCB_NUMBER)) ||
			 (site_no == (ASD_MAX_SCB_SITES-1)) )
#endif
			next_site_no = 0xFFFF;

		/* Initialize SCB Site Opcode field. */
		asd_hwi_set_scbsite_byte(asd, 
					 offsetof(struct hscb_header, opcode),
					 0xFF);	

		/* Initialize SCB Site Flags field. */
		asd_hwi_set_scbsite_byte(asd, SCB_SITE_FLAGS, 0x01);	

		/* 
		 * Set the first SG Element flag to no data transfer.
		 * Also set Domain Select to OffChip memory. 
		 */
		asd_hwi_set_scbsite_byte(asd, 
			offsetof(struct asd_ssp_task_hscb,
				sg_elements[0].flags), SG_NO_DATA);

		/* 
		 * Initialize Q_NEXT field to point to the next SCB site.
		 */
		asd_hwi_set_scbsite_word(asd, SCB_SITE_Q_NEXT, next_site_no);
	}
}

/* 
 * Function:
 *	asd_hwi_post_init_cseq()
 *
 * Description:
 * 	Clear CSEQ Mode n Interrupt status and Response mailbox.
 */
static void
asd_hwi_post_init_cseq(struct asd_softc *asd)
{
	u_int		i;

	for (i = 0; i < 8; i++) {
		asd_hwi_swb_write_dword(asd, CMnINT(i), 0xFFFFFFFF);	
		asd_hwi_swb_read_dword(asd, CMnRSPMBX(i));
	}
	
	/* Reset the External Interrupt Control. */
	asd_hwi_swb_write_byte(asd, CARP2INTCTL, RSTINTCTL); 
}

/* 
 * Function:
 *	asd_hwi_lseq_init_scratch()
 *
 * Description:
 *	Setup and initialize Link sequencers. Initialiaze the mode 
 *	independent and dependent scratch page to the default settings.
 */
static void
asd_hwi_init_lseq_scratch(struct asd_softc *asd)
{
	unsigned	i;
	uint8_t		enabled_phys;
	u_int		link_num;
	
	link_num = 0;
	enabled_phys = asd->hw_profile.enabled_phys;
	do {
		for ( ; link_num < asd->hw_profile.max_phys; link_num++) {
			if (enabled_phys & (1 << link_num)) {
				enabled_phys &= ~(1 << link_num);
				break;
			} 
		}

		/*
		 * Clear out memory
		 */
		for (i = 0 ; i < LmMAPPEDSCR_LEN ; i+=4)
			asd_hwi_swb_write_dword(asd, LmSCRATCH(link_num) + i,
				0x0000);
		
		/* Initialize LmSEQ Mode Independent Page. */
		asd_hwi_init_lseq_mip(asd, link_num); 

		/* Initialize LmSEQ Mode Dependent Page. */
		asd_hwi_init_lseq_mdp(asd, link_num);

	} while (enabled_phys != 0); 
}

/* 
 * Function:
 *	asd_hwi_init_lseq_mip()
 *
 * Description:
 *	Initialize LSEQ Mode Independent Pages 0-3.	 
 */
static void
asd_hwi_init_lseq_mip(struct asd_softc *asd, u_int link_num)
{
	u_int	i;
	
	/* ------------------------------------- */
	/* LSEQ Mode Independent , page 0 setup. */
	/* ------------------------------------- */
	asd_hwi_swb_write_word(asd, LmSEQ_Q_TGTXFR_HEAD(link_num), 0xFFFF);
	asd_hwi_swb_write_word(asd, LmSEQ_Q_TGTXFR_TAIL(link_num), 0xFFFF);
	asd_hwi_swb_write_byte(asd, LmSEQ_LINK_NUMBER(link_num),
			      (uint8_t) link_num);

#if SAS_ENABLE_NOTIFY
	/*
	 * TBRV: For Seagate Gen 1.5 drive, we need to issue NOTIFY primitive 
	 * 	 before we issue the first IO to the drive.
	 *	 Need to investigate how other drives (such as Hitachi, Maxtor)
	 *	 behave. This might be set to default case if all other drives
	 *	 also required NOTIFY primitive to be sent.
	 */
	asd_hwi_swb_write_byte(asd, LmSEQ_SCRATCH_FLAGS(link_num), 
			       SAS_NOTIFY_SPINUP_ENABLED);
#else
	asd_hwi_swb_write_byte(asd, LmSEQ_SCRATCH_FLAGS(link_num), 0x0);
#endif

	asd_hwi_swb_write_dword(asd, LmSEQ_CONNECTION_STATE(link_num), 
				0x08000000);
	asd_hwi_swb_write_word(asd, LmSEQ_CONCTL(link_num), 0x0);
	asd_hwi_swb_write_byte(asd, LmSEQ_CONSTAT(link_num), 0x0);
	asd_hwi_swb_write_byte(asd, LmSEQ_CONNECTION_MODES(link_num), 0x0);
	asd_hwi_swb_write_word(asd, LmSEQ_REG1_ISR(link_num), 0x0);
	asd_hwi_swb_write_word(asd, LmSEQ_REG2_ISR(link_num), 0x0);		
	asd_hwi_swb_write_word(asd, LmSEQ_REG3_ISR(link_num), 0x0);
	asd_hwi_swb_write_dword(asd, LmSEQ_REG0_ISR(link_num), 0x0);
	asd_hwi_swb_write_dword(asd, (LmSEQ_REG0_ISR(link_num) + 4), 0x0);
	
	/* ------------------------------------- */
	/* LSEQ Mode Independent , page 1 setup. */
	/* ------------------------------------- */
#ifdef SEQUENCER_UPDATE
	asd_hwi_swb_write_word(asd, LmSEQ_EST_NEXUS_SCB_PTR0(link_num), 0xFFFF);
	asd_hwi_swb_write_word(asd, LmSEQ_EST_NEXUS_SCB_PTR1(link_num), 0xFFFF);
	asd_hwi_swb_write_word(asd, LmSEQ_EST_NEXUS_SCB_PTR2(link_num), 0xFFFF);
	asd_hwi_swb_write_word(asd, LmSEQ_EST_NEXUS_SCB_PTR3(link_num), 0xFFFF);
	asd_hwi_swb_write_byte(asd, LmSEQ_EST_NEXUS_SCB_OPCD0(link_num), 0x0);
	asd_hwi_swb_write_byte(asd, LmSEQ_EST_NEXUS_SCB_OPCD1(link_num), 0x0);
	asd_hwi_swb_write_byte(asd, LmSEQ_EST_NEXUS_SCB_OPCD2(link_num), 0x0);
	asd_hwi_swb_write_byte(asd, LmSEQ_EST_NEXUS_SCB_OPCD3(link_num), 0x0);
	asd_hwi_swb_write_byte(asd, LmSEQ_EST_NEXUS_SCB_HEAD(link_num), 0x0);
	asd_hwi_swb_write_byte(asd, LmSEQ_EST_NEXUS_SCB_TAIL(link_num), 0x0);
	asd_hwi_swb_write_byte(asd, LmSEQ_EST_NEXUS_BUFS_AVAIL(link_num), 0x0);
#else
	asd_hwi_swb_write_byte(asd, LmSEQ_FRAME_TYPE_MASK(link_num), 0xFF);
	/* Hashed Destination Address (3 bytes). */	
	for (i = 0; i < 3; i++)
		asd_hwi_swb_write_byte(asd, 
				      (LmSEQ_HASHED_DEST_ADDR_MASK(link_num)+i),
				       0xFF);
	/* Reserved field (1 byte). */
	asd_hwi_swb_write_byte(asd, (LmSCRATCH(link_num) + 0x01A4), 0x0);
	
	/* Hashed Source Address (3 bytes). */
	for (i = 0; i < 3; i++)
		asd_hwi_swb_write_byte(asd, 
				      (LmSEQ_HASHED_SRC_ADDR_MASK(link_num)+i),
				       0xFF);	
	/* Reserved fields (2 bytes). */
	asd_hwi_swb_write_word(asd, (LmSCRATCH(link_num) + 0x01A8), 0x0);
	asd_hwi_swb_write_byte(asd, LmSEQ_RETRANSMIT_MASK(link_num), 0x0);
	asd_hwi_swb_write_byte(asd, LmSEQ_NUM_FILL_BYTES_MASK(link_num), 0x0);
	/* Reserved field (4 bytes). */
	asd_hwi_swb_write_dword(asd, (LmSCRATCH(link_num) + 0x01AC), 0x0);
	asd_hwi_swb_write_word(asd, LmSEQ_TAG_MASK(link_num), 0x0);
	asd_hwi_swb_write_word(asd, LmSEQ_TARGET_PORT_XFER_TAG(link_num), 0x0);
	asd_hwi_swb_write_dword(asd, LmSEQ_DATA_OFFSET(link_num), 0xFFFFFFFF);
#endif
	asd_hwi_swb_write_word(asd, LmSEQ_ISR_SAVE_SINDEX(link_num), 0x0);
	asd_hwi_swb_write_word(asd, LmSEQ_ISR_SAVE_DINDEX(link_num), 0x0);	
	
	/* ------------------------------------- */
	/* LSEQ Mode Independent , page 2 setup. */
	/* ------------------------------------- */
	asd_hwi_swb_write_word(asd, LmSEQ_EMPTY_SCB_PTR0(link_num), 0xFFFF);
	asd_hwi_swb_write_word(asd, LmSEQ_EMPTY_SCB_PTR1(link_num), 0xFFFF);
	asd_hwi_swb_write_word(asd, LmSEQ_EMPTY_SCB_PTR2(link_num), 0xFFFF);
	asd_hwi_swb_write_word(asd, LmSEQ_EMPTY_SCB_PTR3(link_num), 0xFFFF);
	asd_hwi_swb_write_byte(asd, LmSEQ_EMPTY_SCB_OPCD0(link_num), 0x0);
	asd_hwi_swb_write_byte(asd, LmSEQ_EMPTY_SCB_OPCD1(link_num), 0x0);
	asd_hwi_swb_write_byte(asd, LmSEQ_EMPTY_SCB_OPCD2(link_num), 0x0);
	asd_hwi_swb_write_byte(asd, LmSEQ_EMPTY_SCB_OPCD3(link_num), 0x0);
	asd_hwi_swb_write_byte(asd, LmSEQ_EMPTY_SCB_HEAD(link_num), 0x0);
	asd_hwi_swb_write_byte(asd, LmSEQ_EMPTY_SCB_TAIL(link_num), 0x0);
	asd_hwi_swb_write_byte(asd, LmSEQ_EMPTY_BUFS_AVAIL(link_num), 0x0);

	for (i = 0; i < 12; i += 4)
		asd_hwi_swb_write_dword(asd, (LmSEQ_ATA_SCR_REGS(link_num) + i),
			       		0x0);	
	 
	/* ------------------------------------- */
	/* LSEQ Mode Independent , page 3 setup. */
	/* ------------------------------------- */
	/* 
	 * Set the desired interval between transmissions of the NOTIFY
	 * (ENABLE SPINUP) primitive to 500 msecs.
	 */ 
	asd_hwi_swb_write_word(asd, LmSEQ_DEV_PRES_TMR_TOUT_CONST(link_num),
			      (SAS_NOTIFY_TIMER_TIMEOUT_CONST - 1));
	/* No delay for the first NOTIFY to be sent to the attached target. */
	asd_hwi_swb_write_word(asd, LmSEQ_NOTIFY_TIMER_DOWN_CNT(link_num),
			       SAS_DEFAULT_NOTIFY_TIMER_DOWN_CNT);
	/* Reserved fields (4 bytes). */
	asd_hwi_swb_write_dword(asd, LmSEQ_SATA_INTERLOCK_TIMEOUT(link_num),
				0x0);
	/* Initialize STP shutdown timeout to 50 usecs. */
#ifndef SEQUENCER_UPDATE
	asd_hwi_swb_write_dword(asd, LmSEQ_STP_SHUTDOWN_TIMEOUT(link_num),
				SAS_DEFAULT_STP_SHUTDOWN_TIMER_TIMEOUT);
#endif
	asd_hwi_swb_write_dword(asd, LmSEQ_SRST_ASSERT_TIMEOUT(link_num),
				SAS_DEFAULT_SRST_ASSERT_TIMEOUT);
	asd_hwi_swb_write_dword(asd, LmSEQ_RCV_FIS_TIMEOUT(link_num),
				SAS_DEFAULT_RCV_FIS_TIMEOUT);
	asd_hwi_swb_write_dword(asd, LmSEQ_ONE_MILLISEC_TIMEOUT(link_num),
				SAS_DEFAULT_ONE_MILLISEC_TIMEOUT);
#ifdef SEQUENCER_UPDATE
	asd_hwi_swb_write_dword(asd, LmSEQ_TEN_MILLISEC_TIMEOUT(link_num),
				SAS_DEFAULT_COMINIT_TIMEOUT);
#else
	asd_hwi_swb_write_dword(asd, LmSEQ_COMINIT_TIMEOUT(link_num),
				SAS_DEFAULT_COMINIT_TIMEOUT);
#endif
	asd_hwi_swb_write_dword(asd, LmSEQ_SMP_RCV_TIMEOUT(link_num),
				SAS_DEFAULT_SMP_RCV_TIMEOUT);			
}

/* 
 * Function:
 *	asd_hwi_init_lseq_mdp()
 *
 * Description:
 *	Initialize LSEQ Mode Dependent Pages.	 
 */
static void
asd_hwi_init_lseq_mdp(struct asd_softc *asd, u_int link_num)
{
	u_int		i, j;
	uint32_t	mode_offset;
	
	/*
	 * -------------------------------------
	 * Mode 0,1,2 and 4/5 have common field on page 0 for the first 
	 * 14 bytes.
	 * -------------------------------------
	 */
	for (i = 0; i < 3; i++) {
		mode_offset = i * LSEQ_MODE_SCRATCH_SIZE; 
#ifdef SEQUENCER_UPDATE
		if (i == 2)
			asd_hwi_swb_write_word(asd, (LmSEQ_RET_ADDR(link_num) + 
					       mode_offset), 0x400); //R_LSEQ_MODE2_TASK(0x1000) >> 2
		else
#endif
		asd_hwi_swb_write_word(asd, (LmSEQ_RET_ADDR(link_num) + 
				       mode_offset), 0xFFFF);
		asd_hwi_swb_write_word(asd, (LmSEQ_REG0_MODE(link_num) +
				       mode_offset), 0x0);
		asd_hwi_swb_write_word(asd, (LmSEQ_MODE_FLAGS(link_num) +
				       mode_offset), 0x0);
#ifdef SEQUENCER_UPDATE
		asd_hwi_swb_write_word(asd, (LmSEQ_RET_ADDR2(link_num) +
				       mode_offset), 0xFFFF);
		asd_hwi_swb_write_word(asd, (LmSEQ_RET_ADDR1(link_num) +
				       mode_offset), 0xFFFF);
#endif
		asd_hwi_swb_write_word(asd, (LmSEQ_RET_ADDR_SAVE(link_num) +
				       mode_offset), 0x0);
		asd_hwi_swb_write_word(asd, (LmSEQ_RET_ADDR_SAVE2(link_num) +
				       mode_offset), 0x0);
		asd_hwi_swb_write_byte(asd, (LmSEQ_OPCODE_TO_CSEQ(link_num) +
				       mode_offset), 0x0);
		asd_hwi_swb_write_word(asd, (LmSEQ_DATA_TO_CSEQ(link_num) +
				       mode_offset), 0x0);
	}
	/*
	 * -------------------------------------
	 *  Mode 5 page 0 overlaps the same scratch page with Mode 0 page 3.
	 * -------------------------------------
	 */
	asd_hwi_swb_write_word(asd, (LmSEQ_RET_ADDR(link_num) +
			       LSEQ_MODE5_PAGE0_OFFSET), 0xFFFF);
	asd_hwi_swb_write_word(asd, (LmSEQ_REG0_MODE(link_num) + 
			       LSEQ_MODE5_PAGE0_OFFSET), 0x0);
	asd_hwi_swb_write_word(asd, (LmSEQ_MODE_FLAGS(link_num) + 
			       LSEQ_MODE5_PAGE0_OFFSET), 0x0);
	asd_hwi_swb_write_word(asd, (LmSEQ_RET_ADDR_SAVE(link_num) + 
			       LSEQ_MODE5_PAGE0_OFFSET), 0x0);
	asd_hwi_swb_write_word(asd, (LmSEQ_RET_ADDR_SAVE2(link_num) + 
			       LSEQ_MODE5_PAGE0_OFFSET), 0x0);
	asd_hwi_swb_write_byte(asd, (LmSEQ_OPCODE_TO_CSEQ(link_num) + 
			       LSEQ_MODE5_PAGE0_OFFSET), 0x0);
	asd_hwi_swb_write_word(asd, (LmSEQ_DATA_TO_CSEQ(link_num) + 
			       LSEQ_MODE5_PAGE0_OFFSET), 0x0);
	
	/*
	 * -------------------------------------
	 * LSEQ Mode Dependent 0, page 0 setup.
	 * -------------------------------------
	 */
	asd_hwi_swb_write_word(asd, LmSEQ_FIRST_INV_DDB_SITE(link_num),
			       ASD_MAX_DDBS);
	asd_hwi_swb_write_word(asd, LmSEQ_EMPTY_TRANS_CTX(link_num), 0x0);
	asd_hwi_swb_write_word(asd, LmSEQ_RESP_LEN(link_num), 0x0);
	asd_hwi_swb_write_word(asd, LmSEQ_FIRST_INV_SCB_SITE(link_num), 
			       ASD_MAX_SCB_SITES);
	asd_hwi_swb_write_dword(asd, LmSEQ_INTEN_SAVE(link_num), 
				(uint32_t) LmM0INTEN_MASK);
	asd_hwi_swb_write_byte(asd, LmSEQ_LNK_RST_FRM_LEN(link_num), 0x0);
	asd_hwi_swb_write_byte(asd, LmSEQ_LNK_RST_PROTOCOL(link_num), 0x0);
	asd_hwi_swb_write_byte(asd, LmSEQ_RESP_STATUS(link_num), 0x0);
	asd_hwi_swb_write_byte(asd, LmSEQ_LAST_LOADED_SGE(link_num), 0x0);
	asd_hwi_swb_write_word(asd, LmSEQ_SAVE_SCBPTR(link_num), 0x0);
		
	/*
	 * -------------------------------------
	 * LSEQ Mode Dependent 1, page 0 setup.
	 * -------------------------------------
	 */
	asd_hwi_swb_write_word(asd, LmSEQ_Q_XMIT_HEAD(link_num), 0xFFFF);
	//asd_hwi_swb_write_word(asd, LmSEQ_M1_EMPTY_TRANS_CTX(link_num), 0x0);
	asd_hwi_swb_write_word(asd, LmSEQ_M1_EMPTY_TRANS_CTX(link_num), 0x0);
#ifdef SEQUENCER_UPDATE
	asd_hwi_swb_write_byte(asd, LmSEQ_FAILED_OPEN_STATUS(link_num), 0x0);
#endif
	asd_hwi_swb_write_byte(asd, LmSEQ_XMIT_REQUEST_TYPE(link_num), 0x0);
	asd_hwi_swb_write_byte(asd, LmSEQ_M1_RESP_STATUS(link_num), 0x0);
	asd_hwi_swb_write_byte(asd, LmSEQ_M1_LAST_LOADED_SGE(link_num), 0x0);
	asd_hwi_swb_write_word(asd, LmSEQ_M1_SAVE_SCBPTR(link_num), 0x0);
	
	/*
	 * -------------------------------------
	 * LSEQ Mode Dependent 2, page 0 setup
	 * -------------------------------------
	 */
	asd_hwi_swb_write_word(asd, LmSEQ_PORT_COUNTER(link_num), 0x0);
	asd_hwi_swb_write_word(asd, LmSEQ_PM_TABLE_PTR(link_num), 0x0);
	asd_hwi_swb_write_word(asd, LmSEQ_SATA_INTERLOCK_TMR_SAVE(link_num),
			       0x0);
#ifdef SEQUENCER_UPDATE
	asd_hwi_swb_write_word(asd, LmSEQ_COPY_SMP_CONN_TAG(link_num),0x0);
	asd_hwi_swb_write_word(asd, LmSEQ_P0M2_OFFS1AH(link_num),0x0);
#endif
	
	/*
	 * -------------------------------------
	 * LSEQ Mode Dependent 4 and 5, page 0 setup.
	 * -------------------------------------
	 */
	asd_hwi_swb_write_byte(asd, LmSEQ_SAVED_OOB_STATUS(link_num), 0x0);
	asd_hwi_swb_write_byte(asd, LmSEQ_SAVED_OOB_MODE(link_num), 0x0);
	asd_hwi_swb_write_word(asd, LmSEQ_Q_LINK_HEAD(link_num), 0xFFFF);
	asd_hwi_swb_write_byte(asd, LmSEQ_LNK_RST_ERR(link_num), 0x0);
	asd_hwi_swb_write_byte(asd, LmSEQ_SAVED_OOB_SIGNALS(link_num), 0x0);
	asd_hwi_swb_write_byte(asd, LmSEQ_SAS_RESET_MODE(link_num), 0x0);
#ifndef SEQUENCER_UPDATE
	asd_hwi_swb_write_dword(asd, LmSEQ_SAVE_COMINIT_TIMER(link_num), 0x0);
#endif
#ifdef SEQUENCER_UPDATE
	asd_hwi_swb_write_byte(asd, LmSEQ_LINK_RESET_RETRY_COUNT(link_num), 0);
	asd_hwi_swb_write_byte(asd, LmSEQ_NUM_LINK_RESET_RETRIES(link_num), 0);
	asd_hwi_swb_write_word(asd, LmSEQ_OOB_INT_ENABLES(link_num), 0);
	/*
	 * Set the desired interval between transmissions of the NOTIFY
	 * (ENABLE SPINUP) primitive.  Must be initilized to val - 1.
	 */
	asd_hwi_swb_write_word(asd, LmSEQ_NOTIFY_TIMER_TIMEOUT(link_num),
			   ASD_NOTIFY_TIMEOUT - 1);
	/* No delay for the first NOTIFY to be sent to the attached target. */
	asd_hwi_swb_write_word(asd, LmSEQ_NOTIFY_TIMER_DOWN_COUNT(link_num),
			   ASD_NOTIFY_DOWN_COUNT);
	asd_hwi_swb_write_word(asd, LmSEQ_NOTIFY_TIMER_INITIAL_COUNT(link_num),
			   ASD_NOTIFY_DOWN_COUNT);
#endif

	
	/*
	 * -------------------------------------
	 * LSEQ Mode Dependent 0 and 1, page 1 setup.
	 * -------------------------------------
	 */
	for (i = 0; i < 2; i++)	{
		/* Start from Page 1 of Mode 0 and 1. */
		mode_offset = LSEQ_PAGE_SIZE + (i*LSEQ_MODE_SCRATCH_SIZE);
		/* All the fields of page 1 can be intialized to 0. */
		for (j = 0; j < LSEQ_PAGE_SIZE; j += 4) {
			asd_hwi_swb_write_dword(asd, 
					       (LmSCRATCH(link_num) +
						mode_offset + j), 0x0);
		}
	}

	/*
	 * -------------------------------------
	 * LSEQ Mode Dependent 2, page 1 setup.
	 * -------------------------------------
	 */
	asd_hwi_swb_write_dword(asd, LmSEQ_INVALID_DWORD_CNT(link_num), 0x0);
	asd_hwi_swb_write_dword(asd, LmSEQ_DISPARITY_ERROR_CNT(link_num), 0x0);
	asd_hwi_swb_write_dword(asd, LmSEQ_LOSS_OF_SYNC_CNT(link_num), 0x0);
			
	/*
	 * -------------------------------------
	 * LSEQ Mode Dependent 4 and 5, page 1 shall be ignored.
	 * -------------------------------------
	 */
	
	/*
	 * -------------------------------------
	 * LSEQ Mode Dependent 0, page 2 setup.
	 * -------------------------------------
	 */
#ifndef SEQUENCER_UPDATE
	asd_hwi_swb_write_dword(asd, LmSEQ_ATTACHED_SAS_ADDR(link_num), 0x0);
	asd_hwi_swb_write_dword(asd, (LmSEQ_ATTACHED_SAS_ADDR(link_num)+4),
				0x0);
	asd_hwi_swb_write_dword(asd, LmSEQ_INITR_SAS_ADDR(link_num), 0x0);
	asd_hwi_swb_write_dword(asd, (LmSEQ_INITR_SAS_ADDR(link_num)+4), 0x0);
	asd_hwi_swb_write_dword(asd, LmSEQ_SMP_RCV_TIMER_TERM_TS(link_num),
				0x0);
	asd_hwi_swb_write_dword(asd, LmSEQ_M0_LAST_LOADED_SGE(link_num), 0x0);
	asd_hwi_swb_write_byte(asd, LmSEQ_SDB_TAG(link_num), 0x0);
	asd_hwi_swb_write_byte(asd, LmSEQ_SDB_MASK(link_num), 0x0);
	asd_hwi_swb_write_byte(asd, LmSEQ_DEVICE_BITS(link_num), 0x0);
	asd_hwi_swb_write_word(asd, LmSEQ_SDB_DDB(link_num), 0x0);
#else
	asd_hwi_swb_write_dword(asd, LmSEQ_SMP_RCV_TIMER_TERM_TS(link_num), 0);
	asd_hwi_swb_write_byte(asd, LmSEQ_DEVICE_BITS(link_num), 0);
	asd_hwi_swb_write_word(asd, LmSEQ_SDB_DDB(link_num), 0);
	asd_hwi_swb_write_byte(asd, LmSEQ_SDB_NUM_TAGS(link_num), 0);
	asd_hwi_swb_write_byte(asd, LmSEQ_SDB_CURR_TAG(link_num), 0);


#endif
	
	/*
	 * -------------------------------------
	 * LSEQ Mode Dependent 1, page 2 setup.
	 * -------------------------------------
	 */
	asd_hwi_swb_write_dword(asd, LmSEQ_TX_ID_ADDR_FRAME(link_num), 0x0);
	asd_hwi_swb_write_dword(asd, LmSEQ_TX_ID_ADDR_FRAME(link_num)+4, 0x0);
	asd_hwi_swb_write_dword(asd, LmSEQ_OPEN_TIMER_TERM_TS(link_num), 0x0);
	asd_hwi_swb_write_dword(asd, LmSEQ_SRST_AS_TIMER_TERM_TS(link_num),
				0x0);
	asd_hwi_swb_write_dword(asd, LmSEQ_M1P2_LAST_LOADED_SGE(link_num), 0x0);

	/*
	 * -------------------------------------
	 * LSEQ Mode Dependent 2, page 2 setup.
	 * -------------------------------------
	 */
	asd_hwi_swb_write_dword(asd, LmSEQ_STP_SHUTDOWN_TIMER_TERM_TS(link_num),
				0x0);
	asd_hwi_swb_write_dword(asd, LmSEQ_CLOSE_TIMER_TERM_TS(link_num), 0x0);	
	asd_hwi_swb_write_dword(asd, LmSEQ_BREAK_TIMER_TERM_TS(link_num), 0x0);
	asd_hwi_swb_write_dword(asd, LmSEQ_DWS_RESET_TIMER_TERM_TS(link_num),
				0x0);
	asd_hwi_swb_write_dword(asd,
				LmSEQ_SATA_INTERLOCK_TIMER_TERM_TS(link_num),
				0x0);
#ifdef SEQUENCER_UPDATE
	asd_hwi_swb_write_dword(asd,
				LmSEQ_MCTL_TIMER_TERM_TS(link_num),
				0x0);
#endif
	asd_hwi_swb_write_byte(asd, LmSEQ_DOWN_TIMER_DOWN_CNT(link_num), 0x0);

	/*
	 * -------------------------------------
	 * LSEQ Mode Dependent 4 and 5, page 2 setup.
	 * -------------------------------------
	 */
	asd_hwi_swb_write_dword(asd, LmSEQ_COMINIT_TIMER_TERM_TS(link_num),
				0x0);
	asd_hwi_swb_write_dword(asd, LmSEQ_RCV_ID_TIMER_TERM_TS(link_num),
				0x0);
	asd_hwi_swb_write_dword(asd, LmSEQ_RCV_FIS_TIMER_TERM_TS(link_num),
				0x0);
#ifdef SEQUENCER_UPDATE
		asd_hwi_swb_write_dword(asd, LmSEQ_DEV_PRES_TIMER_TERM_TS(link_num),	0);
#endif

#ifdef SEQUENCER_UPDATE
	/*
	 * -------------------------------------
	 * LSEQ Mode Dependent 4 and 5, page 1 setup.
	 * -------------------------------------
	 */
	for (i = 0; i < LSEQ_PAGE_SIZE; i+=4)
		asd_hwi_swb_write_dword(asd, LmSEQ_FRAME_TYPE_MASK(link_num)+i, 0);
	asd_hwi_swb_write_byte(asd, LmSEQ_FRAME_TYPE_MASK(link_num),
				0xff);
	asd_hwi_swb_write_word(asd, LmSEQ_HASH_DEST_ADDR_MASK(link_num),
				0xffff);
	asd_hwi_swb_write_byte(asd, (LmSEQ_HASH_DEST_ADDR_MASK(link_num) + 2),
				0xff);
	asd_hwi_swb_write_word(asd, LmSEQ_HASH_SRC_ADDR_MASK(link_num),
				0xffff);
	asd_hwi_swb_write_byte(asd, (LmSEQ_HASH_SRC_ADDR_MASK(link_num) + 2),
				0xff);
	asd_hwi_swb_write_byte(asd, LmSEQ_RETRANS_MASK(link_num),
				0x00);
	asd_hwi_swb_write_byte(asd, LmSEQ_NUM_FILL_BYTES_MASK(link_num),
				0x00);
	asd_hwi_swb_write_word(asd, LmSEQ_NUM_FILL_BYTES_MASK(link_num),
				0x0000);
	asd_hwi_swb_write_word(asd, LmSEQ_TAG_PORT_TRANSFER_MASK(link_num),
				0x0000);
	asd_hwi_swb_write_dword(asd, LmSEQ_DATA_OFFSET_MASK(link_num),
				0xffffffff);
#endif

}

/* 
 * Function:
 *	asd_hwi_init_lseq_cio()
 *
 * Description:
 *	Initialize LmSEQ CIO Registers.	 
 */
static void
asd_hwi_init_lseq_cio(struct asd_softc *asd, u_int link_num)
{
	struct asd_phy	*phy;
	uint32_t	 reqmbx_val;
	u_int		 i;
	
	/* Enabled ARP2HALTC (ARP2 Halted from Halt Code Write). */
#ifdef SEQUENCER_UPDATE
	asd_hwi_swb_write_dword(asd, LmARP2INTEN(link_num), 
		EN_ARP2HALTC | EN_ARP2ILLOPC | EN_ARP2PERR | EN_ARP2CIOPERR);
		//EN_ARP2HALTC);

	asd_hwi_swb_write_dword(asd, LmARP2BREAKADR01(link_num), 0x0fff0fff);
	asd_hwi_swb_write_dword(asd, LmARP2BREAKADR23(link_num), 0x0fff0fff);
#ifdef ASD_DEBUG
	//asd_hwi_set_lseq_breakpoint(asd, link_num, 0x14E4);
//	asd_hwi_set_lseq_breakpoint(asd, link_num, EN_ARP2BREAK0, 0xBEC);
//	asd_hwi_set_lseq_breakpoint(asd, link_num, EN_ARP2BREAK1, 0xD1C);
//	asd_hwi_set_lseq_breakpoint(asd, link_num, EN_ARP2BREAK2, 0xD08);
//	asd_hwi_set_lseq_breakpoint(asd, link_num, EN_ARP2BREAK3, 0xCF8);
#endif
#else
	asd_hwi_swb_write_dword(asd, LmARP2INTEN(link_num), EN_ARP2HALTC);
#endif
 
	asd_hwi_swb_write_byte(asd, LmSCRATCHPAGE(link_num), 0x0);
	
	/* Initialize Mode 0,1, and 2 SCRATCHPAGE to 0. */
	for (i = 0; i < 3; i++)
		asd_hwi_swb_write_byte(asd, LmMnSCRATCHPAGE(link_num, i), 0x0);

	/* Initialize Mode 5 SCRATCHPAGE to 0. */
	asd_hwi_swb_write_byte(asd, LmMnSCRATCHPAGE(link_num, 5), 0x0);
	
	asd_hwi_swb_write_dword(asd, LmRSPMBX(link_num), 0x0);
	/* 
	 * Initialize Mode 0,1,2 and 5 Interrupt Enable and 
	 * Interrupt registers. 
	 */
	asd_hwi_swb_write_dword(asd, LmMnINTEN(link_num, 0), LmM0INTEN_MASK);
	asd_hwi_swb_write_dword(asd, LmMnINT(link_num, 0), LmM0INTMASK);
	/* Mode 1 */
	asd_hwi_swb_write_dword(asd, LmMnINTEN(link_num, 1), LmM1INTEN_MASK);
	asd_hwi_swb_write_dword(asd, LmMnINT(link_num, 1), LmM1INTMASK);
	/* Mode 2 */
	asd_hwi_swb_write_dword(asd, LmMnINTEN(link_num, 2), LmM2INTEN_MASK);
	asd_hwi_swb_write_dword(asd, LmMnINT(link_num, 2), LmM2INTMASK);
	/* Mode 5 */
	asd_hwi_swb_write_dword(asd, LmMnINTEN(link_num, 5), LmM5INTEN_MASK);
	asd_hwi_swb_write_dword(asd, LmMnINT(link_num, 5), LmM5INTMASK);	
			
	/* Enabled HW Timer status. */
	asd_hwi_swb_write_byte(asd, LmHWTSTATEN(link_num), LmHWTSTATEN_MASK);
	
	/* Enabled Primitive Status 0 and 1. */
	asd_hwi_swb_write_dword(asd, LmPRMSTAT0EN(link_num), 
				LmPRMSTAT0EN_MASK);
	asd_hwi_swb_write_dword(asd, LmPRMSTAT1EN(link_num),
				LmPRMSTAT1EN_MASK);
				
	/* Enabled Frame Error. */ 
	asd_hwi_swb_write_dword(asd, LmFRMERREN(link_num), LmFRMERREN_MASK);
	/* Initialize SATA Hold level to 0x28. */
	asd_hwi_swb_write_byte(asd, LmMnHOLDLVL(link_num, 0), 
			       LmMnHOLD_INIT_VALUE);
	
	/* Initialize Mode 0 Transfer Level to 512. */
	asd_hwi_swb_write_byte(asd,  LmMnXFRLVL(link_num, 0), LmMnXFRLVL_512);
	/* Initialize Mode 1 Transfer Level to 256. */
	asd_hwi_swb_write_byte(asd, LmMnXFRLVL(link_num, 1), LmMnXFRLVL_256);
	
	/* Initialize Program Count to 0. */
	asd_hwi_swb_write_word(asd, LmPRGMCNT(link_num), 
			      (LSEQ_IDLE_LOOP_ENTRY / 4));

	/* Enabled Blind SG Move. */
	asd_hwi_swb_write_dword(asd, LmMODECTL(link_num), LmBLIND48);
	
	reqmbx_val = asd_hwi_swb_read_dword(asd, LmREQMBX(link_num));
	
	/* Clear Primitive Status 0 and 1. */
	asd_hwi_swb_write_dword(asd, LmPRMSTAT0(link_num), LmPRMSTAT0CLR_MASK);
	asd_hwi_swb_write_dword(asd, LmPRMSTAT1(link_num), LmPRMSTAT1CLR_MASK);	
	
	/* Clear HW Timer status. */
	asd_hwi_swb_write_byte(asd, LmHWTSTAT(link_num), LmHWTSTAT_MASK);
	
	/* Clear DMA Errors for Mode 0 and 1. */ 
	asd_hwi_swb_write_byte(asd, LmMnDMAERRS(link_num, 0), 0xFF);
	asd_hwi_swb_write_byte(asd, LmMnDMAERRS(link_num, 1), 0xFF);
	
	/* Clear SG DMA Errors for Mode 0 and 1. */ 
	asd_hwi_swb_write_byte(asd, LmMnSGDMAERRS(link_num, 0), 0xFF);
	asd_hwi_swb_write_byte(asd, LmMnSGDMAERRS(link_num, 1), 0xFF);
	
	/* Clear Mode 0 Buffer Parity Error. */		       
	asd_hwi_swb_write_byte(asd, LmMnBUFSTAT(link_num, 0), LmMnBUFPERR);
	
	/* Clear Mode 0 Frame Error register. */
	asd_hwi_swb_write_dword(asd, LmMnFRMERR(link_num, 0), LmMnFRMERR_INIT);
			       	
	/* Reset LSEQ Interrupt Controller. */
	asd_hwi_swb_write_byte(asd, LmARP2INTCTL(link_num), RSTINTCTL);
	
	/*
	 * Chip Rev. A1 can only payload up to 512 bytes in the DATA
         * frame. Chip Rev. B0 can have up to 1024 bytes.
	 * The value is in dwords.
	 */
	if (asd->hw_profile.rev_id != AIC9410_DEV_REV_A1) {
		/* Set the Transmit Size to 1024 bytes, 0 = 256 Dwords. */
		asd_hwi_swb_write_byte(asd, LmMnXMTSIZE(link_num, 1), 0x0);
	} else {
#ifdef SEQUENCER_UPDATE
		asd_hwi_swb_write_byte(asd, LmMnXMTSIZE(link_num, 1), 0x40);
#else
		/* Set the Transmit Size to 512 bytes. */
		asd_hwi_swb_write_byte(asd, LmMnXMTSIZE(link_num, 1), 0x80);
#endif
	}
		
	/* Enable SATA Port Multiplier. */
	asd_hwi_swb_write_byte(asd, LmMnSATAFS(link_num, 1), 0x80);

	/* Set the Phy SAS for the LmSEQ WWN. */
	phy = asd->phy_list[link_num];
	for (i = 0; i < SAS_ADDR_LEN; i++) 
		asd_hwi_swb_write_byte(asd, (LmWWN(link_num) + i),
				       phy->sas_addr[i]);

//#ifdef SEQUENCER_UPDATE
//	asd_hwi_swb_write_word(asd, LmBITL_TIMER(link_num), 1);
//#else
	/* Set the Bus Inactivity Time Limit Timer to 900 ms. */
	asd_hwi_swb_write_word(asd, LmBITL_TIMER(link_num), 9);
//#endif

	/* Initialize Interrupt Vector[0-10] address in Mode 3. */
	asd_hwi_swb_write_word(asd, LmM3INTVEC0(link_num), 
			      ((ASD_SET_INT_VEC(asd, LSEQ_INT_VEC0)) / 4));
	asd_hwi_swb_write_word(asd, LmM3INTVEC1(link_num), 
			      ((ASD_SET_INT_VEC(asd, LSEQ_INT_VEC1)) / 4));
	asd_hwi_swb_write_word(asd, LmM3INTVEC2(link_num), 
			      ((ASD_SET_INT_VEC(asd, LSEQ_INT_VEC2)) / 4));
	asd_hwi_swb_write_word(asd, LmM3INTVEC3(link_num), 
			      ((ASD_SET_INT_VEC(asd, LSEQ_INT_VEC3)) / 4));
	asd_hwi_swb_write_word(asd, LmM3INTVEC4(link_num), 
			      ((ASD_SET_INT_VEC(asd, LSEQ_INT_VEC4)) / 4));
	asd_hwi_swb_write_word(asd, LmM3INTVEC5(link_num), 
			      ((ASD_SET_INT_VEC(asd, LSEQ_INT_VEC5)) / 4));
	asd_hwi_swb_write_word(asd, LmM3INTVEC6(link_num), 
			      ((ASD_SET_INT_VEC(asd, LSEQ_INT_VEC6)) / 4));
	asd_hwi_swb_write_word(asd, LmM3INTVEC7(link_num), 
			      ((ASD_SET_INT_VEC(asd, LSEQ_INT_VEC7)) / 4));
	asd_hwi_swb_write_word(asd, LmM3INTVEC8(link_num), 
			      ((ASD_SET_INT_VEC(asd, LSEQ_INT_VEC8)) / 4));
	asd_hwi_swb_write_word(asd, LmM3INTVEC9(link_num), 
			      ((ASD_SET_INT_VEC(asd, LSEQ_INT_VEC9)) / 4));
	asd_hwi_swb_write_word(asd, LmM3INTVEC10(link_num), 
			      ((ASD_SET_INT_VEC(asd, LSEQ_INT_VEC10)) / 4));

	/* 
	 * Program the Link LED control, applicable only for 
	 * Chip Rev. B or later.
	 */
	if (asd->hw_profile.rev_id != AIC9410_DEV_REV_A1) {
		asd_hwi_swb_write_dword(asd, LmCONTROL(link_num),
					(LEDTIMER | LEDMODE_TXRX | 
					 LEDTIMERS_100ms));
	}

	/* Set the Align Rate for SAS and STP mode. */
	asd_hwi_swb_write_byte(asd, LmM1SASALIGN(link_num), SAS_ALIGN_DEFAULT);
	asd_hwi_swb_write_byte(asd, LmM1STPALIGN(link_num), STP_ALIGN_DEFAULT);
}

/* 
 * Function:
 *	asd_hwi_start_cseq()
 *
 * Description:
 *	Start the Central Sequencer.	 
 */
int
asd_hwi_start_cseq(struct asd_softc *asd)
{
	/* Reset the ARP2 instruction to location zero. */
	asd_hwi_swb_write_word(asd, (uint32_t) CPRGMCNT, 
			      (CSEQ_IDLE_LOOP_ENTRY / 4));

	/* Unpause the CSEQ  */
	return (asd_hwi_unpause_cseq(asd));
}

/* 
 * Function:
 *	asd_hwi_start_lseq()
 *
 * Description:
 *	Start the Link Sequencer (LmSEQ).	 
 */
int
asd_hwi_start_lseq(struct asd_softc  *asd, uint8_t  link_num)
{
	/* Reset the ARP2 instruction to location zero. */
	asd_hwi_swb_write_word(asd, (uint32_t) LmPRGMCNT(link_num), 
			      (LSEQ_IDLE_LOOP_ENTRY / 4));

	/* Unpause the LmSEQ  */
	return (asd_hwi_unpause_lseq(asd, (1U << link_num)));
}

/* 
 * Function:
 *	asd_swap_with_next_hscb()
 *
 * Description:
 *	Swap the hscb pointed to by scb with the next
 *	hscb the central sequencer is expecting to process.
 */
static inline void
asd_swap_with_next_hscb(struct asd_softc *asd, struct scb *scb)
{
	union hardware_scb 	*q_hscb;
	struct map_node 	*q_hscb_map;
	uint64_t 		 saved_hscb_busaddr;

	/*
	 * Our queuing method is a bit tricky.  The card
	 * knows in advance which HSCB (by address) to download,
	 * and we can't disappoint it.  To achieve this, the next
	 * HSCB to download is saved off in asd->next_queued_hscb.
	 * When we are called to queue "an arbitrary scb",
	 * we copy the contents of the incoming HSCB to the one
	 * the sequencer knows about, swap HSCB pointers and
	 * finally assign the SCB to the tag indexed location
	 * in the scb_array.  This makes sure that we can still
	 * locate the correct SCB by SCB_TAG.
	 */
	q_hscb = asd->next_queued_hscb;
	q_hscb_map = asd->next_queued_hscb_map;
	memcpy(q_hscb, scb->hscb, sizeof(*scb->hscb));
	q_hscb->header.next_hscb_busaddr = scb->hscb_busaddr;

	/* Now swap HSCB pointers. */
	asd->next_queued_hscb = scb->hscb;
	asd->next_queued_hscb_map = scb->hscb_map;
	scb->hscb = q_hscb;
	scb->hscb_map = q_hscb_map;
	saved_hscb_busaddr = asd->next_queued_hscb_busaddr;
	asd->next_queued_hscb_busaddr = scb->hscb_busaddr;
	scb->hscb_busaddr = saved_hscb_busaddr;

	/* Now define the mapping from tag to SCB in the scbindex */
	asd->scbindex[SCB_GET_INDEX(scb)] = scb;
}

#ifdef SEQUENCER_UPDATE
int
asd_hwi_cseq_init_step(struct asd_step_data *step_datap)
{
	uint32_t		arp2ctl;

	step_datap->lseq_trace = 0;

	asd_hwi_pause_cseq(step_datap->asd);

	arp2ctl = asd_hwi_swb_read_dword(step_datap->asd, CARP2CTL);

	if (arp2ctl & STEP) {
		printk("CSEQ already stepping\n");
		return 0;
	}

	asd_hwi_swb_write_dword(step_datap->asd, CARP2CTL, arp2ctl | STEP);

	asd_hwi_unpause_cseq(step_datap->asd);

	return 1;
}

static void
asd_hwi_cseq_resume(struct asd_step_data *step_datap)
{
	uint32_t		arp2ctl;

	step_datap->stepping = 0;

	arp2ctl = asd_hwi_swb_read_dword(step_datap->asd, CARP2CTL);

	asd_hwi_swb_write_dword(step_datap->asd, CARP2CTL, (arp2ctl & ~STEP));

	asd_hwi_unpause_cseq(step_datap->asd);
}

int
asd_hwi_lseq_init_step(struct asd_step_data *step_datap,
	unsigned link_num)
{
	uint32_t		arp2ctl;

	step_datap->lseq_trace = 1;
	step_datap->link_num = link_num;

	asd_hwi_pause_lseq(step_datap->asd, 1<<link_num);

	arp2ctl = asd_hwi_swb_read_dword(step_datap->asd, LmARP2CTL(link_num));

	if (arp2ctl & STEP) {
		printk("LSEQ(%d) already stepping\n", link_num);
		return 0;
	}

	asd_hwi_swb_write_dword(step_datap->asd, LmARP2CTL(link_num),
		arp2ctl | STEP);

	asd_hwi_unpause_lseq(step_datap->asd, 1<<link_num);

	return 1;
}

static void
asd_hwi_lseq_resume(struct asd_step_data *step_datap)
{
	uint32_t		arp2ctl;

	step_datap->stepping = 0;

	arp2ctl = asd_hwi_swb_read_dword(step_datap->asd,
		LmARP2CTL(step_datap->link_num));

	asd_hwi_swb_write_dword(step_datap->asd,
		LmARP2CTL(step_datap->link_num), (arp2ctl & ~STEP));

	asd_hwi_unpause_lseq(step_datap->asd, 1 << step_datap->link_num);
}

void
asd_hwi_ss_debug_timeout(
u_long		val
)
{
	unsigned		addr;
	struct asd_step_data	*step_datap;

	step_datap = (struct asd_step_data *)val;

	if (step_datap->instruction_count == 300)
	{
		asd_print("-----------------------\n");

		if (step_datap->lseq_trace == 0)
		{
			asd_hwi_cseq_resume(step_datap);
		}
		else
		{
			asd_hwi_lseq_resume(step_datap);
		}

		return;
	}

	if (step_datap->lseq_trace == 0)
	{
		addr = asd_hwi_swb_read_word(step_datap->asd, CLASTADDR) * 4;

		asd_print("   CSEQ:%20s[0x%x]:0x%04x\n",
			"ARP2_LASTADDR", LASTADDR, addr);

		asd_hwi_unpause_cseq(step_datap->asd);
	}
	else
	{
		addr = asd_hwi_swb_read_word(step_datap->asd,
			LmLASTADDR(step_datap->link_num)) * 4;

		asd_print("   LSEQ:%20s[0x%x]:0x%04x\n", "ARP2_LASTADDR",
			LASTADDR, addr);

		asd_hwi_unpause_lseq(step_datap->asd, 1<<step_datap->link_num);
	}

	step_datap->instruction_count++;
	step_datap->single_step_timer.expires = jiffies + 1;
	step_datap->single_step_timer.data = (u_long) step_datap;
	step_datap->single_step_timer.function = asd_hwi_ss_debug_timeout;

	add_timer(&step_datap->single_step_timer);
}

struct asd_step_data *
asd_hwi_alloc_step(struct asd_softc *asd)
{
	struct asd_step_data	*step_datap;

	step_datap = (struct asd_step_data *)asd_alloc_mem(
		sizeof(struct asd_step_data), GFP_ATOMIC);

	step_datap->asd = asd;
	step_datap->stepping = 1;

	init_timer(&step_datap->single_step_timer);
	step_datap->single_step_timer.expires = jiffies + 1;
	step_datap->single_step_timer.data = (u_long) step_datap;
	step_datap->single_step_timer.function = asd_hwi_ss_debug_timeout;
	step_datap->instruction_count = 0;

	return step_datap;
}

void
asd_hwi_free_step(struct asd_step_data	*step_datap)
{
	asd_free_mem(step_datap);
}

void
asd_hwi_start_step_timer(struct asd_step_data *step_datap)
{

	asd_print("starting single step\n");

	step_datap->stepping = 1;

	add_timer(&step_datap->single_step_timer);
}
#endif

/* 
 * Function:
 *	asd_hwi_post_scb()
 *
 * Description:
 *	Post the SCB to the central sequencer.	 
 */
void
asd_hwi_post_scb(struct asd_softc *asd, struct scb *scb)
{
	ASD_LOCK_ASSERT(asd);

	asd_swap_with_next_hscb(asd, scb);

	/*
	 * Keep a history of SCBs we've downloaded in the qinfifo.
	 */
	asd->qinfifo[ASD_QIN_WRAP(asd)] = SCB_GET_INDEX(scb);
	asd->qinfifonext++;

	if (scb->hscb->header.opcode != SCB_EMPTY_BUFFER) {
		list_add_tail(&scb->hwi_links, &asd->pending_scbs);
		scb->flags |= SCB_PENDING;
	}


	/* Tell the adapter about the newly queued SCB */
	asd_write_dword(asd, SCBPRO, asd->qinfifonext);				
}

/* 
 * Function:
 *	asd_hwi_free_edb()
 *
 * Description:
 *	Release an edb for eventual requeuing to the central sequencer.
 */
void
asd_hwi_free_edb(struct asd_softc *asd, struct scb *scb, int edb_index)
{
	struct asd_empty_hscb 	*escb;
	struct empty_buf_elem	*ebe;
	u_int 		 	 i;

	escb = &scb->hscb->empty_scb;
	ebe = &escb->buf_elem[edb_index];
	if (ELEM_BUFFER_VALID_FIELD(ebe) == ELEM_BUFFER_VALID) {
		ebe->elem_valid_ds = ELEM_BUFFER_INVALID;
		escb->num_valid_elems--;
		if (escb->num_valid_elems != 0)
			return;

		/*
		 * Now that all buffers have been used,
		 * we can recycle the empty scb by reinitializing
		 * it and requeuing it to the sequencer.
		 */
		escb->num_valid_elems = ASD_MAX_EDBS_PER_SCB;
		ebe = &escb->buf_elem[0];
		for (i = 0; i < ASD_MAX_EDBS_PER_SCB; i++, ebe++)
			ebe->elem_valid_ds = ELEM_BUFFER_VALID;

		asd_log(ASD_DBG_RUNTIME, 
			"Requeuing escb %d.\n", SCB_GET_INDEX(scb));
		
		asd_hwi_post_scb(asd, scb);
	}
}

#ifndef SEQUENCER_UPDATE
#define asd_ssp_smp_ddb		asd_ddb
#define asd_sata_stp_ddb	asd_ddb
#endif

void
asd_hwi_init_ddb_sites(struct asd_softc *asd)
{
	unsigned	i;
	unsigned	ddb_site;

	for (ddb_site = 0 ; ddb_site < asd->hw_profile.max_ddbs ; ddb_site++)
	{
		/* Setup the hardware DDB 0 location. */
		asd_hwi_set_ddbptr(asd, ddb_site);

		for (i = 0; i < sizeof(struct asd_ssp_smp_ddb); i += 4)
			asd_hwi_set_ddbsite_dword(asd, i, 0x0);
	}
}

/*
 * Function:
 *	asd_hwi_init_internal_ddb()
 * 
 * Description:
 *	Initialize DDB site 0 and 1 which are used internally by the sequencer.
 */
void
asd_hwi_init_internal_ddb(struct asd_softc *asd)
{
	int		i;
#ifdef SEQUENCER_UPDATE
	unsigned	num_phys;
#endif

	/* Setup the hardware DDB 0 location. */
	asd_hwi_set_ddbptr(asd, 0);

#ifdef SEQUENCER_UPDATE
	asd_hwi_set_ddbsite_word(asd,
		 offsetof(struct asd_int_ddb, q_free_ddb_head), 0xffff);

	asd_hwi_set_ddbsite_word(asd,
		 offsetof(struct asd_int_ddb, q_free_ddb_tail), 0xffff);

	asd_hwi_set_ddbsite_word(asd,
		 offsetof(struct asd_int_ddb, q_free_ddb_cnt), 0x0000);

	asd_hwi_set_ddbsite_word(asd,
		 offsetof(struct asd_int_ddb, q_used_ddb_head), 0xffff);

	asd_hwi_set_ddbsite_word(asd,
		 offsetof(struct asd_int_ddb, q_used_ddb_tail), 0xffff);
#else
	for (i = 0; i < 10; i = i+2) {
		asd_hwi_set_ddbsite_word(asd, 
					(offsetof(struct asd_int_ddb, res1)+i),
					 0x0);
	}
#endif

	asd_hwi_set_ddbsite_word(asd,
		 offsetof(struct asd_int_ddb, shared_mem_lock), 0x0000);

#ifdef SEQUENCER_UPDATE
	asd_hwi_set_ddbsite_word(asd,
		 offsetof(struct asd_int_ddb, smp_conn_tag), 0x0000);

	asd_hwi_set_ddbsite_word(asd,
		 offsetof(struct asd_int_ddb, est_nexus_buf_cnt),
		 	0x0000);


	for (i = 0, num_phys = 0 ; i < 8; i++) {
		if (((1<<i) & asd->hw_profile.enabled_phys) != 0) {
			num_phys++;
		}

	}

	asd_hwi_set_ddbsite_word(asd,
		 offsetof(struct asd_int_ddb, est_nexus_buf_thresh),
		 num_phys * 2);

	for (i = 0; i < 24; i = i+2) {
		asd_hwi_set_ddbsite_word(asd, 
					(offsetof(struct asd_int_ddb, res2)+i),
					 0x0);
	}
#else
	for (i = 0; i < 34; i = i+2) {
		asd_hwi_set_ddbsite_word(asd, 
					(offsetof(struct asd_int_ddb, res2)+i),
					 0x0);
	}
#endif

	asd_hwi_set_ddbsite_byte(asd,
				 offsetof(struct asd_int_ddb, conn_not_active),
				 0xFF);
	asd_hwi_set_ddbsite_byte(asd,
				 offsetof(struct asd_int_ddb, phy_is_up),
				 0x0);
	
	for (i = 0; i < 8; i = i+4) {
		asd_hwi_set_ddbsite_dword(asd,
					 (offsetof(struct asd_int_ddb, 
						   port_map_by_ports) + i),
					  0x0);
		asd_hwi_set_ddbsite_dword(asd,
					 (offsetof(struct asd_int_ddb, 
						   port_map_by_links) + i),
					  0x0);
	}

	/* Setup the hardware DDB 1 location. */
	asd_hwi_set_ddbptr(asd, 1);

	for (i = 0; i < sizeof(struct asd_ssp_smp_ddb); i += 4)
		asd_hwi_set_ddbsite_dword(asd, i, 0x0);
}

void
#ifdef SEQUENCER_UPDATE
asd_hwi_build_ssp_smp_ddb_site(struct asd_softc *asd, struct asd_target *target)
#else
asd_hwi_build_ddb_site(struct asd_softc *asd, struct asd_target *target)
#endif
{
	u_int	i;

	/* Setup the hardware DDB location. */	
	asd_hwi_set_ddbptr(asd, target->ddb_profile.conn_handle);

	asd_hwi_set_ddbsite_byte(asd,
		offsetof(struct asd_ssp_smp_ddb, addr_fr_port),
			 (INITIATOR_PORT_MODE | OPEN_ADDR_FRAME));
	
	asd_hwi_set_ddbsite_byte(asd,
		offsetof(struct asd_ssp_smp_ddb, conn_rate), 
			 target->ddb_profile.conn_rate);

	/* Could this field be set to 0xFFFF after first time initialization. */
	asd_hwi_set_ddbsite_word(asd,
		offsetof(struct asd_ssp_smp_ddb, init_conn_tag), 0xFFFF);

	for (i = 0; i < SAS_ADDR_LEN; i++) {
		asd_hwi_set_ddbsite_byte(asd, offsetof(struct asd_ssp_smp_ddb,
						       dest_sas_addr[i]),
					 target->ddb_profile.sas_addr[i]);
	}

	asd_hwi_set_ddbsite_word(asd,
		offsetof(struct asd_ssp_smp_ddb, send_q_head), 0xFFFF);

	asd_hwi_set_ddbsite_byte(asd,
		offsetof(struct asd_ssp_smp_ddb, sqsuspended), 0x0);
	/* 
	 * This needs to be changed once we support Port Multipier as
	 * Port Multipier has seperate DDB.
	 */
	asd_hwi_set_ddbsite_byte(asd,
		offsetof(struct asd_ssp_smp_ddb, ddb_type), TARGET_PORT_DDB);

	asd_hwi_set_ddbsite_word(asd,
		offsetof(struct asd_ssp_smp_ddb, res1), 0x0);

	asd_hwi_set_ddbsite_word(asd,
		offsetof(struct asd_ssp_smp_ddb, awt_default), 0x0);

	asd_hwi_set_ddbsite_byte(asd,
		offsetof(struct asd_ssp_smp_ddb, comp_features), 0x0);

	asd_hwi_set_ddbsite_byte(asd,
		offsetof(struct asd_ssp_smp_ddb, pathway_blk_cnt), 0x0);

	asd_hwi_set_ddbsite_word(asd,
		offsetof(struct asd_ssp_smp_ddb, arb_wait_time), 0x0);

	asd_hwi_set_ddbsite_dword(asd,
		offsetof(struct asd_ssp_smp_ddb, more_comp_features), 0x0);

	asd_hwi_set_ddbsite_byte(asd,
		offsetof(struct asd_ssp_smp_ddb, conn_mask),
			 target->src_port->conn_mask);

	asd_hwi_set_ddbsite_byte(asd,
		offsetof(struct asd_ssp_smp_ddb, open_affl),
				 target->ddb_profile.open_affl);
	
#ifndef SEQUENCER_UPDATE
	asd_hwi_set_ddbsite_word(asd,
		offsetof(struct asd_ssp_smp_ddb, res2), 0x0);
#else
	asd_hwi_set_ddbsite_byte(asd,
		offsetof(struct asd_ssp_smp_ddb, res2), 0x0);
#endif
	
#ifndef SEQUENCER_UPDATE
	asd_hwi_set_ddbsite_byte(asd,
		offsetof(struct asd_ssp_smp_ddb, stp_close), CLOSE_STP_NO_TX);
#endif
	
	asd_hwi_set_ddbsite_word(asd,
		offsetof(struct asd_ssp_smp_ddb, exec_q_tail), 0xFFFF);

	asd_hwi_set_ddbsite_word(asd,
		offsetof(struct asd_ssp_smp_ddb, send_q_tail), 0xFFFF);

	asd_hwi_set_ddbsite_word(asd,
		offsetof(struct asd_ssp_smp_ddb, sister_ddb),
			 target->ddb_profile.sister_ddb);

#ifdef SEQUENCER_UPDATE
	asd_hwi_set_ddbsite_byte(asd,
		offsetof(struct asd_ssp_smp_ddb, max_concurrent_connections),
			0x00);
#ifdef CONCURRENT_SUPPORT
	if(target->ddb_profile.open_affl & CONCURRENT_CONNECTION_SUPPORT)
	{	
		uint8_t linkwidth;
		uint8_t num_conn;
		linkwidth = target->src_port->conn_mask;
		num_conn = 0;
		while(linkwidth != 0)
		{
			if(linkwidth & 0x80) num_conn++;
			linkwidth <<=1;
		}
		if(num_conn > 4) num_conn = 4;
		asd_hwi_set_ddbsite_byte(asd,
			offsetof(struct asd_ssp_smp_ddb, max_concurrent_connections),
			num_conn);
	}
#endif
	asd_hwi_set_ddbsite_byte(asd,
		offsetof(struct asd_ssp_smp_ddb, concurrent_connections), 0x00);

	asd_hwi_set_ddbsite_byte(asd,
		offsetof(struct asd_ssp_smp_ddb, tunable_number_contexts),
			0x00);
#else
	asd_hwi_set_ddbsite_word(asd,
		offsetof(struct asd_ssp_smp_ddb, ata_cmd_scb_ptr), 0xFFFF);

	asd_hwi_set_ddbsite_dword(asd,
		offsetof(struct asd_ssp_smp_ddb, sata_tag_mask), 0x0);
#endif

	asd_hwi_set_ddbsite_word(asd,
		offsetof(struct asd_ssp_smp_ddb, active_task_cnt), 0x0);
#ifndef SEQUENCER_UPDATE
	asd_hwi_set_ddbsite_dword(asd,
		offsetof(struct asd_ssp_smp_ddb, sata_sactive), 0x0);

	asd_hwi_set_ddbsite_byte(asd,
		offsetof(struct asd_ssp_smp_ddb, no_of_sata_tags), 0x0);
	/*
	 * Need to update this field based on the info from Host register FIS
	 * after OOB.
	 */     
	asd_hwi_set_ddbsite_byte(asd,
		offsetof(struct asd_ssp_smp_ddb, sata_stat), 0x50);

	asd_hwi_set_ddbsite_byte(asd,
		offsetof(struct asd_ssp_smp_ddb, sata_ending_stat), 0x0);
#endif

	asd_hwi_set_ddbsite_byte(asd,
		offsetof(struct asd_ssp_smp_ddb, itnl_reason), 0x0);

#ifndef SEQUENCER_UPDATE
	asd_hwi_set_ddbsite_word(asd,
		offsetof(struct asd_ssp_smp_ddb, ncq_data_scb_ptr), 0xFFFF);
#endif
	/*
	 * We ensure that we don't program zero value as ITNL timeout value.
	 * As zero value is treated by the firmware as infinite value.
       	 */
	asd_hwi_set_ddbsite_word(asd, offsetof(struct asd_ssp_smp_ddb,
		itnl_const),
			((target->ddb_profile.itnl_const != 0) ?
			  target->ddb_profile.itnl_const : 1));

	asd_hwi_set_ddbsite_dword(asd,
		offsetof(struct asd_ssp_smp_ddb, itnl_timestamp), 0x0);
}

#ifdef SEQUENCER_UPDATE
void
asd_hwi_build_sata_stp_ddb_site(struct asd_softc *asd,
	struct asd_target *target)
{
	u_int	i;

	/* Setup the hardware DDB location. */	
	asd_hwi_set_ddbptr(asd, target->ddb_profile.conn_handle);

	asd_hwi_set_ddbsite_byte(asd,
		offsetof(struct asd_sata_stp_ddb, addr_fr_port),
			 (INITIATOR_PORT_MODE | OPEN_ADDR_FRAME));
	
	asd_hwi_set_ddbsite_byte(asd,
		offsetof(struct asd_sata_stp_ddb, conn_rate), 
			 target->ddb_profile.conn_rate);

	/* Could this field be set to 0xFFFF after first time initialization. */
	asd_hwi_set_ddbsite_word(asd,
		offsetof(struct asd_sata_stp_ddb, init_conn_tag), 0xFFFF);

	for (i = 0; i < SAS_ADDR_LEN; i++) {
		asd_hwi_set_ddbsite_byte(asd, offsetof(struct asd_sata_stp_ddb,
						       dest_sas_addr[i]),
					 target->ddb_profile.sas_addr[i]);
	}

	asd_hwi_set_ddbsite_word(asd,
		offsetof(struct asd_sata_stp_ddb, send_q_head), 0xFFFF);

	asd_hwi_set_ddbsite_byte(asd,
		offsetof(struct asd_sata_stp_ddb, sqsuspended), 0x0);
	/* 
	 * This needs to be changed once we support Port Multipier as
	 * Port Multipier has seperate DDB.
	 */
	asd_hwi_set_ddbsite_byte(asd,
		offsetof(struct asd_sata_stp_ddb, ddb_type), TARGET_PORT_DDB);

	asd_hwi_set_ddbsite_word(asd,
		offsetof(struct asd_sata_stp_ddb, res1), 0x0);

	asd_hwi_set_ddbsite_word(asd,
		offsetof(struct asd_sata_stp_ddb, awt_default), 0x0);

	asd_hwi_set_ddbsite_byte(asd,
		offsetof(struct asd_sata_stp_ddb, comp_features), 0x0);

	asd_hwi_set_ddbsite_byte(asd,
		offsetof(struct asd_sata_stp_ddb, pathway_blk_cnt), 0x0);

	asd_hwi_set_ddbsite_word(asd,
		offsetof(struct asd_sata_stp_ddb, arb_wait_time), 0x0);

	asd_hwi_set_ddbsite_dword(asd,
		offsetof(struct asd_sata_stp_ddb, more_comp_features), 0x0);

	asd_hwi_set_ddbsite_byte(asd,
		offsetof(struct asd_sata_stp_ddb, conn_mask),
			 target->src_port->conn_mask);

	asd_hwi_set_ddbsite_byte(asd,
		offsetof(struct asd_sata_stp_ddb, open_affl),
			 target->ddb_profile.open_affl);
	
	asd_hwi_set_ddbsite_word(asd,
		offsetof(struct asd_sata_stp_ddb, res2), 0x0);
	
	asd_hwi_set_ddbsite_byte(asd,
		offsetof(struct asd_sata_stp_ddb, stp_close), CLOSE_STP_NO_TX);
	
	asd_hwi_set_ddbsite_word(asd,
		offsetof(struct asd_sata_stp_ddb, exec_q_tail), 0xFFFF);

	asd_hwi_set_ddbsite_word(asd,
		offsetof(struct asd_sata_stp_ddb, send_q_tail), 0xFFFF);

	asd_hwi_set_ddbsite_word(asd,
		offsetof(struct asd_sata_stp_ddb, sister_ddb),
			 target->ddb_profile.sister_ddb);

	asd_hwi_set_ddbsite_word(asd,
		offsetof(struct asd_sata_stp_ddb, ata_cmd_scb_ptr), 0xFFFF);

	asd_hwi_set_ddbsite_dword(asd,
		offsetof(struct asd_sata_stp_ddb, sata_tag_mask), 0x0);

	asd_hwi_set_ddbsite_word(asd,
		offsetof(struct asd_sata_stp_ddb, active_task_cnt), 0x0);

	asd_hwi_set_ddbsite_dword(asd,
		offsetof(struct asd_sata_stp_ddb, sata_sactive), 0x0);

	asd_hwi_set_ddbsite_byte(asd,
		offsetof(struct asd_sata_stp_ddb, no_of_sata_tags), 0x0);

	/*
	 * Need to update this field based on the info from Host register FIS
	 * after OOB.
	 */     
	asd_hwi_set_ddbsite_byte(asd,
		offsetof(struct asd_sata_stp_ddb, sata_stat), 0x50);

	asd_hwi_set_ddbsite_byte(asd,
		offsetof(struct asd_sata_stp_ddb, sata_ending_stat), 0x0);

	asd_hwi_set_ddbsite_byte(asd,
		offsetof(struct asd_sata_stp_ddb, itnl_reason), 0x0);

	asd_hwi_set_ddbsite_word(asd,
		offsetof(struct asd_sata_stp_ddb, ncq_data_scb_ptr), 0xFFFF);
	/*
	 * We ensure that we don't program zero value as ITNL timeout value.
	 * As zero value is treated by the firmware as infinite value.
       	 */
	asd_hwi_set_ddbsite_word(asd, offsetof(struct asd_sata_stp_ddb,
		itnl_const),
			((target->ddb_profile.itnl_const != 0) ?
			  target->ddb_profile.itnl_const : 1));

	asd_hwi_set_ddbsite_dword(asd,
		offsetof(struct asd_sata_stp_ddb, itnl_timestamp), 0x0);
}

/*
 * Function:
 *	asd_hwi_build_ddb_site()
 *
 * Description:
 * 	Initialiaze and setup the hardware DDB site based on target
 *	DDB profile.
 */	 	  
void
asd_hwi_build_ddb_site(struct asd_softc *asd, struct asd_target *target)
{
	if ((target->transport_type == ASD_TRANSPORT_SSP) ||
		(target->transport_type == ASD_TRANSPORT_SMP)) {

		asd_hwi_build_ssp_smp_ddb_site(asd, target);

		return;
	}

	asd_hwi_build_sata_stp_ddb_site(asd, target);
}
#endif


void
asd_hwi_update_sata(struct asd_softc *asd, struct asd_target *target)
{
	/* Setup the hardware DDB location. */	
	asd_hwi_set_ddbptr(asd, target->ddb_profile.conn_handle);

	asd_hwi_set_ddbsite_byte(asd,
		offsetof(struct asd_sata_stp_ddb, sata_stat),
			target->ddb_profile.sata_status);
}

void
#ifdef SEQUENCER_UPDATE
asd_hwi_update_ssp_smp_conn_mask(struct asd_softc *asd,
	struct asd_target *target)
#else
asd_hwi_update_conn_mask(struct asd_softc *asd, struct asd_target *target)
#endif
{
	/* Setup the hardware DDB location. */	
	asd_hwi_set_ddbptr(asd, target->ddb_profile.conn_handle);

	asd_hwi_set_ddbsite_byte(asd,
		offsetof(struct asd_ssp_smp_ddb, conn_mask),
			 target->src_port->conn_mask);
}

#ifdef SEQUENCER_UPDATE
void
asd_hwi_update_sata_stp_conn_mask(struct asd_softc *asd,
	struct asd_target *target)
{
	/* Setup the hardware DDB location. */	
	asd_hwi_set_ddbptr(asd, target->ddb_profile.conn_handle);

	asd_hwi_set_ddbsite_byte(asd,
		offsetof(struct asd_sata_stp_ddb, conn_mask),
			 target->src_port->conn_mask);
}
#endif


/********************* REGISTERS DUMP STATE routines **************************/

#ifdef ASD_DEBUG

void
asd_hwi_dump_seq_state(struct asd_softc *asd, uint8_t lseq_mask)
{
	uint8_t	lseqs_to_dump;
	u_int	lseq_id;

	/* Dump out CSEQ Registers state. */
	asd_hwi_dump_cseq_state(asd);

	if (lseq_mask == 0x0)
		return;

	lseq_id = 0;
	lseqs_to_dump = lseq_mask;

	while (lseqs_to_dump != 0) { 
		for ( ; lseq_id < asd->hw_profile.max_phys; lseq_id++) {
			if (lseqs_to_dump & (1 << lseq_id)) {
				lseqs_to_dump &= ~(1 << lseq_id);
				break;
			} 
		}
		/* Dump out specific LSEQ Registers state. */
		asd_hwi_dump_lseq_state(asd, lseq_id);
	}
#if KDB_ENABLE
	KDB_ENTER();
#endif
}
	
static void
asd_hwi_dump_cseq_state(struct asd_softc *asd)
{
	unsigned	i;

	asd_print("\nCSEQ DUMP STATE\n");
	asd_print("===============\n");

	asd_print("\nIOP REGISTERS\n");
	asd_print("*************\n");

	asd_print("   %20s[0x%x]:0x%08x\n", "ARP2CTL", ARP2CTL,
		  asd_hwi_swb_read_dword(asd, CARP2CTL));

	asd_print("   %20s[0x%x]:0x%08x\n", "ARP2INT", ARP2INT,
		  asd_hwi_swb_read_dword(asd, CARP2INT));

	asd_print("   %20s[0x%x]:0x%08x\n", "ARP2INTEN", ARP2INTEN,
		  asd_hwi_swb_read_dword(asd, CARP2INTEN));

	asd_print("\nSCRATCH MEMORY\n");
	asd_print("**************\n");

	asd_print("\nPage 2 - Mode 8\n");
	asd_print("---------------\n");

	asd_print("   %20s[0x%x]:0x%08x", "Q_NEW_POINTER", 0x240,
		  asd_hwi_swb_read_dword(asd, CSEQ_Q_NEW_POINTER+4));
	asd_print("%08x\n", asd_hwi_swb_read_dword(asd, CSEQ_Q_NEW_POINTER));

	asd_print("   %20s[0x%x]:0x%08x", "Q_DONE_BASE", 0x248,
		  asd_hwi_swb_read_dword(asd, CSEQ_Q_DONE_BASE+4));
	asd_print("%08x\n", asd_hwi_swb_read_dword(asd, CSEQ_Q_DONE_BASE));

	asd_print("   %20s[0x%x]:0x%08x\n", "Q_DONE_POINTER", 0x250,
		  asd_hwi_swb_read_dword(asd, CSEQ_Q_DONE_POINTER));

	asd_print("   %20s[0x%x]:0x%08x\n", "Q_DONE_PASS", 0x254,
		  asd_hwi_swb_read_dword(asd, CSEQ_Q_DONE_PASS));

	asd_print("\nMode Independent Page 4\n");
	asd_print("-----------------------\n");

	asd_print("   %20s[0x%x]:0x%04x\n", "Q_EXE_HEAD", 0x280,
		  asd_hwi_swb_read_word(asd, CSEQ_Q_EXE_HEAD));

	asd_print("   %20s[0x%x]:0x%04x\n", "Q_EXE_TAIL", 0x282,
		  asd_hwi_swb_read_word(asd, CSEQ_Q_EXE_TAIL));

#ifndef SEQUENCER_UPDATE
	asd_print("   %20s[0x%x]:0x%04x\n", "Q_DONE_HEAD", 0x284,
		  asd_hwi_swb_read_word(asd, CSEQ_Q_DONE_HEAD));

	asd_print("   %20s[0x%x]:0x%04x\n", "Q_DONE_TAIL", 0x286,
		  asd_hwi_swb_read_word(asd, CSEQ_Q_DONE_TAIL));
#endif

	asd_print("   %20s[0x%x]:0x%04x\n", "Q_SEND_HEAD", 0x288,
		  asd_hwi_swb_read_word(asd, CSEQ_Q_SEND_HEAD));

	asd_print("   %20s[0x%x]:0x%04x\n", "Q_SEND_TAIL", 0x28A,
		  asd_hwi_swb_read_word(asd, CSEQ_Q_SEND_TAIL));

	asd_print("   %20s[0x%x]:0x%04x\n", "Q_DMA2CHIM_HEAD", 0x28C,
		  asd_hwi_swb_read_word(asd, CSEQ_Q_DMA2CHIM_HEAD));

	asd_print("   %20s[0x%x]:0x%04x\n", "Q_DMA2CHIM_TAIL", 0x28E,
		  asd_hwi_swb_read_word(asd, CSEQ_Q_DMA2CHIM_TAIL));

	asd_print("   %20s[0x%x]:0x%04x\n", "Q_COPY_HEAD", 0x290,
		  asd_hwi_swb_read_word(asd, CSEQ_Q_COPY_HEAD));

	asd_print("   %20s[0x%x]:0x%04x\n", "Q_COPY_TAIL", 0x292,
		  asd_hwi_swb_read_word(asd, CSEQ_Q_COPY_TAIL));

	asd_print("   %20s[0x%x]:0x%02x\n", "LINK_CTL_Q_MAP", 0x29C,
		  asd_hwi_swb_read_byte(asd, CSEQ_LINK_CTL_Q_MAP));

	asd_print("   %20s[0x%x]:0x%02x\n", "SCRATCH_FLAGS", 0x29F,
		  asd_hwi_swb_read_byte(asd, CSEQ_SCRATCH_FLAGS));

	asd_print("\nMode Independent Page 5\n");
	asd_print("-----------------------\n");

#ifndef SEQUENCER_UPDATE
	asd_print("   %20s[0x%x]:0x%02x\n", "FREE_SCB_MASK", 0x2B5,
		  asd_hwi_swb_read_byte(asd, CSEQ_FREE_SCB_MASK));

	asd_print("   %24s[0x%x]:0x%04x\n", "BUILTIN_FREE_SCB_HEAD", 0x2D6,
		  asd_hwi_swb_read_word(asd, CSEQ_BUILTIN_FREE_SCB_HEAD));

	asd_print("   %24s[0x%x]:0x%04x\n", "BUILTIN_FREE_SCB_TAIL", 0x2B8,
		  asd_hwi_swb_read_word(asd, CSEQ_BUILTIN_FREE_SCB_TAIL));

#else

	asd_print("\nMode Independent Page 6\n");
	asd_print("-----------------------\n");

	asd_print("   %20s[0x%x]:0x%02x\n", "FREE_SCB_MASK", 0x2D5,
		  asd_hwi_swb_read_byte(asd, CSEQ_FREE_SCB_MASK));

	asd_print("   %24s[0x%x]:0x%04x\n", "BUILTIN_FREE_SCB_HEAD", 0x2D6,
		  asd_hwi_swb_read_word(asd, CSEQ_BUILTIN_FREE_SCB_HEAD));

	asd_print("   %24s[0x%x]:0x%04x\n", "BUILTIN_FREE_SCB_TAIL", 0x2D8,
		  asd_hwi_swb_read_word(asd, CSEQ_BUILTIN_FREE_SCB_TAIL));
#ifdef EXTENDED_SCB
	asd_print("   %24s[0x%x]:0x%04x\n", "EXTENDED_FREE_SCB_HEAD", 0x2DA,
		  asd_hwi_swb_read_word(asd, CSEQ_EXTNDED_FREE_SCB_HEAD));

	asd_print("   %24s[0x%x]:0x%04x\n", "EXTENDED_FREE_SCB_TAIL", 0x2DC,
		  asd_hwi_swb_read_word(asd, CSEQ_EXTNDED_FREE_SCB_TAIL));
#endif
#endif

	asd_print("\nMode Independent Page 7\n");
	asd_print("-----------------------\n");

	asd_print("   %20s[0x%x]:0x%04x\n", "Q_EMPTY_HEAD", 0x2F0,
		  asd_hwi_swb_read_word(asd, CSEQ_Q_EMPTY_HEAD));

	asd_print("   %20s[0x%x]:0x%04x\n", "Q_EMPTY_TAIL", 0x2F2,
		  asd_hwi_swb_read_word(asd, CSEQ_Q_EMPTY_TAIL));

	asd_print("   %20s[0x%x]:0x%04x\n", "NEED_EMPTY_SCB", 0x2F4,
		  asd_hwi_swb_read_word(asd, CSEQ_NEED_EMPTY_SCB));

	asd_print("   %20s[0x%x]:0x%02x\n", "EMPTY_REQ_HEAD", 0x2F6,
		  asd_hwi_swb_read_byte(asd, CSEQ_EMPTY_REQ_HEAD));

	asd_print("   %20s[0x%x]:0x%02x\n", "EMPTY_REQ_TAIL", 0x2F7,
		  asd_hwi_swb_read_byte(asd, CSEQ_EMPTY_REQ_TAIL));

	asd_print("   %20s[0x%x]:0x%02x\n", "EMPTY_SCB_OFFSET", 0x2F8,
		  asd_hwi_swb_read_byte(asd, CSEQ_EMPTY_SCB_OFFSET));

	asd_print("   %20s[0x%x]:0x%04x\n", "PRIMITIVE_DATA", 0x2FA,
		  asd_hwi_swb_read_word(asd, CSEQ_PRIMITIVE_DATA));

	asd_print("   %20s[0x%x]:0x%08x\n", "TIMEOUT_CONST", 0x2FC,
		  asd_hwi_swb_read_dword(asd, CSEQ_TIMEOUT_CONSTANT));

	asd_print("\nPage 0 - Mode 8\n");
	asd_print("---------------\n");

	asd_print("   %20s[0x%x]:0x%04x\n", "RET_ADDR", 0x200,
		  asd_hwi_swb_read_word(asd, CSEQ_RET_ADDR));

	asd_print("   %20s[0x%x]:0x%04x\n", "RET_SCBPTR", 0x202,
		  asd_hwi_swb_read_word(asd, CSEQ_RET_SCBPTR));

	asd_print("   %20s[0x%x]:0x%04x\n", "SAVE_SCBPTR", 0x204,
		  asd_hwi_swb_read_word(asd, CSEQ_SAVE_SCBPTR));

	asd_print("   %20s[0x%x]:0x%04x\n", "EMPTY_TC", 0x206,
		  asd_hwi_swb_read_word(asd, CSEQ_EMPTY_TRANS_CTX));

	asd_print("   %20s[0x%x]:0x%04x\n", "RESP_LEN", 0x208,
		  asd_hwi_swb_read_word(asd, CSEQ_RESP_LEN));

	asd_print("   %20s[0x%x]:0x%04x\n", "TMF_SCBPTR", 0x20A,
		  asd_hwi_swb_read_word(asd, CSEQ_TMF_SCBPTR));

	asd_print("   %20s[0x%x]:0x%04x\n", "GLOBAL_PREV_SCB", 0x20C,
		  asd_hwi_swb_read_word(asd, CSEQ_GLOBAL_PREV_SCB));

#ifdef SEQUENCER_UPDATE
	asd_print("   %20s[0x%x]:0x%04x\n", "CLEAR_LU_HEAD", 0x0210,
		  asd_hwi_swb_read_word(asd, CSEQ_CLEAR_LU_HEAD));

	asd_print("   %20s[0x%x]:0x%02x\n", "TMF_OPCODE", 0x0212,
		  asd_hwi_swb_read_byte(asd, CSEQ_TMF_OPCODE));

	asd_print("   %20s[0x%x]:0x%04x\n", "SCRATCH_FLAGS", 0x0213,
		  asd_hwi_swb_read_word(asd, CSEQ_SCRATCH_FLAGS));

	asd_print("   %20s[0x%x]:0x%04x\n", "HSS_SITE", 0x021A,
		  asd_hwi_swb_read_word(asd, CSEQ_HSS_SITE));
#else
	asd_print("   %20s[0x%x]:0x%04x\n", "GLOBAL_HEAD", 0x20E,
		  asd_hwi_swb_read_word(asd, CSEQ_GLOBAL_HEAD));

	asd_print("   %20s[0x%x]:0x%02x\n", "TMF_OPCODE", 0x210,
		  asd_hwi_swb_read_byte(asd, CSEQ_TMF_OPCODE));

	asd_print("   %20s[0x%x]:0x%04x\n", "CLEAR_LU_HEAD", 0x212,
		  asd_hwi_swb_read_word(asd, CSEQ_CLEAR_LU_HEAD));
#endif
	asd_print("   %20s[0x%x]:0x%04x\n", "FIRST_INV_SCB_SITE", 0x21C,
		  asd_hwi_swb_read_word(asd, CSEQ_FIRST_INV_SCB_SITE));

	asd_print("   %20s[0x%x]:0x%04x\n", "FIRST_INV_DDB_SITE", 0x21E,
		  asd_hwi_swb_read_word(asd, CSEQ_FIRST_INV_DDB_SITE));

	asd_print("\nCIO REGISTERS\n");
	asd_print("*************\n");

	asd_print("   %20s[0x%x]:0x%02x\n", "ARP2_MODEPTR", MODEPTR,
		  asd_hwi_swb_read_byte(asd, CMODEPTR));

	asd_print("   %20s[0x%x]:0x%02x\n", "ARP2_ALTMODE", ALTMODE,
		  asd_hwi_swb_read_byte(asd, CALTMODE));

	asd_print("   %20s[0x%x]:0x%02x\n", "ARP2_FLAG", FLAG,
		  asd_hwi_swb_read_byte(asd, CFLAG));

	asd_print("   %20s[0x%x]:0x%02x\n", "ARP2_INTCTL", ARP2INTCTL,
		  asd_hwi_swb_read_byte(asd, CARP2INTCTL));

	asd_print("   %20s[0x%x]:0x%04x\n", "ARP2_PRGMCNT", PRGMCNT,
		  asd_hwi_swb_read_word(asd, CPRGMCNT));

	asd_print("   %20s[0x%x]:0x%02x\n", "ARP2_HALTCODE", ARP2HALTCODE,
		  asd_hwi_swb_read_byte(asd, CARP2HALTCODE));

	asd_print("   %20s[0x%x]:0x%04x\n", "ARP2_CURRADDR", CURRADDR,
		  asd_hwi_swb_read_word(asd, CCURRADDR));

	asd_print("   %20s[0x%x]:0x%04x\n", "ARP2_LASTADDR", LASTADDR,
		  asd_hwi_swb_read_word(asd, CLASTADDR));

	asd_print("   %20s[0x%x]:0x%04x\n", "ARP2_NXTLADDR", NXTLADDR,
		  asd_hwi_swb_read_word(asd, CNXTLADDR));

	asd_print("   %20s[0x%x]:0x%08x\n", "CLINKCON", 0x28,
		  asd_hwi_swb_read_dword(asd, CLINKCON));
		  	
	asd_print("   %20s[0x%x]:0x%02x\n", "CCONMSK", 0x60,
		  asd_hwi_swb_read_byte(asd, CCONMSK));
	
	asd_print("   %20s[0x%x]:0x%02x\n", "CCONEXIST", 0x61,
		  asd_hwi_swb_read_byte(asd, CCONEXIST));
	
	asd_print("   %20s[0x%x]:0x%04x\n", "CCONMODE", 0x62,
		  asd_hwi_swb_read_word(asd, CCONMODE));

	for (i = 0 ; i < 16 ; i++) {
		asd_print("   Mode%d:%20s[0x%x]:0x%08x\n", i,
			"MnSCBPTR", MnSCBPTR,
			asd_hwi_swb_read_word(asd, CSEQm_CIO_REG(i, MnSCBPTR)));
		asd_print("   Mode%d:%20s[0x%x]:0x%08x\n", i,
			"MnDDBPTR", MnDDBPTR,
			asd_hwi_swb_read_word(asd, CSEQm_CIO_REG(i, MnDDBPTR)));
	}

}

//
// Support function to dump a scb site from the hardware
//
void DumpScbSite(struct asd_softc *asd, u_int lseq_id, uint16_t scb)
{
	uint32_t	lseqBaseAddr; 
	uint8_t		regValue8; 
	uint16_t	originalScb;
	uint32_t	index;
	uint8_t		scbSite[128];
	struct scb	*scbptr;
	struct sg_element 	 *sg;


//JD
   /* Go to mode 3 register space  */

	if(scb==0xffff) return;

	lseqBaseAddr = LmSEQ_PHY_BASE(3, lseq_id);

   /* Save the original SCB */
	originalScb = asd_hwi_swb_read_word(asd, (lseqBaseAddr + 0x20));

	asd_hwi_swb_write_word(asd, (lseqBaseAddr + 0x20), scb);
	asd_print("\nSCB: 0x%04x", scb );

	for( index = 0; index < 128; index++)
	{
		if(!(index % 16))
		{
			asd_print("   \n");
		}

		regValue8 = asd_hwi_swb_read_byte(asd,(lseqBaseAddr + 0x100 + index));

		scbSite[index] = regValue8;

		asd_print("%02x ", regValue8 );
	}

	asd_print("   \nSCB Fields:\n");
	asd_print("      scb_index : 0x%02x%02x\n", scbSite[9], scbSite[8]);
	asd_print("      ssp_tag : 0x%02x%02x\n", scbSite[32], scbSite[33]);
	asd_print("      scb_flags2 : 0x%02x\n", scbSite[72]);
	asd_print("      scb_flags  : 0x%02x\n", scbSite[73]);
	asd_print("      retry_count : 0x%02x\n", scbSite[74]);
	asd_print("      active_path_cnt : 0x%02x\n", scbSite[75]);
	asd_print("      scb_flags3 : 0x%02x\n", scbSite[76]);
	asd_print("      scb_tgtcfrcnt : 0x%02x%02x%02x%02x\n", 
                                  scbSite[67],
                                  scbSite[66],
                                  scbSite[65],
                                  scbSite[64]);

	asd_print("      ata_int_vec : 0x%02x%02x\n", scbSite[57],scbSite[56]);
	asd_print("      ata_state : 0x%02x%02x\n", scbSite[59],scbSite[58]);
	asd_print("      ata_hdr_mask : 0x%02x%02x\n", scbSite[79],scbSite[78]);
   /* restore the original SCB */
	asd_hwi_swb_write_word(asd, (lseqBaseAddr + 0x20), originalScb);

	asd_print("\n");
	originalScb=*((uint16_t *)&scbSite[8]);
	if(originalScb == 0xffff) return;
	scbptr=asd->scbindex[originalScb];
	if(scbptr !=NULL)
	{
		asd_print("   \nStruct SCB 0x%x:\n",originalScb);
		asd_print("      sg_count : 0x%x\n", scbptr->sg_count);
		asd_print("      buf_busaddr : 0x%Lx\n", scbptr->platform_data->buf_busaddr);


		for (index=0, sg = scbptr->sg_list; index<scbptr->sg_count ; index++, sg++) {
			asd_print("      sg# : 0x%x (%p)\n", index, sg);
			asd_print("       address : 0x%Lx\n", sg->bus_address);
			asd_print("       length  : 0x%x\n", sg->length);
			asd_print("       flags   : 0x%x\n", sg->flags);
			asd_print("       next_sg_offset  : 0x%x\n\n", sg->next_sg_offset);
		}
		for (index=0; index<3 ; index++) {
			struct asd_ssp_task_hscb *ssp_hscb;
			ssp_hscb=&scbptr->hscb->ssp_task;
			sg = &ssp_hscb->sg_elements[index];
			asd_print("      HSCB->sg# : 0x%x\n", index);
			asd_print("       address : 0x%Lx\n", sg->bus_address);
			asd_print("       length  : 0x%x\n", sg->length);
			asd_print("       flags   : 0x%x\n", sg->flags);
			asd_print("       next_sg_offset  : 0x%x\n\n", sg->next_sg_offset);
		}
	}

   return;
}

static void
asd_hwi_dump_lseq_state(struct asd_softc *asd, u_int lseq_id)
{
	uint32_t lseq_cio_addr;
	uint32_t mode_offset;
	uint16_t saved_reg16;
	uint16_t sm_idx;
	int	 idx;
	int	 mode;

	asd_print("\nLSEQ %d DUMP STATE\n", lseq_id);
	asd_print("=================\n");

	asd_print("\nIOP REGISTERS\n");
	asd_print("*************\n");

	asd_print("   %20s[0x%x]:0x%08x\n", "ARP2CTL", ARP2CTL,
		  asd_hwi_swb_read_dword(asd, LmARP2CTL(lseq_id)));

	asd_print("   %20s[0x%x]:0x%08x\n", "ARP2INT", ARP2INT,
		  asd_hwi_swb_read_dword(asd, LmARP2INT(lseq_id)));

	asd_print("   %20s[0x%x]:0x%08x\n", "ARP2INTEN", ARP2INTEN,
		  asd_hwi_swb_read_dword(asd, LmARP2INTEN(lseq_id)));

	asd_print("   %20s[0x%x]:0x%08x\n", "ARP2BREAKADR01", ARP2BREAKADR01,
		asd_hwi_swb_read_dword(asd, LmARP2BREAKADR01(lseq_id)));

	asd_print("   %20s[0x%x]:0x%08x\n", "ARP2BREAKADR23", ARP2BREAKADR23,
		asd_hwi_swb_read_dword(asd, LmARP2BREAKADR23(lseq_id)));

	asd_print("\nSCRATCH MEMORY\n");
	asd_print("**************\n");

	asd_print("\nMode Independent\n");
	asd_print("----------------\n");

	asd_print("   %20s[0x%x]:0x%02x\n", "SCRATCH_FLAGS", 0x187,
		  asd_hwi_swb_read_byte(asd, LmSEQ_SCRATCH_FLAGS(lseq_id)));

	asd_print("   %20s[0x%x]:0x%08x\n", "CONNECTION_STATE", 0x188,
		  asd_hwi_swb_read_dword(asd, LmSEQ_CONNECTION_STATE(lseq_id)));

	asd_print("   %20s[0x%x]:0x%04x\n", "CONCTL", 0x18C,
		  asd_hwi_swb_read_word(asd, LmSEQ_CONCTL(lseq_id)));

	asd_print("   %20s[0x%x]:0x%02x\n", "CONSTAT", 0x18E,
		  asd_hwi_swb_read_byte(asd, LmSEQ_CONSTAT(lseq_id)));

	for (mode = 0; mode < 3; mode++) {
		asd_print("\nCommon Page 0 - Mode %d\n", mode);
		asd_print("-----------------------\n");

		/* Adjust the mode page. */
		mode_offset = mode * LSEQ_MODE_SCRATCH_SIZE;
		
		asd_print("   %20s[0x%x]:0x%04x\n", "RET_ADDR", 0x0,
			  asd_hwi_swb_read_word(asd, (LmSEQ_RET_ADDR(lseq_id) +
						mode_offset)));

		asd_print("   %20s[0x%x]:0x%04x\n", "RET_ADDR_SAVE", 0x6,
			  asd_hwi_swb_read_word(asd,
				  	       (LmSEQ_RET_ADDR_SAVE(lseq_id) +
						mode_offset)));

		asd_print("   %20s[0x%x]:0x%04x\n", "RET_ADDR_SAVE2", 0x8,
			  asd_hwi_swb_read_word(asd,
				  	       (LmSEQ_RET_ADDR_SAVE2(lseq_id) +
						mode_offset)));

		asd_print("   %20s[0x%x]:0x%04x\n", "MODE_FLAGS", 0x4,
			  asd_hwi_swb_read_word(asd,
				  	       (LmSEQ_MODE_FLAGS(lseq_id) +
						mode_offset)));

		asd_print("   %20s[0x%x]:0x%04x\n", "REG0_MODE", 0x2,
			  asd_hwi_swb_read_word(asd,
				  	       (LmSEQ_REG0_MODE(lseq_id) +
						mode_offset)));

		asd_print("   %20s[0x%x]:0x%04x\n", "DATA_TO_CSEQ", 0xC,
			  asd_hwi_swb_read_word(asd,
				  	       (LmSEQ_DATA_TO_CSEQ(lseq_id) +
						mode_offset)));

		asd_print("   %20s[0x%x]:0x%02x\n", "OPCODE_TO_CSEQ", 0xB,
			  asd_hwi_swb_read_byte(asd,
				  	       (LmSEQ_OPCODE_TO_CSEQ(lseq_id) +
						mode_offset)));
	}

	asd_print("\nPage 0 - Mode 5\n");
	asd_print("----------------\n");

	mode_offset = LSEQ_MODE5_PAGE0_OFFSET;

	asd_print("   %20s[0x%x]:0x%04x\n", "RET_ADDR", 0x0,
		  asd_hwi_swb_read_word(asd, (LmSEQ_RET_ADDR(lseq_id) +
				  	mode_offset)));

	asd_print("   %20s[0x%x]:0x%04x\n", "RET_ADDR_SAVE", 0x6,
		  asd_hwi_swb_read_word(asd, (LmSEQ_RET_ADDR_SAVE(lseq_id) +
					mode_offset)));

	asd_print("   %20s[0x%x]:0x%04x\n", "RET_ADDR_SAVE2", 0x8,
		  asd_hwi_swb_read_word(asd, (LmSEQ_RET_ADDR_SAVE2(lseq_id) +
					mode_offset)));

	asd_print("   %20s[0x%x]:0x%04x\n", "MODE_FLAGS", 0x4,
		  asd_hwi_swb_read_word(asd, (LmSEQ_MODE_FLAGS(lseq_id) +
					mode_offset)));

	asd_print("   %20s[0x%x]:0x%04x\n", "REG0_MODE", 0x2,
		  asd_hwi_swb_read_word(asd, (LmSEQ_REG0_MODE(lseq_id) +
					mode_offset)));

	asd_print("   %20s[0x%x]:0x%04x\n", "DATA_TO_CSEQ", 0xC,
		  asd_hwi_swb_read_word(asd, (LmSEQ_DATA_TO_CSEQ(lseq_id) +
					mode_offset)));

	asd_print("   %20s[0x%x]:0x%02x\n", "OPCODE_TO_CSEQ", 0xB,
		  asd_hwi_swb_read_byte(asd, (LmSEQ_OPCODE_TO_CSEQ(lseq_id) +
					mode_offset)));

	asd_print("   %20s[0x%x]:0x%02x\n", "SAVED_OOB_STATUS", 0x6E,
		  asd_hwi_swb_read_byte(asd, LmSEQ_SAVED_OOB_STATUS(lseq_id)));

	asd_print("   %20s[0x%x]:0x%02x\n", "SAVED_OOB_MODE", 0x6F,
		  asd_hwi_swb_read_byte(asd, LmSEQ_SAVED_OOB_MODE(lseq_id)));

	asd_print("   %20s[0x%x]:0x%02x\n", "SAVED_OOB_SIGNALS", 0x73,
		  asd_hwi_swb_read_byte(asd, LmSEQ_SAVED_OOB_SIGNALS(lseq_id)));

	asd_print("\nPage 1 - Mode 2\n");
	asd_print("----------------\n");

	asd_print("   %20s[0x%x]:0x%08x\n", "INVALID_DWORD_COUNT", 0x120,
		  asd_hwi_swb_read_dword(asd,LmSEQ_INVALID_DWORD_CNT(lseq_id)));

	asd_print("   %20s[0x%x]:0x%08x\n", "DISPARITY_ERROR_COUNT", 0x124,
		  asd_hwi_swb_read_dword(asd,
			  		 LmSEQ_DISPARITY_ERROR_CNT(lseq_id)));

	asd_print("   %20s[0x%x]:0x%08x\n", "LOSS_OF_SYNC_COUNT", 0x128,
		  asd_hwi_swb_read_dword(asd,
			  		 LmSEQ_LOSS_OF_SYNC_CNT(lseq_id)));

	asd_print("\nCIO REGISTERS\n");
	asd_print("*************\n");

	for (mode = 0; mode < 2; mode++) {
		uint16_t scb;
		scb=0xffff;
		asd_print("\nMode %d\n", mode);
		asd_print("------\n");

		idx = 0;
		lseq_cio_addr = LmSEQ_PHY_BASE(mode, lseq_id);
		
		while (LSEQmCIOREGS[idx].width != 0) {
			/* 
			 * Check if we are in the right mode to dump the
			 * contents of the registers.
			 */
			if ((LSEQmCIOREGS[idx].mode & (1 << mode)) != 0) {
				switch(LSEQmCIOREGS[idx].width) {
               			case 8:
					asd_print("%20s[0x%x]: 0x%02x\n",
						  LSEQmCIOREGS[idx].name,
						  LSEQmCIOREGS[idx].offset,
						 (asd_hwi_swb_read_byte(asd,
						 (lseq_cio_addr +
						  LSEQmCIOREGS[idx].offset))));
//JD
					if(LSEQmCIOREGS[idx].offset == 0x65) //"LmMnSGBADR"
					{
						uint8_t savedRegValue8;
						uint8_t regValue8;
						int nIndex;

						savedRegValue8 = asd_hwi_swb_read_byte(asd,(lseq_cio_addr + 0x65));

         /* Set up to display SG Buffer */
						regValue8 = 0;
						asd_hwi_swb_write_byte(asd, (lseq_cio_addr + 0x65), regValue8);
						asd_print("\n  SG Buffer:");
						for (nIndex = 0; nIndex < 128; nIndex++)
						{
            /* Display the buffer in SG element format: 16 bytes a line */
							if ((nIndex % 16) == 0)
							{
								asd_print("\n%18s[%04d-%04d]: 0x", "bytes", nIndex, nIndex+15 );
							}
            /* We need to read a byte at a time from the SG Buffer */

            /* register and displays it. */
							regValue8 = asd_hwi_swb_read_byte(asd,(lseq_cio_addr + 0x53));//LmMnSGBUF
							asd_print("%02x", regValue8);
						}
         /* Restore SG Buffer value */
						asd_hwi_swb_write_byte(asd, (lseq_cio_addr + 0x65), savedRegValue8);
						asd_print("\n\n");
					}

					if(LSEQmCIOREGS[idx].offset == 0x61) //"LmMnPIPECTL"
					{
						uint8_t regValue8;
						int i;

						regValue8 = asd_hwi_swb_read_byte(asd,(lseq_cio_addr + 0x61));
 
      /* Set up to display ACTIVE Device element */
//	LPIPECTL	equ 07h	;	Pipeline control mode:
						regValue8 = (regValue8 & 0xf8);
//	ACTDNONDESTRUCT	equ 02h	;	    Active device-side non-destructive

						regValue8 |= 0x02;
						asd_hwi_swb_write_byte(asd, (lseq_cio_addr + 0x61), regValue8);

						asd_print("\n  ACTIVE Device DMA:");

						for (i = 0; i <= 15; i++)
						{
							if (i == 0)
							{
								asd_print("\n     Address bytes[0-7]:");
							}
							else if (i == 8)
							{
								asd_print("\n       Count bytes[0-4]:");
							}
							else if (i == 12)
							{
								asd_print("\n       Flags bytes[0-4]:");
							}

         /* Read LmMnDMAENG - 0x60 */

							asd_print("%02x", asd_hwi_swb_read_byte(asd,
								(lseq_cio_addr + 0x60)) ); //MnDMAENG
						}
						asd_print("\n");


      /* Set up to display NEXT Device element */

						regValue8 = (regValue8 & 0xf8);
//NXTDNONDESTRUCT	equ 03h
						regValue8 |= 0x03;
						asd_hwi_swb_write_byte(asd, (lseq_cio_addr + 0x61), regValue8);
						asd_print("\n  NEXT Device DMA:");

						for (i = 0; i <= 15; i++)
						{
							if (i == 0)
							{
								asd_print("\n     Address bytes[0-7]:");
							}
							else if (i == 8)
							{
								asd_print("\n       Count bytes[0-4]:");
							}
							else if (i == 12)
							{
								asd_print("\n       Flags bytes[0-4]:");
							}

         /* Read LmMnDMAENG - 0x60 */
							asd_print("%02x", asd_hwi_swb_read_byte(asd,
								(lseq_cio_addr + 0x60)) ); //MnDMAENG
						}
						asd_print("\n");

 

      /* Set up to display ACTIVE Host element */
						regValue8 = (regValue8 & 0xf8);
//	ACTHNONDESTRUCT	equ 05h	;	    Active host-side non-destructive
						regValue8 |= 0x05;
						asd_hwi_swb_write_byte(asd, (lseq_cio_addr + 0x61), regValue8);

						asd_print("\n  ACTIVE Host DMA:");
						for (i = 0; i <= 15; i++)
						{
							if (i == 0)
							{
								asd_print("\n     Address bytes[0-7]:");
							}
							else if (i == 8)
							{
								asd_print("\n       Count bytes[0-4]:");
							}
							else if (i == 12)
							{
								asd_print("\n       Flags bytes[0-4]:");
							}

         /* Read LmMnDMAENG - 0x60 */
							asd_print("%02x", asd_hwi_swb_read_byte(asd,
								(lseq_cio_addr + 0x60)) ); //MnDMAENG

						}
						asd_print("\n");

 

      /* Set up to display NEXT Host element */

						regValue8 = (regValue8 & 0xf8);
//	NXTHNONDESTRUCT	equ 06h	;	    Next host-side non-destructive

						regValue8 |= 0x06;
						asd_hwi_swb_write_byte(asd, (lseq_cio_addr + 0x61), regValue8);

						asd_print("\n  NEXT Host DMA:");

						for (i = 0; i <= 15; i++)
						{
							if (i == 0)
							{
								asd_print("\n     Address bytes[0-7]:");
							}
							else if (i == 8)
							{
								asd_print("\n       Count bytes[0-4]:");
							}
							else if (i == 12)
							{
								asd_print("\n       Flags bytes[0-4]:");
							}

         /* Read LmMnDMAENG - 0x60 */
							asd_print("%02x", asd_hwi_swb_read_byte(asd,
								(lseq_cio_addr + 0x60)) ); //MnDMAENG
						}
						asd_print("\n\n");
					}
 
                  			break;
				case 16:
					asd_print("%20s[0x%x]: 0x%04x\n",
						  LSEQmCIOREGS[idx].name,
						  LSEQmCIOREGS[idx].offset,
						 (asd_hwi_swb_read_word(asd,
						 (lseq_cio_addr +
						  LSEQmCIOREGS[idx].offset))));
//JD
					if(LSEQmCIOREGS[idx].offset == 0x20) //"LmMnSCBPTR"
					{
						scb=asd_hwi_swb_read_word(asd,
						 (lseq_cio_addr + 0x20));
					}
                  			break;
				case 32:
					asd_print("%20s[0x%x]: 0x%08x\n",
						  LSEQmCIOREGS[idx].name,
						  LSEQmCIOREGS[idx].offset,
						 (asd_hwi_swb_read_dword(asd,
						 (lseq_cio_addr +
						  LSEQmCIOREGS[idx].offset))));
					break;
				}
			}
			idx++;
		}
		if(scb !=0xffff) DumpScbSite(asd, lseq_id, scb);
	}

	asd_print("\nCIO REGISTERS\n");
	asd_print("*************\n");

	asd_print("\nMode 5\n");
	asd_print("------\n");

	lseq_cio_addr = LmSEQ_PHY_BASE(5, lseq_id);
	idx = 0;

	while (LSEQmOOBREGS[idx].width != 0) {
		switch(LSEQmOOBREGS[idx].width) {
               	case 8:
			asd_print("%20s[0x%x]: 0x%02x\n",
				  LSEQmOOBREGS[idx].name,
				  LSEQmOOBREGS[idx].offset,
				 (asd_hwi_swb_read_byte(
						asd, (lseq_cio_addr +
						LSEQmOOBREGS[idx].offset))));
                  	break;
		case 16:
			asd_print("%20s[0x%x]: 0x%04x\n",
				  LSEQmOOBREGS[idx].name,
				  LSEQmOOBREGS[idx].offset,
				 (asd_hwi_swb_read_word(
						asd, (lseq_cio_addr +
						LSEQmOOBREGS[idx].offset))));
                  	break;
		case 32:
			asd_print("%20s[0x%x]: 0x%08x\n",
				  LSEQmOOBREGS[idx].name,
				  LSEQmOOBREGS[idx].offset,
				 (asd_hwi_swb_read_dword(
						asd, (lseq_cio_addr +
						LSEQmOOBREGS[idx].offset))));
			break;
		}
		idx++;
	}

	asd_print("\nLSEQ %d STATE MACHINES.\n", lseq_id);
	asd_print("**********************\n");

	saved_reg16 = asd_hwi_swb_read_word(asd, LmSMDBGCTL(lseq_id));

	for (sm_idx = 0; sm_idx < 32; sm_idx++) {
		asd_hwi_swb_write_word(asd, LmSMDBGCTL(lseq_id), sm_idx);

		asd_print("   %20s[0x%x]:0x%08x\n", "SMSTATE", sm_idx,
			  asd_hwi_swb_read_dword(asd, LmSMSTATE(lseq_id)));

		asd_print("   %20s[0x%x]:0x%08x\n", "SMSTATEBRK", sm_idx,
			  asd_hwi_swb_read_dword(asd, LmSMSTATEBRK(lseq_id)));
	}

	asd_hwi_swb_write_word(asd, LmSMDBGCTL(lseq_id), saved_reg16);
}


/**
 * asd_dump_ddb_site -- dump a CSEQ DDB site
 * @asd_ha: pointer to host adapter structure
 * @site_no: site number of interest
 */
void asd_hwi_dump_ddb_site_raw(struct asd_softc *asd, uint16_t site_no)
{
	uint16_t	index;

	if (site_no >= asd->hw_profile.max_ddbs)
		return;

	for (index = 0; index < 0x80 ; index+=16) {
		asd_log(ASD_DBG_INFO,"%02x : %02x %02x %02x %02x %02x %02x %02x %02x - %02x %02x %02x %02x %02x %02x %02x %02x\n",\
                           index,\
                           asd_hwi_get_ddbsite_byte(asd,index),\
                           asd_hwi_get_ddbsite_byte(asd,index+1),\
                           asd_hwi_get_ddbsite_byte(asd,index+2),\
                           asd_hwi_get_ddbsite_byte(asd,index+3),\
                           asd_hwi_get_ddbsite_byte(asd,index+4),\
                           asd_hwi_get_ddbsite_byte(asd,index+5),\
                           asd_hwi_get_ddbsite_byte(asd,index+6),\
                           asd_hwi_get_ddbsite_byte(asd,index+7),\
                           asd_hwi_get_ddbsite_byte(asd,index+8),\
                           asd_hwi_get_ddbsite_byte(asd,index+9),\
                           asd_hwi_get_ddbsite_byte(asd,index+10),\
                           asd_hwi_get_ddbsite_byte(asd,index+11),\
                           asd_hwi_get_ddbsite_byte(asd,index+12),\
                           asd_hwi_get_ddbsite_byte(asd,index+13),\
                           asd_hwi_get_ddbsite_byte(asd,index+14),\
                           asd_hwi_get_ddbsite_byte(asd,index+15));
	}
}
void asd_hwi_dump_ddb_sites_raw(struct asd_softc *asd)
{
	uint16_t site_no;
	u8	opcode;
	for (site_no = 1; site_no < asd->hw_profile.max_ddbs; site_no++) {
		/* We are only interested in DDB sites currently used.
                 * Re-use opcode to store the connection rate.
		 */
		/* Setup the hardware DDB site. */
		asd_hwi_set_ddbptr(asd, site_no);

		opcode=asd_hwi_get_ddbsite_byte(asd,0x0f);
//		if ((opcode & 0x0F) == 0)
//			continue;
		asd_log(ASD_DBG_INFO,"DDB: %x\n",site_no);

		asd_hwi_dump_ddb_site_raw(asd, site_no);
	}
}
void asd_hwi_dump_scb_site_raw(struct asd_softc *asd, uint16_t site_no)
{

	uint16_t	index;

	for (index = 0; index < ASD_SCB_SIZE; index+=16) {
		asd_log(ASD_DBG_INFO,"%02x : %02x %02x %02x %02x %02x %02x %02x %02x - %02x %02x %02x %02x %02x %02x %02x %02x\n",\
                           index,\
                           asd_hwi_get_scbsite_byte(asd,index),\
                           asd_hwi_get_scbsite_byte(asd, index+1),\
                           asd_hwi_get_scbsite_byte(asd, index+2),\
                           asd_hwi_get_scbsite_byte(asd, index+3),\
                           asd_hwi_get_scbsite_byte(asd, index+4),\
                           asd_hwi_get_scbsite_byte(asd, index+5),\
                           asd_hwi_get_scbsite_byte(asd, index+6),\
                           asd_hwi_get_scbsite_byte(asd, index+7),\
                           asd_hwi_get_scbsite_byte(asd, index+8),\
                           asd_hwi_get_scbsite_byte(asd, index+9),\
                           asd_hwi_get_scbsite_byte(asd, index+10),\
                           asd_hwi_get_scbsite_byte(asd, index+11),\
                           asd_hwi_get_scbsite_byte(asd, index+12),\
                           asd_hwi_get_scbsite_byte(asd, index+13),\
                           asd_hwi_get_scbsite_byte(asd, index+14),\
                           asd_hwi_get_scbsite_byte(asd, index+15));
	}
}
/* 
 * Function:
 *	asd_hwi_init_scb_sites()
 *
 * Description:
 *	Initialize HW SCB sites.	 
 */
void asd_hwi_dump_scb_sites_raw(struct asd_softc *asd)
{
	uint16_t	site_no;
	u8	opcode;

#ifndef EXTENDED_SCB
	for (site_no = 0; site_no < ASD_MAX_SCB_SITES; site_no++) {
#else
	for (site_no = 0; site_no < (ASD_MAX_SCB_SITES + ASD_EXTENDED_SCB_NUMBER); site_no++) {
#endif
		/* 
		 * Adjust to the SCB site that we want to access in command
		 * context memory.
		 */	 
		asd_hwi_set_scbptr(asd, site_no);
		/* We are only interested in SCB sites currently used.
		 */
		opcode = asd_hwi_get_scbsite_byte(asd,10);
		if (opcode == 0xFF)
			continue;

		asd_log(ASD_DBG_INFO,"\nSCB: 0x%x\n", site_no);
		asd_hwi_dump_scb_site_raw(asd, site_no);
	}


}

void
#ifdef SEQUENCER_UPDATE
asd_hwi_dump_ssp_smp_ddb_site(struct asd_softc *asd, u_int site_no)
#else
asd_hwi_dump_ddb_site(struct asd_softc *asd, u_int site_no)
#endif
{
	int i;
	uint16_t scb;
	if (site_no >= ASD_MAX_DDBS)
		return;

	/* Setup the hardware DDB site. */
	asd_hwi_set_ddbptr(asd, site_no);

	asd_print("\nDDB: 0x%x\n", site_no);
	asd_print("---------\n\n");
	asd_print("dest_sas_addr: 0x");
	for(i=0;i<8;i++)
	{
		asd_print("%02x",
		  asd_hwi_get_ddbsite_byte(asd,
		  	offsetof(struct asd_ssp_smp_ddb, dest_sas_addr)+i)); 
	}
	asd_print("\n RAW DATA:");
	for(i=0;i<60;i++)
	{
		if( !(i % 16) )
			asd_print("\n");
		asd_print("%02x ",
		  asd_hwi_get_ddbsite_byte(asd,i)); 
	}
	asd_print("\n");
	asd_print("SendQ Target Head: 0x%04x.\n",
		  asd_hwi_get_ddbsite_word(asd,
		  	offsetof(struct asd_ssp_smp_ddb, send_q_head))); 
	asd_print("SendQ Suspended: 0x%02x.\n",
		  asd_hwi_get_ddbsite_byte(asd,
		  	offsetof(struct asd_ssp_smp_ddb, sqsuspended)));
	asd_print("DDB Type: 0x%02x.\n",
		  asd_hwi_get_ddbsite_byte(asd,
		  	offsetof(struct asd_ssp_smp_ddb, ddb_type)));
	asd_print("AWT Default: 0x%04x.\n",
		  asd_hwi_get_ddbsite_word(asd,
		  	offsetof(struct asd_ssp_smp_ddb, awt_default)));
	asd_print("Pathway Blocked Count: 0x%02x.\n",
		  asd_hwi_get_ddbsite_byte(asd,
		  	offsetof(struct asd_ssp_smp_ddb, pathway_blk_cnt)));
	asd_print("Conn Mask: 0x%02x.\n",
		  asd_hwi_get_ddbsite_byte(asd,
		  	offsetof(struct asd_ssp_smp_ddb, conn_mask)));
	asd_print("DDB Flags: 0x%02x.\n",
		  asd_hwi_get_ddbsite_byte(asd,
		  	offsetof(struct asd_ssp_smp_ddb, open_affl)));
	asd_print("ExecQ Target Tail: 0x%04x.\n",
		  asd_hwi_get_ddbsite_word(asd,
		  	offsetof(struct asd_ssp_smp_ddb, exec_q_tail)));
	asd_print("SendQ Target Tail: 0x%04x.\n",
		  asd_hwi_get_ddbsite_word(asd,
		  	offsetof(struct asd_ssp_smp_ddb, send_q_tail)));
#ifdef SEQUENCER_UPDATE
	asd_print("Max Number of Concurrent Connections: 0x%02x.\n",
		  asd_hwi_get_ddbsite_byte(asd,
		  	offsetof(struct asd_ssp_smp_ddb, max_concurrent_connections)));
	asd_print("Current Number of Concurrent Connections: 0x%02x.\n",
		  asd_hwi_get_ddbsite_byte(asd,
		  	offsetof(struct asd_ssp_smp_ddb, concurrent_connections)));
	asd_print("Tunable Number of Contexts: 0x%02x.\n",
		  asd_hwi_get_ddbsite_byte(asd,
		  	offsetof(struct asd_ssp_smp_ddb, tunable_number_contexts)));
#endif
	asd_print("Active Task Count: 0x%04x.\n",
		  asd_hwi_get_ddbsite_word(asd,
		  	offsetof(struct asd_ssp_smp_ddb, active_task_cnt)));
	asd_print("ITNL Reason: 0x%02x.\n",
		  asd_hwi_get_ddbsite_byte(asd,
		  	offsetof(struct asd_ssp_smp_ddb, itnl_reason)));
	asd_print("INTL Timeout Const: 0x%04x.\n",
		  asd_hwi_get_ddbsite_word(asd,
		  	offsetof(struct asd_ssp_smp_ddb, itnl_const)));
	scb = asd_hwi_get_ddbsite_word(asd,
		  	offsetof(struct asd_sata_stp_ddb, ata_cmd_scb_ptr));
	if((scb!=0xffff)&&(scb!=0))
	{
		uint8_t conn_mask;
		u_int	lseq_id;
		conn_mask = asd_hwi_get_ddbsite_byte(asd,
		  	offsetof(struct asd_ssp_smp_ddb, conn_mask));
		lseq_id=0;
		for ( ; lseq_id < 8; lseq_id++) {
			if (conn_mask & (1 << lseq_id)) {
				conn_mask &= ~(1 << lseq_id);
				break;
			} 
		}
		/* Dump out specific scb Registers state. */
		DumpScbSite(asd, lseq_id, scb);
	}
}

#ifdef SEQUENCER_UPDATE
void
asd_hwi_dump_sata_stp_ddb_site(struct asd_softc *asd, u_int site_no)
{
	if (site_no >= ASD_MAX_DDBS)
		return;

	/* Setup the hardware DDB site. */
	asd_hwi_set_ddbptr(asd, site_no);

	asd_print("\nDDB: 0x%x\n", site_no);
	asd_print("---------\n\n");

	asd_print("SendQ Target Head: 0x%04x.\n",
		  asd_hwi_get_ddbsite_word(asd,
		  	offsetof(struct asd_sata_stp_ddb, send_q_head)));
	asd_print("SendQ Suspended: 0x%02x.\n",
		  asd_hwi_get_ddbsite_byte(asd,
		  	offsetof(struct asd_sata_stp_ddb, sqsuspended)));
	asd_print("DDB Type: 0x%02x.\n",
		  asd_hwi_get_ddbsite_byte(asd,
		  	offsetof(struct asd_sata_stp_ddb, ddb_type)));
	asd_print("AWT Default: 0x%04x.\n",
		  asd_hwi_get_ddbsite_word(asd,
		  	offsetof(struct asd_sata_stp_ddb, awt_default)));
	asd_print("Pathway Blocked Count: 0x%02x.\n",
		  asd_hwi_get_ddbsite_byte(asd,
		  	offsetof(struct asd_sata_stp_ddb, pathway_blk_cnt)));
	asd_print("Conn Mask: 0x%02x.\n",
		  asd_hwi_get_ddbsite_byte(asd,
		  	offsetof(struct asd_sata_stp_ddb, conn_mask)));
	asd_print("Open Reject Status: 0x%02x.\n",
		  asd_hwi_get_ddbsite_byte(asd,
		  	offsetof(struct asd_sata_stp_ddb, stp_close)));
	asd_print("ExecQ Target Tail: 0x%04x.\n",
		  asd_hwi_get_ddbsite_word(asd,
		  	offsetof(struct asd_sata_stp_ddb, exec_q_tail)));
	asd_print("SendQ Target Tail: 0x%04x.\n",
		  asd_hwi_get_ddbsite_word(asd,
		  	offsetof(struct asd_sata_stp_ddb, send_q_tail)));
	asd_print("Active Task Count: 0x%04x.\n",
		  asd_hwi_get_ddbsite_word(asd,
		  	offsetof(struct asd_sata_stp_ddb, active_task_cnt)));
	asd_print("ITNL Reason: 0x%02x.\n",
		  asd_hwi_get_ddbsite_byte(asd,
		  	offsetof(struct asd_sata_stp_ddb, itnl_reason)));
	asd_print("INTL Timeout Const: 0x%04x.\n",
		  asd_hwi_get_ddbsite_word(asd,
		  	offsetof(struct asd_sata_stp_ddb, itnl_const)));
}
#endif

/*
 * Template to hold setting breakpoint for ARP2 codes.
 */
void
asd_hwi_set_cseq_breakpoint(struct asd_softc *asd, uint16_t addr)
{
	uint32_t	inten;

	asd_hwi_swb_write_dword(asd, CARP2BREAKADR01, addr >> 2);

	asd_hwi_swb_write_dword(asd, CARP2INT, 0xF);

	asd_hwi_swb_write_dword(asd, CARP2CTL, 
		asd_hwi_swb_read_dword(asd, CARP2CTL) | BREAKEN0);

	inten = asd_hwi_swb_read_dword(asd, CARP2INTEN);

	asd_hwi_swb_write_dword(asd, CARP2INTEN, inten | EN_ARP2BREAK0);
}
#if 0
void
asd_hwi_set_lseq_breakpoint(struct asd_softc *asd, int phy_id, uint16_t addr)
{
	uint32_t	inten;

	asd_hwi_swb_write_dword(asd, LmARP2BREAKADR01(phy_id), addr >> 2);

	asd_hwi_swb_write_dword(asd, LmARP2INT(phy_id), 0xF);

	asd_hwi_swb_write_dword(asd, LmARP2CTL(phy_id), 
		asd_hwi_swb_read_dword(asd, LmARP2CTL(phy_id)) | BREAKEN0);

	inten = asd_hwi_swb_read_dword(asd, LmARP2INTEN(phy_id));

	asd_hwi_swb_write_dword(asd, LmARP2INTEN(phy_id),
		inten | EN_ARP2BREAK0);
}
#else
void
asd_hwi_set_lseq_breakpoint(struct asd_softc *asd, int phy_id, uint32_t breakid, uint16_t addr)
{
	uint32_t	inten;
	uint32_t	brkadr;

	switch(breakid)
	{
	case EN_ARP2BREAK1:
		brkadr = asd_hwi_swb_read_dword(asd, LmARP2BREAKADR01(phy_id)) & (~BREAKADR1_MASK);
		brkadr |= ((addr >> 2) << 16);
		asd_hwi_swb_write_dword(asd, LmARP2BREAKADR01(phy_id), brkadr);
		break;
	case EN_ARP2BREAK2:
		brkadr = asd_hwi_swb_read_dword(asd, LmARP2BREAKADR23(phy_id)) & (~BREAKADR2_MASK);
		brkadr |= (addr >> 2);
		asd_hwi_swb_write_dword(asd, LmARP2BREAKADR23(phy_id), brkadr);
		break;
	case EN_ARP2BREAK3:
		brkadr = asd_hwi_swb_read_dword(asd, LmARP2BREAKADR23(phy_id)) & (~BREAKADR3_MASK);
		brkadr |= ((addr >> 2) << 16);
		asd_hwi_swb_write_dword(asd, LmARP2BREAKADR23(phy_id), brkadr);
		break;
	case EN_ARP2BREAK0:
	default:
		brkadr = asd_hwi_swb_read_dword(asd, LmARP2BREAKADR01(phy_id)) & (~BREAKADR0_MASK);
		brkadr |= (addr >> 2);
		asd_hwi_swb_write_dword(asd, LmARP2BREAKADR01(phy_id), brkadr);
		break;
	}

	asd_hwi_swb_write_dword(asd, LmARP2INT(phy_id), 0xF);

	asd_hwi_swb_write_dword(asd, LmARP2CTL(phy_id), 
		asd_hwi_swb_read_dword(asd, LmARP2CTL(phy_id)) | (breakid<<8));

	inten = asd_hwi_swb_read_dword(asd, LmARP2INTEN(phy_id));

	asd_hwi_swb_write_dword(asd, LmARP2INTEN(phy_id),
		inten | breakid);
}
#endif
#define DUMP_MEMSIZE		1024

void
asd_hwi_read_mem(struct asd_softc *asd, uint8_t *memory,
	unsigned addr, unsigned len)
{
	unsigned	i;

	for (i = 0 ; i < (len / sizeof(unsigned)) ; i+=sizeof(unsigned))
	{
		*((unsigned *)memory + i) = asd_hwi_swb_read_dword(
			asd, addr + i);
	}
}

#define ISPRINT(c) (((c) >= ' ') && ((c) <= '~'))

void
asd_printhex(
unsigned char	*s,
unsigned	addr,
unsigned	len,
unsigned	chunks
)
{
	unsigned	i;
	unsigned	count;
	unsigned	num;
	char		*format_string;
	char		*blank_string;

	i = 0;

	for (count = 0 ; count < len ; count = count + num, s = s + num,
		addr = addr + num)
	{
		num = ((len - count) > 16) ? 16 : (len - count);

		asd_print("0x%08x: ", addr);

		switch (chunks)
		{
		case 1:
			blank_string = "   ";
			for (i = 0 ; i < num ; i += chunks)
			{
				asd_print("%02x ", *((unsigned char *)(s + i)));
			}
			break;
		case 2:
			blank_string = "     ";
			for (i = 0 ; i < num ; i += chunks)
			{
				asd_print("%04x ",
					*((unsigned short *)(s + i)));
			}
			break;
		case 4:
			blank_string = "         ";
			for (i = 0 ; i < num ; i += chunks)
			{
				asd_print("%08x ", *((unsigned *)(s + i)));
			}
			break;
		}


		while (i != num)
		{
			asd_print(blank_string);
			i++;
		}

		for (i = 0 ; i < num ; i++)
		{
			if (ISPRINT(*(s + i)))
			{
				asd_print("%c", *(s + i));
			}
			else
			{
				asd_print(".");
			}
		}

		asd_print("\n");
	}
}

void
asd_hwi_dump_seq_raw(struct asd_softc *asd)
{
	uint8_t		*memory;
	unsigned	index;
	unsigned	size;

	memory = (uint8_t *)asd_alloc_mem(DUMP_MEMSIZE, GFP_KERNEL);

	index = 0;
	
	size = 0x40;
	printk("\nCSEQ: Mode Dependent Scratch Mode 0 Pages 0-1\n");
	asd_hwi_read_mem(asd, memory, CMAPPEDSCR + index, size);
	asd_printhex(memory, CMAPPEDSCR + index, size, 2);
	index += size;

	size = 0x40;
	printk("\nCSEQ: Mode Dependent Scratch Mode 1 Pages 0-1\n");
	asd_hwi_read_mem(asd, memory, CMAPPEDSCR + index, size);
	asd_printhex(memory, CMAPPEDSCR + index + index, size, 2);
	index += size;

	size = 0x40;
	printk("\nCSEQ: Mode Dependent Scratch Mode 2 Pages 0-1\n");
	asd_hwi_read_mem(asd, memory, CMAPPEDSCR + index, size);
	asd_printhex(memory, CMAPPEDSCR + index + index, size, 2);
	index += size;

	size = 0x40;
	printk("\nCSEQ: Mode Dependent Scratch Mode 3 Pages 0-1\n");
	asd_hwi_read_mem(asd, memory, CMAPPEDSCR + index, size);
	asd_printhex(memory, CMAPPEDSCR + index + index, size, 2);
	index += size;

	size = 0x40;
	printk("\nCSEQ: Mode Dependent Scratch Mode 4 Pages 0-1\n");
	asd_hwi_read_mem(asd, memory, CMAPPEDSCR + index, size);
	asd_printhex(memory, CMAPPEDSCR + index + index, size, 2);
	index += size;

	size = 0x40;
	printk("\nCSEQ: Mode Dependent Scratch Mode 5 Pages 0-1\n");
	asd_hwi_read_mem(asd, memory, CMAPPEDSCR + index, size);
	asd_printhex(memory, CMAPPEDSCR + index + index, size, 2);
	index += size;

	size = 0x40;
	printk("\nCSEQ: Mode Dependent Scratch Mode 6 Pages 0-1\n");
	asd_hwi_read_mem(asd, memory, CMAPPEDSCR + index, size);
	asd_printhex(memory, CMAPPEDSCR + index + index, size, 2);
	index += size;

	size = 0x40;
	printk("\nCSEQ: Mode Dependent Scratch Mode 7 Pages 0-1\n");
	asd_hwi_read_mem(asd, memory, CMAPPEDSCR + index, size);
	asd_printhex(memory, CMAPPEDSCR + index + index, size, 2);
	index += size;

	size = 0x60;
	printk("\nCSEQ: Mode Dependent Scratch Mode 8 Pages 0-2\n");
	printk("CSEQ: Mode Independent Scratch Pages 0-2\n");
	asd_hwi_read_mem(asd, memory, CMAPPEDSCR + index, size);
	asd_printhex(memory, CMAPPEDSCR + index + index, size, 2);
	index += size;

	size = 0x60;
	printk("\nCSEQ: Mode Independent Scratch Pages 0-2\n");
	asd_hwi_read_mem(asd, memory, CMAPPEDSCR + index, size);
	asd_printhex(memory, CMAPPEDSCR + index + index, size, 2);
	index += size;

	index += 0x20; // skipping hole

	size = 0x80;
	printk("\nCSEQ: Mode Independent Scratch Pages 4-7\n");
	asd_hwi_read_mem(asd, memory, CMAPPEDSCR + index, size);
	asd_printhex(memory, CMAPPEDSCR + index + index, size, 2);
	index += size;

	size = 0x20;
	printk("\nCSEQ: Mode Dependent Scratch Mode 0 Pages 2\n");
	asd_hwi_read_mem(asd, memory, CMAPPEDSCR + index, size);
	asd_printhex(memory, CMAPPEDSCR + index, size, 2);
	index += size;

	size = 0x20;
	printk("\nCSEQ: Mode Dependent Scratch Mode 1 Pages 2\n");
	asd_hwi_read_mem(asd, memory, CMAPPEDSCR + index, size);
	asd_printhex(memory, CMAPPEDSCR + index, size, 2);
	index += size;

	size = 0x20;
	printk("\nCSEQ: Mode Dependent Scratch Mode 2 Pages 2\n");
	asd_hwi_read_mem(asd, memory, CMAPPEDSCR + index, size);
	asd_printhex(memory, CMAPPEDSCR + index, size, 2);
	index += size;

	size = 0x20;
	printk("\nCSEQ: Mode Dependent Scratch Mode 3 Pages 2\n");
	asd_hwi_read_mem(asd, memory, CMAPPEDSCR + index, size);
	asd_printhex(memory, CMAPPEDSCR + index, size, 2);
	index += size;

	size = 0x20;
	printk("\nCSEQ: Mode Dependent Scratch Mode 4 Pages 2\n");
	asd_hwi_read_mem(asd, memory, CMAPPEDSCR + index, size);
	asd_printhex(memory, CMAPPEDSCR + index, size, 2);
	index += size;

	size = 0x20;
	printk("\nCSEQ: Mode Dependent Scratch Mode 5 Pages 2\n");
	asd_hwi_read_mem(asd, memory, CMAPPEDSCR + index, size);
	asd_printhex(memory, CMAPPEDSCR + index, size, 2);
	index += size;

	size = 0x20;
	printk("\nCSEQ: Mode Dependent Scratch Mode 6 Pages 2\n");
	asd_hwi_read_mem(asd, memory, CMAPPEDSCR + index, size);
	asd_printhex(memory, CMAPPEDSCR + index, size, 2);
	index += size;

	size = 0x20;
	printk("\nCSEQ: Mode Dependent Scratch Mode 7 Pages 2\n");
	asd_hwi_read_mem(asd, memory, CMAPPEDSCR + index, size);
	asd_printhex(memory, CMAPPEDSCR + index, size, 2);
	index += size;

	printk("\n-----------------------------------------------------\n\n");

	index = 0;
	
	size = 0x40;
	printk("\nLSEQ: Mode Dependent Scratch Mode 0 Pages 0-1\n");
	asd_hwi_read_mem(asd, memory, LmSCRATCH(0) + index, size);
	asd_printhex(memory, LmSCRATCH(0) + index, size, 2);
	index += size;

	size = 0x40;
	printk("\nLSEQ: Mode Dependent Scratch Mode 1 Pages 0-1\n");
	asd_hwi_read_mem(asd, memory, LmSCRATCH(0) + index, size);
	asd_printhex(memory, LmSCRATCH(0) + index + index, size, 2);
	index += size;

	size = 0x40;
	printk("\nLSEQ: Mode Dependent Scratch Mode 2 Pages 0-1\n");
	asd_hwi_read_mem(asd, memory, LmSCRATCH(0) + index, size);
	asd_printhex(memory, LmSCRATCH(0) + index + index, size, 2);
	index += size;

	size = 0x40;
	printk("\nLSEQ: Mode Dependent Scratch Mode 3 Pages 0-1\n");
	asd_hwi_read_mem(asd, memory, LmSCRATCH(0) + index, size);
	asd_printhex(memory, LmSCRATCH(0) + index + index, size, 2);
	index += size;

	size = 0x40;
	printk("\nLSEQ: Mode Dependent Scratch Mode 4 Pages 0-1\n");
	asd_hwi_read_mem(asd, memory, LmSCRATCH(0) + index, size);
	asd_printhex(memory, LmSCRATCH(0) + index + index, size, 2);
	index += size;

	size = 0x40;
	printk("\nLSEQ: Mode Dependent Scratch Mode 5 Pages 0-1\n");
	asd_hwi_read_mem(asd, memory, LmSCRATCH(0) + index, size);
	asd_printhex(memory, LmSCRATCH(0) + index + index, size, 2);
	index += size;

	size = 0x40;
	printk("\nLSEQ: Mode Dependent Scratch Mode 6 Pages 0-1\n");
	asd_hwi_read_mem(asd, memory, LmSCRATCH(0) + index, size);
	asd_printhex(memory, LmSCRATCH(0) + index + index, size, 2);
	index += size;

	size = 0x40;
	printk("\nLSEQ: Mode Dependent Scratch Mode 7 Pages 0-1\n");
	asd_hwi_read_mem(asd, memory, LmSCRATCH(0) + index, size);
	asd_printhex(memory, LmSCRATCH(0) + index + index, size, 2);
	index += size;

	size = 0x60;
	printk("\nLSEQ: Mode Dependent Scratch Mode 8 Pages 0-2\n");
	printk("LSEQ: Mode Independent Scratch Pages 0-2\n");
	asd_hwi_read_mem(asd, memory, LmSCRATCH(0) + index, size);
	asd_printhex(memory, LmSCRATCH(0) + index + index, size, 2);
	index += size;

	size = 0x60;
	printk("\nLSEQ: Mode Independent Scratch Pages 0-2\n");
	asd_hwi_read_mem(asd, memory, LmSCRATCH(0) + index, size);
	asd_printhex(memory, LmSCRATCH(0) + index + index, size, 2);
	index += size;

	index += 0x20; // skipping hole

	size = 0x80;
	printk("\nLSEQ: Mode Independent Scratch Pages 4-7\n");
	asd_hwi_read_mem(asd, memory, LmSCRATCH(0) + index, size);
	asd_printhex(memory, LmSCRATCH(0) + index + index, size, 2);
	index += size;

	size = 0x20;
	printk("\nLSEQ: Mode Dependent Scratch Mode 0 Pages 2\n");
	asd_hwi_read_mem(asd, memory, LmSCRATCH(0) + index, size);
	asd_printhex(memory, LmSCRATCH(0) + index, size, 2);
	index += size;

	size = 0x20;
	printk("\nLSEQ: Mode Dependent Scratch Mode 1 Pages 2\n");
	asd_hwi_read_mem(asd, memory, LmSCRATCH(0) + index, size);
	asd_printhex(memory, LmSCRATCH(0) + index, size, 2);
	index += size;

	size = 0x20;
	printk("\nLSEQ: Mode Dependent Scratch Mode 2 Pages 2\n");
	asd_hwi_read_mem(asd, memory, LmSCRATCH(0) + index, size);
	asd_printhex(memory, LmSCRATCH(0) + index, size, 2);
	index += size;

	size = 0x20;
	printk("\nLSEQ: Mode Dependent Scratch Mode 3 Pages 2\n");
	asd_hwi_read_mem(asd, memory, LmSCRATCH(0) + index, size);
	asd_printhex(memory, LmSCRATCH(0) + index, size, 2);
	index += size;

	size = 0x20;
	printk("\nLSEQ: Mode Dependent Scratch Mode 4 Pages 2\n");
	asd_hwi_read_mem(asd, memory, LmSCRATCH(0) + index, size);
	asd_printhex(memory, LmSCRATCH(0) + index, size, 2);
	index += size;

	size = 0x20;
	printk("\nLSEQ: Mode Dependent Scratch Mode 5 Pages 2\n");
	asd_hwi_read_mem(asd, memory, LmSCRATCH(0) + index, size);
	asd_printhex(memory, LmSCRATCH(0) + index, size, 2);
	index += size;

	size = 0x20;
	printk("\nLSEQ: Mode Dependent Scratch Mode 6 Pages 2\n");
	asd_hwi_read_mem(asd, memory, LmSCRATCH(0) + index, size);
	asd_printhex(memory, LmSCRATCH(0) + index, size, 2);
	index += size;

	size = 0x20;
	printk("\nLSEQ: Mode Dependent Scratch Mode 7 Pages 2\n");
	asd_hwi_read_mem(asd, memory, LmSCRATCH(0) + index, size);
	asd_printhex(memory, LmSCRATCH(0) + index, size, 2);
	index += size;
	asd_free_mem(memory);
}

#endif /* ASD_DEBUG */
