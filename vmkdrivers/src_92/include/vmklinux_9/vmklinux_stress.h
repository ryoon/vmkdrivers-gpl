/* ****************************************************************
 * Portions Copyright 2008 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/

#ifndef _VMKLNX26_STRESS_H_
#define _VMKLNX26_STRESS_H_

#include "vmkapi.h"

static inline vmk_Bool vmklnx_stress_counter(vmk_StressOptionHandle opt)
{
   vmk_Bool result;
#ifdef VMX86_DEBUG
   VMK_ReturnStatus status =
#endif //VMX86_DEBUG
      vmk_StressOptionCounter(opt, &result);
   VMK_ASSERT(status == VMK_OK);
   return result;
}

static inline uint32_t vmklnx_stress_option(vmk_StressOptionHandle opt)
{
   uint32_t result;
#ifdef VMX86_DEBUG
   VMK_ReturnStatus status =
#endif // VMX86_DEBUG
      vmk_StressOptionValue(opt, &result);
   VMK_ASSERT(status == VMK_OK);
   return result;
}

#define VMKLNX_STRESS_COUNTER(_opt) vmklnx_stress_counter(_opt)
#define VMKLNX_STRESS_OPTION(_opt)  vmklnx_stress_option(_opt)

#endif /* _VMKLNX26_STRESS_H_ */
