/*********************  P r o g r a m  -  M o d u l e ***********************
 *
 *         Name: m36_drv.c
 *      Project: M36 module driver (MDIS V4.x)
 *
 *       Author: ds
 *        $Date: 2010/09/21 17:47:59 $
 *    $Revision: 1.11 $
 *
 *  Description: Low level driver for M36 modules
 *
 *               The M36 module is a 16 bit analog input module with 8
 *               grounded or 16 differential inputs. This depends on the input
 *               adapter ADxx. The type of the adapter must be set (2).
 *               The module uses interrupts.
 *
 *               The driver handles the M36 input ports as 8/16 channels.
 *
 *               Each channel can be separately enabled/disabled (1)(2).
 *
 *               A channel specific gain factor (1x/2x/4x/8x) can be set by
 *               the driver(1)(2).
 *               A global gain factor of 16 can be programmed using an on-board
 *               jumper.
 *
 *               Internal autocalibration is performed by M36_Init() and can
 *               also be activated by setstat call (1).
 *
 *               The measuring mode for all channels can be set to unipolar or
 *               bipolar (1)(2).
 *
 *               The conversion is triggered by an internal sampling rate of
 *               100kHz or an external trigger frequency (1kHz..90kHz) (1)(2).
 *               On each conversion, one channel will be sampled.
 *               It can be chosen if only the enabled channels or all channels
 *               should be sampled (2).
 *               The time for one complete measurement cycle is approximately
 *               10usec multiplied by the number of enabled channels.
 *               To provide short measure cycles, all unused channels should be
 *               disabled if no interrupt is used.
 *
 *               After one complete measurement cycle an interrupt may be
 *               generated if enabled. On each interrupt, the input values of
 *               the enabled channels will be stored in an input buffer.
 *
 *               ATTENTION: The interrupt rate depends on the sampling
 *                          frequency and the number of channels that will be
 *                          sampled. (f_trig ~ f_samp / ch_nbr)
 *                          If you use the internal sampling rate of 100kHz
 *                          and not all channels are enabled, it is
 *                          recommended to sample all channels (set descriptor
 *                          entry 'SAMPLE_ALL=1'. Otherwise the interrupt rate
 *                          will be very high.
 *
 *               The buffering method depends on the block read i/o mode, which
 *               can be defined via M_BUF_RD_MODE setstat (1).
 *
 *               (1) = defineable via status call
 *               (2) = defineable via descriptor key
 *
 *     Required: ---
 *     Switches: _ONE_NAMESPACE_PER_DRIVER_
 *
 *-------------------------------[ History ]---------------------------------
 *
 * $Log: m36_drv.c,v $
 * Revision 1.11  2010/09/21 17:47:59  ts
 * R: channel and code mismatch in Prototype declaration of GetStat/SetStat
 * M: corrected copy/paste error
 *
 * Revision 1.10  2009/09/23 17:48:21  MRoth
 * R: Porting to MDIS5 (according porting guide rev. 0.7)
 * M: a) added support for 64bit (Set/GetStat prototypes, m_read calls)
 *    b) added casts to avoid compiler warnings
 *    c) put all MACCESS macros conditionals in brackets
 *
 * Revision 1.9  2008/01/10 14:54:12  ts
 * load PLD only if module is M36, not on M36N
 * Cosmetics, comments added.
 *
 * Revision 1.8  2007/12/10 14:42:51  ts
 * Flash routines for Calibration added
 * support M36 and M36N mod ids
 *
 * Revision 1.7  2004/04/15 12:19:47  cs
 * Minor modifications for MDIS4/2004 conformity
 *       some typecasts for win2k compliance
 *
 * Revision 1.6  2002/07/25 16:12:07  DSchmidt
 * Calibrate(): added timeout to prevent deadlook if a M36 module doesn't work
 *
 * Revision 1.5  2002/06/13 13:59:52  kp
 * support swapped variant
 * all symbols now static (except GetEntry)
 *
 * Revision 1.4  1998/11/26 16:18:22  Schmidt
 * M36_Init : descriptor entry SAMPLE_ALL added
 * M36_Irq  : performance improved
 *
 * Revision 1.3  1998/11/18 14:43:06  see
 * missing MBUF_Ident and M36_PldIdent added to idFuncTbl
 *
 * Revision 1.2  1998/11/18 11:40:10  see
 * M36_GetStat: M36_EXT_PIN: value assignment caused compiler error
 * PldLoad: return(0) removed, since void
 *
 * Revision 1.1  1998/11/17 10:04:03  Schmidt
 * Added by mcvs
 *
 *---------------------------------------------------------------------------
 * (c) Copyright 1998 by MEN mikro elektronik GmbH, Nuernberg, Germany
 ****************************************************************************/

static const char RCSid[]="$Id: m36_drv.c,v 1.11 2010/09/21 17:47:59 ts Exp $";

#define _NO_LL_HANDLE		/* ll_defs.h: don't define LL_HANDLE struct */

#include <MEN/men_typs.h>   /* system dependend definitions   */
#include <MEN/maccess.h>    /* hw access macros and types     */
#include <MEN/dbg.h>        /* debug functions                */
#include <MEN/oss.h>        /* oss functions                  */
#include <MEN/desc.h>       /* descriptor functions           */
#include <MEN/mbuf.h>       /* buffer lib functions and types */
#include <MEN/modcom.h>     /* id prom functions              */
#include <MEN/mdis_api.h>   /* MDIS global defs               */
#include <MEN/mdis_com.h>   /* MDIS common defs               */
#include <MEN/mdis_err.h>   /* MDIS error codes               */

#include <MEN/ll_defs.h>    /* low level driver definitions   */

#include "m36_pld.h"		/* PLD ident/data prototypes      */

/*-----------------------------------------+
|  DEFINES                                 |
+-----------------------------------------*/
/* general */
#define CH_NUMBER_SINGLE	16	/* nr of device channels (single ended mode) */
#define CH_NUMBER_DIFF		8	/* nr of device channels (differential mode) */
#define CH_BYTES			2		/* nr of bytes per channel */
#define USE_IRQ				TRUE	/* interrupt required  */
#define ADDRSPACE_COUNT		1		/* nr of required address spaces */
#define ADDRSPACE_SIZE		256		/* size of address space */

#define MOD_ID_MAGIC		0x5346  /* id prom magic word */
#define MOD_ID_SIZE			128		/* id prom size [bytes] */

#define MOD_ID_M36			0x24	/* classic M36  mod id = 36(dez) */
#define MOD_ID_M36N			0x7d24	/* New M36N  mod id    = 32036(dez) */

/* 16Z045-01 mapped at 0xF0 in M-Module Addr Space */
#define	M36_FLASH_ADDLO	 	0xF0	/* Flash IF Address loword	*/
#define	M36_FLASH_ADDHI	 	0xF2	/* Flash IF Address hiword	*/
#define	M36_FLASH_DATA	 	0xF4	/* Flash IF Data */
#define FL_ACC_TOUT			1000000	/* Timeout counter for Flash operations */

/* debug settings */
#define DBG_MYLEVEL			llHdl->dbgLevel
#define DBH					llHdl->dbgHdl

/* register offsets */
#define DATA_REG(i)	(0x00 + ((i)<<1))	/* data register    0..15 */
#define ADR_REG(i)	(0x20 + ((i)<<1))	/* address register 0..15 */
#define CFG_REG(i)	(0x40 + ((i)<<1))	/* config register  0..15 */
#define STAT_REG	0x60				/* status  register */
#define CTRL_REG	0x70				/* control register */
#define LOAD_REG	0xfe				/* FLEX load register */

/* CTRL_REG bitmask */
#define RST		0x04	/* IRQ reset pending irq */
#define CAL		0x02	/* CAL calibration mode */
#define EXT		0x01	/* SMP external trigger */

/* CTRLSTAT_REG bitmask */
#define IRQ		0x04	/* IRQ  irq not active */
#define BIN		0x02	/* BI   binary input status */
#define BUSY	0x01	/* BUSY calibration in progress */

/* LOAD_REG bitmask */
#define TDO		0x01	/* data */
#define TCK		0x02	/* clock */
#define TMS		0x08	/* tms */

/* single bit setting macros */
#define bitset(byte,mask)		((byte) |=  (mask))
#define bitclr(byte,mask)		((byte) &= ~(mask))
#define bitmove(byte,mask,bit)	(bit ? bitset(byte,mask) : bitclr(byte,mask))

/* helpers for M36N Flash Access */
#define HIWD(V) 	((V & 0xffff0000)>>16)
#define LOWD(V) 	( V & 0x0000ffff)
#define Z45_ADDR(V) ((V & 0x1fffffff)|(1<<30))


/*--- Flash command codes ---*/
#define C_WRITE         0x40 /* write (program) data */
#define C_CSR           0x50 /* clear status register */
#define C_RSR           0x70 /* read status register */
#define C_ERASE         0x20 /* erase single block */
#define C_IDENTIFIER    0x90 /* read manufacturer code */
#define C_CONFIRM       0xd0 /* confirm/resume */
#define C_READ          0xff /* reset to read mode */
#define C_LOCK_SETUP    0x60 /* lock setup */
#define C_LOCK          0x01 /* lock command */
#define C_UNLOCK        0xD0 /* unlock command */
#define C_LOCK_DOWN     0x2F /* lock down command */
#define C_PRCR_SETUP    0x60 /* program read configuration register setup */
#define C_PRCR          0x03 /* program read configuration register */

