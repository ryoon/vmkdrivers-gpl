/* **********************************************************
 * Copyright 1998 - 2010,2012 VMware, Inc.  All rights reserved.
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

/** \brief File integerity for synchronous file I/O. */
#define VMK_CHARDEV_OFLAG_SYNC         0x00101000

/** \brief Data integrity for synchronous file I/O. */
#define VMK_CHARDEV_OFLAG_DSYNC        0x00001000

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
 * \brief Opaque poll token handle.
 */
typedef void *vmk_PollToken;

/**
 * \brief Opaque poll context handle.
 */
typedef void *vmk_PollContext;

/**
 * \brief Opaque character device handle.
 */
typedef struct vmk_CharDevHandleInt *vmk_CharDevHandle;

/**
 * \brief Identifier for logical graphics devices.
 */
#define VMK_CHARDEV_IDENTIFIER_GRAPHICS "com.vmware.graphics"

/**
 * \brief Character device driver's file operations.
 */
struct vmk_CharDevOps;

/*
 ***********************************************************************
 * vmk_CharOpAssociate --                                         */ /**
 *
 * \brief Associates Char device with a device.
 *
 * Handler used by vmkernel to notify the driver that its character
 * operations are ready for use.
 *
 * \note  This callback is permitted to block
 *
 * \param[in]  devicePrivate  Points to the driver internal structure
 *                            associated with the device to be
 *                            operated on.  Before calling
 *                            vmk_DeviceRegister(), the driver must
 *                            assign this pointer to the
 *                            devicePrivate member of
 *                            vmk_CharDevRegData.
 * \param[in]  charDevHandle  Handle for character device layer
 *                            operations.
 *
 * \retval VMK_OK              Capabilities associated successfully.
 * \retval VMK_FAILURE         Capabilities association failed.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_CharOpAssociate)(vmk_AddrCookie devicePrivate,
                                                vmk_CharDevHandle charDevHandle);

/*
 ***********************************************************************
 * vmk_CharOpDisassociate --                                      */ /**
 *
 * \brief Disassociates Char device from device.
 *
 * Handler used by vmkernel to notify driver its character operations
 * are no longer being used.
 *
 * \note  This callback is permitted to block
 *
 * \param[in] devicePrivate  Points to the driver internal structure
 *                           associated with the device to be operated
 *                           on. Before calling vmk_DeviceRegister(),
 *                           the driver must assign this pointer to
 *                           the devicePrivate member of
 *                           vmk_CharDevRegData.
 *
 * \retval VMK_OK            Capabilities disassociated successfully.
 * \retval VMK_FAILURE       Capabilities disassociate failed.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_CharOpDisassociate)(vmk_AddrCookie devicePrivate);

/**
 * \brief Character device driver's operations
 */
typedef struct vmk_CharDevRegOps {
   /** \brief Associate callback. */
   vmk_CharOpAssociate         associate;
   /** \brief Disocciate callback. */
   vmk_CharOpDisassociate      disassociate;
   /** \brief File operations. */
   const struct vmk_CharDevOps *fileOps;
} vmk_CharDevRegOps;

/** \brief Character device registration data. */
typedef struct vmk_CharDevRegData {
   /** \brief Module creating this device. */
   vmk_ModuleID moduleID;
   /** \brief Device operations. */
   const struct vmk_CharDevRegOps *deviceOps;
   /** \brief Device private data. */
   vmk_AddrCookie devicePrivate;
} vmk_CharDevRegData;

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
 * \note This callback is allowed to block.
 *
 * \param[in]  attr  File-descriptor attributes for this operation.
 *
 * \retval VMK_OK   The open function completed successfully.
 * \retval Other    The call failed.
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
 * \note This callback is allowed to block.
 *
 * \param[in]  attr  File-descriptor attributes for this operation.
 *
 * \retval VMK_OK   The close function completed successfully.
 * \retval Other    The call failed.
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
 * this case, which is the usual case for third-party modules, the
 * result parameter's value indicates the integer status code that
 * should be passed back to the user process.
 * 
 * Note that the userData may be a pointer from either a 32-bit or 
 * a 64-bit process.  The callerSize parameter instructs how the 
 * ioctl callback should interpret the userData in the case it is a 
 * pointer.
 *
 * \note This callback is permitted to block.
 *
 * \param[in]     attr       File-descriptor attributes for this
 *                           operation.
 * \param[in]     cmd        Command code corresponding to the
 *                           requested operation.
 * \param[in,out] userData   Opaque data passed by the user to the 
 *                           ioctl command.  May be a user-space
 *                           pointer.
 * \param[in]     callerSize The size (VMK_IOCTL_CALLER_32 or 
 *                           VMK_IOCTL_CALLER_64) of the calling 
 *                           process.
 * \param[out]    result     The result of the ioctl command, to be
 *                           passed to the user process.  
 *
 * \retval VMK_OK   The ioctl function completed successfully.
 * \retval Other    The call failed.
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
 * \retval VMK_OK   The read function completed successfully, even if
 *                  there was no data to read.
 * \retval Other    Note that some error codes are interpreted as a
 *                  request to retry this IO.  These include any error
 *                  with RETRY in its name and VMK_WOULD_BLOCK.
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
 * A write callback must always indicate how many bytes have been
 * written, even if an error was encountered during the write.  The
 * nature of the error is then reflected in the return status.
 *
 * \note This callback is permitted to block.
 *
 * \param[in]     attr       File-descriptor attributes for this
 *                           operation.
 * \param[in]     buffer     User buffer of bytes to read from.
 * \param[in]     nbytes     Number of bytes to write.
 * \param[in]     ppos       Offset associated with this write request.
 * \param[out]    nwritten   Number of bytes written.
 *
 * \retval VMK_OK   The write function completed successfully.
 * \retval Other    Note that some error codes are interpreted as a
 *                  request to retry this IO.  These include any error
 *                  with RETRY in its name and VMK_WOULD_BLOCK.
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
 * \retval Other    The call failed.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_CharDevPollFn)(vmk_CharDevFdAttr *attr,
                                              vmk_PollContext pollCtx,
                                              unsigned *pollMask);

/**
 * \brief Character device driver's file operations.
 */
typedef struct vmk_CharDevOps {
   vmk_CharDevOpenFn   open;
   vmk_CharDevCloseFn  close;
   vmk_CharDevIoctlFn  ioctl;
   vmk_CharDevPollFn   poll;
   vmk_CharDevReadFn   read;
   vmk_CharDevWriteFn  write;
} vmk_CharDevOps;

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

/*
 ***********************************************************************
 * vmk_CharDeviceGetAlias --                                      */ /**
 *
 * \brief Gets the alias for the specified character device.
 *
 * \note  This function may block.
 * \note  The device must be in the associated state.
 *
 * \param[in]  charDevHandle   Handle of the character device layer's
 *                             logical device.
 * \param[out] alias           Name structure to fill in with alias.
 *
 * \retval VMK_OK              On success.
 * \retval VMK_BAD_PARAM       Invalid device.
 * \retval VMK_NOT_INITIALIZED No alias has been set.
 * \retval VMK_NAME_TOO_LONG   Alias is too long for a vmk_Name
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_CharDeviceGetAlias(vmk_CharDevHandle charDevHandle,
                                        vmk_Name *alias);

#endif /* _VMKAPI_CHAR_H_ */
/** @} */
