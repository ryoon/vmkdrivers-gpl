/* **********************************************************
 * Copyright 2004 - 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ******************************************************************************
 * Assertions                                                            */ /**
 *
 * \addtogroup Core
 * @{
 *
 * \defgroup Assert System Panic and Assertions
 * @{
 ******************************************************************************
 */

#ifndef _VMKAPI_ASSERT_H_
#define _VMKAPI_ASSERT_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include <stdarg.h>

/*
 ******************************************************************************
 * vmk_vPanic --                                                         */ /**
 *
 * \brief Indicate that an unrecoverable condition was discovered
 *
 * A module indicates with this function that it discovered an unrecoverable
 * condition. Depending on the system state the VMKernel might initiate a system
 * panic or forcefully disable and unload the module. The caller is guaranteed
 * that no code from the module referenced by moduleID will be executed after a
 * call to this function.
 *
 * \printformatstringdoc
 * \note This function will not return.
 *
 * \param[in]   moduleID        ModuleID of the panicking module
 * \param[in]   fmt             Error string in printf format
 * \param[in]   ap              Additional parameters for the error string
 *
 ******************************************************************************
 */
void vmk_vPanic(vmk_ModuleID moduleID,
                const char  *fmt,
                va_list      ap);

/*
 ******************************************************************************
 * vmk_PanicWithModuleID --                                              */ /**
 *
 * \brief Indicate that an unrecoverable condition was discovered
 *
 * A module indicates with this function that it discovered an unrecoverable
 * condition. Depending on the system state the VMKernel might initiate a system
 * panic or forcefully disable and unload the module. The caller is guaranteed
 * that no code from the module referenced by moduleID will be executed after a
 * call to this function.
 *
 * \printformatstringdoc
 * \note This function will not return.
 *
 * \param[in]   moduleID        ModuleID of the panicking module
 * \param[in]   fmt             Error string in printf format
 *
 ******************************************************************************
 */
void
vmk_PanicWithModuleID(vmk_ModuleID moduleID,
                      const char *fmt,
                      ...)
VMK_ATTRIBUTE_PRINTF(2,3);


/*
 ******************************************************************************
 * vmk_Panic --                                                          */ /**
 *
 * \brief Convenience wrapper around vmk_PanicWithModuleID for module local
 *        panic calls
 *
 * \printformatstringdoc
 * \note This function will not return.
 *
 * \param[in] _fmt      Error string in printf format
 * \param[in] _args     Arguments to the error string
 *
 ******************************************************************************
 */
#define vmk_Panic(_fmt, _args...)                                             \
   do {                                                                       \
      vmk_PanicWithModuleID(vmk_ModuleCurrentID, _fmt, ## _args);             \
   } while(0)


/*
 ******************************************************************************
 * VMK_ASSERT_BUG --                                                     */ /**
 *
 * \brief Panics the system if a runtime expression evalutes to false
 *        regardless of debug build status.
 *
 * \param[in] _cond_    Expression to check.
 *
 ******************************************************************************
 */
#define VMK_ASSERT_BUG(_cond_)                                    \
   do {                                                           \
      if (VMK_UNLIKELY(!(_cond_))) {                              \
         vmk_Panic("Failed at %s:%d -- VMK_ASSERT(%s)",           \
                   __FILE__, __LINE__, #_cond_);                  \
         /* Ensure that we don't lose warnings in condition */    \
         if (0) { if (_cond_) {;} } (void)0;                      \
      }                                                           \
   } while(0)


/*
 ******************************************************************************
 * VMK_ASSERT --                                                         */ /**
 *
 * \brief Panics the system if a runtime expression evalutes to false
 *        only in debug builds.
 *
 * \param[in] _cond_    Expression to check.
 *
 ******************************************************************************
 */
#if defined(VMX86_DEBUG)
#define VMK_ASSERT(_cond_) VMK_ASSERT_BUG(_cond_)
#else
#define VMK_ASSERT(_cond_)
#endif


/*
 ******************************************************************************
 * VMK_ASSERT_ON_COMPILE --                                              */ /**
 *
 * \ingroup Assert
 * \brief Fail compilation if a condition does not hold true
 *
 * \note This macro must be used inside the context of a function.
 *
 * \param[in] _cond_    Expression to check.
 *
 ******************************************************************************
 */
#define VMK_ASSERT_ON_COMPILE(_cond_)                    \
   do {                                                  \
      switch(0) {                                        \
         case 0:                                         \
         case (_cond_):                                  \
            ;                                            \
      }                                                  \
   } while(0)                                            \


/*
 ******************************************************************************
 * VMK_ASSERT_LIST --                                                    */ /**
 *
 * \brief  To put a VMK_ASSERT_ON_COMPILE() outside a function, wrap it in
 *         VMK_ASSERT_LIST(). The first parameter must be unique in each .c
 *         file where it appears
 *
 * \par Example usage with VMK_ASSERT_ON_COMPILE:
 *
 * \code
 * VMK_ASSERT_LIST(fs_assertions,
 *    VMK_ASSERT_ON_COMPILE(sizeof(fs_diskLock) == 128);
 *    VMK_ASSERT_ON_COMPILE(sizeof(fs_LockRes) == DISK_BLK_SIZE);
 * )
 * \endcode
 *
 * \param[in] _name_          Unique name of the list of assertions.
 * \param[in] _assertions_    Individual assert-on-compile statements.
 *
 ******************************************************************************
 */
#define VMK_ASSERT_LIST(_name_, _assertions_)      \
   static inline void _name_(void) {               \
      _assertions_                                 \
   }


/*
 ***********************************************************************
 * VMK_NOT_REACHED --                                             */ /**
 *
 * \ingroup Assert
 * \brief Panic if code reaches this call
 *
 ***********************************************************************
 */
#define VMK_NOT_REACHED() \
   vmk_Panic("Failed at %s:%d -- NOT REACHED", __FILE__, __LINE__)

#endif /* _VMKAPI_ASSERT_H_ */
/** @} */
/** @} */