/*--- flash type with device id ---*/
#define PC28F640P30T85  0x8817 /* device id */

/* Configuration Register */
#define CONF_REG        0xBFCF /* 1011111111001111
                                  |||||||||||||||'- Continuous-word burst
                                  ||||||||||||||'--
                                  |||||||||||||'---
                                  ||||||||||||'---- no wrap
                                  |||||||||||'----- reserved
                                  ||||||||||'------ reserved
                                  |||||||||'------ rising edge
                                  ||||||||'------- linear burst sequence
                                  |||||||'-------- WAIT deasserted one data
                                                   cycle before valid data
                                  ||||||'--------- WAIT signal is active high
                                  |||||'---------- Latency count code 7
                                  ||||'-----------
                                  |||'------------
                                  ||'------------- reserved
                                  |'-------------- asynchronous page mode read
                               */

/* register device identifier */
#define BIT_LOCK    0
#define OFFSET_LOCK 4

/* status register */
#define BIT_BWS     0   /* BEFP(buffer enhanced factory programming) Status */
#define BIT_BLS     1   /* block locked status, 1 = locked, 0 = unlocked */
#define BIT_PSS     2   /* program suspend status */
#define BIT_VPPS    3   /* VPP Status, 0 = VPP ok, 1 = VPP < VPPLK */
#define BIT_PS      4   /* program status, 0 = successful, 1 = fail */
#define BIT_ES      5   /* erase status, 0 = successful, 1 = fail */
#define BIT_ESS     6   /* erase suspend status */
#define BIT_DWS     7   /* device write status */


/*-----------------------------------------+
|  TYPEDEFS                                |
+-----------------------------------------*/
/* ll handle */
typedef struct {
	/* general */
    MACCESS         	ma;             /* hw access handle */
    int32           	memAlloc;		/* size allocated for the handle */
    OSS_HANDLE      	*osHdl;         /* oss handle */
    OSS_IRQ_HANDLE  	*irqHdl;        /* irq handle */
    DESC_HANDLE     	*descHdl;       /* desc handle */

	MDIS_IDENT_FUNCT_TBL idFuncTbl;		/* id function table */
	/* debug */
    u_int32         	dbgLevel;		/* debug level */
	DBG_HANDLE      	*dbgHdl;        /* debug handle */

    u_int32         	irqCount;       /* interrupt counter */
    u_int32         	idCheck;		/* id check enabled */
	u_int32				chNumber;		/* number of channels */
	u_int32				singleEnded;	/* single ended mode */
	int32				nbrEnabledCh;	/* number of enabled channels */
	u_int32				extTrig;		/* external trigger */
	u_int32				bipolar;		/* bipolar mode */
	u_int32				sampleAll;		/* sample all channels */
	/* buffers */
    MBUF_HANDLE     	*bufHdl;		/* input buffer handle */

	/* misc for M36N support */
    u_int32         	modType;        /* MOD_ID_M36 or MOD_ID_M36N */

	/* channel parameters */
	u_int32		enable[CH_NUMBER_SINGLE];	/* enable/disable the channel */
	u_int32		gain[CH_NUMBER_SINGLE];		/* gain factor */
	u_int32		dataReg[CH_NUMBER_SINGLE];	/* data register */
	u_int32		cfgReg[CH_NUMBER_SINGLE];	/* config register */
} LL_HANDLE;

/* include files which need LL_HANDLE */
#include <MEN/ll_entry.h>   /* low level driver jumptable  */
#include <MEN/m36_drv.h>    /* M36 driver header file */


/*-----------------------------------------+
|  PROTOTYPES                              |
+-----------------------------------------*/
static char* Ident( void );
static int32 Cleanup(LL_HANDLE *llHdl, int32 retCode);
static void PldLoad(LL_HANDLE *llHdl);
static int32 Calibrate(LL_HANDLE *llHdl);
static void InitAllChan(LL_HANDLE *llHdl);
static void ConfigChan(LL_HANDLE *llHdl, int32 ch);

static int32 M36_Init(DESC_SPEC *descSpec, OSS_HANDLE *osHdl,
					   MACCESS *ma, OSS_SEM_HANDLE *devSemHdl,
					   OSS_IRQ_HANDLE *irqHdl, LL_HANDLE **llHdlP);
static int32 M36_Exit(LL_HANDLE **llHdlP );
static int32 M36_Read(LL_HANDLE *llHdl, int32 ch, int32 *value);
static int32 M36_Write(LL_HANDLE *llHdl, int32 ch, int32 value);
static int32 M36_SetStat(LL_HANDLE *llHdl,int32 code, int32 ch,
							INT32_OR_64 value32_or_64);
static int32 M36_GetStat(LL_HANDLE *llHdl, int32 code, int32 ch,
							INT32_OR_64 *value32_or_64P );
static int32 M36_BlockRead(LL_HANDLE *llHdl, int32 ch, void *buf, int32 size,
							int32 *nbrRdBytesP);
static int32 M36_BlockWrite(LL_HANDLE *llHdl, int32 ch, void *buf, int32 size,
							 int32 *nbrWrBytesP);
static int32 M36_Irq(LL_HANDLE *llHdl );
static int32 M36_Info(int32 infoType, ... );

/* ts: Functions for Stratix Flash Access */
static int32 M36_FlashRead(	LL_HANDLE *llHdl, u_int32 address);
static void  M36_FlashWrite(	LL_HANDLE *llHdl, u_int32 address, u_int16 val);
static int32 M36_FlashLockBlock( LL_HANDLE *llHdl, u_int32 offset );
static int32 M36_FlashUnlockBlock( LL_HANDLE *llHdl, u_int32 offset );
static int32 M36_FlashWriteWord(	LL_HANDLE *llHdl,u_int32 address, u_int16 val);
static int32 M36_FlashReadStatus( LL_HANDLE *llHdl );
static int32 M36_FlashEraseBlock( LL_HANDLE *llHdl, u_int32 offset );





/**************************** M36_GetEntry *********************************
 *
 *  Description:  Initialize drivers jump table
 *
 *---------------------------------------------------------------------------
 *  Input......:  ---
 *  Output.....:  drvP  pointer to the initialized jump table structure
 *  Globals....:  ---
 ****************************************************************************/
#ifdef _ONE_NAMESPACE_PER_DRIVER_
    void LL_GetEntry( LL_ENTRY* drvP )
#else
# ifdef	MAC_BYTESWAP
	void M36_SW_GetEntry( LL_ENTRY*	drvP )
# else
	void M36_GetEntry( LL_ENTRY* drvP )
# endif
#endif
{
    drvP->init        = M36_Init;
    drvP->exit        = M36_Exit;
    drvP->read        = M36_Read;
    drvP->write       = M36_Write;
    drvP->blockRead   = M36_BlockRead;
    drvP->blockWrite  = M36_BlockWrite;
    drvP->setStat     = M36_SetStat;
    drvP->getStat     = M36_GetStat;
    drvP->irq         = M36_Irq;
    drvP->info        = M36_Info;
}

/******************************** M36_Init ***********************************
 *
 *  Description:  Allocate and return ll handle, initialize hardware
 *
 *                The function initializes all channels with the
 *                definitions made in the descriptor. The interrupt
 *                is disabled.
 *
 *                The following descriptor keys are used:
 *
 *                Deskriptor key        Default          Range
 *                --------------------  ---------------  -------------
 *                DEBUG_LEVEL_DESC      OSS_DBG_DEFAULT  see dbg.h
 *                DEBUG_LEVEL_MBUF      OSS_DBG_DEFAULT  see dbg.h
 *                DEBUG_LEVEL           OSS_DBG_DEFAULT  see dbg.h
 *                ID_CHECK              1                0..1
 *                PLD_LOAD              1                0..1
 *                SINGLE_ENDED          1                0..1
 *                EXT_TRIG              1                0..1
 *                BIPOLAR               0                0..1
 *                SAMPLE_ALL            0                0..1
 *                IN_BUF/MODE           0                0..3
 *                IN_BUF/SIZE           320              0..max
 *                IN_BUF/HIGHWATER      320              0..max
 *                IN_BUF/TIMEOUT        1000             0..max
 *                CHANNEL_n/ENABLE      1                0..1
 *                CHANNEL_n/GAIN		0                0..3
 *
 *                PLD_LOAD defines, if the PLD should be loaded at INIT.
 *                   0 = PLD load disabled
 *                   1 = PLD load enabled
 *                With PLD_LOAD=0, ID_CHECK is implicitely disabled.
 *                (This key key is for test purposes and should always be set to 1)
 *
 *                SINGLE_ENDED defines, if the input adapter (ADxx) of the
 *                module supports single ended or differential inputs.
 *
 *                   0 = differential
 *                   1 = single ended
 *
 *                EXT_TRIG defines the sampling mode
 *
 *                   0 = internal trigger (100kHz)
 *                   1 = external trigger (1..90kHz)
 *
 *                BIPOLAR defines the measuring mode of all channels.
 *
 *                   0 = unipolar
 *                   1 = bipolar
 *
 *                SAMPLE_ALL defines if all channels or only the enabled
 *                channels will be sampled.
 *
 *                   0 = sample only the enabled channels
 *                   1 = sample all channels
 *
 *                MODE defines the buffer's block i/o mode (see MDIS-Doc.):
 *
 *                   0 = M_BUF_USRCTRL
 *                   1 = M_BUF_CURRBUF
 *                   2 = M_BUF_RINGBUF
 *                   3 = M_BUF_RINGBUF_OVERWR
 *
 *                SIZE defines the size of the input buffer [bytes]
 *                (minimum size is 16).
 *
 *                HIGHWATER defines the buffer level in [bytes], of the
 *                corresponding highwater buffer event (see MDIS-Doc.).
 *
 *                TIMEOUT defines the buffers read timeout [msec]
 *                (where timeout=0: no timeout) (see MDIS-Doc.).
 *
 *                ENABLE enables/disables channel n. If disabled,
 *                the corresponding channel can not be read.
 *
 *                   0 = disable
 *                   1 = enable
 *
 *                GAIN defines the gain factor of channel n.
 *
 *                   0 = factor 1
 *                   1 = factor 2
 *                   2 = factor 4
 *                   3 = factor 8
 *
 *---------------------------------------------------------------------------
 *  Input......:  descSpec   pointer to descriptor data
 *                osHdl      oss handle
 *                ma         hw access handle
 *                devSemHdl  device semaphore handle
 *                irqHdl     irq handle
 *  Output.....:  llHdlP     ptr to low level driver handle
 *                return     success (0) or error code
 *  Globals....:  ---
 ****************************************************************************/
