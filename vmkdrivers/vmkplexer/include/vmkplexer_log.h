
/* **********************************************************
 * Copyright 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * vmkplexer_log.h --
 *
 *      Definitions of logging macros for the vmkplexer.
 *
 *      To use the logging macros here, you need to first specify the 
 *	log handle that you want to use by either creating a new one or
 *      by re-using an existing one that is created in a another .c file
 *      of the same module.
 *
 *      To create a new log handle for your module, include the following
 *      lines in the header portion of the .c file that provides the 
 *      initialization code for the module.
 *
 *         #define VMKPLXR_LOG_HANDLE <your handle name>
 *         #include "vmkplexer_log.h"
 *
 *      In the initialization section, add the following macro call
 *
 *         VMKPLXR_CREATE_LOG();
 *
 *      In the cleanup section, add the following macro call
 *
 *         VMKPLXR_DESTROY_LOG();
 *
 *      To use a handle that is created in another file, which is part of
 *      the same module of your .c file, include the following lines in
 *      your .c file,
 *
 *         #define VMKPLXR_REUSE_LOG_HANDLE <name of the handle you want>
 *         #include "vmkplexer_log.h"
 *
 *      There is no need for calling VMKPLXR_CREATE_LOG() if you are
 *      re-using an existing log handle.
 *
 *      NOTE: You can use VMKPLXR_REUSE_LOG_HANDLE to specify a log handle
 *      that is exported by a totally separated module, but you need
 *      to synchronize with the module that owns the handle to ensure 
 *      that the handle is created before being used in your code.
 *
 *	Logging macros available in here are:
 *
 *         VMKPLXR_DEBUG          - Log a message for debug build only
 *         VMKPLXR_INFO           - Log an info message
 *         VMKPLXR_THROTTLED_INFO - Log an info message with throttling control
 *         VMKPLXR_WARN           - Log a warning message
 *         VMKPLXR_WIRPID         - Log a warning if not debug build; otherwise panic
 *         VMKPLXR_THROTTLED_WARN - Log a warning message with throttling control
 *         VMKPLXR_ALERT          - Log an alert message
 *         VMKPLXR_PANIC          - Log a message and panic the system
 *         VMKPLXR_LOG_ONLY       - Specify a line of code just for debug build
 *
 *      See the macros below for their arugment lists, they are 
 *      self-explanatory.
 */

#ifndef _VMKPLEXER_LOG_H_
#define _VMKPLEXER_LOG_H_

#define INCLUDE_ALLOW_DISTRIBUTE

#include "vmkapi.h"

extern vmk_LogComponent  vmkplexerLog;

#define __XHANDLE__(name)        name ## vmkplexerLog
#define __XLOGHANDLE__(name)     __XHANDLE__(name)
#define __XLOGPROPS__(name)      name ## vmkplexerLogrops
#define __XLOGINITPROPS__(name)  __XLOGPROPS__(name)
#define __XNAME_STR__(name)      # name
#define __XLOGNAME_STR__(name)   __XNAME_STR__(name)

#if defined(VMKPLXR_LOG_HANDLE)
vmk_LogComponent __XLOGHANDLE__(VMKPLXR_LOG_HANDLE);
#elif defined(VMKPLXR_REUSE_LOG_HANDLE)
#define VMKPLXR_LOG_HANDLE VMKPLXR_REUSE_LOG_HANDLE
extern vmk_LogComponent __XLOGHANDLE__(VMKPLXR_LOG_HANDLE);
#else /* !defined(VMKPLXR_LOG_HANDLE) && !defined(VMKPLXR_REUSE_LOG) */
/*
 * A null value means use the default vmkplexer log.
 */
#define VMKPLXR_LOG_HANDLE
#endif /* defined(VMKPLXR_LOG_HANDLE) */

