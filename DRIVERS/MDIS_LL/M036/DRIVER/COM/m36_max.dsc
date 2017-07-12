#************************** MDIS4 device descriptor *************************
#
#        Author: ds
#         $Date: 1998/11/26 16:18:27 $
#     $Revision: 1.3 $
#
#   Description: Metadescriptor for M36
#
#****************************************************************************

M36_1  {
	#------------------------------------------------------------------------
	#	general parameters (don't modify)
	#------------------------------------------------------------------------
    DESC_TYPE       = U_INT32       1         # descriptor type (1=device)
    HW_TYPE         = STRING        M036      # hardware name of device

	#------------------------------------------------------------------------
	#	base board configuration
	#------------------------------------------------------------------------
    BOARD_NAME      = STRING        D201_1    # device name of baseboard
    DEVICE_SLOT     = U_INT32       0         # used slot on baseboard (0..n)
    IRQ_ENABLE      = U_INT32       0         # irq enabled after init

	#------------------------------------------------------------------------
	#	debug levels (optional)
	#   this keys have only effect on debug drivers
	#------------------------------------------------------------------------
    DEBUG_LEVEL         = U_INT32   0xc0008007    # LL driver
    DEBUG_LEVEL_MK      = U_INT32   0xc0008007    # MDIS kernel
    DEBUG_LEVEL_OSS     = U_INT32   0xc0008002    # OSS calls
    DEBUG_LEVEL_DESC    = U_INT32   0xc0008002    # DESC calls
    DEBUG_LEVEL_MBUF    = U_INT32   0xc0008000    # MBUF calls

	#------------------------------------------------------------------------
	#	device parameters
	#------------------------------------------------------------------------
    ID_CHECK            = U_INT32   1             # check module ID prom
	PLD_LOAD 			= U_INT32 	1       	  # load PLD initially

	#--- general parameters
	SINGLE_ENDED 		= U_INT32 	1			  # input adapter type (0..1)
 	EXT_TRIG 			= U_INT32 	1             # trigger mode (0..1)
	BIPOLAR 			= U_INT32 	0			  # measuring mode (0..1)
	SAMPLE_ALL			= U_INT32	0			  # sample all channels (0..1)

	#--- input buffer parameters
	IN_BUF {
		MODE 			= U_INT32 	0			  # buffer mode (M_BUF_xxx)
		SIZE 			= U_INT32 	320           # buffer size [bytes]
		HIGHWATER 		= U_INT32 	320           # buffer highwater mark [bytes]
		TIMEOUT 		= U_INT32 	1000          # buffer read timeout [msec]
	}

	#--- channel parameters
	CHANNEL_0 {
		ENABLE 			= U_INT32 	1             # channel enable (0..1)  
		GAIN 			= U_INT32 	0			  # channel gain (0..3)
	}            
	CHANNEL_1 {
		ENABLE 			= U_INT32 	1             # channel enable (0..1)  
		GAIN 			= U_INT32 	0			  # channel gain (0..3)
	}            
	CHANNEL_2 {
		ENABLE 			= U_INT32 	1             # channel enable (0..1)  
		GAIN 			= U_INT32 	0			  # channel gain (0..3)
	}            
	CHANNEL_3 {
		ENABLE 			= U_INT32 	1             # channel enable (0..1)  
		GAIN 			= U_INT32 	0			  # channel gain (0..3)
	}            
	CHANNEL_4 {
		ENABLE 			= U_INT32 	1             # channel enable (0..1)  
		GAIN 			= U_INT32 	0			  # channel gain (0..3)
	}            
	CHANNEL_5 {
		ENABLE 			= U_INT32 	1             # channel enable (0..1)  
		GAIN 			= U_INT32 	0			  # channel gain (0..3)
	}            
	CHANNEL_6 {
		ENABLE 			= U_INT32 	1             # channel enable (0..1)  
		GAIN 			= U_INT32 	0			  # channel gain (0..3)
	}            
	CHANNEL_7 {
		ENABLE 			= U_INT32 	1             # channel enable (0..1)  
		GAIN 			= U_INT32 	0			  # channel gain (0..3)
	}            
	CHANNEL_8 {
		ENABLE 			= U_INT32 	1             # channel enable (0..1)  
		GAIN 			= U_INT32 	0			  # channel gain (0..3)
	}            
	CHANNEL_9 {
		ENABLE 			= U_INT32 	1             # channel enable (0..1)  
		GAIN 			= U_INT32 	0			  # channel gain (0..3)
	}            
	CHANNEL_10 {
		ENABLE 			= U_INT32 	1             # channel enable (0..1)  
		GAIN 			= U_INT32 	0			  # channel gain (0..3)
	}            
	CHANNEL_11 {
		ENABLE 			= U_INT32 	1             # channel enable (0..1)  
		GAIN 			= U_INT32 	0			  # channel gain (0..3)
	}            
	CHANNEL_12 {
		ENABLE 			= U_INT32 	1             # channel enable (0..1)  
		GAIN 			= U_INT32 	0			  # channel gain (0..3)
	}            
	CHANNEL_13 {
		ENABLE 			= U_INT32 	1             # channel enable (0..1)  
		GAIN 			= U_INT32 	0			  # channel gain (0..3)
	}            
	CHANNEL_14 {
		ENABLE 			= U_INT32 	1             # channel enable (0..1)  
		GAIN 			= U_INT32 	0			  # channel gain (0..3)
	}            
	CHANNEL_15 {
		ENABLE 			= U_INT32 	1             # channel enable (0..1)  
		GAIN 			= U_INT32 	0			  # channel gain (0..3)
	}            
}