static int32 M36_Init(
    DESC_SPEC       *descP,
    OSS_HANDLE      *osHdl,
    MACCESS         *ma,
    OSS_SEM_HANDLE  *devSemHdl,
    OSS_IRQ_HANDLE  *irqHdl,
    LL_HANDLE       **llHdlP
)
{
    LL_HANDLE *llHdl = NULL;
    u_int32 gotsize, pldLoad, ch;
    u_int32 bufSize, bufMode, bufTout, bufHigh, bufDbgLevel;
    int32 error;
    u_int32 value;

    /*------------------------------+
    |  prepare the handle           |
    +------------------------------*/
	/* alloc */
    if ((*llHdlP = llHdl = (LL_HANDLE*)
		 OSS_MemGet(osHdl, sizeof(LL_HANDLE), &gotsize)) == NULL)
       return(ERR_OSS_MEM_ALLOC);

	/* clear */
    OSS_MemFill(osHdl, gotsize, (char*)llHdl, 0x00);

	/* init */
    llHdl->memAlloc   = gotsize;
    llHdl->osHdl      = osHdl;
    llHdl->irqHdl     = irqHdl;
    llHdl->ma		  = *ma;

    /*------------------------------+
    |  init id function table       |
    +------------------------------*/
	/* drivers ident function */
	llHdl->idFuncTbl.idCall[0].identCall = Ident;
	llHdl->idFuncTbl.idCall[1].identCall = M36_PldIdent;
	/* libraries ident functions */
	llHdl->idFuncTbl.idCall[2].identCall = DESC_Ident;
	llHdl->idFuncTbl.idCall[3].identCall = OSS_Ident;
	llHdl->idFuncTbl.idCall[4].identCall = MBUF_Ident;
	/* terminator */
	llHdl->idFuncTbl.idCall[5].identCall = NULL;

    /*------------------------------+
    |  prepare debugging            |
    +------------------------------*/
	DBG_MYLEVEL = OSS_DBG_DEFAULT;	/* set OS specific debug level */
	DBGINIT((NULL,&DBH));

    DBGWRT_1((DBH, "LL - M36_Init\n"));

    /*------------------------------+
    |  scan descriptor              |
    +------------------------------*/
	/* prepare access */
    if ((error = DESC_Init(descP, osHdl, &llHdl->descHdl)))
		return( Cleanup(llHdl,error) );

    /* DEBUG_LEVEL_DESC */
    if ((error = DESC_GetUInt32(llHdl->descHdl, OSS_DBG_DEFAULT,
								&value, "DEBUG_LEVEL_DESC")) &&
		error != ERR_DESC_KEY_NOTFOUND)
		return( Cleanup(llHdl,error) );

	DESC_DbgLevelSet(llHdl->descHdl, value);	/* set level */

    /* DEBUG_LEVEL_MBUF */
    if ((error = DESC_GetUInt32(llHdl->descHdl, OSS_DBG_DEFAULT,
								&bufDbgLevel, "DEBUG_LEVEL_MBUF")) &&
		error != ERR_DESC_KEY_NOTFOUND)
		return( Cleanup(llHdl,error) );

    /* DEBUG_LEVEL */
    if ((error = DESC_GetUInt32(llHdl->descHdl, OSS_DBG_DEFAULT,
								&llHdl->dbgLevel, "DEBUG_LEVEL")) &&
		error != ERR_DESC_KEY_NOTFOUND)
		return( Cleanup(llHdl,error) );

    /* ID_CHECK */
    if ((error = DESC_GetUInt32(llHdl->descHdl, TRUE,
								&llHdl->idCheck, "ID_CHECK")) &&
		error != ERR_DESC_KEY_NOTFOUND)
		return( Cleanup(llHdl,error) );

    /* PLD_LOAD */
    if ((error = DESC_GetUInt32(llHdl->descHdl, TRUE,
								&pldLoad, "PLD_LOAD")) &&
		error != ERR_DESC_KEY_NOTFOUND)
		return( Cleanup(llHdl,error) );

	if (pldLoad == FALSE)
		llHdl->idCheck = FALSE;

	/* SINGLE_ENDED */
    if ((error = DESC_GetUInt32(llHdl->descHdl, TRUE,
								&llHdl->singleEnded, "SINGLE_ENDED")) &&
		error != ERR_DESC_KEY_NOTFOUND)
		return( Cleanup(llHdl,error) );

    if( llHdl->singleEnded == TRUE )
        llHdl->chNumber = CH_NUMBER_SINGLE;
    else
        llHdl->chNumber = CH_NUMBER_DIFF;

	/* EXT_TRIG */
    if ((error = DESC_GetUInt32(llHdl->descHdl, TRUE,
								&llHdl->extTrig, "EXT_TRIG")) &&
		error != ERR_DESC_KEY_NOTFOUND)
		return( Cleanup(llHdl,error) );

		if (llHdl->extTrig > 1)
			return( Cleanup(llHdl,ERR_LL_ILL_PARAM) );

	/* BIPOLAR */
	if ((error = DESC_GetUInt32(llHdl->descHdl, 0, &llHdl->bipolar,
								"BIPOLAR")) &&
		error != ERR_DESC_KEY_NOTFOUND)
		return( Cleanup(llHdl,error) );

	if (llHdl->bipolar > 1)
		return( Cleanup(llHdl,ERR_LL_ILL_PARAM) );

	/* SAMPLE_ALL */
	if ((error = DESC_GetUInt32(llHdl->descHdl, 0, &llHdl->sampleAll,
								"SAMPLE_ALL")) &&
		error != ERR_DESC_KEY_NOTFOUND)
		return( Cleanup(llHdl,error) );

	if (llHdl->bipolar > 1)
		return( Cleanup(llHdl,ERR_LL_ILL_PARAM) );

    /* IN_BUF/SIZE */
    if ((error = DESC_GetUInt32(llHdl->descHdl, 320, &bufSize,
								"IN_BUF/SIZE")) &&
		error != ERR_DESC_KEY_NOTFOUND)
		return( Cleanup(llHdl,error) );

	if (bufSize < 8)
       return( Cleanup(llHdl,ERR_LL_ILL_PARAM) );

	/* IN_BUF/MODE */
	if ((error = DESC_GetUInt32(llHdl->descHdl, M_BUF_USRCTRL, &bufMode,
								"IN_BUF/MODE")) &&
		error != ERR_DESC_KEY_NOTFOUND)
		return( Cleanup(llHdl,error) );

	/* IN_BUF/TIMEOUT */
	if ((error = DESC_GetUInt32(llHdl->descHdl, 1000, &bufTout,
								"IN_BUF/TIMEOUT")) &&
		error != ERR_DESC_KEY_NOTFOUND)
		return( Cleanup(llHdl,error) );

	/* IN_BUF/HIGHWATER */
	if ((error = DESC_GetUInt32(llHdl->descHdl, 320, &bufHigh,
								"IN_BUF/HIGHWATER")) &&
		error != ERR_DESC_KEY_NOTFOUND)
		return( Cleanup(llHdl,error) );

	/* clear number of enabled channels */
	llHdl->nbrEnabledCh = 0;

	/* channel params */
	for (ch=0; ch<llHdl->chNumber; ch++) {
		/* CHANNEL_n/ENABLE */
		if ((error = DESC_GetUInt32(llHdl->descHdl, 1, &llHdl->enable[ch],
									"CHANNEL_%d/ENABLE", ch)) &&
			error != ERR_DESC_KEY_NOTFOUND)
			return( Cleanup(llHdl,error) );

		if (llHdl->enable[ch] > 1)
			return( Cleanup(llHdl,ERR_LL_ILL_PARAM) );

		/* count number of enabled channels */
		if (llHdl->enable[ch])
			llHdl->nbrEnabledCh++;

		/* CHANNEL_n/GAIN */
		if ((error = DESC_GetUInt32(llHdl->descHdl, 0, &llHdl->gain[ch],
									"CHANNEL_%d/GAIN", ch)) &&
			error != ERR_DESC_KEY_NOTFOUND)
			return( Cleanup(llHdl,error) );

		if (llHdl->gain[ch] > 0x03)
			return( Cleanup(llHdl,ERR_LL_ILL_PARAM) );
	}

    /*------------------------------+
    |  install buffer               |
    +------------------------------*/
	/* create input buffer */
	if ((error = MBUF_Create(llHdl->osHdl, devSemHdl, llHdl,
							 bufSize, CH_BYTES, bufMode, MBUF_RD,
							 bufHigh, bufTout, irqHdl, &llHdl->bufHdl)))
		return( Cleanup(llHdl,error) );

	/* set debug level */
	MBUF_SetStat(llHdl->bufHdl, NULL, M_BUF_RD_DEBUG_LEVEL, bufDbgLevel);

    /*------------------------------+
    |  check module id              |
    +------------------------------*/
	if (llHdl->idCheck) {
		int modIdMagic = m_read((U_INT32_OR_64)llHdl->ma, 0);
		int modId      = m_read((U_INT32_OR_64)llHdl->ma, 1);

		if (modIdMagic != MOD_ID_MAGIC) {
			DBGWRT_ERR((DBH,
						" *** M36_Init: illegal magic=0x%04x\n",modIdMagic));
			error = ERR_LL_ILL_ID;
			return( Cleanup(llHdl,error) );
		}

		if ((modId != MOD_ID_M36) && (modId != MOD_ID_M36N)) {
			DBGWRT_ERR((DBH," *** M36_Init: illegal id=%d\n",modId));
			error = ERR_LL_ILL_ID;
			return( Cleanup(llHdl,error) );
		}

		/* store type for later use in calibration Function */
		llHdl->modType = modId;

	}

    DBGWRT_1((DBH, " M36_Init: \n" ));

    /*------------------------------+
    |  load PLD                     |
    +------------------------------*/
	if (pldLoad && (llHdl->modType == MOD_ID_M36))
		PldLoad( llHdl );

    /*------------------------------+
    |  init hardware                |
    +------------------------------*/
	/* config the trigger mode */
	MWRITE_D16(llHdl->ma, CTRL_REG, llHdl->extTrig ? EXT : 0x00);

	/* initialize all channels */
	InitAllChan(llHdl);

	/* start calibration if its not a M36N */
	if (llHdl->modType != MOD_ID_M36N)
		return( Calibrate(llHdl) );
	else
		return(ERR_SUCCESS);

}



