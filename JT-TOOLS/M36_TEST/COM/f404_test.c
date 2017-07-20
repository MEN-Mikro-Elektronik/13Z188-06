/****************************************************************************
 ************                                                    ************
 ************                 M 3 6 _ R E A D                    ************
 ************                                                    ************
 ****************************************************************************
 *
 *       Author: ds
 *        $Date: 2009/09/23 17:48:29 $
 *    $Revision: 1.4 $
 *
 *  Description: M36 test program for simple read operations
 *
 *     Required: usr_oss.l usr_utl.l
 *     Switches: -
 *
 *-------------------------------[ History ]---------------------------------
 *
 * $Log: m36_test.c,v $
 * Revision 1.4  2009/09/23 17:48:29  MRoth
 * R: Porting to MDIS5
 * M: added support for 64bit (MDIS_PATH)
 *
 * Revision 1.3  2004/04/15 12:20:02  cs
 * Minor modifications for MDIS4/2004 conformity
 *       added stdlib.h and string.h
 *       replaced function _itoa by sprintf for ANSI compliance
 *
 * Revision 1.2  2002/07/25 16:12:15  DSchmidt
 * cosmetics
 *
 * Revision 1.1  2000/04/11 14:43:18  Schmidt
 * Initial Revision
 *
 *---------------------------------------------------------------------------
 * (c) Copyright 2000 by MEN mikro elektronik GmbH, Nuernberg, Germany
 ****************************************************************************/

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
#define UNUSED			0xffff
#define MAX_CHAN_NBR	16

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
	printf("Usage: m36_test [<opts>] <device> [<opts>]\n");
	printf("Function: M36 test program for simple read operations\n");
	printf("Options:\n");
	printf("    ----- device specific -----\n");
	printf("    device       device name                              [none]\n");
	printf("    -m=<mode>     measuring mode                          [none]\n");
	printf("                  0=unipolar\n");
	printf("                  1=bipolar\n");
	printf("    -t=<trig>     trigger mode                            [none]\n");
	printf("                  0 = internal trigger	\n");
	printf("                  1 = external trigger	\n");
	printf("    -e=<string>   enable/disable all channels             [none]\n");
	printf("                    eg. -e=dee -> disable ch0\n");
	printf("                               -> enable  ch1\n");
	printf("                               -> enable  ch2\n");
	printf("                               -> disable ch3..7/15\n");
	printf("    ----- channel specific -----\n");
	printf("    -c=<chan>     channel number (0..7/15)                [none]\n");
	printf("    -g=<gain>     gain factor                             [none]\n");
	printf("                  0 = x1\n");
	printf("                  1 = x2\n");
	printf("                  2 = x4\n");
	printf("                  3 = x8\n");
	printf("                  4 = x16 (on-board jumper must be set !)\n");
	printf("    -n=<1..oo>   number of values to read                 [0]\n");
	printf("                  Note: If specified, the read values will\n");
	printf("                        be stored in file m36_ch<chan>xx.dat\n");
	printf("    -d=<msec>    delay between reads                      [0]\n");
	printf("    -p=<mode>    print mode                               [raw hex]\n");
	printf("                  0 = raw hex value \n");
	printf("                  1 = volt	\n");
	printf("                  2 = ampere (only for gain factor x8)\n");
	printf("    -l           loop mode                                [no]\n");
	printf("\n");
	printf("(c) 2000 by MEN mikro elektronik GmbH\n\n");
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
	int32	value, n, gainfac, chanNbr, chEnable[MAX_CHAN_NBR], ch=0;
	int32	mode, trig, chan, gain, disp, delay, loopmode, endis;
	u_int32	nr=0, valNbr, startTime, stopTime;
	char	*device, *str, *errstr, buf[40], *ptr, filename[20], chanStr[5];
	double	volt, curr, *valBuf=NULL;
	FILE	*fileH;

	printf("\n");

	/*--------------------+
    |  check arguments    |
    +--------------------*/
	if ((errstr = UTL_ILLIOPT("m=t=e=c=g=d=p=n=l?", buf))) {	/* check args */
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

	mode     = ((str = UTL_TSTOPT("m=")) ? atoi(str) : UNUSED);
	trig     = ((str = UTL_TSTOPT("t=")) ? atoi(str) : UNUSED);
	chan     = ((str = UTL_TSTOPT("c=")) ? atoi(str) : UNUSED);
	gain     = ((str = UTL_TSTOPT("g=")) ? atoi(str) : 0);
	disp     = ((str = UTL_TSTOPT("p=")) ? atoi(str) : 0);
	valNbr   = ((str = UTL_TSTOPT("n=")) ? atoi(str) : 0);
	delay    = ((str = UTL_TSTOPT("d=")) ? atoi(str) : 0);
	loopmode = (UTL_TSTOPT("l") ? 1 : 0);

	if ( (str = UTL_TSTOPT("e=")) ) {
		endis = TRUE;
		ptr = str;
		/* fill chEnable */
		for( ch=0; ch<MAX_CHAN_NBR; ch++ ){
			if( *ptr++ == 'e' )
				chEnable[ch] = 1;
			else
				chEnable[ch] = 0;
		}
	}
	else
		endis = UNUSED;

	/*---------------------------+
    |  check argument conflictss |
    +---------------------------*/
	if ((disp == 2) && (gain != 3)) {
		printf("\n*** option -p=2 only available with option -g=3\n\n");
		usage();
		return(1);
	}

	if ( valNbr && loopmode){
		printf("\n*** option -n=.. not possible with option -l\n\n");
		usage();
		return(1);
	}

	if( disp > 2){
		printf("\n*** option -d=%d out of range (-d=0..2)\n", disp);
		usage();
		return(1);
	}

	gainfac = 1 << gain;		/* calculate gain factor */

	/*---------------------------+
    |  create value buffer       |
    +---------------------------*/
	if( valNbr ){
		valBuf = (double*)malloc( valNbr * sizeof(double) );
		if( valBuf == NULL ){
			printf("\n*** cannot allocate memory for buffer\n");
			return(1);
		}
	}

	/*--------------------+
    |  open path          |
    +--------------------*/
	if ((path = M_open(device)) < 0) {
		PrintError("open");
		return(1);
	}

	/*---------------------------+
    |  config - device specific  |
    +---------------------------*/
	/* set measuring mode */
	if( mode != UNUSED ){
		if ((M_setstat(path, M36_BIPOLAR, mode)) < 0) {
			PrintError("setstat M36_BIPOLAR");
			goto abort;
		}
	}

	/* set trigger mode */
	if( trig != UNUSED ){
		if ((M_setstat(path, M36_EXT_TRIG, trig)) < 0) {
			PrintError("setstat M36_EXT_TRIG");
			goto abort;
		}
	}

	/* enable/disable all channels */
	if( endis != UNUSED ){

		/* get number of channels (8 or 16) */
		if ((M_getstat(path, M_LL_CH_NUMBER, &chanNbr)) < 0) {
			PrintError("getstat M_LL_CH_NUMBER");
			goto abort;
		}

		for( ch=0; ch<chanNbr; ch++ ){
			/* set current channel */
			if ((M_setstat(path, M_MK_CH_CURRENT, ch)) < 0) {
				PrintError("setstat M_MK_CH_CURRENT");
				goto abort;
			}
			/* enable/disable channel */
			if ((M_setstat(path, M36_CH_ENABLE, chEnable[ch])) < 0) {
				PrintError("setstat M36_CH_ENABLE");
				goto abort;
			}
		}
	}

	/*---------------------------+
    |  config - channel specific |
    +---------------------------*/
	if( chan != UNUSED ){

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

		if( gain != UNUSED ){
			/* check for SW gain */
			if (gain != 4) {
				/* set gain */
				if ((M_setstat(path, M36_CH_GAIN, gain)) < 0) {
					PrintError("setstat M36_CH_GAIN");
					goto abort;
				}
			}
		}
	}

    /*--------------------+
    |  print info         |
    +--------------------*/
	if( (mode != UNUSED) || ( trig != UNUSED ) || ( endis != UNUSED ) ){
		printf("Modified device parameters\n", chan);
		printf("--------------------------\n", chan);
		if( mode != UNUSED )
			printf("measuring mode  : %s\n",(mode==0 ? "unipolar":"bipolar"));
		if( trig != UNUSED )
			printf("trigger mode    : %s\n",(trig==0 ? "intern":"extern"));

		if( endis != UNUSED ){
			printf("channel:          00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15\n");
			printf("en/disabled:       ");
			for( ch=0; ch<chanNbr; ch++ )
				printf("%c  ", chEnable[ch] ? 'e' : 'd');
			printf("\n");
		}
			printf("\n");
	}

	if( chan != UNUSED ){
		printf("Modified channel parameters\n", chan);
		printf("---------------------------\n", chan);
		printf("channel number            : %d\n", chan);
		if( gainfac != UNUSED )
			printf("gain factor               : x%d\n", gainfac);
		if( valNbr )
			printf("number of values to read  : %d\n", valNbr);
		if( delay )
			printf("delay between reads       : %dmsec\n", delay);
		printf("\n");
	}

	if( chan != UNUSED ){

		startTime = UOS_MsecTimerGet();

		/*--------------------+
		|  read               |
		+--------------------*/
		do {
			printf("read value #%6d: ", nr+1);


			if ((M_read(path,&value)) < 0) {
				PrintError("read");
				goto abort;
			}

			switch (disp) {

				/* raw hex value */
				case 0:
					printf("0x%04x (%s)\n",
						value, (mode==0 ? "unipolar":"bipolar"));
					if( valNbr )
						valBuf[nr] = value;
					break;
				/* voltage */
				case 1:
					if (mode)
						volt = (int16)value * 20.0 / (0xffff * gainfac);	/* bipolar */
					else
						volt = (u_int16)value * 10.0 / (0xffff * gainfac);	/* unipolar */
					printf("0x%04x = %7.3f V (%s)\n",
						value, volt, (mode==0 ? "unipolar":"bipolar"));
					if( valNbr )
						valBuf[nr] = volt;
					break;
				/* current */
				case 2:
					if (mode)
						curr = (int16)value * 40.0 / 0xffff;	/* bipolar */
					else
						curr = (u_int16)value * 20.0 / 0xffff;	/* unipolar */
					printf("0x%04x = %7.3f mA (%s)\n",
						value, curr, (mode==0 ? "unipolar":"bipolar"));
					if( valNbr )
						valBuf[nr] = curr;
					break;
			}

			nr++;
			if( delay )
				UOS_Delay(delay);

		} while( (loopmode && UOS_KeyPressed() == -1) ||
				 ( valNbr && (valNbr > nr))		  );

		stopTime = UOS_MsecTimerGet();
		printf("%d values in %dmsec read (resolution %dmsec)\n\n",
			nr,stopTime-startTime,UOS_MsecTimerResolution());

		/*-----------------------+
		|  write values to file  |
		+-----------------------*/
		if( valNbr ){

			/* build filename (e.g. m36_ch1.dat) */
			strcpy( filename, "m36_ch");
			/* _itoa( chan, chanStr, 10 );  replaced by sprintf due to ANSI compatibility */
                        sprintf( chanStr, "%d", chan );
			strcat( filename, chanStr);
			strcat( filename, ".dat");

			/* open file */
			if( (fileH = fopen( filename, "w" )) == NULL ){
				printf("\n*** cannot open file %s\n",filename);
				goto abort;
			}

			/* write to file */
			for( nr=0; nr<valNbr; nr++ ){
				switch (disp) {
				/* raw hex value */
				case 0:
					fprintf( fileH, "%d;%d;",nr+1,(int32)valBuf[nr] );
					break;
				/* voltage */
				case 1:
				/* current */
				case 2:
					fprintf( fileH, "%d;%f;",nr+1,valBuf[nr] );
					break;
				}
			}

			/* close file */
			fclose( fileH );
		}
	}

	/*--------------------+
    |  hold path open     |
    +--------------------*/
	printf("Path to device still open... (Press key to abort)\n");
	UOS_KeyWait();
	goto abort;

	/*--------------------+
    |  cleanup            |
    +--------------------*/
	abort:
	if (M_close(path) < 0)
		PrintError("close");
	if( valBuf != NULL )
		free( (void*)valBuf );

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



