/****************************************************************************
 ************                                                    ************
 ************                 M 3 6 _ R E A D                    ************
 ************                                                    ************
 ****************************************************************************
 *
 *       Author: ds
 *        $Date: 2009/09/23 17:48:25 $
 *    $Revision: 1.7 $
 *
 *  Description: Configure and read M36 input channel
 *
 *     Required: usr_oss.l usr_utl.l
 *     Switches: -
 *
 *-------------------------------[ History ]---------------------------------
 *
 * $Log: m36_read.c,v $
 * Revision 1.7  2009/09/23 17:48:25  MRoth
 * R: Porting to MDIS5
 * M: added support for 64bit (MDIS_PATH)
 *
 * Revision 1.6  2008/01/10 15:05:49  ts
 * Fixed: gain factor was multiplied into the displayed volts value although the
 * raw 16bit value from data register already contains the gain. So displayed Value
 * was gain^2 * applied voltage
 *
 * Revision 1.5  2002/07/25 16:12:10  DSchmidt
 * cosmetics
 *
 * Revision 1.4  2002/06/13 13:59:59  kp
 * cosmetics
 *
 * Revision 1.3  1998/11/27 13:38:38  see
 * hex output format was wrong
 *
 * Revision 1.2  1998/11/26 16:18:36  Schmidt
 * option for voltage and current measurement added
 *
 * Revision 1.1  1998/11/17 15:30:54  Schmidt
 * Added by mcvs
 *
 *---------------------------------------------------------------------------
 * (c) Copyright 1998 by MEN mikro elektronik GmbH, Nuernberg, Germany
 ****************************************************************************/

static const char RCSid[]="$Header: /dd2/CVSR/COM/DRIVERS/MDIS_LL/M036/TOOLS/M36_READ/COM/m36_read.c,v 1.7 2009/09/23 17:48:25 MRoth Exp $";

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <MEN/men_typs.h>
#include <MEN/usr_oss.h>
#include <MEN/usr_utl.h>
#include <MEN/mdis_api.h>
#include <MEN/m36_drv.h>

/*--------------------------------------+
|   DEFINES                             |
+--------------------------------------*/

/*--------------------------------------+
|   PROTOTYPES                          |
+--------------------------------------*/
static void usage(void);
static void PrintError(char *info);

/********************************* usage ************************************
 *
 *  Description: Print program usage
 *
 *---------------------------------------------------------------------------
 *  Input......: -
 *  Output.....: -
 *  Globals....: -
 ****************************************************************************/
static void usage(void)
{
	printf("Usage: m36_read [<opts>] <device> [<opts>]\n");
	printf("Function: Configure and read M36 channel\n");
	printf("Options:\n");
	printf("    device       device name                 [none]\n");
	printf("    -c=<chan>    channel number (0..7/15)    [0]\n");
	printf("    -g=<gain>    gain factor                 [x1]\n");
	printf("                  0 = x1\n");
	printf("                  1 = x2\n");
	printf("                  2 = x4\n");
	printf("                  3 = x8\n");
	printf("                  4 = x16 (on-board jumper must be set !)\n");
	printf("    -m=<mode>    measuring mode              [unipolar]\n");
	printf("                  0=unipolar\n");
	printf("                  1=bipolar\n");
	printf("    -t=<trig>    trigger mode                [intern]\n");
	printf("                  0 = internal trigger	\n");
	printf("                  1 = external trigger	\n");
	printf("    -d=<mode>    display mode                [raw hex]\n");
	printf("                  0 = raw hex value \n");
	printf("                  1 = hex and volt	\n");
	printf("                  2 = hex and ampere (only for gain factor x8)\n");
	printf("    -l           loop mode                   [no]\n");
	printf("\n");
	printf("(c) 1998 by MEN mikro elektronik GmbH\n\n");
}

/********************************* main *************************************
 *
 *  Description: Program main function
 *
 *---------------------------------------------------------------------------
 *  Input......: argc,argv	argument counter, data ..
 *  Output.....: return	    success (0) or error (1)
 *  Globals....: -
 ****************************************************************************/