/****************************** M36_Exit *************************************
 *
 *  Description:  De-initialize hardware and cleanup memory
 *
 *                The function calls Cleanup().
 *
 *---------------------------------------------------------------------------
 *  Input......:  llHdlP  	ptr to low level driver handle
 *  Output.....:  return    success (0) or error code
 *  Globals....:  ---
 ****************************************************************************/
static int32 M36_Exit(
   LL_HANDLE    **llHdlP
)
{
    LL_HANDLE *llHdl = *llHdlP;
	int32 error = 0;

    DBGWRT_1((DBH, "LL - M36_Exit\n"));

    /*------------------------------+
    |  de-init hardware             |
    +------------------------------*/
	/* nothing to do */

    /*------------------------------+
    |  cleanup memory               |
    +------------------------------*/
	error = Cleanup(llHdl,error);

	return(error);
}

/****************************** M36_Read *************************************
 *
 *  Description:  Reads value from device
 *
 *                The function reads the state of the current channel.
 *
 *                If the channel is not enabled an ERR_LL_READ error
 *                is returned.
 *
 *---------------------------------------------------------------------------
 *  Input......:  llHdl    ll handle
 *                ch       current channel
 *  Output.....:  valueP   read value
 *                return   success (0) or error code
 *  Globals....:  ---
 ****************************************************************************/
static int32 M36_Read(
    LL_HANDLE *llHdl,
    int32 ch,
    int32 *value
)
{
    DBGWRT_1((DBH, "LL - M36_Read: ch=%d\n",ch));

	/* channel disabled ? */
	if ( llHdl->enable[ch] == 0)
		return(ERR_LL_READ);

	/* read value of channel */
	*value = MREAD_D16(llHdl->ma, llHdl->dataReg[ch]);

	return(ERR_SUCCESS);
}

/****************************** M36_Write ************************************
 *
 *  Description:  Write value to device
 *
 *                The function is not supported and returns always an
 *                ERR_LL_ILL_FUNC error.
 *
 *---------------------------------------------------------------------------
 *  Input......:  llHdl    ll handle
 *                ch       current channel
 *                value    value to write
 *  Output.....:  return   success (0) or error code
 *  Globals....:  ---
 ****************************************************************************/
static int32 M36_Write( /* nodoc */
    LL_HANDLE *llHdl,
    int32 ch,
    int32 value
)
{
    DBGWRT_1((DBH, "LL - M36_Write: ch=%d\n",ch));

	return(ERR_LL_ILL_FUNC);
}

/****************************** M36_SetStat **********************************
 *
 *  Description:  Set driver status
 *
 *                Following status codes are supported:
 *
 *                Code                 Description                Values
 *                -------------------  -------------------------  ----------
 *                M_LL_DEBUG_LEVEL     driver debug level         see dbg.h
 *                M_MK_IRQ_ENABLE      interrupt enable           0..1
 *                M_LL_IRQ_COUNT       interrupt counter          0..max
 *                M_LL_CH_DIR          direction of curr ch       M_CH_IN
 *                M_BUF_xxx            input buffer codes         see MDIS
 *                -------------------  -------------------------  ----------
 *                M36_CH_ENABLE        enable/disable curr ch     0..1
 *                                      0 = disable
 *                                      1 = enable
 *                M36_CH_GAIN          gain factor of curr ch     0..3
 *					                    0 = factor 1
 *                                      1 = factor 2
 *                                      2 = factor 4
 *                                      3 = factor 8
 *                                      4 = factor 16 (M36N)
 *                                      Note: ch must be enabled
 *
 *                M36_BIPOLAR          measuring mode for all ch  0..1
 *                                      0 = unipolar
 *                                      1 = bipolar
 *                                      Note: ch must be enabled
 *                M36_EXT_TRIG         defines the sampling mode  0..1
 *                                      0 = internal trigger
 *                                      1 = external trigger
 *                M36_CALIBRATE        start calibration          -
 *                                      Note: interrupt must be
 *                                            disabled
 *
 *---------------------------------------------------------------------------
 *  Input......:  llHdl          ll handle
 *                code           status code
 *                ch             current channel
 *                value32_or_64  data or
 *                               ptr to block data struct (M_SG_BLOCK)  (*)
 *                (*) = for block status codes
 *  Output.....:  return         success (0) or error code
 *  Globals....:  ---
 ****************************************************************************/
static int32 M36_SetStat(
    LL_HANDLE *llHdl,
    int32  code,
    int32  ch,
    INT32_OR_64 value32_or_64
	)
{
	int32		value	= (int32)value32_or_64;	/* 32bit value     */
	INT32_OR_64	valueP	= value32_or_64;		/* stores 32/64bit pointer */

	int32 error = ERR_SUCCESS;
    M_SG_BLOCK *sg = (M_SG_BLOCK *)valueP;
	u_int16 *dataP;
	u_int32 i = 0;

    DBGWRT_1((DBH, "LL - M36_SetStat: ch=%d code=0x%04x value=0x%x\n",
			  ch,code,value));

    switch(code) {
        /*--------------------------+
		 |  debug level             |
		 +--------------------------*/
	case M_LL_DEBUG_LEVEL:
		llHdl->dbgLevel = value;
		break;
        /*--------------------------+
		  |  enable interrupts      |
		  +------------------------*/
	case M_MK_IRQ_ENABLE:
		error = 0 /* ERR_LL_UNK_CODE */;	/* say: not supported */
		break;
        /*--------------------------+
		 |  set irq counter         |
		 +--------------------------*/
	case M_MK_IRQ_COUNT:
		llHdl->irqCount = value;
		break;
        /*--------------------------+
		  |  channel direction        |
		  +--------------------------*/
	case M_LL_CH_DIR:
		if (value != M_CH_IN)
			error = ERR_LL_ILL_DIR;
		break;
        /*--------------------------+
		  | channel enable            |
		  +--------------------------*/
	case M36_CH_ENABLE:
		if ( (value < 0) || (value > 1) ) {
			error = ERR_LL_ILL_PARAM;
			break;
		}
		if ( llHdl->enable[ch] != (u_int32)value ) {
			/* update number of enabled channels */
			value ? llHdl->nbrEnabledCh++ : llHdl->nbrEnabledCh--;
			llHdl->enable[ch] = value;
			/* initialize all channels */
			InitAllChan(llHdl);
		}
		break;
        /*--------------------------+
		  |  channel gain             |
		  +--------------------------*/
	case M36_CH_GAIN:
		if ( (value < 0x00) || (value > 0x04 /*(M36N) was: 0x03 */) ) {
			error = ERR_LL_ILL_PARAM;
			break;
		}
		/* channel disabled ? */
		if ( llHdl->enable[ch] == 0) {
			error = ERR_LL_DEV_BUSY;
			break;
		}
		/* update gain */
		llHdl->gain[ch] = value;
		/* configure the channel */
		ConfigChan(llHdl, ch);
		break;
        /*--------------------------+
		  |  measuring mode         |
		  +-------------------------*/
	case M36_BIPOLAR:
		if ( (value < 0x00) || (value > 0x01) ) {
			error = ERR_LL_ILL_PARAM;
			break;
		}
		/* update measuring mode */
		llHdl->bipolar = value;
		/* initialize all channels */
		InitAllChan(llHdl);
		break;
        /*--------------------------+
		  | trigger mode (int/ext)  |
		  +--------------------------*/
	case M36_EXT_TRIG:
		if ( (value < 0) || (value > 1) ) {
			error = ERR_LL_ILL_PARAM;
			break;
		}
		if (value){
			MSETMASK_D16(llHdl->ma, CTRL_REG, EXT);	/* external */
		}
		else{
			MCLRMASK_D16(llHdl->ma, CTRL_REG, EXT); /* internal */
		}
		llHdl->extTrig = value;
		break;
		/*-------------------------+
		  |  start calibration      |
		  +-------------------------*/
	case M36_CALIBRATE:
		/* M36N autocalibrates itself */
		if( llHdl->modType != MOD_ID_M36N )
			error = Calibrate(llHdl);
		else
			error = ERR_LL_ILL_FUNC;
		break;

/* --- Flash Functions for internal use only! --- */

		/*-------------------------+
		  | M36N Erase Calib Data   |
		  +-------------------------*/

	case M36_FLASH_ERASE:
		M36_FlashUnlockBlock( llHdl, 0xf0000 );
		M36_FlashEraseBlock( llHdl,  0xf0000 );
		M36_FlashLockBlock( llHdl, 	 0xf0000 );
		OSS_MikroDelay(llHdl->osHdl, 1000);
		break;

		/*--------------------------+
		  | M36N Flash Block Write  |
		  +-------------------------*/
	case M36_BLK_FLASH:

		dataP = (u_int16*)sg->data;
		M36_FlashUnlockBlock( llHdl, 0xf0000 );
		for ( i = 0; i < 0x800; i +=2 )
			M36_FlashWriteWord( llHdl, 0xff800 + i, *dataP++ );
		M36_FlashLockBlock( llHdl, 0xf0000 );

		break;

		/*--------------------------+
		 |  MBUF + unknown          |
		 +--------------------------*/
	default:
		if (M_BUF_CODE(code))
			error = MBUF_SetStat(llHdl->bufHdl, NULL, code, value);
		else
			error = ERR_LL_UNK_CODE;
    }

	return(error);
}

