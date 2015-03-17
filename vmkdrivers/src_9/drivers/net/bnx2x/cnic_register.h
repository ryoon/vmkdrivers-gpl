/* cnic_register.h: QLogic CNIC Registration Agent
 *
 * Copyright (c) 2010-2014 QLogic Corporation
 *
 * Portions Copyright (c) VMware, Inc. 2010-2014, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v.2 as published by
 * the Free Software Foundation.
 *
 * Written by: 
 */


#ifndef CNIC_REGISTER_H
#define CNIC_REGISTER_H

#define CNIC_REGISTER_MODULE_VERSION	"1.78.75.v60.7"
#define CNIC_REGISTER_MODULE_RELDATE	"May 21, 2014"
extern int cnic_register_adapter(const char * name, void *callback);
extern void *cnic_register_get_callback(const char * name);
extern void cnic_register_cancel(const char * name);
extern int cnic_register_get_table_size(void);
extern int cnic_register_get_callback_by_index(int index, char * name, void **callback);
extern void cnic_register_release_all_callbacks();

#endif /* CNIC_REGISTER_H */
