/****************************************************************************
 ************                                                    ************
 ************              M 3 6 _ B L K R E A D                 ************
 ************                                                    ************
 ****************************************************************************
 *
 *       Author: ds
 *        $Date: 2014/12/03 11:52:57 $
 *    $Revision: 1.8 $
 *
 *  Description: Configure and read M36 input channels (blockwise)
 *
 *     Required: usr_oss.l usr_utl.l
 *     Switches: -
 *
 *-------------------------------[ History ]---------------------------------
 *
 * $Log: m36_blkread.c,v $
 * Revision 1.8  2014/12/03 11:52:57  ts
 * R: block read returned wrong values, not considering gain factor
 * M: corrected wrong calculation, removed gain from it (is already set as
 *    a SetStat on M36N)
 *
 * Revision 1.7  2009/09/23 17:48:27  MRoth
 * R: Porting to MDIS5
 * M: a) added support for 64bit (MDIS_PATH)
 *    b) added casts to avoid compiler warnings
 *
 * Revision 1.6  2009/05/26 14:48:19  ts
 * R: program build failed under windows
 * M: added specifier __MAPILIB to declaration of signal handler
 *
 * Revision 1.5  2004/04/15 12:19:59  cs
 * Minor modifications for MDIS4/2004 conformity
 *       added stdlib.h and string.h
 *
 * Revision 1.4  2002/06/13 14:00:01  kp
 * cosmetics
 *
 * Revision 1.3  1998/11/26 16:18:40  Schmidt
 * option for voltage and current measurement added
 *
 * Revision 1.2  1998/11/18 14:43:13  see
 * enable interrupts after highwater signal and info output
 * read loop: UOS_Delay(500) removed
 *
 * Revision 1.1  1998/11/17 15:31:03  Schmidt
 * Added by mcvs
 *
 *---------------------------------------------------------------------------
 * (c) Copyright 1998 by MEN mikro elektronik GmbH, Nuernberg, Germany
 ****************************************************************************/

