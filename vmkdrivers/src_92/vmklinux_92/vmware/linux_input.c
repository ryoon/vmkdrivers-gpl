/* **********************************************************
 * Copyright 2007-2008 VMware, Inc.  All rights reserved.
 * **********************************************************/

#include <linux/kbd_kern.h>
#include "vmkapi.h"

/*
 *  This weirdness is necessary to match up with the VMkernel
 *  which maps function keys and alt-function keys to high values
 *  instead of key sequences.  Also, just to be fun, Linux numbers
 *  from 0 whereas VMkernel (and FreeBSD) number from 1, so "+ 1".
 *
 *  Also, either ALT key but _NOT_ both plus a FUNCTION key will
 *  cause a console switch.  This is better than Linux, which only
 *  does a console switch with the left ALT key plus a FUNCTION key.
 */

#define KEYBOARD_INVALID_VALUE	(u_char)255

u_char vmklnx_map_key(u_char value, u_char keytype)
{
   u_char map_value = KEYBOARD_INVALID_VALUE;
	
   /* function keys */
   if (keytype == KT_FN) {
      switch (value) {
         case KVAL(K_FIND):
            map_value = VMK_INPUT_KEY_HOME;
            break;
         case KVAL(K_PGUP):
            map_value = VMK_INPUT_KEY_PAGEUP;
            break;
         case KVAL(K_SELECT):
            map_value = VMK_INPUT_KEY_END;
            break;
         case KVAL(K_PGDN):
            map_value = VMK_INPUT_KEY_PAGEDOWN;
            break;
         case KVAL(K_INSERT):
            map_value = VMK_INPUT_KEY_INSERT;
            break;
         case KVAL(K_REMOVE):
            map_value = VMK_INPUT_KEY_DELETE;
            break;
         default:
            if ((char)value >= KVAL(K_F1) && value <= KVAL(K_F12)) {
               map_value = value - K_F1 + VMK_INPUT_KEY_F1;
            } else {
               vmk_InputPutsQueue(func_table[value]);
            }
            break;
         }
      }

      /* cursor */
      if (keytype == KT_CUR) {
         switch (value) {
            case KVAL(K_UP):
               map_value = VMK_INPUT_KEY_UP;
               break;
            case KVAL(K_LEFT):
               map_value = VMK_INPUT_KEY_LEFT;
               break;
            case KVAL(K_RIGHT):
               map_value = VMK_INPUT_KEY_RIGHT;
               break;
            case KVAL(K_DOWN):
		map_value = VMK_INPUT_KEY_DOWN;
		break;
	     default:
		printk("Unknown cursor key\n");
		break;
         }
   }

   /* console */
   if (keytype == KT_CONS) {
      if ((char)value >= KVAL(K_F1) && value <= KVAL(K_F12)) {
         map_value = value - K_F1 + VMK_INPUT_KEY_ALT_F1;
      }
   }
	
   if (map_value != KEYBOARD_INVALID_VALUE) {
      vmk_InputPutQueue(map_value);
   }

   return map_value;
}

void set_console(int value)
{
   /* need this for right alt+fn */
   if (value >= 12) {
      value -= 12;
   }

   if (vmklnx_map_key(value, KT_CONS) == KEYBOARD_INVALID_VALUE) {
      printk("Unable to switch to console %d\n", (value + 1));
   }
}