/****************************** M36_GetStat **********************************
 *
 *  Description:  Get driver status
 *
 *                Following status codes are supported:
 *
 *                Code                 Description                Values
 *                -------------------  -------------------------  ----------
 *                M_LL_DEBUG_LEVEL     driver debug level         see dbg.h
 *                M_LL_CH_NUMBER       number of channels         8 or 16
 *                M_LL_CH_DIR          direction of curr ch       M_CH_IN
 *                M_LL_CH_LEN          length of curr chan [bits] 16
 *                M_LL_CH_TYP          description of curr ch     M_CH_ANALOG
 *                M_LL_IRQ_COUNT       interrupt counter          0..max
 *                M_LL_ID_CHECK        eeprom is checked          0..1
 *                M_LL_ID_SIZE         eeprom size [bytes]        128
 *                M_LL_BLK_ID_DATA     eeprom raw data            -
 *                M_MK_BLK_REV_ID      ident function table ptr   -
 *                M_BUF_xxx            input buffer codes         see MDIS
 *                -------------------  -------------------------  ----------
 *                M36_CH_ENABLE        enable/disable curr ch     0..1
 *                                      0 = disable
 *                                      1 = enable
 *                M36_CH_GAIN          gain factor of curr ch     0..3
 *					                    0 = factor 1
 *                                      1 = factor 2
 *                                      2 = factor 4
 *                                      3 = factor 8
 *                M36_BIPOLAR          measuring mode for all ch  0..1
 *                                      0 = unipolar
 *                                      1 = bipolar
 *                M36_EXT_TRIG         defines sampling mode      0..1
 *                                      0 = internal trigger
 *                                      1 = external trigger
 *                M36_EXT_PIN          state of binary input      0..1
 *                M36_SINGLE_ENDED     def. type of input adapter 0..1
 *                                      0 = differential
 *                                      1 = single ended
 *                M36_NBR_ENABLED_CH    number of enabled channels 0..16
 *
 *---------------------------------------------------------------------------
 *  Input......:  llHdl           ll handle
 *                code            status code
 *                ch              current channel
 *                value32_or_64P  ptr to block data struct (M_SG_BLOCK)  (*)
 *                (*) = for block status codes
 *  Output.....:  value32_or_64P  data ptr or
 *                                ptr to block data struct (M_SG_BLOCK)  (*)
 *                return          success (0) or error code
 *                (*) = for block status codes
 *  Globals....:  ---
 ****************************************************************************/
static int32 M36_GetStat(
    LL_HANDLE *llHdl,
    int32  code,
    int32  ch,
    INT32_OR_64 *value32_or_64P
	)
{

	int32 *valueP		  = (int32*)value32_or_64P;	/* pointer to 32bit value  */
	INT32_OR_64	*value64P = value32_or_64P;		 	/* stores 32/64bit pointer  */
	M_SG_BLOCK	*blk	  = (M_SG_BLOCK*)value32_or_64P; /* stores block struct pointer */

	u_int16 *dataP;
	/* u_int32 *dataP; */
	u_int32 i = 0;
	int32 error = ERR_SUCCESS;
	u_int32 lo=0, hi=0, lo1=0, hi1=0, lo2=0, hi2=0, longval=0;

    DBGWRT_1((DBH, "LL - M36_GetStat: ch=%d code=0x%04x\n",  ch,code));

    switch(code)
    {
        /*--------------------------+
		  |  debug level              |
		  +--------------------------*/
	case M_LL_DEBUG_LEVEL:
		*valueP = llHdl->dbgLevel;
		break;
        /*--------------------------+
		  |  nr of channels           |
		  +--------------------------*/
	case M_LL_CH_NUMBER:
		*valueP = llHdl->chNumber;
		break;
        /*--------------------------+
		  |  channel direction        |
		  +--------------------------*/
	case M_LL_CH_DIR:
		*valueP = M_CH_IN;
		break;
        /*--------------------------+
		  |  channel length [bits]    |
		  +--------------------------*/
	case M_LL_CH_LEN:
		*valueP = 16;
		break;
        /*--------------------------+
		  |  channel type info        |
		  +--------------------------*/
	case M_LL_CH_TYP:
		*valueP = M_CH_ANALOG;
		break;
        /*--------------------------+
		  |  irq counter              |
		  +--------------------------*/
	case M_LL_IRQ_COUNT:
		*valueP = llHdl->irqCount;
		break;
        /*--------------------------+
		  |  id prom check enabled    |
		  +--------------------------*/
	case M_LL_ID_CHECK:
		*valueP = llHdl->idCheck;
		break;
        /*--------------------------+
		  |   id prom size            |
		  +--------------------------*/
	case M_LL_ID_SIZE:
		*valueP = MOD_ID_SIZE;
		break;
        /*--------------------------+
		  |   id prom data            |
		  +--------------------------*/
	case M_LL_BLK_ID_DATA:
	{
		u_int32 n;
		u_int16 *dataP = (u_int16*)blk->data;

		if (blk->size < MOD_ID_SIZE)		/* check buf size */
			return(ERR_LL_USERBUF);

		for (n=0; n<MOD_ID_SIZE/2; n++)		/* read MOD_ID_SIZE/2 words */
			*dataP++ = (int16)m_read((U_INT32_OR_64)llHdl->ma, (int8)n);

		break;
	}
	/*--------------------------+
	  |   ident table pointer     |
	  |   (treat as non-block!)   |
	  +--------------------------*/
	case M_MK_BLK_REV_ID:
		*value64P = (INT32_OR_64)&llHdl->idFuncTbl;
		break;
        /*--------------------------+
		  | channel enable            |
		  +--------------------------*/
	case M36_CH_ENABLE:
		*valueP = (int32)llHdl->enable[ch];
		break;
        /*--------------------------+
		  |  channel gain             |
		  +--------------------------*/
	case M36_CH_GAIN:
		*valueP = (int32)llHdl->gain[ch];
		break;
        /*--------------------------+
		  |  measuring mode           |
		  +--------------------------*/
	case M36_BIPOLAR:
		*valueP = (int32)llHdl->bipolar;
		break;
        /*--------------------------+
		  | trigger mode              |
		  +--------------------------*/
	case M36_EXT_TRIG:
		*valueP = (int32)llHdl->extTrig;
		break;
        /*--------------------------+
		  |  binary input             |
		  +--------------------------*/
	case M36_EXT_PIN:
		*valueP =  ((MREAD_D16(llHdl->ma, STAT_REG) & BIN) ? 1 : 0);
		break;
        /*--------------------------+
		  | single ended            |
		  +-------------------------*/
	case M36_SINGLE_ENDED:
		*valueP = (int32)llHdl->singleEnded;
		break;
        /*--------------------------+
		  | number of enabled chan  |
		  +-------------------------*/
	case M36_NBR_ENABLED_CH:
		*valueP = llHdl->nbrEnabledCh;
		break;
        /*--------------------------+
		  | Dump Register space     |
		  +--------------------------*/
	case M36_REG_DUMP:
		DBGWRT_2((DBH, "LL - M36_REG_DUMP Dump Registers:"));
		for ( i = 0; i < 0x100; i +=2 ) {
			if (!( i % 0x10))
				DBGWRT_2((DBH, "\n  0x%02x ", i));
			DBGWRT_2((DBH, "%04x ", MREAD_D16(llHdl->ma, i)));
		}
		*valueP = 0;
		break;


		/* ---	For MEN internal use only --- */

		/*-------------------------+
		 | Flash Block Read        |
		 +-------------------------*/
	case M36_BLK_FLASH:
		dataP 	= (u_int16*)blk->data;
		for ( i = 0; i < 0x800; i += 2 )
			*dataP++=(u_int16)M36_FlashRead(llHdl, 0xff800 + i);
		break;

        /*--------------------------+
		 | Get 18bit raw ADC values |
		 +--------------------------*/
	case M36_GET_RAWDAT:
		do {
			/*
			 * FPGA writes the 18bit raw data Registers in swapped manner
			 * ( compared to all other registers )
			 */
			hi1 = OSS_Swap16(MREAD_D16( llHdl->ma, (ch*4) + 0x82 ));
			lo1 = OSS_Swap16(MREAD_D16( llHdl->ma, (ch*4) + 0x80 ));
			hi2 = OSS_Swap16(MREAD_D16( llHdl->ma, (ch*4) + 0x82 ));
			lo2 = OSS_Swap16(MREAD_D16( llHdl->ma, (ch*4) + 0x80 ));

		} while((lo1 != lo2) || (hi1 != hi2)); /* stable data? */

		hi = ((hi1 << 8) & 0xff00 ) | ((hi1 >> 8) & 0xff );
		lo = ((lo1 << 8) & 0xff00 ) | ((lo1 >> 8) & 0xff );

		if (hi & 0x2) /* fill sign 18bit up to become a 32bit int */
			hi |=0xfffc;
		else
			hi &=0x0001;

		longval = (hi << 16) | lo;
		*valueP = longval;
		break;

        /*--------------------------+
		 |  MBUF + unknown          |
		 +--------------------------*/
	default:
		if (M_BUF_CODE(code))
			error = MBUF_GetStat(llHdl->bufHdl, NULL, code, valueP);
		else
			error = ERR_LL_UNK_CODE;
    }

	return(error);
}


