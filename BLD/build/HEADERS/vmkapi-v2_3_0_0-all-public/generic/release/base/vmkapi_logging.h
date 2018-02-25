/* **********************************************************
 * Copyright 2007 - 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Logging                                                        */ /**
 * \addtogroup Core
 * @{
 * \defgroup Logging Kernel Logging
 *
 * The logging interfaces provide a means of writing informational
 * and error messages to the kernel's logs.
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_LOGGING_H_
#define _VMKAPI_LOGGING_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include <stdarg.h>

/** \brief Opaque log component handle. */
typedef struct vmk_LogComponentInt *vmk_LogComponent;

/** \brief Log handle guaranteed to be invalid. */
#define VMK_INVALID_LOG_HANDLE NULL

/** \brief Log urgency level. */
typedef enum {
   VMK_LOG_URGENCY_DEBUG,
   VMK_LOG_URGENCY_NORMAL,
   VMK_LOG_URGENCY_WARNING,
   VMK_LOG_URGENCY_ALERT
} vmk_LogUrgency ;

/** \brief Types of log throttling */
typedef enum {
   /** Log is not throttled. All messages will be logged. */
   VMK_LOG_THROTTLE_NONE=0,

   /**
    * An internal message count will be kept and messages will
    * only be logged as the count reaches certain wider-spaced values.
    */
   VMK_LOG_THROTTLE_COUNT=1,

   /**
    * Messages will be logged depending on the return value
    * of a custom log throttling function.
    */
   VMK_LOG_THROTTLE_CUSTOM=2,
} vmk_LogThrottleType;

/*
 ***********************************************************************
 * vmk_LogThrottleFunc --                                         */ /**
 *
 * \brief Custom throttling function for a log component.
 *
 * \note  This callback is not allowed to block.
 *
 * A log throttling function will be called each time an attempt to
 * log a message to a log component is made. If this function returns
 * VMK_TRUE, the message will be logged. Otherwise, the message will
 * not be logged.
 *
 * \param[in] arg    Private data argument
 *
 * \return Whether or not the logger should log the current log message.
 * \retval VMK_TRUE     Log the current message.
 * \retval VMK_FALSE    Do not log the current message.
 *
 ***********************************************************************
 */
typedef vmk_Bool (*vmk_LogThrottleFunc)(void *arg);

/**
 * \brief Properties that define the type of throttling for
 *        a particular log component.
 */
typedef struct vmk_LogThrottleProperties {
   /** Type of log throttling to use. */
   vmk_LogThrottleType type;

   /** Properties for the specified log throttling type. */
   union {
      /** Properties for a custom log throttler. */
      struct {
         /**
          * Throttling function to call on each message submitted to the
          * log component.
          */
         vmk_LogThrottleFunc throttler;

         /**
          * Private data argument to pass to the log throttling function
          * on each call.
          */
         void *arg;
      } custom;
   } info;
} vmk_LogThrottleProperties;

/**
 * \brief Properties that define a logging component.
 */
typedef struct vmk_LogProperties {
   /** Name of the log component. */
   vmk_Name name;
   /** Module that owns the logging component. */
   vmk_ModuleID module;
   /** Heap to allocate the component and any other tracking information. */
   vmk_HeapID heap;
   /** Default log level. */
   vmk_int32 defaultLevel;
   /**
    * Throttling properties for the new component or NULL if no throttling
    * is required.
    */
   const vmk_LogThrottleProperties *throttle;
} vmk_LogProperties;

/*
 ***********************************************************************
 * vmk_StatusToString --                                          */ /**
 *
 * \brief Convert a status into a human readable text string
 *
 * \note  This function will not block.
 *
 * \note  The strings returned from this function for a particular
 *        status can change across releases regardless of binary
 *        compatibility.  Do not rely on specific string contents.
 *        These are only intended to be used for display in logging.
 *
 * \param[in] status    Return status code to convert to a
 *                      human-readable string.
 *
 * \return Human-readable string that describes the supplied status.
 *
 ***********************************************************************
 */
const char *vmk_StatusToString(
   VMK_ReturnStatus status);


/*
 ***********************************************************************
 * VMK_ASSERT_STATUS_OK --                                        */ /**
 *
 * \brief Assert macro to check status. Panics the system if _status_
 *        is not VMK_OK, logging the actual status in progress.
 *
 * \param[in] _status_    Return status code to assert on.
 *
 ***********************************************************************
 */
