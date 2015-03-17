/* **********************************************************
 * Copyright 2008 - 2009, 2013 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Input                                                          */ /**
 * \addtogroup Device
 * @{
 * \defgroup Input Human Input Device Interfaces
 *
 * Interfaces that allow to enqueue keyboard character(s) to vmkernel 
 * and forward input events to the host.
 * @{ 
 *
 ***********************************************************************
 */

#ifndef _VMKAPI_INPUT_H_
#define _VMKAPI_INPUT_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief Values for special keys (beyond normal ASCII codes).
 */
enum {
   VMK_INPUT_KEY_F1 = (vmk_int8) 0x81,
   VMK_INPUT_KEY_F2,
   VMK_INPUT_KEY_F3,
   VMK_INPUT_KEY_F4,
   VMK_INPUT_KEY_F5,
   VMK_INPUT_KEY_F6,
   VMK_INPUT_KEY_F7,
   VMK_INPUT_KEY_F8,
   VMK_INPUT_KEY_F9,
   VMK_INPUT_KEY_F10,
   VMK_INPUT_KEY_F11,
   VMK_INPUT_KEY_F12,
   VMK_INPUT_KEY_SHIFT_F1,
   VMK_INPUT_KEY_SHIFT_F2,
   VMK_INPUT_KEY_SHIFT_F3,
   VMK_INPUT_KEY_SHIFT_F4,
   VMK_INPUT_KEY_SHIFT_F5,
   VMK_INPUT_KEY_SHIFT_F6,
   VMK_INPUT_KEY_SHIFT_F7,
   VMK_INPUT_KEY_SHIFT_F8,
   VMK_INPUT_KEY_SHIFT_F9,
   VMK_INPUT_KEY_SHIFT_F10,
   VMK_INPUT_KEY_SHIFT_F11,
   VMK_INPUT_KEY_SHIFT_F12,
   VMK_INPUT_KEY_CTRL_F1,
   VMK_INPUT_KEY_CTRL_F2,
   VMK_INPUT_KEY_CTRL_F3,
   VMK_INPUT_KEY_CTRL_F4,
   VMK_INPUT_KEY_CTRL_F5,
   VMK_INPUT_KEY_CTRL_F6,
   VMK_INPUT_KEY_CTRL_F7,
   VMK_INPUT_KEY_CTRL_F8,
   VMK_INPUT_KEY_CTRL_F9,
   VMK_INPUT_KEY_CTRL_F10,
   VMK_INPUT_KEY_CTRL_F11,
   VMK_INPUT_KEY_CTRL_F12,
   VMK_INPUT_KEY_CTRLSHIFT_F1,
   VMK_INPUT_KEY_CTRLSHIFT_F2,
   VMK_INPUT_KEY_CTRLSHIFT_F3,
   VMK_INPUT_KEY_CTRLSHIFT_F4,
   VMK_INPUT_KEY_CTRLSHIFT_F5,
   VMK_INPUT_KEY_CTRLSHIFT_F6,
   VMK_INPUT_KEY_CTRLSHIFT_F7,
   VMK_INPUT_KEY_CTRLSHIFT_F8,
   VMK_INPUT_KEY_CTRLSHIFT_F9,
   VMK_INPUT_KEY_CTRLSHIFT_F10,
   VMK_INPUT_KEY_CTRLSHIFT_F11,
   VMK_INPUT_KEY_CTRLSHIFT_F12,
   VMK_INPUT_KEY_HOME,
   VMK_INPUT_KEY_UP,
   VMK_INPUT_KEY_PAGEUP,
   VMK_INPUT_KEY_NUMMINUS,
   VMK_INPUT_KEY_LEFT,
   VMK_INPUT_KEY_CENTER,
   VMK_INPUT_KEY_RIGHT,
   VMK_INPUT_KEY_NUMPLUS,
   VMK_INPUT_KEY_END,
   VMK_INPUT_KEY_DOWN,
   VMK_INPUT_KEY_PAGEDOWN,
   VMK_INPUT_KEY_INSERT,
   VMK_INPUT_KEY_DELETE,
   VMK_INPUT_KEY_UNUSED1,
   VMK_INPUT_KEY_UNUSED2,
   VMK_INPUT_KEY_UNUSED3,
   VMK_INPUT_KEY_ALT_F1,
   VMK_INPUT_KEY_ALT_F2,
   VMK_INPUT_KEY_ALT_F3,
   VMK_INPUT_KEY_ALT_F4,
   VMK_INPUT_KEY_ALT_F5,
   VMK_INPUT_KEY_ALT_F6,
   VMK_INPUT_KEY_ALT_F7,
   VMK_INPUT_KEY_ALT_F8,
   VMK_INPUT_KEY_ALT_F9,
   VMK_INPUT_KEY_ALT_F10,
   VMK_INPUT_KEY_ALT_F11,
   VMK_INPUT_KEY_ALT_F12,
};