/******************************* M36_BlockRead *******************************
 *
 *  Description:  Read data block from device
 *
 *                Following block i/o modes are supported:
 *
 *                   M_BUF_USRCTRL         direct input
 *                   M_BUF_RINGBUF         buffered input
 *                   M_BUF_RINGBUF_OVERWR  buffered input
 *                   M_BUF_CURRBUF         buffered input
 *
 *                (Can be defined via M_BUF_RD_MODE setstat, see MDIS-Doc.)
 *
 *                Direct Input Mode
 *                -----------------
 *                For the M_BUF_USRCTRL mode, the function reads all input
 *                channels, which are enabled for block i/o in ascending order
 *                into the given data buffer:
 *
 *                   +---------+
 *                   |  word 0 |  first enabled input channel
 *                   +---------+
 *                   |  word 1 |
 *                   +---------+
 *                   |   ...   |
 *                   +---------+
 *                   |  word k |  last enabled input channel
 *                   +---------+
 *
 *                The maximum size (number of words) which can be read depends
 *                on the number of enabled input channels and can be queried
 *                via the M36_NBR_ENABLED_CH getstat.
 *
 *                If no input channel is enabled ERR_LL_READ is returned.
 *
 *                Buffered Input Mode
 *                -------------------
 *                For all other modes, the function copies requested number
 *                of bytes from the input buffer to the given data buffer.
 *                The interrupt of the carrier board must be enabled for
 *                buffered input modes. (see also function M36_Irq)
 *
 *                For details on buffered input modes refer to the MDIS-Doc.
 *
 *---------------------------------------------------------------------------
 *  Input......:  llHdl        ll handle
 *                ch           current channel
 *                buf          data buffer
 *                size         data buffer size in bytes
 *  Output.....:  nbrRdBytesP  number of read bytes
 *                return       success (0) or error code
 *  Globals....:  ---
 ****************************************************************************/
static int32 M36_BlockRead(
     LL_HANDLE *llHdl,
     int32     ch,
     void      *buf,
     int32     size,
     int32     *nbrRdBytesP
)
{
	u_int16 *bufP = (u_int16*)buf;
    u_int32 n;
	int32 bufMode;
	int32 error;

    DBGWRT_1((DBH, "LL - M36_BlockRead: ch=%d, size=%d\n",ch,size));

	/* get current buffer mode */
	if ((error = MBUF_GetBufferMode(llHdl->bufHdl, &bufMode)))
		return(error);

	/*-------------------------+
	| read from hardware       |
	+-------------------------*/
	if (bufMode == M_BUF_USRCTRL) {
		/* check if any channel to read */
		if (llHdl->nbrEnabledCh == 0)
			return(ERR_LL_READ);

		/* check size */
		if (size < (CH_BYTES * llHdl->nbrEnabledCh))
			return(ERR_LL_USERBUF);

		/* read all enabled channels */
		for (n=0; n<llHdl->chNumber; n++)
			if (llHdl->enable[n])
				*bufP++ = MREAD_D16(llHdl->ma, llHdl->dataReg[n]);

		*nbrRdBytesP = (int32)( (INT32_OR_64)bufP - (INT32_OR_64)buf );
	}

	/*-------------------------+
	| read from input buffer   |
	+-------------------------*/
	else {
		/* read from buffer */
		if ((error = MBUF_Read(llHdl->bufHdl,
							   (u_int8*)bufP, size, nbrRdBytesP)))
			return(error);
	}

	return(ERR_SUCCESS);
}

/****************************** M36_BlockWrite *******************************
 *
 *  Description:  Write data block to device
 *
 *                The function is not supported and returns always an
 *                ERR_LL_ILL_FUNC error.
 *
 *---------------------------------------------------------------------------
 *  Input......:  llHdl        ll handle
 *                ch           current channel
 *                buf          data buffer
 *                size         data buffer size
 *  Output.....:  nbrWrBytesP  number of written bytes
 *                return       success (0) or error code
 *  Globals....:  ---
 ****************************************************************************/
static int32 M36_BlockWrite(	/* nodoc */
     LL_HANDLE *llHdl,
     int32     ch,
     void      *buf,
     int32     size,
     int32     *nbrWrBytesP
)
{
    DBGWRT_1((DBH, "LL - M36_BlockWrite: ch=%d, size=%d\n",ch,size));

	/* return nr of written bytes */
	*nbrWrBytesP = 0;

	return(ERR_LL_ILL_FUNC);
}


/****************************** M36_Irq *************************************
 *
 *  Description:  Interrupt service routine
 *
 *                The interrupt is triggered every time the internal address
 *                pointer of the current data element points to data element
 *                0 (when all enabled channels are sampled).
 *
 *                If an input buffer is used, all input channels, which
 *                are enabled for block i/o are stored in ascending order
 *                in the input buffer:
 *
 *                   +---------+
 *                   |  word 0 |  first enabled input channel
 *                   +---------+
 *                   |  word 1 |
 *                   +---------+
 *                   |   ...   |
 *                   +---------+
 *                   |  word k |  last enabled input channel
 *                   +---------+
 *
 *---------------------------------------------------------------------------
 *  Input......:  llHdl    ll handle
 *  Output.....:  return   LL_IRQ_DEVICE	irq caused from device
 *                         LL_IRQ_DEV_NOT   irq not caused from device
 *                         LL_IRQ_UNKNOWN   unknown
 *  Globals....:  ---
 ****************************************************************************/
static int32 M36_Irq(
   LL_HANDLE *llHdl
)
{
	u_int16 *bufP;
	int32	got;
	u_int32	ch;
	int32	nbrRdCh = 0;	/* number of read channels */
	int32	nbrOfBlocks;

    IDBGWRT_1((DBH, "LL - M36_Irq:\n"));

	/*----------------------+
	| reset irq             |
	+----------------------*/
	MSETMASK_D16(llHdl->ma, CTRL_REG, RST);

	/*----------------------+
	| fill buffer           |
	+----------------------*/
	/* get buffer ptr - check for overrun ? */
	if( (bufP = (u_int16*)MBUF_GetNextBuf(llHdl->bufHdl,
							llHdl->nbrEnabledCh, &got)) != 0 ) {

		/* for all channels */
		for( ch=0; ch<llHdl->chNumber; ch++ ) {

			/* read enabled channel */
			if( llHdl->enable[ch] ) {

				/* fill buffer entry */
				*bufP++ = MREAD_D16(llHdl->ma, llHdl->dataReg[ch]);
				nbrRdCh++;

				if( (nbrRdCh < llHdl->nbrEnabledCh)	/* read another channel ? */
					&& (nbrRdCh == got) ) {			/* got space full ? */

					/* calculate missing buffer space */
					nbrOfBlocks = llHdl->nbrEnabledCh - nbrRdCh;

					/* get missing buffer space - wrap around buffer */
					if( (bufP = (u_int16*)MBUF_GetNextBuf(llHdl->bufHdl,
											nbrOfBlocks, &got)) == 0 ) {
						/* wrap around failed */
						IDBGWRT_ERR((DBH,
									 "*** LL - M36_Irq: wrap around failed\n"));
						break;
					}
					IDBGWRT_3((DBH,
						 "LL - M36_Irq: nbrRdCh=%d, nbrOfBlocks=%d, got=%d\n",
						nbrRdCh,nbrOfBlocks,got));
				}
			}
		} /*for*/

        MBUF_ReadyBuf( llHdl->bufHdl );  /* blockread ready */
    }
	llHdl->irqCount++;

	return(LL_IRQ_UNKNOWN);		/* say: unknown */
}

