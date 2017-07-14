/***********************  I n c l u d e  -  F i l e  ************************
 *  
 *         Name: m36_pld.h
 *
 *       Author: ds
 *        $Date: 2004/04/15 12:19:49 $
 *    $Revision: 1.2 $
 * 
 *  Description: Prototypes for PLD data array and ident function
 *                      
 *     Switches: -
 *
 *-------------------------------[ History ]---------------------------------
 *
 * $Log: m36_pld.h,v $
 * Revision 1.2  2004/04/15 12:19:49  cs
 * Minor modifications for MDIS4/2004 conformity
 *       added variants for swapped access
 *
 * Revision 1.1  1998/11/17 10:04:11  Schmidt
 * Added by mcvs
 *
  *---------------------------------------------------------------------------
 * (c) Copyright 1998 by MEN mikro elektronik GmbH, Nuernberg, Germany 
 ****************************************************************************/

#ifndef _M36_PLD_H
#define _M36_PLD_H

#ifdef __cplusplus
	extern "C" {
#endif

# ifndef _ONE_NAMESPACE_PER_DRIVER_
        /* variant for swapped access */
#       ifdef ID_SW
#		define M36_PldIdent 	M36_SW_PldIdent
#               define M36_PldData 	M36_SW_PldData
#       endif
# endif

/*--------------------------------------+
|   EXTERNALS                           |
+--------------------------------------*/
extern const u_int8 M36_PldData[];

/*--------------------------------------+
|   PROTOTYPES                          |
+--------------------------------------*/
char* M36_PldIdent(void);


#ifdef __cplusplus
	}
#endif

#endif	/* _M36_PLD_H */