#define VMKPLXR_CREATE_LOG()                                                   \
   do {                                                                        \
      vmk_LogProperties __XLOGINITPROPS__(VMKPLXR_LOG_HANDLE);                 \
      if (vmk_NameInitialize(&__XLOGINITPROPS__(VMKPLXR_LOG_HANDLE).name,      \
                             __XLOGNAME_STR__(VMKPLXR_LOG_HANDLE)) != VMK_OK) {\
         VMKPLXR_WARN("Can't register log with name '%s'",                     \
                     __XLOGNAME_STR__(VMKPLXR_LOG_HANDLE));                    \
         __XLOGHANDLE__(VMKPLXR_LOG_HANDLE) = vmkplexerLog;                    \
      } else {                                                                 \
         __XLOGINITPROPS__(VMKPLXR_LOG_HANDLE).module = vmk_ModuleStackTop();  \
         __XLOGINITPROPS__(VMKPLXR_LOG_HANDLE).heap =                          \
                                    vmk_ModuleGetHeapID(                       \
                                             vmk_ModuleStackTop());            \
         __XLOGINITPROPS__(VMKPLXR_LOG_HANDLE).defaultLevel = 0;               \
         __XLOGINITPROPS__(VMKPLXR_LOG_HANDLE).throttle = NULL;                \
         if (vmk_LogRegister(&(__XLOGINITPROPS__(VMKPLXR_LOG_HANDLE)),         \
                             &(__XLOGHANDLE__(VMKPLXR_LOG_HANDLE)))            \
                             != VMK_OK) {                                      \
            VMKPLXR_WARN("Can't register log name %s",                         \
                        __XLOGNAME_STR__(VMKPLXR_LOG_HANDLE));                 \
            __XLOGHANDLE__(VMKPLXR_LOG_HANDLE) = vmkplexerLog;                 \
         }                                                                     \
      }                                                                        \
   } while(0)

#define VMKPLXR_DESTROY_LOG()                                              \
   vmk_LogUnregister(__XLOGHANDLE__(VMKPLXR_LOG_HANDLE))

#ifdef VMX86_LOG
#define VMKPLXR_LOG_ONLY(x)   x
#else
#define VMKPLXR_LOG_ONLY(x)
#endif

#ifdef VMX86_DEBUG
#define VMKPLXR_DEBUG_BUILD	1
#else
#define VMKPLXR_DEBUG_BUILD	0
#endif

static inline int                                                         \
__xCountCheckx__(vmk_uint32 count) {                                      \
   return count <     100                            ||                   \
         (count <   12800 && (count &    0x7F) == 0) ||                   \
         (count < 1024000 && (count &   0x3FF) == 0) ||                   \
                             (count & 0xEFFFF) == 0;                      \
}

/*
 * The argument "level" associates a debug level to the log message.
 *
 * To see the debug message printed in the system log, you would need
 * to change the debug level of the log handle to the level either 
 * the same level or above the level that the message is at.
 *
 * The debug level can be adjusted via a VSI node, the path of the node
 * would be:
 *    /system/modules/vmkplexer/loglevels/<VMKPLXR_LOG_HANDLE>
 */
#define VMKPLXR_DEBUG(level, fmt, args...)                                 \
   vmk_LogDebug(__XLOGHANDLE__(VMKPLXR_LOG_HANDLE), level,                 \
                fmt, ##args)

#define VMKPLXR_INFO(fmt, args...)                                         \
   vmk_Log(__XLOGHANDLE__(VMKPLXR_LOG_HANDLE), fmt, ##args)

#define VMKPLXR_THROTTLED_INFO(count, fmt, args...)                        \
   if (__xCountCheckx__(++count)) {                                       \
      vmk_Log(__XLOGHANDLE__(VMKPLXR_LOG_HANDLE),                          \
              "This message has repeated %d times: " fmt,                 \
              count, ##args);                                             \
   }

#define VMKPLXR_WARN(fmt, args...)                                         \
   vmk_Warning(__XLOGHANDLE__(VMKPLXR_LOG_HANDLE), fmt, ##args)

#define VMKPLXR_WIRPID(fmt, args...)                                       \
   if (VMKPLXR_DEBUG_BUILD) {                                              \
      vmk_Panic(fmt, ##args);                                              \
   } else {                                                                \
      vmk_Warning(__XLOGHANDLE__(VMKPLXR_LOG_HANDLE), fmt, ##args);        \
   }

#define VMKPLXR_THROTTLED_WARN(count, fmt, args...)                        \
   if (__xCountCheckx__(++count)) {                                       \
      vmk_Warning(__XLOGHANDLE__(VMKPLXR_LOG_HANDLE),                      \
                  "This message has repeated %d times: " fmt,             \
                  count, ##args);                                         \
   }

#define VMKPLXR_ALERT(fmt, args...)                                        \
   vmk_Alert(__XLOGHANDLE__(VMKPLXR_LOG_HANDLE), fmt, ##args)

#define VMKPLXR_PANIC(fmt, args...)                                        \
   vmk_Panic("vmkplexer: " fmt "\n", ##args)

#endif // _VMKPLEXER_LOG_H_