/****************************** M36_Info ************************************
 *
 *  Description:  Get information about hardware and driver requirements.
 *
 *                Following info codes are supported:
 *
 *                Code                      Description
 *                ------------------------  -----------------------------
 *                LL_INFO_HW_CHARACTER      hardware characteristics
 *                LL_INFO_ADDRSPACE_COUNT   nr of required address spaces
 *                LL_INFO_ADDRSPACE         address space information
 *                LL_INFO_IRQ               interrupt required
 *                LL_INFO_LOCKMODE          process lock mode required
 *
 *                The LL_INFO_HW_CHARACTER code returns all address and
 *                data modes (OR'ed), which are supported from the
 *                hardware (MDIS_MAxx, MDIS_MDxx).
 *
 *                The LL_INFO_ADDRSPACE_COUNT code returns the number
 *                of address spaces used from the driver.
 *
 *                The LL_INFO_ADDRSPACE code returns information about one
 *                specific address space (MDIS_MAxx, MDIS_MDxx). The returned
 *                data mode represents the widest hardware access used from
 *                the driver.
 *
 *                The LL_INFO_IRQ code returns, if the driver supports an
 *                interrupt routine (TRUE or FALSE).
 *
 *                The LL_INFO_LOCKMODE code returns, which process locking
 *                mode is required from the driver (LL_LOCK_xxx).
 *
 *---------------------------------------------------------------------------
 *  Input......:  infoType	   info code
 *                ...          argument(s)
 *  Output.....:  return       success (0) or error code
 *  Globals....:  ---
 ****************************************************************************/
static int32 M36_Info(
   int32  infoType,
   ...
)
{
    int32   error = ERR_SUCCESS;
    va_list argptr;

    va_start(argptr, infoType );

    switch(infoType) {
		/*-------------------------------+
        |  hardware characteristics      |
        |  (all addr/data modes OR'ed)   |
        +-------------------------------*/
        case LL_INFO_HW_CHARACTER:
		{
			u_int32 *addrModeP = va_arg(argptr, u_int32*);
			u_int32 *dataModeP = va_arg(argptr, u_int32*);

			*addrModeP = MDIS_MA08;
			*dataModeP = MDIS_MD08 | MDIS_MD16;
			break;
	    }
		/*-------------------------------+
        |  nr of required address spaces |
        |  (total spaces used)           |
        +-------------------------------*/
        case LL_INFO_ADDRSPACE_COUNT:
		{
			u_int32 *nbrOfAddrSpaceP = va_arg(argptr, u_int32*);

			*nbrOfAddrSpaceP = ADDRSPACE_COUNT;
			break;
	    }
		/*-------------------------------+
        |  address space type            |
        |  (widest used data mode)       |
        +-------------------------------*/
        case LL_INFO_ADDRSPACE:
		{
			u_int32 addrSpaceIndex = va_arg(argptr, u_int32);
			u_int32 *addrModeP = va_arg(argptr, u_int32*);
			u_int32 *dataModeP = va_arg(argptr, u_int32*);
			u_int32 *addrSizeP = va_arg(argptr, u_int32*);

			if (addrSpaceIndex >= ADDRSPACE_COUNT)
				error = ERR_LL_ILL_PARAM;
			else {
				*addrModeP = MDIS_MA08;
				*dataModeP = MDIS_MD16;
				*addrSizeP = ADDRSPACE_SIZE;
			}

			break;
	    }
		/*-------------------------------+
        |   interrupt required           |
        +-------------------------------*/
        case LL_INFO_IRQ:
		{
			u_int32 *useIrqP = va_arg(argptr, u_int32*);

			*useIrqP = USE_IRQ;
			break;
	    }
		/*-------------------------------+
        |   process lock mode            |
        +-------------------------------*/
        case LL_INFO_LOCKMODE:
		{
			u_int32 *lockModeP = va_arg(argptr, u_int32*);

			*lockModeP = LL_LOCK_CALL;
			break;
	    }
		/*-------------------------------+
        |   (unknown)                    |
        +-------------------------------*/
        default:
          error = ERR_LL_ILL_PARAM;
    }

    va_end(argptr);
    return(error);
}

/*******************************  Ident  ************************************
 *
 *  Description:  Return ident string
 *
 *---------------------------------------------------------------------------
 *  Input......:  -
 *  Output.....:  return  ptr to ident string
 *  Globals....:  -
 ****************************************************************************/
static char* Ident( void )
{
    return( "M36 - M36 low level driver: $Id: m36_drv.c,v 1.11 2010/09/21 17:47:59 ts Exp $" );
}

/********************************* Cleanup **********************************
 *
 *  Description:  Close all handles, free memory and return error code
 *		          NOTE: The ll handle is invalid after calling this function
 *
 *---------------------------------------------------------------------------
 *  Input......:  llHdl		ll handle
 *                retCode	return value
 *  Output.....:  return	retCode
 *  Globals....:  -
 ****************************************************************************/
static int32 Cleanup(	/* nodoc */
   LL_HANDLE    *llHdl,
   int32        retCode
)
{
    /*------------------------------+
    |  close handles                |
    +------------------------------*/
	/* clean up desc */
	if (llHdl->descHdl)
		DESC_Exit(&llHdl->descHdl);

	/* clean up buffer */
	if (llHdl->bufHdl)
		MBUF_Remove(&llHdl->bufHdl);

	/* cleanup debug */
	DBGEXIT((&DBH));

    /*------------------------------+
    |  free memory                  |
    +------------------------------*/
    /* free my handle */
    OSS_MemFree(llHdl->osHdl, (int8*)llHdl, llHdl->memAlloc);

    /*------------------------------+
    |  return error code            |
    +------------------------------*/
	return(retCode);
}

/******************************** PldLoad ***********************************
 *
 *  Description:  Loading PLD with binary data.
 *                - binary data is stored in field 'M36_PldData'
 *
 *---------------------------------------------------------------------------
 *  Input......:  llHdl		ll handle
 *  Output.....:  ---
 *  Globals....:  ---
 ****************************************************************************/
static void PldLoad(	/* nodoc */
	LL_HANDLE *llHdl
)
{
	u_int8	ctrl = 0x00;					/* control word */
	u_int8  *dataP = (u_int8*)M36_PldData;	/* point to binary data */
	u_int8	byte;							/* current byte */
	u_int8	n;								/* count */
	u_int32	size;							/* size of binary data */

	DBGWRT_1((DBH, "LL - M36: PldLoad\n"));

	/* read+skip size */
	size  = (u_int32)(*dataP++) << 24;
	size |= (u_int32)(*dataP++) << 16;
	size |= (u_int32)(*dataP++) <<  8;
	size |= (u_int32)(*dataP++);

	/* for all bytes */
	while(size--) {
		byte = *dataP++;	/* get next data byte */
		n = 4;			/* data byte: 4*2 bit */

		/* write data 2 bits */
		while(n--) {
			/* clear TCK */
      		bitclr (ctrl,TCK);
			MWRITE_D16(llHdl->ma, LOAD_REG, ctrl);

			/* write TDO/TMS bits */
			bitmove( ctrl, TDO, byte & 0x01);
			bitmove( ctrl, TMS, byte & 0x02);
			MWRITE_D16(llHdl->ma, LOAD_REG, ctrl);

			/* set TCK (pulse) */
			bitset (ctrl,TCK);
			MWRITE_D16(llHdl->ma, LOAD_REG, ctrl);

			/* shift byte */
			byte >>= 2;
		}
	}
}

/******************************* InitAllChan ********************************
 *
 *  Description:  Initialize all enabled channels
 *                - config data elements
 *                  (create ring buffer with n entries, n=nbr of enabled ch)
 *                - set for each channel: measuring mode and gain factor
 *
 *---------------------------------------------------------------------------
 *  Input......:  llHdl		ll handle
 *  Output.....:  ---
 *  Globals....:  ---
 ****************************************************************************/
static void InitAllChan(	/* nodoc */
	LL_HANDLE *llHdl
)
{
	u_int16 ch;			/* current channel */
	u_int16 currDat;	/* current data element */
	u_int16 prevDat;	/* previous data element */

    DBGWRT_1((DBH, "LL - M36: InitAllChan\n"));

	/* beginn with first data element */
	prevDat = (int16)llHdl->nbrEnabledCh - 1;
	currDat = 0;

	/* search for enabled channels */
	for (ch=0; ch<llHdl->chNumber; ch++) {
		if ( (llHdl->sampleAll) || (llHdl->enable[ch])) {
			/* assign data register to channel */
			llHdl->dataReg[ch] = DATA_REG(currDat);
			/* assign config register to channel */
			llHdl->cfgReg[ch] = CFG_REG(prevDat);

			/* set address register of previous data element */
			MWRITE_D16(llHdl->ma, ADR_REG(prevDat), currDat);

			/* update prevDat and currDat */
			prevDat = currDat;
			currDat++;
			/* configure the channel */
			ConfigChan(llHdl, ch);
		}
	}
}

/******************************* ConfigChan *********************************
 *
 *  Description:  Configure the specified channel
 *                - set measuring mode
 *                - set gain factor
 *
 *---------------------------------------------------------------------------
 *  Input......:  llHdl		ll handle
 *                ch        current channel
 *  Output.....:  ---
 *  Globals....:  ---
 ****************************************************************************/
static void ConfigChan(	/* nodoc */
	LL_HANDLE *llHdl,
	int32     ch
)
{
	u_int16 cfg;		/* config data */

    DBGWRT_1((DBH, "LL - M36: ConfigChan\n"));

	/* set config register for the channel */
	cfg = (u_int16)(llHdl->bipolar  << 7) |
		  (u_int16)(llHdl->gain[ch] << 4) |
		  (u_int16) ch;

	MWRITE_D16(llHdl->ma, llHdl->cfgReg[ch], cfg);

}