static const char RCSid[]="$Header: /dd2/CVSR/COM/DRIVERS/MDIS_LL/M036/TOOLS/M36_BLKREAD/COM/m36_blkread.c,v 1.8 2014/12/03 11:52:57 ts Exp $";

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
static void PrintMdisError(char *info);
static void PrintUosError(char *info);
static void __MAPILIB SigHandler(u_int32 sigCode);

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
	printf("Usage: m36_blkread [<opts>] <device> [<opts>]\n");
	printf("Function: Configure and read M36 channels (blockwise) \n");
	printf("Options:\n");
	printf("    device       device name                          [none]\n");
	printf("    -a=<ch>      first channel to read (0..7/15)      [0]\n");
	printf("    -z=<ch>      last channel to read (0..7/15)       [7/15]\n");
	printf("    -b=<mode>    block i/o mode                       [USRCTRL]\n");
	printf("                  0 = M_BUF_USRCTRL\n");
	printf("                  1 = M_BUF_CURRBUF\n");
	printf("                  2 = M_BUF_RINGBUF\n");
	printf("                  3 = M_BUF_RINGBUF_OVERWR\n");
	printf("    -s=<size>    block size in bytes                  [128]\n");
	printf("    -o=<msec>    block read timeout [msec] (0=none)   [0]\n");
	printf("    -g=<gain>    gain factor for all channels         [x1]\n");
	printf("                  0 = x1\n");
	printf("                  1 = x2\n");
	printf("                  2 = x4\n");
	printf("                  3 = x8\n");
	printf("                  4 = x16 (on-board jumper must be set !)\n");
	printf("    -m=<mode>    measuring mode                       [unipolar]\n");
	printf("                  0=unipolar\n");
	printf("                  1=bipolar\n");
	printf("    -t=<trig>    trigger mode                         [intern]\n");
	printf("                  0 = internal trigger	\n");
	printf("                  1 = external trigger	\n");
	printf("    -d=<mode>    display mode                         [raw hex]\n");
	printf("                  0 = raw hex value \n");
	printf("                  1 = volt	\n");
	printf("                  2 = ampere (only for gain factor x8)\n");
	printf("    -h           install buffer highwater signal      [no]\n");
	printf("    -l           loop mode                            [no]\n");
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
	int32	firstCh, lastCh, blkmode, blksize, tout;
	int32	gain, mode, trig, disp, signal, loopmode, n, ch, chNbr, gotsize;
	u_int8	*blkbuf = NULL;
	u_int8	*bp = NULL;
	u_int8	*bp0 = NULL;
	u_int8	*bmax = NULL;
	char	*device,*str,*errstr,buf[40];
	double	volt, curr;

	/*--------------------+
    |  check arguments    |
    +--------------------*/
	if ((errstr = UTL_ILLIOPT("a=z=b=s=o=g=m=t=d=hl?", buf))) {	/* check args */
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

	firstCh  = ((str = UTL_TSTOPT("a=")) ? atoi(str) : 0);
	lastCh   = ((str = UTL_TSTOPT("z=")) ? atoi(str) : -1);
	blkmode  = ((str = UTL_TSTOPT("b=")) ? atoi(str) : M_BUF_USRCTRL);
	blksize  = ((str = UTL_TSTOPT("s=")) ? atoi(str) : 128);
	tout     = ((str = UTL_TSTOPT("o=")) ? atoi(str) : 0);
	gain     = ((str = UTL_TSTOPT("g=")) ? atoi(str) : 0);
	mode     = ((str = UTL_TSTOPT("m=")) ? atoi(str) : 0);
	trig     = ((str = UTL_TSTOPT("t=")) ? atoi(str) : 0);
	disp     = ((str = UTL_TSTOPT("d=")) ? atoi(str) : 0);
	signal   = (UTL_TSTOPT("h") ? 1 : 0);
	loopmode = (UTL_TSTOPT("l") ? 1 : 0);

	/* check for option conflict */
	if ((disp == 2) && (gain != 3)) {
		printf("\n*** option -d=2 only available with option -g=3\n\n");
		usage();
		return(1);
	}

	/* check for valid display mode */
	if ( (disp<0) || (disp>2) ) {
	  printf("*** option -d=%d out of range (-d=0..2)\n", (int)disp);
		return(1);
	}

	
	/*--------------------+
    |  create buffer      |
    +--------------------*/
	if ((blkbuf = (u_int8*)malloc(blksize)) == NULL) {
	  printf("*** can't alloc %d bytes\n",(int)blksize);
		return(1);
	}

	if (signal) {
		/*--------------------+
		|  install signal     |
		+--------------------*/
		/* install signal handler */
		if (UOS_SigInit(SigHandler)) {
			PrintUosError("SigInit");
			return(1);
		}
		/* install signal */
		if (UOS_SigInstall(UOS_SIG_USR1)) {
			PrintUosError("SigInstall");
			goto abort;
		}
	}

	/*--------------------+
    |  open path          |
    +--------------------*/
	if ((path = M_open(device)) < 0) {
		PrintMdisError("open");
		return(1);
	}

	/*--------------------+
    |  config             |
    +--------------------*/
	/* set block i/o mode */
	if ((M_setstat(path, M_BUF_RD_MODE, blkmode)) < 0) {
			PrintMdisError("setstat M_BUF_RD_MODE");
		goto abort;
	}
	/* set block read timeout */
	if ((M_setstat(path, M_BUF_RD_TIMEOUT, tout)) < 0) {
			PrintMdisError("setstat M_BUF_RD_TIMEOUT");
		goto abort;
	}
	/* set measuring mode */
	if ((M_setstat(path, M36_BIPOLAR, mode)) < 0) {
		PrintMdisError("setstat M36_BIPOLAR");
		goto abort;
	}
	/* set trigger mode */
	if ((M_setstat(path, M36_EXT_TRIG, trig)) < 0) {
		PrintMdisError("setstat M36_EXT_TRIG");
		goto abort;
	}
	/* get number of channels */
	if ((M_getstat(path, M_LL_CH_NUMBER, &chNbr)) < 0) {
		PrintMdisError("getstat M_LL_CH_NUMBER");
		goto abort;
	}

	if (lastCh == -1)
		lastCh = chNbr - 1;

	/* channel specific settings */
	for (ch=0; ch<chNbr; ch++) {
		/* set current channel */
		if ((M_setstat(path, M_MK_CH_CURRENT, ch)) < 0) {
			PrintMdisError("setstat M_MK_CH_CURRENT");
			goto abort;
		}
		if ( (ch < firstCh) || (ch > lastCh) ) {
			/* disable channel */
			if ((M_setstat(path, M36_CH_ENABLE, 0)) < 0) {
				PrintMdisError("setstat M36_CH_ENABLE");
				goto abort;
			}
		}
		else {
			/* enable channel */
			if ((M_setstat(path, M36_CH_ENABLE, 1)) < 0) {
				PrintMdisError("setstat M36_CH_ENABLE");
				goto abort;
			}
			/* check for SW gain */
			if (gain != 4) {
				/* set gain */
				if ((M_setstat(path, M36_CH_GAIN, gain)) < 0) {
					PrintMdisError("setstat M36_CH_GAIN");
					goto abort;
				}
			}
		}
	}

	if (signal) {
		/* enable buffer highwater signal */
		if ((M_setstat(path, M_BUF_RD_SIGSET_HIGH, UOS_SIG_USR1)) < 0) {
				PrintMdisError("setstat M_BUF_RD_SIGSET_HIGH");
			goto abort;
		}
	}

    /*--------------------+
    |  print info         |
    +--------------------*/
	printf("first channel       : %d\n",(int)firstCh);
	printf("last channel        : %d\n",(int)lastCh);
	printf("block i/o mode      : ");
	switch (blkmode)
	{
		case 0: printf("M_BUF_USRCTRL\n");
			break;
		case 1: printf("M_BUF_CURRBUF\n");
			break;
		case 2: printf("M_BUF_RINGBUF\n");
			break;
		case 3: printf("M_BUF_RINGBUF_OVERWR\n");
			break;
	}
	printf("block size          : %ld bytes\n",blksize);
	printf("block read timeout  : %ld msec\n",tout);
	printf("gain factor         : %d\n", 1 << gain);
	printf("measuring mode      : %s\n",(mode==0 ? "unipolar":"bipolar"));
	printf("trigger mode        : %s\n",(trig==0 ? "intern":"extern"));
	printf("buf highwater signal: %s\n",(signal==0 ? "no":"yes"));
	printf("loop mode           : %s\n\n",(loopmode==0 ? "no":"yes"));

    /*--------------------+
    |  enable interrupt   |
    +--------------------*/
	if (blkmode)
		/* enable interrupt */
		if ((M_setstat(path, M_MK_IRQ_ENABLE, 1)) < 0) {
					PrintMdisError("setstat M_MK_IRQ_ENABLE");
			goto abort;
		}

    /*--------------------+
    |  read block         |
    +--------------------*/
	do {
		printf("\nwaiting for data ..\n\n");

		if ((gotsize = M_getblock(path,(u_int8*)blkbuf,blksize)) < 0) {
			PrintMdisError("getblock");
			break;
		}

		/* raw hex value */
		if (disp == 0) {
				UTL_Memdump("raw hex value:",(char*)blkbuf,gotsize,2);
		}
		/* voltage or current */
		else {
			bmax = blkbuf + gotsize;

			if (disp==1)
				printf("voltage: (%ld bytes)\n",gotsize);
			else
				printf("current: (%ld bytes)\n",gotsize);

			for (bp=bp0=blkbuf; bp0<bmax; bp0+=16) {
				printf("%08x+%04x: ",(int32)((INT32_OR_64)blkbuf), (int16)(bp-blkbuf) );

				for (bp=bp0,n=0; n<16; n+=2, bp+=2) {	/* word aligned */
					/* voltage */
					if (disp==1) {
						if (mode)
						  volt = (*(int16*)bp * 20.0) / 0xffff;	/* bipolar */
						else
						  volt = (*(int16*)bp * 10.0) / 0xffff;	/* unipolar */

						if (bp<bmax)  printf("%7.3fV",volt);
							else      printf("      ");
					}
					/* current */
					else {
						if (mode)
						   curr = *(int16*)bp * 40.0 / 0xffff;	/* bipolar */
						else
                  				   curr = *(u_int16*)bp * 20.0 / 0xffff;	/* unipolar */

						if (bp<bmax)  printf("%7.3fmA",curr);
							else      printf("       ");
					}
				}
				printf("\n");
			}
		} /* voltage or current */

	} while(loopmode && UOS_KeyPressed() == -1);

	/*--------------------+
    |  cleanup            |
    +--------------------*/
	abort:
	if (blkmode)
		/* disable interrupt */
		if ((M_setstat(path, M_MK_IRQ_ENABLE, 0)) < 0) {
				PrintMdisError("setstat M_MK_IRQ_ENABLE");
		}

	if (signal)
		/* terminate signal handling */
		UOS_SigExit();

	if (M_close(path) < 0)
		PrintMdisError("close");

	return(0);
}

