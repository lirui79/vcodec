/* ***** BEGIN LICENSE BLOCK *****
* Source last modified: : stream_hdr_utils.h,v 1.1 2009-03-11 14:10:07 kima Exp $
*
* REALNETWORKS CONFIDENTIAL--NOT FOR DISTRIBUTION IN SOURCE CODE FORM
* Portions Copyright (c) 1995-2005 RealNetworks, Inc.
* (C) COPYRIGHT 2008 VeriSilicon Inc.
* All Rights Reserved.
*
* The contents of this file, and the files included with this file,
* are subject to the current version of the Real Format Source Code
* Porting and Optimization License, available at
* https://helixcommunity.org/2005/license/realformatsource (unless
* RealNetworks otherwise expressly agrees in writing that you are
* subject to a different license).  You may also obtain the license
* terms directly from RealNetworks.  You may not use this file except
* in compliance with the Real Format Source Code Porting and
* Optimization License. There are no redistribution rights for the
 * source code of this file. Please see the Real Format Source Code
 * Porting and Optimization License for the rights, obligations and
* limitations governing use of the contents of the file.
*
* RealNetworks is the developer of the Original Code and owns the
* copyrights in the portions it created.
*
* This file, and the files included with this file, is distributed and
* made available on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, AND REALNETWORKS HEREBY DISCLAIMS ALL
* SUCH WARRANTIES, INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT
* OR NON-INFRINGEMENT.
*
* Technology Compatibility Kit Test Suite(s) Location:
* https://rarvcode-tck.helixcommunity.org
*
* Contributor(s):
*
* ***** END LICENSE BLOCK ***** */


#ifndef _RM_TYPES_H_
#define _RM_TYPES_H_

#include "helix_types.h"
/*
 * Seek origin definitions - these are
 * exactly the same as the respective
 * definitions in <stdio.h>
 */
#define HX_SEEK_ORIGIN_SET 0
#define HX_SEEK_ORIGIN_CUR 1
#define HX_SEEK_ORIGIN_END 2

/* Function pointer definitions */
//typedef UINT32 (*rm_read_func_ptr) (void*   pUserRead,
//                                    BYTE*   pBuf, /* Must be at least ulBytesToRead long */
//                                    UINT32  ulBytesToRead);
//typedef void (*rm_seek_func_ptr) (void*  pUserRead,
//                                  UINT32 ulOffset,
//                                  UINT32 ulOrigin);

/* Function pointer definitions */
typedef UINT32 (*rm_read_func_ptr)(void *pUserRead,
								   BYTE *pBuf,	/* Must be at least ulBytesToRead long */
								   UINT32 ulBytesToRead);
typedef UINT32 (*rm_write_func_ptr)(void *pUserRead,
									BYTE *pBuf,
									UINT32 ulBytesToRead);
typedef void (*rm_seek_func_ptr)(void *pUserRead,
								 UINT32 ulOffset,
								 UINT32 ulOrigin);
typedef INT32 (*rm_tell_func_ptr)(void *pUserRead);

/*
* rm_parser definition. This is opaque to the user.
*/
typedef void rm_parser;

#endif
