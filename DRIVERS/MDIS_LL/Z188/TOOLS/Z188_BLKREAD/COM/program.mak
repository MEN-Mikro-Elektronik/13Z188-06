#***************************  M a k e f i l e  *******************************
#
#         Author: ds
#          $Date: 2004/04/15 12:19:57 $
#      $Revision: 1.2 $
#
#    Description: Makefile definitions for the M36 example program
#
#---------------------------------[ History ]---------------------------------
#
#   $Log: program.mak,v $
#   Revision 1.2  2004/04/15 12:19:57  cs
#   Minor modifications for MDIS4/2004 conformity
#         removed unused MAK_OPTIM=$(OPT_1)
#         added mdis_err.h to MAK_INCL
#
#   Revision 1.1  1998/11/17 15:30:59  Schmidt
#   Added by mcvs
#
#-----------------------------------------------------------------------------
#   (c) Copyright 1998 by MEN mikro elektronik GmbH, Nuernberg, Germany
#*****************************************************************************

MAK_NAME=m36_blkread

MAK_LIBS=$(LIB_PREFIX)$(MEN_LIB_DIR)/mdis_api$(LIB_SUFFIX)    \
         $(LIB_PREFIX)$(MEN_LIB_DIR)/usr_oss$(LIB_SUFFIX)     \
         $(LIB_PREFIX)$(MEN_LIB_DIR)/usr_utl$(LIB_SUFFIX)     \

MAK_INCL=$(MEN_INC_DIR)/m36_drv.h     \
         $(MEN_INC_DIR)/men_typs.h    \
         $(MEN_INC_DIR)/mdis_api.h    \
         $(MEN_INC_DIR)/usr_oss.h     \
         $(MEN_INC_DIR)/usr_utl.h     \

MAK_INP1=m36_blkread$(INP_SUFFIX)

MAK_INP=$(MAK_INP1)
