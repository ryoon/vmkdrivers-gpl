/* **********************************************************
 * Copyright 1998 - 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Character Devices                                              */ /**
 * \defgroup CharDev Character Devices
 *
 * Interfaces that allow management of vmkernel's UNIX-like character
 * device nodes.
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_CHAR_H_
#define _VMKAPI_CHAR_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/*
 * UNIX-style open flags supported for character devices
 */
/** \brief Read-only. */
#define VMK_CHARDEV_OFLAG_RDONLY       0x00000000

/** \brief Write-only. */
#define VMK_CHARDEV_OFLAG_WRONLY       0x00000001

/** \brief Read-write. */
#define VMK_CHARDEV_OFLAG_RDWR         0x00000002

/** \brief Mask for read/write flags. */
#define VMK_CHARDEV_OFLAG_RDWR_MASK    0x00000003

/** \brief Exclusive access. */
#define VMK_CHARDEV_OFLAG_EXCLUSIVE    0x00000080

/** \brief Append to end of file.  Always set for writes */
#define VMK_CHARDEV_OFLAG_APPEND       0x00000400

/** \brief Don't block for file operations. */
#define VMK_CHARDEV_OFLAG_NONBLOCK     0x00000800

/** \brief Synchronous file operations. */
#define VMK_CHARDEV_OFLAG_SYNC         0x00001000

/** \brief Use direct I/O. */
#define VMK_CHARDEV_OFLAG_DIRECT       0x00004000

/** \brief Flags for poll entry point */
typedef enum vmk_PollEvent {
   /** \brief No events are available */
   VMKAPI_POLL_NONE    = 0x00,
   /** \brief The device is ready for reading */
   VMKAPI_POLL_READ    = 0x01,
   /** \brief The device is ready for writing */
   VMKAPI_POLL_WRITE   = 0x04,
   /** \brief The file was closed during polling for read status */
   VMKAPI_POLL_RDHUP   = 0x08,
   /** \brief The file was closed during polling for write status */
   VMKAPI_POLL_WRHUP   = 0x10,
   /** \brief The file is no longer valid */
   VMKAPI_POLL_INVALID = 0x20,
} vmk_PollEvent;


/**
 * \brief Character device's file descriptor's attibutes.
 */
typedef struct vmk_CharDevFdAttr {
   /**
    * \brief UNIX-style file flags used when opening the device
    * from the host.
    */
   vmk_uint32	openFlags;
   
   /**
    * \brief Client data associated with the device.
    *
    * This is device-specific data provided by the driver during 
    * registration.
    */
   vmk_AddrCookie clientDeviceData;
                                   
   /**
    * \brief Client data associated with the file descriptor.
    *
    * May be used by the character driver to store information
    * persistent across syscalls
    *
    * The field can be updated by the driver at any time during
    * a syscall.
    */
   vmk_AddrCookie clientInstanceData; 
                       /* For use by the character device driver */
                       /* across open/ioctl/close calls */
} vmk_CharDevFdAttr;

/**
 * \brief Opaque handle to a character device.
 */
typedef struct vmkCharDevInt* vmk_CharDev;

/**
 * \brief A default initialization value for a vmk_CharDev.
 */
#define VMK_INVALID_CHARDEV (NULL)

/**
 * \brief Opaque poll token handle.
 */
typedef void *vmk_PollToken;

/**
 * \brief Opaque poll context handle.
 */
typedef void *vmk_PollContext;

