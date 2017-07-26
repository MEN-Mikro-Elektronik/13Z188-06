/***********************  I n c l u d e  -  F i l e  ************************
 *
 *         Name: z188_drv.h
 *
 *       Author: czoz
 *        $Date: 2017/07/18 15:48:32 $
 *    $Revision: 0.1 $
 *
 *  Description: Header file for Z188 driver
 *               - Z188 specific status codes
 *               - This is just an extension to m36_drv.h which is used as well
 *
 *---------------------------------------------------------------------------
 * (c) Copyright 2017 by MEN mikro elektronik GmbH, Nuernberg, Germany
 ****************************************************************************/

#ifndef _Z188_DRV_H
#define _Z188_DRV_H

#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------+
|  DEFINES                                 |
+-----------------------------------------*/

/* Z188 specific status codes (STD) */    /* S,G: S=setstat, G=getstat    */
#define Z188_MODE         M_DEV_OF+0x0b   /* S,G: measure mode of curr ch */

#define Z188_MODE_VOLTAGE 0x0
#define Z188_MODE_CURRENT 0x1

#ifdef __cplusplus
}
#endif

#endif /* _Z188_DRV_H */