/********************************* PrintMdisError ***************************
 *
 *  Description: Print MDIS error message
 *
 *---------------------------------------------------------------------------
 *  Input......: info	info string
 *  Output.....: -
 *  Globals....: -
 ****************************************************************************/
static void PrintMdisError(char *info)
{
	printf("*** can't %s: %s\n", info, M_errstring(UOS_ErrnoGet()));
}

/********************************* PrintUosError ****************************
 *
 *  Description: Print MDIS error message
 *
 *---------------------------------------------------------------------------
 *  Input......: info	info string
 *  Output.....: -
 *  Globals....: -
 ****************************************************************************/
static void PrintUosError(char *info)
{
	printf("*** can't %s: %s\n", info, UOS_ErrString(UOS_ErrnoGet()));
}

/********************************* SigHandler *******************************
 *
 *  Description: Signal handler
 *
 *---------------------------------------------------------------------------
 *  Input......: sigCode	signal code received
 *  Output.....: -
 *  Globals....: -
 ****************************************************************************/
static void __MAPILIB SigHandler(u_int32 sigCode)
{
	switch(sigCode) {
		case UOS_SIG_USR1:
			printf(">>> Buffer highwater notification\n");
			break;
		default:
			printf(">>> signal=%d received\n",sigCode);
	}
}


