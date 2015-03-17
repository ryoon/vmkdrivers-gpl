/*
 * Portions Copyright 2008 VMware, Inc.
 */
/* inffast.h -- header to use inffast.c
 * Copyright (C) 1995-2003 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* WARNING: this file should *not* be used by applications. It is
   part of the implementation of the compression library and is
   subject to change. Applications should only use zlib.h.
 */

#if defined (__VMKLNX__)
void vmklnx_inflate_fast (z_streamp strm, unsigned start);
#else
void inflate_fast (z_streamp strm, unsigned start);
#endif
