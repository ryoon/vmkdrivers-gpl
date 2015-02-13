/*
 * QLogic iSCSI HBA Driver
 * Copyright (c)  2003-2007 QLogic Corporation
 *
 * See LICENSE.qla4xxx for copyright and licensing details.
 */

#ifndef __QL4IM_DUMP_H_
#define __QL4IM_DUMP_H_
/*
 * Dump Image Header
 */
struct dump_image_header {
	uint32_t cookie;                /* 0x00  QLGC 		*/
	uint8_t  dump_id_string[12];    /* 0x04  "40xx Dump   "	*/
	uint64_t time_stamp;		/* 0x10  timeb struct used by ftime() */
	uint32_t total_image_size;      /* 0x18  image size excluding header  */
	uint32_t core_dump_offset;      /* 0x1c  also represents size of header */
	uint32_t probe_dump_offset;     /* 0x20  */
	uint32_t queue_dump_offset;     /* 0x24  */
	uint32_t reserved1[6];          /* 0x28  */
	uint8_t driver[0x30];           /* 0x40  "qla4xxx_x vx.xx.xx-dx" */
	uint8_t ioctlmod[0x30];         /* 0x70  "qisioctl_x vx.xx.xx-dx" */
	uint8_t reserved2[0x60];        /* 0xA0  */
};                  /* 0x100 (256) bytes */

#define DUMP_IMAGE_HEADER_SIZE   (sizeof(struct dump_image_header))
#define DUMP_IMAGE_HEADER_OFFSET 0

#define QLGC_COOKIE     0x43474C51


/*
 * Core Dump
 */
struct core_dump {
	uint32_t PCIRegProc[1024];      /* 4096    bytes */
	uint32_t SRAM[524288];          /* 2097152 bytes */
	uint32_t OAPCoreReg[64];        /* 256     bytes */
	uint32_t OAPAuxReg[778];        /* 3112    bytes */
	uint32_t IAPCoreReg[64];        /* 256     bytes */
	uint32_t IAPAuxReg[778];        /* 3112    bytes */
	uint32_t IAPSRAM[2048];         /* 8192    bytes */
	uint32_t HostPCIRegPage[4][64];   /* 4 * 256     bytes */
};                            /* 2117200 total */

#define CORE_DUMP_SIZE   (sizeof(struct core_dump))
#define CORE_DUMP_OFFSET (0 + DUMP_IMAGE_HEADER_SIZE)


/*
 * Probe Dump
 */
struct probe_dump {
	uint8_t  data[28416];/* actually 28160 = 0x40x55x8 */
};

#define PROBE_DUMP_SIZE   (sizeof(struct probe_dump))
#define PROBE_DUMP_OFFSET (CORE_DUMP_OFFSET + CORE_DUMP_SIZE)


/*
 * Dump Image
 */
struct dump_image{
	struct dump_image_header	dump_header;
	struct core_dump		core_dump;
	struct probe_dump		probe_dump;
};

#define DUMP_IMAGE_SIZE (sizeof(struct dump_image))

/****************************************************************
 *
 *                      Core Dump Defines
 *
 ****************************************************************/

/* Defines used when accessing MADI */
#define MADI_STAT_DATA_VALID   0
#define MADI_STAT_DATA_INVALID 1
#define MADI_STAT_COMMAND_BUSY 3

#define MADI_DEST_SRAM         0x00000000
#define MADI_DEST_CORE_REG     0x40000000
#define MADI_DEST_AUX_REG      0x80000000
#define MADI_DEST_ARC_DEBUG    0xc0000000

#define MADI_STAT_MASK         0x18000000
#define MADI_READ_CMD          0x20000000
			
#define PROC_OAP 1
#define PROC_IAP 2

#define RAM_START  0
#define RAM_END    0x1fffff
#define CORE_START 0
#define CORE_END   63
#define AUX_START  0
#define AUX_END    0x309
#define LDST_START 0x07f00000
#define LDST_END   0x07f01fff
#define PCI_START  0x08000000
#define PCI_END    0x08000fff


/****************************************************************
 *
 *                      Probe Dump Defines
 *
 ****************************************************************/

#define MUX_SELECT_MAX  0x40
#define MAX_MODULE      0x40
#define MAX_CLOCK       0x4

#define  SYSCLK   0
#define  PCICLK   1
#define  NRXCLK   2
#define  CPUCLK   3

#define  CLK_BIT(x)  (1<<x)


typedef enum
{
     probe_DA = 1
   , probe_BPM
   , probe_ODE
   , probe_SRM0
   , probe_SRM1
   , probe_PMD
   , probe_PRD
   , probe_SDE
   , probe_RMD
   , probe_IDE
   , probe_TDE
   , probe_RA
   , probe_REG
   , probe_RMI
   , probe_OAP
   , probe_ECM
   , probe_NPF
   , probe_IAP
   , probe_OTP
   , probe_TTM
   , probe_ITP
   , probe_MAM
   , probe_BLM
   , probe_ILM
   , probe_IFP
   , probe_IPV
   , probe_OIP
   , probe_OFB
   , probe_MAC
   , probe_IFB
   , probe_PCORE
   , probe_NRM0
   , probe_NRM1
   , probe_SCM0
   , probe_SCM1
   , probe_NCM0
   , probe_NCM1
   , probe_RBM0
   , probe_RBM1
   , probe_RBM2
   , probe_RBM3
   , probe_ERM0
   , probe_ERM1
   , probe_PERF0
   , probe_PERF1
} probe_Module;

typedef struct
{
   char     *moduleName;
   uint32_t   clocks;
   uint32_t   maxSelect;
} PROBEMUX_INFO;


#define PROBE_RE  0x8000
#define PROBE_UP  0x4000
#define PROBE_LO  0x0000

struct probe_data {
   uint32_t   high;
   uint32_t   low;
};

extern void ql4_core_dump(struct scsi_qla_host *ha, void *pdump);

#endif