/**
 * \brief Keyboard scancode mapping modes.
 */
typedef enum vmk_KeyboardKeymapMode {
   VMK_KEYMAP_MODE_INVALID=0,
   VMK_KEYMAP_MODE_XLATE=1,
   VMK_KEYMAP_MODE_MEDIUMRAW=2,
   VMK_KEYMAP_MODE_RAW=3,
   VMK_KEYMAP_MODE_UNICODE=4,
} vmk_KeyboardKeymapMode;

/**
 * \brief Keyboard driver type identifiers.
 */
typedef enum vmk_KeyboardDriverType {
   VMK_KEYBOARD_DRIVER_TYPE_INVALID=0,
   VMK_KEYBOARD_DRIVER_TYPE_USB=1,
} vmk_KeyboardDriverType;

/**
 * \brief Opaque handle for a keyboard interrupt handler.
 */
typedef void *vmk_KeyboardInterruptHandle;

/**
 * \brief Opaque handle for unregistering keyboard driver.
 */
typedef struct vmk_KeyboardDriverHandleInt *vmk_KeyboardDriverHandle;

/**
 * \brief Attributes struct for keyboard driver
 */
typedef struct vmk_KeyboardDriverAttributes {
   vmk_KeyboardDriverType driverType;
   int (*SetKeymapMode)(vmk_KeyboardKeymapMode mode);
   vmk_KeyboardKeymapMode (*GetKeymapMode)(void);
} vmk_KeyboardDriverAttributes;

/*
 ***********************************************************************
 * vmk_InputPutQueue         --                                   */ /**
 *
 * \ingroup Input
 * \brief Enqueue a keyboard character to vmkernel.
 *
 *        Does nothing if vmkernel is not the audience.
 *
 * \param[in]  ch    Input character (ASCII or special).
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_InputPutQueue(int ch);

/*
 ***********************************************************************
 * vmk_InputPutsQueue --                                          */ /**
 *
 * \ingroup Input
 * \brief Enqueue multiple keyboard characters to vmkernel.
 *
 *        Does nothing if vmkernel is not the audience.
 *
 * \param[in]  cp    Input characters (ASCII or special).
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_InputPutsQueue(char *cp);

/*
 ***********************************************************************
 * vmk_InputInterruptHandler --                                   */ /**
 *
 * \ingroup Input
 * \brief Interrupt handler pointer type provided to VMkernel
 *
 * \param[in] irq          Source specific IRQ info.
 * \param[in] context      Source specific context info.
 * \param[in] registers    Source specific register state.
 *
 ***********************************************************************
 */
typedef void (*vmk_InputInterruptHandler)(int irq,
                                          void *context,
                                          void *registers);

/*
 ***********************************************************************
 * vmk_RegisterInputKeyboardInterruptHandler --                   */ /**
 *
 * \ingroup Input
 * \brief Register an interrupt handler for polling an external
 *        keyboard.
 *
 * \note This function \em must be called at module load time.
 *
 * \param[in]  handler     Interrupt handler.
 * \param[in]  irq         Interrupt vector.
 * \param[in]  context     Context info.
 * \param[in]  registers   Register or other state.
 * \param[out] handle      Handle for registered keyboard interrupt
 *                         handler.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_RegisterInputKeyboardInterruptHandler(vmk_InputInterruptHandler *handler,
                                          vmk_uint32 irq,
                                          void *context,
                                          void *registers,
                                          vmk_KeyboardInterruptHandle *handle);

/*
 ***********************************************************************
 * vmk_UnregisterInputKeyboardInterruptHandler --                 */ /**
 *
 * \ingroup Input
 * \brief Unregister a keyboard interrupt handler.
 *
 * \param[in] handle    Handle for registered keyboard interrupt
 *                      handler.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_UnregisterInputKeyboardInterruptHandler(vmk_KeyboardInterruptHandle handle);

/*
 ***********************************************************************
 * vmk_RegisterKeyboardDriver --                                  */ /**
 *
 * \ingroup Input
 * \brief Register a keyboard driver.
 *
 * \param[in]  attributes Attributes for registered keyboard driver.
 * \param[out] handlePtr  Handle pointer for registered keyboard driver.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_RegisterKeyboardDriver(vmk_KeyboardDriverAttributes *attributes, 
                           vmk_KeyboardDriverHandle *handlePtr);

/*
 ***********************************************************************
 * vmk_UnregisterKeyboardDriver --                                */ /**
 *
 * \ingroup Input
 * \brief Unregister a keyboard driver.
 *
 * \param[in] handle Handle for registered keyboard driver.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_UnregisterKeyboardDriver(vmk_KeyboardDriverHandle handle);

#endif
/** @} */
/** @} */