/**
 ***********************************************************************
 * vmk_CharDevOpenFn --                                           */ /**
 *
 * \brief A character device driver's open callback.
 * 
 * If the open operation indicates success, the reference count for
 * the driver's module will be incremented, which will prevent the
 * module from being unloaded until the file is closed.  It is 
 * guaranteed that the driver's corresponding close callback will
 * be invoked for this attr object in the future if the open call
 * was successful (either when the user-space application closes the
 * file or when the application itself is closed).
 * If the open operation fails, no further operations for this attr 
 * object will be issued.
 *
 * \note This callback is permitted to block.
 *
 * \param[in]  attr  File-descriptor attributes for this operation.
 *
 * \retval VMK_OK   The open function completed successfully.
 * \retval Other    The call failed and the error will be passed to 
 *                  the user.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_CharDevOpenFn)(vmk_CharDevFdAttr *attr);

/**
 ***********************************************************************
 * vmk_CharDevCloseFn --                                          */ /**
 *
 * \brief A character device driver's close callback.
 * 
 * If the driver's close operation fails, it will not be retried.  After 
 * executing the close callback, the driver module's reference count is 
 * decremented, which may make the module eligible for unloading (if no 
 * other outstanding references exist).
 *
 * \note This callback is permitted to block.
 *
 * \param[in]  attr  File-descriptor attributes for this operation.
 *
 * \retval VMK_OK   The close function completed successfully.
 * \retval Other    The call failed and the error will be passed to 
 *                  the user.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_CharDevCloseFn)(vmk_CharDevFdAttr *attr);

/**
 ***********************************************************************
 * vmk_CharDevIoctlFn --                                          */ /**
 *
 * \brief A character device driver's ioctl callback.
 * 
 * An ioctl callback may, depending on the convention of the ioctl
 * command, return two status codes.  One is the return value of the
 * callback, and the other is returned via the result parameter.
 * For non-VMware (driver-specific) ioctl commands, the return 
 * value only indicates whether or not the callback executed.  In 
 * this case, which is the usual case for third-party modules, the result 
 * parameter's value indicates the integer status code that
 * should be passed back to the user process.
 * 
 * Note that the userData may be a pointer from either a 32-bit or 
 * a 64-bit process.  The callerSize parameter instructs how the 
 * ioctl callback should interpret the userData in the case it is a 
 * pointer.
 *
 * \note This callback is permitted to block.
 *
 * \param[in]     attr       File-descriptor attributes for this operation.
 * \param[in]     cmd        Command code corresponding to the requested 
 *                           operation.
 * \param[in,out] userData   Opaque data passed by the user to the 
 *                           ioctl command.  May be a user-space pointer.
 * \param[in]     callerSize The size (VMK_IOCTL_CALLER_32 or 
 *                           VMK_IOCTL_CALLER_64) of the calling 
 *                           process.
 * \param[out]    result     The result of the ioctl command, to be 
 *                           passed to the user process.  
 *
 * \retval VMK_OK   The ioctl function completed successfully.
 * \retval Other    The call failed and the error will be passed to 
 *                  the user.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_CharDevIoctlFn)(vmk_CharDevFdAttr *attr,
                                               unsigned int cmd,
                                               vmk_uintptr_t userData,
                                               vmk_IoctlCallerSize callerSize,
                                               vmk_int32 *result);

/**
 ***********************************************************************
 * vmk_CharDevReadFn --                                           */ /**
 *
 * \brief A character device driver's read callback.
 * 
 * The buffer for a read callback is a user-space buffer and therefore
 * must be written to indirectly using vmk_CopyToUser.
 * A read callback must always indicate how many bytes have been read,
 * even if an error was encountered during the read.  The nature of the 
 * error is then reflected in the return status.
 *
 * \note This callback is permitted to block.
 *
 * \param[in]     attr       File-descriptor attributes for this operation.
 * \param[out]    buffer     User buffer of bytes to write to.
 * \param[in]     nbytes     Number of bytes to read.
 * \param[in]     ppos       Offset associated with this read request.
 * \param[out]    nread      Number of bytes read.
 *
 * \retval VMK_OK   The read function completed successfully.
 * \retval Other    The call failed and the error will be passed to 
 *                  the user.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_CharDevReadFn)(vmk_CharDevFdAttr *attr,
                                              char *buffer,
                                              vmk_ByteCount nbytes,
                                              vmk_loff_t *ppos,
                                              vmk_ByteCountSigned *nread);

/**
 ***********************************************************************
 * vmk_CharDevWriteFn --                                          */ /**
 *
 * \brief A character device driver's write callback.
 *
 * The buffer for a write callback is a user-space buffer and therefore
 * must be read from indirectly using vmk_CopyFromUser.
 * A write callback must always indicate how many bytes have been written,
 * even if an error was encountered during the write.  The nature of the
 * error is then reflected in the return status.
 *
 * \note This callback is permitted to block.
 *
 * \param[in]     attr       File-descriptor attributes for this operation.
 * \param[in]     buffer     User buffer of bytes to read from.
 * \param[in]     nbytes     Number of bytes to write.
 * \param[in]     ppos       Offset associated with this write request.
 * \param[out]    nwritten   Number of bytes written.
 *
 * \retval VMK_OK   The write function completed successfully.
 * \retval Other    The call failed and the error will be passed to 
 *                  the user.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_CharDevWriteFn)(vmk_CharDevFdAttr *attr,
                                               char *buffer,
                                               vmk_ByteCount nbytes,
                                               vmk_loff_t *ppos,
                                               vmk_ByteCountSigned *nwritten);

/**
 ***********************************************************************
 * vmk_CharDevPollFn --                                           */ /**
 *
 * \brief A character device driver's poll callback.
 * 
 * \note This callback is permitted to block.
 * 
 * \param[in]     attr       File-descriptor attributes for this operation.
 * \param[in]     pollCtx    Poll context of the calling thread.  This 
 *                           is an opaque handle supplied by the kernel.
 *                           The driver must associate a globally unique 
 *                           token (such as a pointer) of its choosing 
 *                           with this poll context using 
 *                           vmk_CharDevSetPollContext.  Poll wakeups 
 *                           are thus performed, later, using
 *                           vmk_CharDevWakePollers by supplying that 
 *                           driver-chosen token.
 * \param[out]    pollMask   Bitmask of vmk_PollEvent indicators 
 *                           reflecting the current status of the 
 *                           character device.
 *
 * \retval VMK_OK   The poll function completed successfully.
 * \retval Other    The call failed and the error will be passed to 
 *                  the user.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_CharDevPollFn)(vmk_CharDevFdAttr *attr,
                                              vmk_PollContext pollCtx,
                                              unsigned *pollMask);

/**
 * \brief Character device driver's entry points
 */
