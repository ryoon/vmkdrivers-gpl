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

#if defined(VMK_BUILDING_FOR_KERNEL_MODE)

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
 *        panic calls.
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

#else


/*
 ******************************************************************************
 * Panic --                                                              */ /**
 *
 * \brief User-provided Panic function that should log the error string
 *        in printf format to the application's standard logging mechanism,
 *        potentially including additional diagnostic info, such as a stack
 *        trace. Panic must not return and it must terminate the application.
 *
 * \printformatstringdoc
 * \note This function must not return and the implementation has to be
 *        provided by the user of the vmkapi headers.
 *
 * \param[in] fmt       Error string in printf format, followed by arguments
 *                      to the error string.
 *
 ******************************************************************************
 */
extern void Panic(const char *fmt, ...) VMK_ATTRIBUTE_PRINTF(1, 2);

#define vmk_Panic(_fmt, _args...)                                             \
   do {                                                                       \
      Panic(_fmt, ## _args);                                                  \
   } while(0)

#endif /* VMK_BUILDING_FOR_KERNEL_MODE */


/*
 ******************************************************************************
 * VMK_ASSERT_PANIC --                                                   */ /**
 *
 * \brief Helper for VMK_ASSERT_BUG. Panics with the passed message during an
 *        assertion failure. If _fmt_ is empty, the stringified version of
 *        _cond_ is used as the message.
 *
 * \note  Compilation will fail if _fmt_ is not a string constant.
 *
 * \param[in] _cond_    Expression checked in VMK_ASSERT_PANIC.
 * \param[in] _fmt_     Format string constant.
 * \param[in] ...       Optional arguments specified by _fmt_.
 *
 ******************************************************************************
 */
#define VMK_ASSERT_PANIC(_cond_,_fmt_,...)                              \
   do {                                                                 \
      if (*(_fmt_) == '\0') {                                           \
         vmk_Panic("Failed at %s:%d -- VMK_ASSERT(%s)",                 \
                   __FILE__, __LINE__, #_cond_);                        \
      } else {                                                          \
         vmk_Panic("Failed at %s:%d -- " _fmt_,                         \
                   __FILE__, __LINE__, ## __VA_ARGS__);                 \
      }                                                                 \
   } while(0)


/*
 ******************************************************************************
 * VMK_ASSERT_BUG --                                                     */ /**
 *
 * \brief Panics the system if a runtime expression evalutes to false
 *        regardless of debug build status. If optional parameters are
 *        passed, those are used to print the panic message instead of
 *        the stringified version of the checked expression.
 *
 * \note  Compilation will fail if the first variadic argument is not
 *        a string constant.
 *
 * \param[in] _cond_    Expression to check.
 * \param[in] ...       Optional format string constant followed by arguments
 *                      that will form a more informative message that just
 *                      printing the stringified form of the checked
 *                      expression.
 *
 ******************************************************************************
 */
#define VMK_ASSERT_BUG(_cond_,...)                                \
   do {                                                           \
      if (VMK_UNLIKELY(!(_cond_))) {                              \
         VMK_ASSERT_PANIC(_cond_, "" __VA_ARGS__);                \
         /* Ensure that we don't lose warnings in condition */    \
         if (0) { if (_cond_) {;} } (void)0;                      \
      }                                                           \
   } while(0)


/*
 ******************************************************************************
 * VMK_ASSERT --                                                         */ /**
 *
 * \brief Panics the system if a runtime expression evalutes to false
 *        only in debug builds. If optional parameters are passed
 *        those are used to print the panic message instead of the
 *        stringified version of the checked expression.
 *
 * \note  Compilation will fail if the first variadic argument is not
 *        a string constant.
 *
 * \param[in] _cond_    Expression to check.
 * \param[in] ...       Optional format string constant followed by arguments
 *                      that will form a more informative message that just
 *                      printing the stringified form of the checked
 *                      expression.
 *
 ******************************************************************************
 */
#if defined(VMX86_DEBUG)
#define VMK_ASSERT(_cond_,...) VMK_ASSERT_BUG(_cond_, ## __VA_ARGS__)
#else
#define VMK_ASSERT(_cond_,...)
#endif


/*
 ******************************************************************************
 * VMK_ASSERT_OP --                                                      */ /**
 *
 * \brief Internal helper. Asserts _a_ _op_ _b_.
 *
 * \param[in] _a_       First comparand.
 * \param[in] _b_       Second comparand.
 * \param[in] _op_      Operation to compare with.
 * \param[in] _opname_  Operation name for pretty printing.
 *
 ******************************************************************************
 */
#define VMK_ASSERT_OP(_a_, _b_, _op_, _opname_)          \
   VMK_ASSERT((_a_) _op_ (_b_),                          \
              "VMK_ASSERT_"                              \
              VMK_STRINGIFY(_opname_)                    \
              "("                                        \
              VMK_STRINGIFY(_a_)                         \
              "(0x%lx), "                                \
              VMK_STRINGIFY(_b_)                         \
              "(0x%lx))",                                \
              (vmk_uint64) (_a_), (vmk_uint64) (_b_))


/*
 ******************************************************************************
 * VMK_ASSERT_EQ --                                                      */ /**
 *
 * \brief Asserts _a_ == _b_.
 *
 * \param[in] _a_       First comparand.
 * \param[in] _b_       Second comparand.
 *
 ******************************************************************************
 */
#define VMK_ASSERT_EQ(_a_, _b_) VMK_ASSERT_OP(_a_, _b_, ==, EQ)


/*
 ******************************************************************************
 * VMK_ASSERT_NE --                                                      */ /**
 *
 * \brief Asserts _a_ != _b_.
 *
 * \param[in] _a_       First comparand.
 * \param[in] _b_       Second comparand.
 *
 ******************************************************************************
 */
#define VMK_ASSERT_NE(_a_, _b_) VMK_ASSERT_OP(_a_, _b_, !=, NE)


/*
 ******************************************************************************
 * VMK_ASSERT_GE --                                                      */ /**
 *
 * \brief Asserts _a_ >= _b_.
 *
 * \param[in] _a_       First comparand.
 * \param[in] _b_       Second comparand.
 *
 ******************************************************************************
 */
#define VMK_ASSERT_GE(_a_, _b_) VMK_ASSERT_OP(_a_, _b_, >=, GE)


/*
 ******************************************************************************
 * VMK_ASSERT_LE --                                                      */ /**
 *
 * \brief Asserts _a_ <= _b_.
 *
 * \param[in] _a_       First comparand.
 * \param[in] _b_       Second comparand.
 *
 ******************************************************************************
 */
#define VMK_ASSERT_LE(_a_, _b_) VMK_ASSERT_OP(_a_, _b_, <=, LE)


/*
 ******************************************************************************
 * VMK_ASSERT_GT --                                                      */ /**
 *
 * \brief Asserts _a_ > _b_.
 *
 * \param[in] _a_       First comparand.
 * \param[in] _b_       Second comparand.
 *
 ******************************************************************************
 */
#define VMK_ASSERT_GT(_a_, _b_) VMK_ASSERT_OP(_a_, _b_, >, GT)


/*
 ******************************************************************************
 * VMK_ASSERT_LT --                                                      */ /**
 *
 * \brief Asserts _a_ < _b_.
 *
 * \param[in] _a_       First comparand.
 * \param[in] _b_       Second comparand.
 *
 ******************************************************************************
 */
#define VMK_ASSERT_LT(_a_, _b_) VMK_ASSERT_OP(_a_, _b_, <, LT)


/*
 ******************************************************************************
 * VMK_ASSERT_NOT_NULL --                                                */ /**
 *
 * \brief Asserts _a_ != NULL.
 *
 * \param[in] _a_       Value to compare to NULL.
 *
 ******************************************************************************
 */
#define VMK_ASSERT_NOT_NULL(_a_) VMK_ASSERT((_a_) != NULL)


/*
 ******************************************************************************
 * VMK_ASSERT_NULL --                                                    */ /**
 *
 * \brief Asserts _a_ == NULL.
 *
 * \param[in] _a_       Value to compare to NULL.
 *
 ******************************************************************************
 */
#define VMK_ASSERT_NULL(_a_)                           \
   VMK_ASSERT((_a_) == NULL,                           \
              "VMK_ASSERT_NULL("                       \
              VMK_STRINGIFY(_a_)                       \
              "(%p))",                                 \
              (_a_))


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
