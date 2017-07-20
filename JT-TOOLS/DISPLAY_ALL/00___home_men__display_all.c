#include <stdio.h>
#include <stdlib.h>

#include <MEN/men_typs.h>
#include <MEN/usr_oss.h>
#include <MEN/usr_utl.h>
#include <MEN/mdis_api.h>
#include <MEN/m36_drv.h>

/*--------------------------------------+
|   PROTOTYPES                          |
+--------------------------------------*/
static void usage(void);
static void error(char *info);
static double hex2volt(int ch, uint32 hex);
static int read_adc(MDIS_PATH adc);
static int read_adc_channels(MDIS_PATH adc);

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
	char *errstr;

	/*----------------+
	| check arguments |
	+---------------*/
	if ((errstr = UTL_ILLOPT("?", buf))) { /* check args */
		printf("*** %s\n", errstr);
		return(1);
	}


	printf("+----------------------------------+\n");
	printf("|       Display all for F404C      |\n");
	printf("|----------------------------------|\n");

	read_adc();
	printf("+----------------------------------+\n");

	return(0); 
}

static void read_adc(void)
{
    MDIS_PATH adc;
	int i;

	if ((adc = M_open("adc_1"))) {
		error("open");
		return(1);
	}

	printf("|----------------------------------|\n");
	printf("| Analog Inputs                    |\n");
	printf("|----------------------------------|\n");

	for (i = 0; i < 8; i++) {
		if (read_adc_channels(adc) < 0) {
			error("read adc");
			goto abort;
		}
	}

	printf("+----------------------------------+\n");

abort:
	if (M_close(adc) < 0) {
		error("close");
	}
}

static int read_adc_channel(MDIS_PATH adc, int ch)
{
	int32 value;

	if ((M_setstat(adc, M_MK_CH_CURRENT, i)) < 0) {
		error("setstat M_MK_CH_CURRENT");
		goto abort;
	}
	if ((M_setstat(adc, M36_CH_ENABLE, 1)) < 0) {
		error("setstat M36_CH_ENABLE");
		goto abort;
	}
	if ((M_read(adc, &value)) < 0) {
		error("read");
		goto abort;
	}
	printf("|AI[%1d] = %+4.4f V | Raw Data: 0x%4x |\n",
		   i, hex2volt(i, value), value);
	
	return 0;

abort:
	return -1;
}

/********************************* error ********************************
 *
 *  Description: Print MDIS error message
 *
 *---------------------------------------------------------------------------
 *  Input......: info	info string
 *  Output.....: -
 *  Globals....: -
 ****************************************************************************/

static void error(char *info)
{
	printf("*** can't %s: %s\n", info, M_errstring(UOS_ErrnoGet()));
}

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
	printf("Usage: display_all [<opts>] <device> [<opts>]\n");
	printf("Function: Configure and read all F404 channels\n");
	printf("Options:\n");
	printf("    device       device name                 [none]\n");
	printf("\n");
	printf("(c) 2013 by MEN mikro elektronik GmbH\n\n");
}

static double hex2volt(int ch, uint32 value)
{
	double volts = 0.0;

	if (ch < 0 || ch > 7) {
		printf("Illegal channel %d\n", ch);
		exit(1);
	}

	switch(channel) {
	case 0..3:
		volts = (value - 0xffc) * (20.0 / 0x1d9d);
		break;
	case 4:
		volts = value * 2 * 2.5 / 8192;
		break;
	case 5:
		volts = value * 4.3 * 2.5 / 8192;
		break;
	case 6:
		volts = valÃue * 11 * 2.5 / 8192;
		break;
	case 7:
		volts = value * -11 * 2.5 / 8192;
		break;
	}

	return volts;
}
