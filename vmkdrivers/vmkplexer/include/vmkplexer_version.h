/* **********************************************************
 * Copyright 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * vmkplexer_version.h
 */

#include "buildNumber.h"

/*
 * Macro to convert a symbol to a string
 */
#define __VMKPLX_STRINGIFY(sym)         #sym
#define VMKPLX_STRINGIFY(sym)           __VMKPLX_STRINGIFY(sym)
#define VMKPLX_BUILD_VERSION            VMKPLX_STRINGIFY(PRODUCT_VERSION)