int main(int argc, char **argv)
{
	MDIS_PATH path=0;
	int32	chan, gain, mode, disp, loopmode, value, trig, n, gainfac;
	char	*device, *str, *errstr, buf[40];
	double	volt, curr;

	/*--------------------+
    |  check arguments    |
    +--------------------*/
	if ((errstr = UTL_ILLIOPT("c=g=m=t=d=l?", buf))) {	/* check args */
		printf("*** %s\n", errstr);
		return(1);
	}

	if (UTL_TSTOPT("?")) {						/* help requested ? */
		usage();
		return(1);
	}

	/*--------------------+
    |  get arguments      |
    +--------------------*/
	for (device=NULL, n=1; n<argc; n++)
		if (*argv[n] != '-') {
			device = argv[n];
			break;
		}

	if (!device) {
		usage();
		return(1);
	}

	chan     = ((str = UTL_TSTOPT("c=")) ? atoi(str) : 0);
	gain     = ((str = UTL_TSTOPT("g=")) ? atoi(str) : 0);
	mode     = ((str = UTL_TSTOPT("m=")) ? atoi(str) : 0);
	trig     = ((str = UTL_TSTOPT("t=")) ? atoi(str) : 0);
	disp     = ((str = UTL_TSTOPT("d=")) ? atoi(str) : 0);
	loopmode = (UTL_TSTOPT("l") ? 1 : 0);

	/* check for option conflict */
	if ((disp == 2) && (gain != 3)) {
		printf("\n*** option -d=2 only available with option -g=3\n\n");
		usage();
		return(1);
	}

	gainfac = 1 << gain;		/* calculate gain factor */

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
	/* set measuring mode */
	if ((M_setstat(path, M36_BIPOLAR, mode)) < 0) {
		PrintError("setstat M36_BIPOLAR");
		goto abort;
	}
	/* set current channel */
	if ((M_setstat(path, M_MK_CH_CURRENT, chan)) < 0) {
		PrintError("setstat M_MK_CH_CURRENT");
		goto abort;
	}
	/* enable channel */
	if ((M_setstat(path, M36_CH_ENABLE, 1)) < 0) {
		PrintError("setstat M36_CH_ENABLE");
		goto abort;
	}
	/* check for SW gain */
	if (gain != 4) {
		/* set gain */
		if ((M_setstat(path, M36_CH_GAIN, gain)) < 0) {
			PrintError("setstat M36_CH_GAIN");
			goto abort;
		}
	}
	/* set trigger mode */
	if ((M_setstat(path, M36_EXT_TRIG, trig)) < 0) {
		PrintError("setstat M36_EXT_TRIG");
		goto abort;
	}

    /*--------------------+
    |  print info         |
    +--------------------*/
	printf("channel number      : %d\n", chan);
	printf("gain factor         : x%d\n", gainfac);
	printf("measuring mode      : %s\n",(mode==0 ? "unipolar":"bipolar"));
	printf("trigger mode        : %s\n\n",(trig==0 ? "intern":"extern"));

	/*--------------------+
    |  read               |
    +--------------------*/
	do {
		if ((M_read(path,&value)) < 0) {
			PrintError("read");
			goto abort;
		}
		switch (disp) {
			/* raw hex value */
			case 0:
				printf("read: 0x%04x (%s)\n",
					value, (mode==0 ? "unipolar":"bipolar"));
				break;
			/* voltage */
			case 1:
				if (mode)
					volt = (int16)value * 20.0 / 0xffff;	/* bipolar */
				else
					volt = (u_int16)value * 10.0 / 0xffff;	/* unipolar */
				printf("read: 0x%04x = %7.4f V (%s)\n",
					value, volt, (mode==0 ? "unipolar":"bipolar"));
				break;
			/* current */
			case 2:
				if (mode)
					curr = (int16)value * 40.0 / 0xffff;	/* bipolar */
				else
					curr = (u_int16)value * 20.0 / 0xffff;	/* unipolar */
				printf("read: 0x%04x = %7.4f mA (%s)\n",
					value, curr, (mode==0 ? "unipolar":"bipolar"));
				break;
			/* invalid */
			default:
				printf("*** option -d=%d out of range (-d=0..2)\n", disp);
		}

		UOS_Delay(100);
	} while(loopmode && UOS_KeyPressed() == -1);

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



