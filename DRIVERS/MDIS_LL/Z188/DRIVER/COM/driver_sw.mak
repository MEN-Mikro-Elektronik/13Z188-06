#***************************  M a k e f i l e  *******************************
#
#         Author: kp
#          $Date: 2002/07/25 16:12:04 $
#      $Revision: 1.3 $
#            $Id: driver_sw.mak,v 1.3 2002/07/25 16:12:04 DSchmidt Exp $
#
#    Description: makefile descriptor for M36 (swapped)
#
#---------------------------------[ History ]---------------------------------
#
#   $Log: driver_sw.mak,v $
#   Revision 1.3  2002/07/25 16:12:04  DSchmidt
#   m36_pld.h added
#
#   Revision 1.2  2002/06/13 16:35:33  kp
#   fixed missing m36_pld and mbuf lib
#
#   Revision 1.1  2002/06/13 13:59:51  kp
#   Initial Revision
#
#-----------------------------------------------------------------------------
#   (c) Copyright 2002 by MEN mikro elektronik GmbH, Nuernberg, Germany
#*****************************************************************************

MAK_NAME=m36_sw

MAK_SWITCH=$(SW_PREFIX)MAC_MEM_MAPPED \
		   $(SW_PREFIX)MAC_BYTESWAP \
		   $(SW_PREFIX)ID_SW


MAK_LIBS=$(LIB_PREFIX)$(MEN_LIB_DIR)/desc$(LIB_SUFFIX)		\
         $(LIB_PREFIX)$(MEN_LIB_DIR)/id_sw$(LIB_SUFFIX)		\
         $(LIB_PREFIX)$(MEN_LIB_DIR)/mbuf$(LIB_SUFFIX)     \
         $(LIB_PREFIX)$(MEN_LIB_DIR)/oss$(LIB_SUFFIX)		\
         $(LIB_PREFIX)$(MEN_LIB_DIR)/dbg$(LIB_SUFFIX) 


MAK_INCL=$(MEN_MOD_DIR)/m36_pld.h     \
		 $(MEN_INC_DIR)/m36_drv.h     \
         $(MEN_INC_DIR)/men_typs.h    \
         $(MEN_INC_DIR)/oss.h         \
         $(MEN_INC_DIR)/mdis_err.h    \
         $(MEN_INC_DIR)/mbuf.h        \
         $(MEN_INC_DIR)/maccess.h     \
         $(MEN_INC_DIR)/desc.h        \
         $(MEN_INC_DIR)/mdis_api.h    \
         $(MEN_INC_DIR)/mdis_com.h    \
         $(MEN_INC_DIR)/modcom.h      \
         $(MEN_INC_DIR)/ll_defs.h     \
         $(MEN_INC_DIR)/ll_entry.h    \
         $(MEN_INC_DIR)/dbg.h    \

MAK_INP1=m36_drv$(INP_SUFFIX)
MAK_INP2=m36_pld$(INP_SUFFIX)

MAK_INP=$(MAK_INP1) $(MAK_INP2)



