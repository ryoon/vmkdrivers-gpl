/* **********************************************************
 * Copyright 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * vmkplexer_init.h
 *
 *      Prototypes for initialization functions called at 
 *      module-init time for vmkplexer.
 */

#ifndef VMKPLEXER_INIT_H
#define VMKPLEXER_INIT_H

extern VMK_ReturnStatus vmkplxr_ChardevsInit(void);

extern VMK_ReturnStatus vmkplxr_ProcfsInit(void);
extern void             vmkplxr_ProcfsCleanup(void);

extern VMK_ReturnStatus vmkplxr_EntropyInit(void);

extern VMK_ReturnStatus vmkplxr_ScsiInit(void);
extern void             vmkplxr_ScsiCleanup(void);

#endif /* VMKPLEXER_INIT_H */