/******************************* Calibrate ***********************************
 *
 *  Description:  Start auto-calibration.
 *
 *                Returns ERR_LL_DEV_NOTRDY error code if the device is not
 *                ready (a timeout occurs).
 *
 *---------------------------------------------------------------------------
 *  Input......:  llHdl		ll handle
 *  Output.....:  return    success (0) or error code
 *  Globals....:  ---
 ****************************************************************************/
static int32 Calibrate(	/* nodoc */
	LL_HANDLE *llHdl
)
{
	u_int16 toutLoopCount, regVal;
	#define TOUT_DELAY 1	/* delay [msec]				 */
	#define TOUT_LIMIT 100	/* limit [TOUT_DELAY * msec] */

    DBGWRT_1((DBH, "LL - M36: Calibrate\n"));

	/* force internal trigger */
	if (llHdl->extTrig){
		MCLRMASK_D16(llHdl->ma, CTRL_REG, EXT);
	}
	/* wait for sample */
	MSETMASK_D16(llHdl->ma, CTRL_REG, RST);			/* irq reset */
	toutLoopCount = 0;
	do{
		OSS_Delay( llHdl->osHdl, TOUT_DELAY );
		regVal = MREAD_D16(llHdl->ma, STAT_REG);
		if( toutLoopCount++ > TOUT_LIMIT )
			goto ABORT;
	}while( regVal & IRQ );				/* wait for irq=0 (active) */

	/* calibration mode ON */
	MSETMASK_D16(llHdl->ma, CTRL_REG, CAL);

	/* wait for calibration started */
	toutLoopCount = 0;
	do{
		OSS_Delay( llHdl->osHdl, TOUT_DELAY );
		regVal = MREAD_D16(llHdl->ma, CTRL_REG);
		if( toutLoopCount++ > TOUT_LIMIT )
			goto ABORT;
	}while( regVal & CAL );

	/* wait for calibration ready */
	toutLoopCount = 0;
	do{
		OSS_Delay( llHdl->osHdl, TOUT_DELAY );
		regVal = MREAD_D16(llHdl->ma, STAT_REG);
		if( toutLoopCount++ > TOUT_LIMIT )
			goto ABORT;
	} while( regVal & BUSY );


	OSS_Delay( llHdl->osHdl, 100);	/* (ca. 43msec required) */

	/* restore trigger */
	if (llHdl->extTrig){
		MSETMASK_D16(llHdl->ma, CTRL_REG, EXT);
	}

	return(ERR_SUCCESS);

ABORT:
	DBGWRT_ERR((DBH," *** LL - M36: Calibrate: timeout after %dmsec\n",
		TOUT_DELAY*TOUT_LIMIT));

	return(ERR_LL_DEV_NOTRDY);
}

/*****************************************************************************/
/**	M36_FlashRead, helper function to access M36N Stratix Flash
 *
 */
static int32 M36_FlashRead(
	LL_HANDLE *llHdl,
	u_int32 address 		/* nodoc  */
	)
{

	u_int16 value = 0xffff;
    MWRITE_D16( llHdl->ma, M36_FLASH_ADDHI, HIWD(Z45_ADDR(address)) );
    MWRITE_D16( llHdl->ma, M36_FLASH_ADDLO, LOWD(Z45_ADDR(address)) );
    value = MREAD_D16( llHdl->ma, M36_FLASH_DATA );

    DBGWRT_3((DBH,"M36_FlashRead: addr 0x%08x = 0x%04x\n", address, value));
	return value;

}

/*****************************************************************************/
/**	M36_FlashWrite, helper function to access M36N Stratix Flash
 *
 */
static void M36_FlashWrite(
	LL_HANDLE *llHdl,
	u_int32 address,
	u_int16 val 			/* nodoc */
	)
{

	DBGWRT_3((DBH,"M36_FlashWrite addr 0x%08x = 0x%04x\n", address, val));

    MWRITE_D16( llHdl->ma, M36_FLASH_ADDHI, HIWD( Z45_ADDR(address )) );
    MWRITE_D16( llHdl->ma, M36_FLASH_ADDLO, LOWD( Z45_ADDR(address )) );
	MWRITE_D16( llHdl->ma, M36_FLASH_DATA, val);
}

/*****************************************************************************/
/**	M36_FlashReadStatus, helper function to access M36N Stratix Flash
 *
 */
static int32 M36_FlashReadStatus(
	LL_HANDLE *llHdl 	/* nodoc */
	)
{

	int32 retVal = 0;

    /* issue 'Read status Register' command */
	M36_FlashWrite( llHdl, 0, C_RSR );
	retVal = M36_FlashRead( llHdl, 0);
	DBGWRT_3((DBH,"M36_FlashReadStatus = 0x%04x\n", retVal ));
	return retVal;
}

/*****************************************************************************/
/**	M36_FlashWriteWord, helper function to access M36N Stratix Flash
 *
 */
static int32 M36_FlashWriteWord(
	LL_HANDLE *llHdl,
	u_int32 address,
	u_int16 val			/* nodoc */
	)
{

	int32 statusReg=0;
	u_int32 timeout = FL_ACC_TOUT;
//    int32 retVal = 0;

	/* issue 'Write' Command */
	M36_FlashWrite( llHdl, address, C_WRITE );

    /* write data */
	M36_FlashWrite( llHdl, address, val );

    /* read status register */
    do {
		statusReg = M36_FlashReadStatus( llHdl );
		--timeout;
    } while( timeout && !(statusReg & (1<<BIT_DWS)));

    /* full status check */
    /* retVal = FullStatusCheck( adr ); */
    return timeout? 0 : ERR_LL_WRITE;
}

/*****************************************************************************/
/**	M36_FlashUnlockBlock, helper function to access M36N Stratix Flash
 *
 */
static int32 M36_FlashUnlockBlock(
	LL_HANDLE *llHdl,
	u_int32 offset 	/* nodoc */
	)
{
    //u_int16 lockStatus = 0;
	int32 lockStatus = 0;
	u_int32 timeout = FL_ACC_TOUT;

	DBGWRT_3((DBH,"M36_FlashUnlockBlock addr 0x%x\n", offset));

    do {
        /* setup of lock operations */
		M36_FlashWrite( llHdl, offset, C_LOCK_SETUP );

        /* unlock a block */
		M36_FlashWrite( llHdl, offset, C_UNLOCK );

        /* read device identifier information (lock status) */
		M36_FlashWrite( llHdl, offset + OFFSET_LOCK, C_IDENTIFIER );
        lockStatus = M36_FlashRead( llHdl, offset + OFFSET_LOCK);
		--timeout;
    } while( timeout && (lockStatus & (1<< BIT_LOCK )) ); /* unlock ok? */

    /* reach read array mode */
	M36_FlashWrite( llHdl, 0 , C_READ );

	if (!timeout) {
		DBGWRT_ERR((DBH," *** M36_FlashUnlockBlock: Timeout occured!\n"));
		return ERR_LL_WRITE;
	} else {
		return 0;
	}
}

/*****************************************************************************/
/**	M36_FlashLockBlock, helper function to access M36N Stratix Flash
 *
 */
static int32 M36_FlashLockBlock( LL_HANDLE *llHdl, u_int32 offset )
{

	int32 lockStatus = 0;
	u_int32 timeout = FL_ACC_TOUT;

    do {

        /* setup of lock operations */
		M36_FlashWrite( llHdl, offset, C_LOCK_SETUP );

        /* lock this block */
		M36_FlashWrite( llHdl, offset, C_LOCK );

        /* read device identifier information (lock status) */
		M36_FlashWrite( llHdl, offset + OFFSET_LOCK, C_IDENTIFIER );
        lockStatus = M36_FlashRead( llHdl, offset + OFFSET_LOCK);
		--timeout;
    } while( timeout && !(lockStatus & ( 1 << BIT_LOCK )) ); /* lock ok? */

    /* reach read array mode */
	M36_FlashWrite( llHdl,0 , C_READ );

	if (!timeout) {
	DBGWRT_ERR((DBH," *** M36_FlashLockBlock: Timeout occured!\n"));
	return ERR_LL_WRITE;
	} else {
		return 0;
	}

}

/*****************************************************************************/
/**	M36_FlashEraseBlock, helper function to access M36N Stratix Flash
 *
 */
static int32 M36_FlashEraseBlock(
	LL_HANDLE *llHdl,
	u_int32 offset 		/* nodoc */
	)
{

	int32 statusReg = 0;
	u_int32 timeout = FL_ACC_TOUT;

	/* setup of block erase operation */
	M36_FlashWrite( llHdl, offset, C_ERASE );

	/* erase confirm */
	M36_FlashWrite( llHdl, offset, C_CONFIRM );

    /* read status register */
    do{
        statusReg = M36_FlashReadStatus( llHdl );
		--timeout;
    } while( timeout &&  !(statusReg & ( 1 << BIT_DWS )) );

    /* full status check */
    /* retVal = FullStatusCheck( adr ); */

    /* reach read array mode */
	M36_FlashWrite( llHdl,0 , C_READ );

	if (!timeout) {
		DBGWRT_ERR((DBH," *** M36_FlashEraseBlock: Timeout occured!\n"));
		return ERR_LL_WRITE;
	} else {
		return 0;
	}
}