#define VMK_ASSERT_STATUS_OK(_status_)          \
   VMK_ASSERT(_status_ == VMK_OK,               \
              "unexpected status: %s",          \
              vmk_StatusToString(_status_));


/*
 ***********************************************************************
 * vmk_LogRegister --                                             */ /**
 *
 * \brief Register a log component
 *
 * \note  This function will not block.
 *
 * \param[in]  props          Properties for the new logging component.
 * \param[out] handle         Handle to the newly created log component.
 *
 * \retval VMK_BAD_PARAM         Bad LogThrottleType specified in
 *                               the throttle properties.
 * \retval VMK_NO_MEMORY         Not enough memory on the heap or
 *                               to create the new logging component.
 * \retval VMK_INVALID_MODULE    Supplied module ID is invalid.
 * \retval VMK_EXISTS            A log component with the same name has
 *                               already been registered.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_LogRegister(
   const vmk_LogProperties *props,
   vmk_LogComponent *handle);

/*
 ***********************************************************************
 * vmk_LogUnregister --                                           */ /**
 *
 * \brief Unregister a log component
 *
 * \note  This function will not block.
 *
 * Should be used to unregister an existing logging component
 *
 * \note Should be called before the module heap is destroyed.
 *
 * \param[in] handle       Pointer to handle for log component to be
 *                         unregistered.
 *
 * \retval VMK_NOT_FOUND   Supplied log component is invalid/unregisterd.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_LogUnregister(
   vmk_LogComponent handle);

/*
 ***********************************************************************
 * vmk_LogHeapAllocSize --                                        */ /**
 *
 * \brief Amount of heap space needed per registered log component
 *
 * \note  This function will not block.
 *
 * \retval Number of bytes to set aside in a heap per log component
 *
 ***********************************************************************
 */
vmk_ByteCount vmk_LogHeapAllocSize(void);

/*
 ***********************************************************************
 * vmk_LogGetName --                                              */ /**
 *
 * \brief Get log component name as a printable string.
 *
 * \note  This function will not block.
 *
 * \param[in] handle    Log component handle.
 *
 * \return The name associated with the supplied log component handle
 *         as a printable string.
 *
 ***********************************************************************
 */
const char * vmk_LogGetName(
   vmk_LogComponent handle);

/*
 ***********************************************************************
 * vmk_LogGetCurrentLogLevel --                                   */ /**
 *
 * \brief Get current log level of the given component
 *
 * \note  This function will not block.
 *
 * \param[in] handle Log component handle.
 *
 * \return Current log level of the given component returned.
 *
 ***********************************************************************
 */
vmk_int32 vmk_LogGetCurrentLogLevel(
   vmk_LogComponent handle);

/*
 ***********************************************************************
 * vmk_LogDebug --                                                */ /**
 *
 * \brief Log a message to a logging component on debug builds only.
 *
 * Should be used to log information messages and non-error conditions.
 *
 * Messages are logged only if the component's log level is greater
 * than or equal to the minimum log level specified.
 *
 * \printformatstringdoc
 * \note  This function will not block.
 *
 * \param[in] handle    Log component handle,
 * \param[in] min       Minimum log level required to print the message,
 * \param[in] fmt       Format string,
 * \param[in] args      List of message arguments,
 *
 ***********************************************************************
 */
