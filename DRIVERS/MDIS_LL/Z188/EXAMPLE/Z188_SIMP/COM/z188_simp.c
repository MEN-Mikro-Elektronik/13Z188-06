/****************************************************************************
 ************                                                    ************
 ************                   M36_SIMP                        ************
 ************                                                    ************
 ****************************************************************************
 *
 *       Author: ds
 *        $Date: 2009/09/23 17:48:23 $
 *    $Revision: 1.7 $
 *
 *  Description: Simple example program for the M36 driver.
 *               Configure and read all channels.
 *
 *     Required: MDIS user interface library
 *     Switches: NO_MAIN_FUNC	(for systems with one namespace)
 *
 *-------------------------------[ History ]---------------------------------
 *
 * $Log: m36_simp.c,v $
 * Revision 1.7  2009/09/23 17:48:23  MRoth
 * R: Porting to MDIS5
 * M: added support for 64bit (MDIS_PATH)
 *
 * Revision 1.6  2002/09/03 09:23:48  UFranke
 * bugfix:
 *   - m36_simp was defined twice for VxWorks
 *
 * Revision 1.5  2002/06/13 13:59:57  kp
 * cosmetics
 *
 * Revision 1.4  1998/11/27 13:38:36  see
 * hex output format was wrong
 *
 * Revision 1.3  1998/11/26 16:18:32  Schmidt
 * now reads all channels
 *
 * Revision 1.2  1998/11/18 14:43:09  see
 * Usage: function description was empty
 *
 * Revision 1.1  1998/11/17 15:30:44  Schmidt
 * Added by mcvs
 *
 *---------------------------------------------------------------------------
 * (c) Copyright 1998 by MEN mikro elektronik GmbH, Nuernberg, Germany
 ****************************************************************************/

static const char RCSid[]="$Id: m36_simp.c,v 1.7 2009/09/23 17:48:23 MRoth Exp $";

#include <stdio.h>
#include <string.h>
#include <MEN/men_typs.h>
#include <MEN/mdis_api.h>
#include <MEN/mdis_err.h>
#include <MEN/usr_oss.h>
#include <MEN/usr_utl.h>
#include <MEN/m36_drv.h>

/*--------------------------------------+
|   PROTOTYPES                          |
+--------------------------------------*/
static MDIS_PATH _m36_simp(char *device);
static void PrintError(char *info);

/********************************* main *************************************
 *
 *  Description: MAIN entry (not used in systems with one namespace)
 *
 *---------------------------------------------------------------------------
 *  Input......: argc, argv		command line arguments/counter
 *  Output.....: -
 *  Globals....: -
 ****************************************************************************/
int main(int argc, char *argv[])
{
	if (argc < 2 || strcmp(argv[1],"-?")==0) {
		printf("Syntax: m36_simp <device> <chan>\n");
		printf("Function: M36 example for reading all channels\n");
		printf("Options:\n");
		printf("    device       device name\n");
		printf("\n");
		return(1);
	}

    return _m36_simp(argv[1]);
}


/******************************* _m36_simp **********************************
 *
 *  Description: Example (directly called in systems with one namespace)
 *               - open path
 *               - enable and configure all channels
 *               - read all channels
 *               - close path
 *
 *---------------------------------------------------------------------------
 *  Input......: device    device name
 *               chan      channel number
 *  Output.....: return    success (0) or error (1)
 *  Globals....: -
 ****************************************************************************/
static MDIS_PATH _m36_simp(char *device)
{
	MDIS_PATH path=0;
	int32	value, ch, chanNbr;
	double	volt;

	/*--------------------+
    |  open path          |
    +--------------------*/
	if ((path = M_open(device)) < 0) {
		PrintError("open");
		return(1);
	}

	/*--------------------+
    |  config             |
    +--------------------*/
	/* set measuring mode: bipolar */
	if ((M_setstat(path, M36_BIPOLAR, 1)) < 0) {
		PrintError("setstat M36_BIPOLAR");
		goto abort;
	}
	/* set trigger mode: intern */
	if ((M_setstat(path, M36_EXT_TRIG, 0)) < 0) {
		PrintError("setstat M36_EXT_TRIG");
		goto abort;
	}
	/* get number of channels */
	if ((M_getstat(path, M_LL_CH_NUMBER, &chanNbr)) < 0) {
		PrintError("getstat M_LL_CH_NUMBER");
		goto abort;
	}

	/* enable all channels, set gain */
	for (ch=0; ch<chanNbr; ch++) {
		/* set channel */
		if ((M_setstat(path, M_MK_CH_CURRENT, ch)) < 0) {
			PrintError("setstat M_MK_CH_CURRENT");
			goto abort;
		}
		/* enable channel */
		if ((M_setstat(path, M36_CH_ENABLE, 1)) < 0) {
			PrintError("setstat M36_CH_ENABLE");
			goto abort;
		}
		/* set gain: 1x */
		if ((M_setstat(path, M36_CH_GAIN, 0)) < 0) {
			PrintError("setstat M36_CH_GAIN");
			goto abort;
		}
	}

	/* select channel 0 */
	if ((M_setstat(path, M_MK_CH_CURRENT, 0)) < 0) {
		PrintError("setstat M_MK_CH_CURRENT");
		goto abort;
	}
	/* set autoincrement mode */
	if ((M_setstat(path, M_MK_IO_MODE, M_IO_EXEC_INC)) < 0) {
		PrintError("setstat M_MK_CH_CURRENT");
		goto abort;
	}

	/*--------------------+
    |  read all channels  |
    +--------------------*/
	printf("measuring mode : bipolar\n");
	printf("trigger mode   : intern\n");
	printf("gain factor    : 1x\n\n");
	printf("--chan------value----voltage-----\n");

	for (ch=0; ch<chanNbr; ch++) {
		/* read channel and increment */
		if ((M_read(path,&value)) < 0) {
			PrintError("read");
			goto abort;
		}
		/* calculate voltage */
		volt = (int16)value * 20.0 / 0xffff;	/* bipolar */
		/* print values */
		printf("    %2d     0x%04x  =  %6.3f Volt\n",
				(int)ch, (int)value, volt);
	}

	/*--------------------+
    |  cleanup            |
    +--------------------*/
	abort:
	if (M_close(path) < 0)
		PrintError("close");

	return(0);
}

/********************************* PrintError ********************************
 *
 *  Description: Print MDIS error message
 *
 *---------------------------------------------------------------------------
 *  Input......: info	info string
 *  Output.....: -
 *  Globals....: -
 ****************************************************************************/

static void PrintError(char *info)
{
	printf("*** can't %s: %s\n", info, M_errstring(UOS_ErrnoGet()));
}