typedef struct vmk_CharDevOps {
   vmk_CharDevOpenFn   open;
   vmk_CharDevCloseFn  close;
   vmk_CharDevIoctlFn  ioctl;
   vmk_CharDevPollFn   poll;
   vmk_CharDevReadFn   read;
   vmk_CharDevWriteFn  write;
} vmk_CharDevOps;

/**
 ***********************************************************************
 * vmk_CharDevCleanupFn --                                        */ /**
 *
 * \brief Prototype for a character device driver's cleanup callback.
 *
 * \param[in]  private  Optional private data to be used by the callback
 *
 * \retval VMK_OK The cleanup function executed correctly.
 *                This is not an indicator of the success or failure of
 *                the operations in the function, but merely that they
 *                ran.  Any other return value is not allowed.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_CharDevCleanupFn)(vmk_AddrCookie private);

/*
 ***********************************************************************
 * vmk_CharDevRegister --                                         */ /**
 *
 * \brief Register the specified character device, to be invoked from
 *        user-space.
 *
 * \param[in]  module         Module that owns the character device.
 * \param[in]  name           The name of the device - this must be unique.
 * \param[in]  fileOps        Table of the driver file operations.
 *                            Neither open nor close can be supplied 
 *                            without the other.
 *                            If read or write operations are supplied, 
 *                            then open and close must also be supplied.
 * \param[in]  cleanup        Function automatically invoked to clean up
 *                            after all file ops have ceased and the 
 *                            device has been unregistered.  May be NULL.
 * \param[in]  devicePrivate  Data given to the driver for each file 
 *                            op and cleaned up after unregistration.
 * \param[out] assignedHandle Handle to the registered character device.
 *
 * \retval VMK_BUSY           A device with that name is already registered
 * \retval VMK_NO_RESOURCES   Unable to allocate internal slot for the device.
 * \retval VMK_BAD_PARAM      Module ID was invalid, or one or more
 *                            specified driver ops are NULL.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_CharDevRegister(
   vmk_ModuleID module,
   const char *name,
   const vmk_CharDevOps *fileOps,
   vmk_CharDevCleanupFn cleanup,
   vmk_AddrCookie devicePrivate,
   vmk_CharDev *assignedHandle);

/*
 ***********************************************************************
 * vmk_CharDevUnregister --                                       */ /**
 *
 * \brief Unregister a character device.
 *
 * The character device will be unregistered automatically by
 * the kernel only after all open files to the device have been
 * closed.  If no files are open when vmk_CharDevUnregister is
 * called, the device may be unregistered immediately and have the
 * cleanup function registered with it invoked.  If the device has 
 * files open, vmk_CharDevUnregister internally defers the device for 
 * later automatic removal and returns to the caller immediately.  When 
 * the last file is closed, the device will then be destroyed and the 
 * cleanup function invoked.
 * 
 * \note No new open files to the device can be created after calling
 *       vmk_CharDevUnregister.
 * \note The vmkernel will prevent a module from being unloaded while
 *       it has open files associated with a character device, even
 *       if that device has been requested to be unregistered.
 *
 * \param[in] deviceHandle Handle of device assigned during registration.
 *
 * \retval VMK_NOT_FOUND The device does not exist.
 * \retval VMK_OK The device was either unregistered or internally
 *                deferred for unregistration once all associated files
 *                close.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_CharDevUnregister(vmk_CharDev deviceHandle);

/*
 ***********************************************************************
 * vmk_CharDevWakePollers --                                      */ /**
 *
 * \brief Wake up all users waiting on a poll call with the specified
 *        token.
 *
 * \param[in] token  Context on which worlds are waiting.
 *
 ***********************************************************************
 */
void vmk_CharDevWakePollers(vmk_PollToken token);

/*
 ***********************************************************************
 * vmk_CharDevSetPollContext --                                   */ /**
 *
 * \brief Set the poll context of the calling world to the specified
 *        context.
 *
 * \param[in]  pollCtx  The poll context of the calling thread.
 * \param[out] token    The token to set in the poll context.
 *
 ***********************************************************************
 */
void vmk_CharDevSetPollContext(vmk_PollContext pollCtx, vmk_PollToken *token);

#endif /* _VMKAPI_CHAR_H_ */
/** @} */