#ifndef VMX86_LOG
#define vmk_LogDebug(handle, min, fmt, args...)
#else
#define vmk_LogDebug(handle, min, fmt, args...)  \
   vmk_LogLevel(VMK_LOG_URGENCY_DEBUG, handle, min, \
                "%s: %s:%d: " fmt "\n", vmk_LogGetName(handle), \
                __FUNCTION__, __LINE__, ##args)
#endif

/*
 ***********************************************************************
 * vmk_Log --                                                     */ /**
 *
 * \brief Log message to a logging component at its current log level.
 *
 * Should be used to log information messages and non-error conditions.
 * 
 * \printformatstringdoc
 * \note  This function will not block.
 *
 * \param[in] handle    Log component handle.
 * \param[in] fmt       Format string.
 * \param[in] args      List of message arguments.
 *
 ***********************************************************************
 */
#define vmk_Log(handle, fmt, args...)  \
   vmk_LogLevel(VMK_LOG_URGENCY_NORMAL, \
                handle, vmk_LogGetCurrentLogLevel(handle),   \
                "%s: %s:%d: " fmt "\n", vmk_LogGetName(handle), \
                __FUNCTION__, __LINE__, ##args)

/*
 ***********************************************************************
 * vmk_Warning --                                                 */ /**
 *
 * \brief Log a warning message to a logging component at its current
 *        log level.
 *
 * Should be used to log abnormal conditions.
 *
 * \printformatstringdoc
 * \note  This function will not block.
 *
 * \param[in] handle    Log component handle.
 * \param[in] fmt       Format string.
 * \param[in] args      List of message arguments.
 *
 ***********************************************************************
 */
#define vmk_Warning(handle, fmt, args...) \
    vmk_LogLevel(VMK_LOG_URGENCY_WARNING, \
                 handle, vmk_LogGetCurrentLogLevel(handle),   \
                 "%s: %s:%d: " fmt "\n", vmk_LogGetName(handle), \
                 __FUNCTION__, __LINE__, ##args)

/*
 ***********************************************************************
 * vmk_Alert --                                                   */ /**
 *
 * \brief Log an alert message to a logging component at its current
 *        log level.
 *
 * Should be used to notify users of system alerts.
 *
 * \printformatstringdoc
 * \note  This function will not block.
 *
 * \param[in] handle    Log component handle.
 * \param[in] fmt       Format string.
 * \param[in] args      List of message arguments.
 *
 ***********************************************************************
 */
#define vmk_Alert(handle, fmt, args...) \
    vmk_LogLevel(VMK_LOG_URGENCY_ALERT, \
                 handle, vmk_LogGetCurrentLogLevel(handle),   \
                 "%s: %s:%d: " fmt "\n", vmk_LogGetName(handle), \
                 __FUNCTION__, __LINE__, ##args)

/*
 ***********************************************************************
 * vmk_LogDebugMessage --                                         */ /**
 *
 * \brief Log an information message to the vmkernel log unconditionally
 *        on debug builds only.
 *
 * Should be used to log information messages and non-error conditions
 * when no log component is available.
 * 
 * \printformatstringdoc
 * \note  This function will not block.
 *
 * \param[in] fmt       Format string.
 * \param[in] args      List of message arguments.
 *
 ***********************************************************************
 */
#ifndef VMX86_LOG
#define vmk_LogDebugMessage(fmt, args...)
#else
#define vmk_LogDebugMessage(fmt, args...)  \
   vmk_LogNoLevel(VMK_LOG_URGENCY_NORMAL, fmt "\n", ##args)
#endif

/*
 ***********************************************************************
 * vmk_LogMessage --                                              */ /**
 *
 * \brief Log an information message to the vmkernel log unconditionally.
 *
 * Should be used to log information messages and non-error conditions
 * when no log component is available.
 *
 * \printformatstringdoc
 * \note  This function will not block.
 *
 * \param[in] fmt       Format string.
 * \param[in] args      List of message arguments.
 *
 ***********************************************************************
 */
#define vmk_LogMessage(fmt, args...) \
    vmk_LogNoLevel(VMK_LOG_URGENCY_NORMAL, fmt "\n", ##args)

/*
 ***********************************************************************
 * vmk_WarningMessage --                                          */ /**
 *
 * \brief Log a warning or error message to the vmkernel log
 *        unconditionally.
 *
 * Should be used to log abnormal conditions when no log component is
 * available.
 *
 * \printformatstringdoc
 * \note  This function will not block.
 *
 * \param[in] fmt       Format string.
 * \param[in] args      List of message arguments.
 *
 ***********************************************************************
 */
#define vmk_WarningMessage(fmt, args...) \
    vmk_LogNoLevel(VMK_LOG_URGENCY_WARNING, fmt "\n", ##args)


/*
 ***********************************************************************
 * vmk_AlertMessage --                                            */ /**
 *
 * \brief Log a system alert to the vmkernel log and the console
 *        unconditionally.
 *
 * Should be used to log severe problems when no log component is
 * available.
 *
 * \printformatstringdoc
 * \note  This function will not block.
 *
 * \param[in] fmt       Format string.
 * \param[in] args      List of message arguments.
 *
 ***********************************************************************
 */
#define vmk_AlertMessage(fmt, args...) \
    vmk_LogNoLevel(VMK_LOG_URGENCY_ALERT, fmt "\n", ##args)

/*
 ***********************************************************************
 * vmk_LogSetCurrentLogLevel --                                   */ /**
 *
 * \brief Set current log level of a given log component
 *
 * \note  This function will not block.
 *
 * \param[in] handle    Log component handle to modify.
 * \param[in] level     Log level to set component to.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_LogSetCurrentLogLevel(
   vmk_LogComponent handle,
   vmk_int32 level);

/*
 ***********************************************************************
 * vmk_vLogLevel --                                               */ /**
 *
 * \brief Log a message using a log component
 *
 * \printformatstringdoc
 * \note  This function will not block.
 *
 * Output a log message to the vmkernel log if the current log level
 * on the given log component is equal to or greater than the given
 * log level.
 *
 * \param[in] urgency   How urgent the message is.
 * \param[in] handle    Log component handle.
 * \param[in] level     Minimum log level the component must be set to
 *                      in order to print the message.
 * \param[in] fmt       Format string.
 * \param[in] ap        List of message arguments.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_vLogLevel(
   vmk_LogUrgency urgency,
   vmk_LogComponent handle,
   vmk_int32 level,
   const char *fmt,
   va_list ap);

/*
 ***********************************************************************
 * vmk_LogLevel --                                                */ /**
 *
 * \brief Log a message using a log component
 *
 * \printformatstringdoc
 * \note  This function will not block.
 *
 * Output a log message to the vmkernel log if the current log level
 * on the given log component is equal to or greater than the given
 * log level.
 *
 *
 * \param[in] urgency   How urgent the message is.
 * \param[in] handle    Log component handle.
 * \param[in] level     Minimum log level the component must be set to
 *                      in order to print the message.
 * \param[in] fmt       Format string.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_LogLevel(
   vmk_LogUrgency urgency,
   vmk_LogComponent handle,
   vmk_int32 level,
   const char *fmt,
   ...)
VMK_ATTRIBUTE_PRINTF(4,5);

/*
 ***********************************************************************
 * vmk_vLogNoLevel --                                             */ /**
 *
 * \brief Log an information message to the vmkernel log
 *        unconditionally with a va_list.
 *
 * \printformatstringdoc
 * \note  This function will not block.
 *
 * Should be used to log information messages and non-error conditions
 * when no log component is available.
 *
 * \param[in] urgency   How urgent the message is.
 * \param[in] fmt       Format string.
 * \param[in] ap        List of message arguments.
 *
 ***********************************************************************
 */
VMK_ReturnStatus  vmk_vLogNoLevel(
   vmk_LogUrgency urgency,
   const char *fmt,
   va_list ap);

/*
 ***********************************************************************
 * vmk_LogNoLevel --                                              */ /**
 *
 * \brief Log an information message to the vmkernel log
 *        unconditionally with variable arguments.
 *
 * \printformatstringdoc
 * \note  This function will not block.
 *
 * Should be used to log information messages and non-error conditions
 * when no log component is available.
 *
 *
 * \param[in] urgency   How urgent the message is.
 * \param[in] fmt       Format string.
 *
 ***********************************************************************
 */
VMK_ReturnStatus  vmk_LogNoLevel(
   vmk_LogUrgency urgency,
   const char *fmt,
   ...)
VMK_ATTRIBUTE_PRINTF(2,3);


/*
 ***********************************************************************
 * vmk_LogFindLogComponentByName --                               */ /**
 *
 * \brief Get a log component by the given log component name and
 *        the module ID.
 *
 * \note  This function will not block.
 *
 * \param[in]  id       Module ID that registered the log component
 * \param[in]  name     Log component name
 * \param[out] handle   Returns log component handle of the specified
 *                      name and the module ID.
 *
 * \retval VMK_NOT_FOUND  The given log component name does not exist.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_LogFindLogComponentByName(
   vmk_ModuleID id,
   const vmk_Name *name,
   vmk_LogComponent *handle);

/*
 ***********************************************************************
 * vmk_LogBacktrace --                                            */ /**
 *
 * \brief Write the current stack backtrace to a log component.
 *
 * \note  This function will not block.
 *
 * This routine logs at the component's current logging level and
 * at NORMAL urgency.
 *
 * \param[in] handle   Log component to write the backtrace to.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_LogBacktrace(
   vmk_LogComponent handle);

/*
 ***********************************************************************
 * vmk_LogBacktraceMessage --                                     */ /**
 *
 * \brief Write the current stack backtrace to the vmkernel log.
 *
 * \note  This function will not block.
 *
 * Should be used to log the backtrace when no logging component
 * is available.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_LogBacktraceMessage(void);

#endif /* _VMKAPI_LOGGING_H_ */
/** @} */
/** @} */
