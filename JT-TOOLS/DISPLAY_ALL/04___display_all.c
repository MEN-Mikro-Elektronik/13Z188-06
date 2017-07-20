#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <MEN/men_typs.h>
#include <MEN/usr_oss.h>
#include <MEN/usr_utl.h>
#include <MEN/mdis_api.h>
#include <MEN/m36_drv.h>
#include <MEN/z17_drv.h>
#include <MEN/z82_drv.h>

#define NUM_SAMPLES 10
#define DIGIT(x) ((x) - '0')
#define BIT(x) (1 << (x))

enum adc_mode {
	VOLTAGE = 0,
	CURRENT = 1,
};

struct channel {
	double value;
	u_int32 hex;
	enum adc_mode mode;
};

struct imp {
	MDIS_PATH path;
	float value;
	u_int32 t1, t2;
};

struct adc {
	MDIS_PATH path;
	struct channel channels[8];
};

struct gpio {
	MDIS_PATH path;
	u_int32 value;
	int32 dir;
	int32 in;
	u_int32 out[4];
	int32 irq_sense;
	int32 irq_pin;
	int32 dber;
	int32 dber_time[16];
};

struct f404 {
	char *gpio;
	char *adc;
	char *imp1;
	char *imp2;
} f404s[] = {
	{
		.gpio = "gpio_1",
		.adc = "adc_1",
		.imp1 = "imp_1_1",
		.imp2 = "imp_1_2",
	},
	{
		.gpio = "gpio_2",
		.adc = "adc_2",
		.imp1 = "imp_2_1",
		.imp2 = "imp_2_2",
	},
};

/*--------------------------------------+
 |	  PROTOTYPES						|
 +--------------------------------------*/
static void usage(void);
static void help(void);
static void error(char *info);
static double hex2volt(int ch, int32 hex);
static void read_adc(struct adc *adc);
static int read_adc_channel(struct adc *adc, int ch);
static void clear(void);
static struct adc *init_adc(char *path);
static void deinit_adc(struct adc *adc);
static void display_adc_channel(struct adc *adc, int ch);
static void display_adc(struct adc *adc);
static struct imp *init_imp(char *path);
static void deinit_imp(struct imp *imp);
static void read_imp(struct imp *imp);
static void display_imp(struct imp *imp, int nr);
static void imp_menu(struct imp **imp);
static void reset_imp(struct imp *imp);
static void adc_menu(struct adc *adc);
static void toggle_adc_mode(struct adc *adc, int ch);
static struct gpio *init_gpio(const char *path);
static void deinit_gpio(struct gpio *gpio);
static void display_gpio_in(struct gpio *gpio);
static void display_gpio_out(struct gpio *gpio);
static void display_gpio_dber(struct gpio *gpio);
static void display_gpio_last_irq(struct gpio *gpio);
static void display_gpio_sense(struct gpio *gpio);
static void read_gpio(struct gpio *gpio);
static char gpio_state(int port, int bit);
static void gpio_menu_out(struct gpio *gpio);
static void gpio_menu_sense(struct gpio *gpio);
static void gpio_menu_sense_pin(struct gpio *gpio, int pin);
static void __MAPILIB signal_handler(u_int32 sig);
static void gpio_menu_dber(struct gpio *gpio);
static void gpio_menu_dber_pin(struct gpio *gpio, int pin);
static void write_gpio_dber(struct gpio *gpio, int num, int sense);
static void display_gpio_dber_time(struct gpio *gpio);
static void gpio_menu_dber_time(struct gpio *gpio);
static void gpio_menu_dber_time_pin(struct gpio *gpio, int pin);
static void write_gpio_dber_time(struct gpio *gpio, int pin, int value);

/*--------------------------------------+
 |	  GLOBAL VARIABLES					|
 +--------------------------------------*/
static time_t last_irq;
static int have_irq;
static sig_atomic_t last_sig;


/********************************* main *************************************
 *
 *	Description: Program main function
 *
 *---------------------------------------------------------------------------
 *	Input......: argc,argv	argument counter, data ..
 *	Output.....: return		success (0) or error (1)
 *	Globals....: -
 ****************************************************************************/
int main(int argc, char **argv)
{
	char *errstr;
	char buf[40];
	int32 key;
	struct adc *adc = NULL;
	struct imp *imp[2];
	struct gpio *gpio;
	time_t now;
	char tbuf[64];
	int card;
	char *str;

	/*----------------+
	  | check arguments |
	  +---------------*/
	if ((errstr = UTL_ILLIOPT("c=?", buf))) { /* check args */
		printf("*** %s\n", errstr);
		return(1);
	}

	if (UTL_TSTOPT("?")) {
		usage();
		return(1);
	}

	card = ((str = UTL_TSTOPT("c=")) ? atoi(str) : 0);

	if (card < 0 || card > 1) {
		usage();
		return(1);
	}

	adc = init_adc(f404s[card].adc);
	if (!adc) {
		exit(1);
	}

	imp[0] = init_imp(f404s[card].imp1);
	imp[1] = init_imp(f404s[card].imp2);

	if (imp[0] == NULL || imp[1] == NULL) {
		exit(1);
	}

	last_irq = 0;

	gpio = init_gpio(f404s[card].gpio);
	if (!gpio) {
		exit(1);
	}

	for(;;) {
		read_gpio(gpio);
		read_adc(adc);
		read_imp(imp[0]);
		read_imp(imp[1]);
		clear();
		now = time(NULL);
		strncpy(tbuf, ctime(&now), sizeof(tbuf));
		tbuf[strlen(tbuf) - 1] = '\0';
		printf("+----------------------------------------------+\n");
		printf("|            Display all for F404C             |\n");
		printf("|                   Card %d                    |\n", card);
		printf("|             Press 'h' for help               |\n");
		printf("|           %s           |\n", tbuf);
		printf("|----------------------------------------------|\n");
		printf("|----------------------------------------------|\n");
		printf("| Digital Outputs                              |\n");
		printf("|----------------------------------------------|\n");
		display_gpio_out(gpio);
		printf("|----------------------------------------------|\n");
		printf("| Digital Inputs                               |\n");
		printf("|----------------------------------------------|\n");
		display_gpio_in(gpio);
		printf("|----------------------------------------------|\n");
		printf("| Digital Inputs Debouncer                     |\n");
		printf("|----------------------------------------------|\n");
		display_gpio_dber(gpio);
		printf("|----------------------------------------------|\n");
		printf("| Digital Inputs Debounce Time                 |\n");
		printf("|----------------------------------------------|\n");
		display_gpio_dber_time(gpio);
		printf("|----------------------------------------------|\n");
		display_gpio_last_irq(gpio);
		if (last_sig != 0) {
			printf("| last_sig = %d|\n");
		}
		printf("|----------------------------------------------|\n");
		printf("|----------------------------------------------|\n");
		printf("| Digital Inputs IRQ Sensing                   |\n");
		printf("|----------------------------------------------|\n");
		display_gpio_sense(gpio);
		printf("|----------------------------------------------|\n");
		printf("|----------------------------------------------|\n");
		printf("| Frequency Inputs                             |\n");
		printf("|----------------------------------------------|\n");
		display_imp(imp[0], 1);
		display_imp(imp[1], 2);
		printf("|----------------------------------------------|\n");
		printf("| Analog Inputs                                |\n");
		printf("|----------------------------------------------|\n");
		display_adc(adc);
		printf("+----------------------------------------------+\n");
		printf("F404> ");
		fflush(stdout);
		key = UOS_KeyPressed();
		switch (key) {
		case 'a':
			adc_menu(adc);
			break;
		case 'd':
			gpio_menu_dber(gpio);
			break;
		case 'f':
			imp_menu(imp);
			break;
		case 'h':
			help();
			break;
		case 'o':
			gpio_menu_out(gpio);
			break;
		case 'q':
			goto out;
			break;
		case 's':
			gpio_menu_sense(gpio);
			break;
		case 't':
			gpio_menu_dber_time(gpio);
		default:
			break;
		}

		UOS_Delay(1000);
	}

 out:
	deinit_adc(adc);
	deinit_imp(imp[0]);
	deinit_imp(imp[1]);
	deinit_gpio(gpio);

	return(0);
}

static void imp_menu(struct imp **imp)
{
	int num;

	do {
		clear();
		printf("+----------------------------------+\n");
		printf("|       Display all for F404C      |\n");
		printf("|----------------------------------|\n");
		printf("|----------------------------------|\n");
		printf("| Reset Frequency Inputs           |\n");
		printf("| 1-2 to select Frequency Input    |\n");
		printf("|                                  |\n");
		printf("|    Press q to exit               |\n");
		printf("|----------------------------------|\n");
		printf("FI?> ");
		fflush(stdout);
		num = UOS_KeyPressed();

		if ((char) num == 'q') {
			return;
		}
		num = DIGIT(num);

		if (num == 1 || num == 2) {
			reset_imp(imp[num]);
			return;
		}

		UOS_Delay(1000);
	} while(1);
}

static void reset_imp(struct imp *imp)
{
	M_write(imp->path, 0);
	imp->value = 0;
}

static void display_imp(struct imp *imp, int nr)
{
	printf("|  FI[%d] %7.2f p/s   |", nr, imp->value);
	if (nr == 2) {
		printf("\n");
	}
}

static void read_imp(struct imp *imp)
{
	int32 value;
	u_int32 tdiff;

	if ((M_read(imp->path, &value)) < 0) {
		error("read imp");
		return;
	}

	imp->t2 = UOS_MsecTimerGet();

	M_write(imp->path, 0);

	tdiff = imp->t2 - imp->t1;
	imp->t1 = imp->t2;

	if (tdiff == 0) {
		tdiff = 1000;
	}
	imp->value = value * 1000.0 / tdiff;
}

static void adc_menu(struct adc *adc)
{
	int num;

	do {
		clear();
		printf("+----------------------------------+\n");
		printf("|       Display all for F404C      |\n");
		printf("|----------------------------------|\n");
		printf("|----------------------------------|\n");
		printf("| Toggle ADC Mode                  |\n");
		printf("|    Press q to exit               |\n");
		printf("|    0-3 to change mode on channels|\n");
		printf("|----------------------------------|\n");
		printf("AI?> ");
		fflush(stdout);
		num = UOS_KeyPressed();

		if ((char) num == 'q') {
			return;
		}
		num = DIGIT(num);

		if (num >= 0 && num <= 3) {
			toggle_adc_mode(adc, num);
			return;
		}

		UOS_Delay(1000);
	} while(1);
}

static void toggle_adc_mode(struct adc *adc, int ch)
{
	if ((M_setstat(adc->path, M_MK_CH_CURRENT, ch)) < 0) {
		error("setstat M_MK_CH_CURRENT");
		goto abort;
	}

	if (adc->channels[ch].mode == VOLTAGE) {
		adc->channels[ch].mode = CURRENT;
		if ((M_setstat(adc->path, F404_MODE, F404_MODE_CURRENT)) < 0) {
			error("setstat F404_MODE");
		}
	} else {
		adc->channels[ch].mode = VOLTAGE;
		if ((M_setstat(adc->path, F404_MODE, F404_MODE_VOLTAGE)) < 0) {
			error("setstat F404_MODE");
		}
	}
abort:
	return;
}

static void read_adc(struct adc *adc)
{
	int i;

	for (i = 0; i < 8; i++) {
		if (read_adc_channel(adc, i) < 0) {
			error("read adc");
			return;
		}
	}

	return;
}

static void display_adc(struct adc *adc)
{
	int i;

	for (i = 0; i < 8; i++) {
		display_adc_channel(adc, i);
	}

	return;
}

static void display_adc_channel(struct adc *adc, int ch)
{
	char *unit;

	if (adc->channels[ch].mode == VOLTAGE) {
		unit = "V";
	} else {
		unit = "mA";
	}

	printf("|AI[%1d] = %+02.4f %s | Raw Data: 0x%04x |\n",
		ch, adc->channels[ch].value, unit,
		adc->channels[ch].hex);

	return;
}

static int read_adc_channel(struct adc *adc, int ch)
{
	int32 value = 0;

	if ((M_setstat(adc->path, M_MK_CH_CURRENT, ch)) < 0) {
		error("setstat M_MK_CH_CURRENT");
		goto abort;
	}

	if ((M_setstat(adc->path, M36_CH_ENABLE, 1)) < 0) {
		error("setstat M36_CH_ENABLE");
		goto abort;
	}

	if ((M_read(adc->path, &value)) < 0) {
		error("read");
		goto abort;
	}

	adc->channels[ch].hex = value;
	if (adc->channels[ch].mode == VOLTAGE) {
		adc->channels[ch].value = hex2volt(ch, value);
	} else {
		adc->channels[ch].value = hex2volt(ch, value) * 1000 / 250;
	}

	return 0;

abort:
	return -1;
}

/********************************* error ********************************
 *
 *	Description: Print MDIS error message
 *
 *---------------------------------------------------------------------------
 *	Input......: info	info string
 *	Output.....: -
 *	Globals....: -
 ****************************************************************************/

static void error(char *info)
{
	printf("*** can't %s: %s\n", info, M_errstring(UOS_ErrnoGet()));
}

/********************************* usage ************************************
 *
 *	Description: Print program usage
 *
 *---------------------------------------------------------------------------
 *	Input......: -
 *	Output.....: -
 *	Globals....: -
 ****************************************************************************/
static void usage(void)
{
	printf("Usage: display_all [<opts>] <device> [<opts>]\n");
	printf("	Use 'h' key inside application to display help\n");
	printf("Function: Configure and read all F404 channels\n");
	printf("\n");
	printf("Options:\n");
	printf("    -c card number (0 or 1)\n");
	printf("\n");
	printf("(c) 2013 by MEN mikro elektronik GmbH\n\n");
}

static double hex2volt(int ch, int32 value)
{
	double volts = 0.0;

	if (ch < 0 || ch > 7) {
		printf("Illegal channel %d\n", ch);
		return 0.0;
	}

	switch(ch) {
	case 0:
	case 1:
	case 2:
	case 3:
		volts = (value - 0xffc) * (20.0 / 0x1d9d);
		break;
	case 4:
		volts = value * 2 * 2.5 / 8192;
		break;
	case 5:
		volts = value * 4.3 * 2.5 / 8192;
		break;
	case 6:
		volts = value * 11 * 2.5 / 8192;
		break;
	case 7:
		volts = value * -10 * 2.5 / 8192;
		break;
	}

	return volts;
}

static void clear(void)
{
	system("clear");
}

static struct adc * init_adc(char *path)
{
	struct adc *adc = NULL;
	int i;

	adc = malloc(sizeof(*adc));
	if (adc == NULL) {
		printf("can't allocate memory for adc\n");
		return NULL;
	}

	if ((adc->path = M_open(path)) < 0) {
		error("open");
		free(adc);
		return NULL;
	}

	for (i = 0; i < 8; i++) {
		adc->channels[i].mode = VOLTAGE;
	}

	return adc;
}

static void deinit_adc(struct adc *adc)
{
	M_close(adc->path);
	free(adc);
}

static struct imp *init_imp(char *path)
{
	struct imp *imp;

	imp = malloc(sizeof(*imp));
	if (imp == NULL) {
		printf("can't allocate memory for impulse counter\n");
		return NULL;
	}

	if ((imp->path = M_open(path)) < 0) {
		error("open");
		free(imp);
		return NULL;
	}

	imp->t2 = 0;
	imp->t1 = UOS_MsecTimerGet();

	return imp;
}

static void deinit_imp(struct imp *imp)
{
	M_close(imp->path);
	free(imp);
}

static void help(void)
{
	clear();
	printf("+----------------------------------+\n");
	printf("|       Display all for F404C      |\n");
	printf("+----------------------------------+\n");
	printf("+----------------------------------+\n");
	printf("|               HELP               |\n");
	printf("| Keys:                            |\n");
	printf("|   General:                       |\n");
	printf("|   q - Quit application           |\n");
	printf("|   h - This help                  |\n");
	printf("|                                  |\n");
	printf("|   Analog Inputs:                 |\n");
	printf("|   a - Toggle ADC voltage/cuurent |\n");
	printf("|                                  |\n");
	printf("|   Frequency Inputs:              |\n");
	printf("|   f - Reset FI menu              |\n");
	printf("|                                  |\n");
	printf("|   Digital Outputs                |\n");
	printf("|   o - Set DO menu                |\n");
	printf("|                                  |\n");
	printf("|   Digital Inputs                 |\n");
	printf("|   s - Set IRQ sense menu         |\n");
	printf("|   d - Set DI debouncer           |\n");
	printf("|   t - Set DI debounce time preset|\n");
	printf("+----------------------------------+\n");

	UOS_KeyWait();
}

static struct gpio *init_gpio(const char *path)
{
	struct gpio *g = NULL;
	int i;

	g = malloc(sizeof(*g));
	if (!g) {
		printf("can't allocate memory for GPIO\n");
		return NULL;
	}

	if ((g->path = M_open(path)) < 0) {
		error("open");
		free(g);
		return NULL;
	}

	if ((M_getstat(g->path, Z17_DIRECTION, &g->dir)) < 0) {
		error("getstat Z17_DIRECTION");
		deinit_gpio(g);
		return NULL;
	}

	/* Make sure DOs are initialized low */
	for (i = 0; i < 4; i++) {
		g->out[i] = 0;
		M_setstat(g->path, Z17_CLR_PORTS, BIT(0));
	}

	/* Enable global device interrupt */
	if (M_setstat(g->path, M_MK_IRQ_ENABLE, TRUE) < 0) {
		error("setstat M_MK_IRQ_ENABLE");
		deinit_gpio(g);
		return NULL;
	}

	/* Set Port IRQ sensing to both edges */
	if (M_setstat(g->path, Z17_IRQ_SENSE, 0xffffffff) < 0) {
		error("setstat Z17_IRQ_SENSE");
		deinit_gpio(g);
		return NULL;
	}

	UOS_SigInit(signal_handler);
	UOS_SigInstall(UOS_SIG_USR1);

	if (M_setstat(g->path, Z17_SET_SIGNAL, UOS_SIG_USR1) < 0) {
		error("setstat Z17_SET_SIGNAL");
		deinit_gpio(g);
		return NULL;
	}

	return g;
}

static void deinit_gpio(struct gpio *gpio)
{
	M_close(gpio->path);
	free(gpio);
}

static char gpio_state(int port, int bit)
{
	char state;

	if (port & BIT(bit)) {
		state = 'H';
	} else {
		state = 'L';
	}

	return state;
}

static int bit2dec(int byte)
{
	int i;

	for (i = 0; i < 32; i++) {
		if (byte & BIT(i)) {
			return i;
		}
	}

	return 0;
}

static void display_gpio_dber(struct gpio *gpio)
{
	int i;
	char state;

	/* Digital Inputs 0 - 3 */
	for (i = 0; i < 4; i++) {
		state = gpio_state(gpio->dber, i);
		printf("| DI[%2d] %c |", i, state);
	}
	printf("\n");

	/* Digital Inputs 4 - 7 */
	for (i = 4; i < 8; i++) {
		state = gpio_state(gpio->dber, i);
		printf("| DI[%2d] %c |", i, state);
	}
	printf("\n");

	/* Digital Inputs 8 - 12 */
	for (i = 8; i < 12; i++) {
		state = gpio_state(gpio->dber, i);
		printf("| DI[%2d] %c |", i, state);
	}
	printf("\n");

	/* Digital Inputs 12 - 15 */
	for (i = 12; i < 16; i++) {
		state = gpio_state(gpio->dber, i);
		printf("| DI[%2d] %c |", i, state);
	}
	printf("\n");
}

static void display_gpio_dber_time(struct gpio *gpio)
{
	int i;


	/* Digital Inputs 0 - 3 */
	for (i = 0; i < 4; i++) {
		printf("| DI[%2d] 0x%x |", i, gpio->dber_time[i]);
	}
	printf("\n");

	/* Digital Inputs 4 - 7 */
	for (i = 4; i < 8; i++) {
		printf("| DI[%2d] 0x%x |", i, gpio->dber_time[i]);
	}
	printf("\n");

	/* Digital Inputs 8 - 12 */
	for (i = 8; i < 12; i++) {
		printf("| DI[%2d] 0x%x |", i, gpio->dber_time[i]);
	}
	printf("\n");

	/* Digital Inputs 12 - 15 */
	for (i = 12; i < 16; i++) {
		printf("| DI[%2d] 0x%x |", i, gpio->dber_time[i]);
	}
	printf("\n");
}

static void display_gpio_last_irq(struct gpio *gpio)
{
	char tbuf[64];

	if (last_irq == 0) {
		strncpy(tbuf, "none", sizeof(tbuf));
	} else {
		strncpy(tbuf, ctime(&last_irq), sizeof(tbuf));
		tbuf[strlen(tbuf) - 1] = '\0';
	}
	printf("|Last Interrupt DI[%02d] %s|\n",
		   bit2dec(gpio->irq_pin), tbuf);
}

static void display_gpio_out(struct gpio *gpio)
{
	int i;

	for (i = 0; i < 4; i++) {
		printf("| DO[%2d] %c |", i, (gpio->out[i] ? 'H' : 'L'));
	}
	printf("\n");
}

static const char gpio_irq_sense[] = {
	'o',
	'r',
	'f',
	'b',
};

static void display_gpio_sense(struct gpio *gpio)
{
	int32 value;
	int i;
	int j;

	for (i = 0, j = 0; i < 32; i += 2, j++) {
		value = (gpio->irq_sense >> i) & 0x3;
		printf("| DO[%2d] %c |", j, gpio_irq_sense[value]);
		if ((j + 1) % 4 == 0) {
			printf("\n");
		}
	}
}

static void display_gpio_in(struct gpio *gpio)
{
	int i;
	char state;

	/* Digital Inputs 0 - 3 */
	for (i = 0; i < 4; i++) {
		state = gpio_state(gpio->in, i);
		printf("| DI[%2d] %c |", i, state);
	}
	printf("\n");

	/* Digital Inputs 4 - 7 */
	for (i = 4; i < 8; i++) {
		state = gpio_state(gpio->in, i);
		printf("| DI[%2d] %c |", i, state);
	}
	printf("\n");

	/* Digital Inputs 8 - 12 */
	for (i = 8; i < 12; i++) {
		state = gpio_state(gpio->in, i);
		printf("| DI[%2d] %c |", i, state);
	}
	printf("\n");

	/* Digital Inputs 12 - 15 */
	for (i = 12; i < 16; i++) {
		state = gpio_state(gpio->in, i);
		printf("| DI[%2d] %c |", i, state);
	}
	printf("\n");

}

static void read_gpio(struct gpio *gpio)
{
	int i;

	if (have_irq) {
		have_irq = 0;
		if (M_getstat(gpio->path, Z17_IRQ_LAST_REQUEST, &gpio->irq_pin) < 0) {
			error("getstat Z17_IRQ_LAST_REQUEST");
			return;
		}
	}

	if (M_getstat(gpio->path, Z17_DEBOUNCE, &gpio->dber) < 0) {
		error("getstat Z17_DEBOUNCE");
		return;
	}

	if (M_getstat(gpio->path, Z17_IRQ_SENSE, &gpio->irq_sense) < 0) {
		error("getstat Z17_IRQ_SENSE");
		return;
	}

	for (i = 0; i < 16; i++) {
		if (M_setstat(gpio->path, M_MK_CH_CURRENT, i) < 0) {
			printf("M_MK_CH_CURRENT i = %d\n", i);
			error("setstat M_MK_CH_CURRENT");
			return;
		}
		if (M_getstat(gpio->path, Z127_DEBOUNCE_TIME,
					  &gpio->dber_time[i]) < 0) {
			error("getstat Z127_DEBOUNCE_TIME");
			return;
		}
	}

	if ((M_read(gpio->path, &gpio->in) < 0)) {
		error("read gpio");
		return;
	}
}

static int write_gpio(struct gpio *gpio, int num)
{
	int32 out;

	if (gpio->out[num] == 0) {
		gpio->out[num] = 1;
	} else {
		gpio->out[num] = 0;

	}

	out = gpio->out[num];

	if (M_setstat(gpio->path,
				  out ? Z17_SET_PORTS : Z17_CLR_PORTS,
				  BIT(num)) < 0) {
		error("write gpio");
	}

	return 0;
}

static void write_gpio_sense(struct gpio *gpio, int num, int sense)
{
	int32 mask;

	if (M_getstat(gpio->path, Z17_IRQ_SENSE, &gpio->irq_sense) < 0) {
		error("getstat Z17_IRQ_SENSE");
		return;
	}

	mask = 3 << (num * 2);
	gpio->irq_sense &= ~mask;
	gpio->irq_sense |= sense << (num * 2);

	if (M_setstat(gpio->path, Z17_IRQ_SENSE, gpio->irq_sense) < 0) {
		error("setstat Z17_IRQ_SENSE");
		return;
	}

	if (M_getstat(gpio->path, Z17_IRQ_SENSE, &gpio->irq_sense) < 0) {
		error("getstat Z17_IRQ_SENSE");
		return;
	}
}

static void gpio_menu_out(struct gpio *gpio)
{
	int num;

	do {
		clear();
		printf("+----------------------------------+\n");
		printf("|       Display all for F404C      |\n");
		printf("|----------------------------------|\n");
		printf("|----------------------------------|\n");
		printf("| Toggle Digital Outputs Mode      |\n");
		printf("|    Press q to exit               |\n");
		printf("|    0-3 to change mode on DOs     |\n");
		printf("|----------------------------------|\n");
		printf("DO?> ");
		fflush(stdout);
		num = UOS_KeyPressed();

		if ((char) num == 'q') {
			return;
		}
		num = DIGIT(num);

		if (num >= 0 && num <= 3) {
			write_gpio(gpio, num);
			return;
		}

		UOS_Delay(1000);
	} while(1);
}

static int key2hex(char key)
{
	if (key >= '0' && key <= '9') {
		return DIGIT(key);
	} else if (key >= 'a' && key <= 'f') {
		return key - 'a' + 10;
	}

	return -1;
}

static void gpio_menu_sense(struct gpio *gpio)
{
	int num;

	do {
		clear();
		printf("+----------------------------------+\n");
		printf("|       Display all for F404C      |\n");
		printf("|----------------------------------|\n");
		printf("|----------------------------------|\n");
		printf("| Toggle Digital Input IRQ Mode    |\n");
		printf("|    Press q to exit               |\n");
		printf("|    0-f to change IRQs on DIs     |\n");
		printf("|----------------------------------|\n");
		printf("DO?> ");
		fflush(stdout);
		num = UOS_KeyPressed();

		if ((char) num == 'q') {
			return;
		}

		num = key2hex(num);

		if (num != -1) {
			gpio_menu_sense_pin(gpio, num);
		}

		UOS_Delay(1000);
	} while(1);
}

static void gpio_menu_sense_pin(struct gpio *gpio, int pin)
{
	int num;
	int sense = -1;

	do {
		clear();
		printf("+----------------------------------+\n");
		printf("|       Display all for F404C      |\n");
		printf("|----------------------------------|\n");
		printf("|----------------------------------|\n");
		printf("| Toggle Digital Input IRQ Mode    |\n");
		printf("|    Press q to exit               |\n");
		printf("|    o,r,f,b to change IRQs on DIs |\n");
		printf("|----------------------------------|\n");
		printf("DO?> ");
		fflush(stdout);
		num = UOS_KeyPressed();

		if ((char) num == 'q') {
			return;
		}

		if (num == 'o') {
			sense = 0;
		} else if (num == 'r') {
			sense = 1;
		} else if (num == 'f') {
			sense = 2;
		} else if (num == 'b') {
			sense = 3;
		}

		if (sense != -1) {
			write_gpio_sense(gpio, pin, sense);
			return;
		}

		UOS_Delay(1000);
	} while(1);
}

static void __MAPILIB signal_handler(u_int32 sig)
{
	if (sig != UOS_SIG_USR1) {
		last_sig = sig;
		return;
	}

	last_irq = time(NULL);
	have_irq = 1;
}

static void gpio_menu_dber(struct gpio *gpio)
{
	int num;

	do {
		clear();
		printf("+----------------------------------+\n");
		printf("|       Display all for F404C      |\n");
		printf("|----------------------------------|\n");
		printf("|----------------------------------|\n");
		printf("| Toggle Digital Input DBER Mode   |\n");
		printf("|    Press q to exit               |\n");
		printf("| 0-f to change DBER on DIs        |\n");
		printf("|----------------------------------|\n");
		printf("DO?> ");
		fflush(stdout);
		num = UOS_KeyPressed();

		if ((char) num == 'q') {
			return;
		}

		num = key2hex(num);

		if (num != -1) {
			gpio_menu_dber_pin(gpio, num);
		}

		UOS_Delay(1000);
	} while(1);
}

static void gpio_menu_dber_pin(struct gpio *gpio, int pin)
{
	int num;
	int sense = -1;

	do {
		clear();
		printf("+----------------------------------+\n");
		printf("|       Display all for F404C      |\n");
		printf("|----------------------------------|\n");
		printf("|----------------------------------|\n");
		printf("| Toggle Digital Input DBER Mode   |\n");
		printf("|    Press q to exit               |\n");
		printf("| h,l to change DBER on DIs        |\n");
		printf("|----------------------------------|\n");
		printf("DO?> ");
		fflush(stdout);
		num = UOS_KeyPressed();

		if ((char) num == 'q') {
			return;
		}

		if (num == 'h') {
			sense = 1;
		} else if (num == 'l') {
			sense = 0;
		}

		if (sense != -1) {
			write_gpio_dber(gpio, pin, sense);
			return;
		}

		UOS_Delay(1000);
	} while(1);
}

static void write_gpio_dber(struct gpio *gpio, int num, int sense)
{

	if (M_getstat(gpio->path, Z17_DEBOUNCE, &gpio->dber) < 0) {
		error("getstat Z17_IRQ_SENSE");
		return;
	}

	if (sense) {
		gpio->dber |= BIT(num);
	} else {
		gpio->dber &= ~BIT(num);
	}

	if (M_setstat(gpio->path, Z17_DEBOUNCE, gpio->dber) < 0) {
		error("setstat Z17_IRQ_SENSE");
		return;
	}

	if (M_getstat(gpio->path, Z17_DEBOUNCE, &gpio->dber) < 0) {
		error("getstat Z17_IRQ_SENSE");
		return;
	}
}

static void gpio_menu_dber_time(struct gpio *gpio)
{
	int num;

	do {
		clear();
		printf("+----------------------------------+\n");
		printf("|      Display all for F404C       |\n");
		printf("|----------------------------------|\n");
		printf("|----------------------------------|\n");
		printf("| Toggle Digital Input DBER Mode   |\n");
		printf("|    Press q to exit               |\n");
		printf("| 0-f to change DBER on DIs        |\n");
		printf("|----------------------------------|\n");
		printf("DO?> ");
		fflush(stdout);
		num = UOS_KeyPressed();

		if ((char) num == 'q') {
			return;
		}

		num = key2hex(num);

		if (num != -1) {
			gpio_menu_dber_time_pin(gpio, num);
		}

		UOS_Delay(1000);
	} while(1);
}

static int16 dber_time[16] = {
	[0x0] = 0x0000,
	[0x1] = 0x1111,
	[0x2] = 0x2222,
	[0x3] = 0x3333,
	[0x4] = 0x4444,
	[0x5] = 0x5555,
	[0x6] = 0x6666,
	[0x7] = 0x7777,
	[0x8] = 0x8888,
	[0x9] = 0x9999,
	[0xa] = 0xaaaa,
	[0xb] = 0xbbbb,
	[0xc] = 0xcccc,
	[0xd] = 0xdddd,
	[0xe] = 0xeeee,
	[0xf] = 0xffff,
};

static void gpio_menu_dber_time_pin(struct gpio *gpio, int pin)
{
	int num;

	do {
		clear();
		printf("+----------------------------------+\n");
		printf("|		Display all for F404C	   |\n");
		printf("|----------------------------------|\n");
		printf("|----------------------------------|\n");
		printf("| Toggle Digital Input DBER Mode   |\n");
		printf("|	 Press q to exit			   |\n");
		printf("| 0-f to change DBER on DIs        |\n");
		printf("|                                  |\n");
		printf("| 0 = 0x0000                       |\n");
		printf("| 1 = 0x1111                       |\n");
		printf("| 2 = 0x2222                       |\n");
		printf("| 3 = 0x3333                       |\n");
		printf("| 4 = 0x4444                       |\n");
		printf("| 5 = 0x5555                       |\n");
		printf("| 6 = 0x6666                       |\n");
		printf("| 7 = 0x7777                       |\n");
		printf("| 8 = 0x8888                       |\n");
		printf("| 9 = 0x9999                       |\n");
		printf("| a = 0xaaaa                       |\n");
		printf("| b = 0xbbbb                       |\n");
		printf("| c = 0xcccc                       |\n");
		printf("| d = 0xdddd                       |\n");
		printf("| e = 0xeeee                       |\n");
		printf("| f = 0xffff                       |\n");
		printf("|                                  |\n");
		printf("|----------------------------------|\n");
		printf("DO?> ");
		fflush(stdout);
		num = UOS_KeyPressed();

		if ((char) num == 'q') {
			return;
		}

		num = key2hex(num);

		if (num != -1) {
			write_gpio_dber_time(gpio, pin, num);
			return;
		}

		UOS_Delay(1000);
	} while(1);
}

static void write_gpio_dber_time(struct gpio *gpio, int pin, int value)
{

	if (M_setstat(gpio->path, M_MK_CH_CURRENT, pin) < 0) {
		error("setstat M_MK_CH_CURRENT");
		return;
	}

	if (M_getstat(gpio->path, Z127_DEBOUNCE_TIME, &gpio->dber_time[pin]) < 0) {
		error("getstat Z17_DEBOUNCE_TIME");
		return;
	}

	gpio->dber_time[pin] = dber_time[value];

	if (M_setstat(gpio->path, Z127_DEBOUNCE_TIME, gpio->dber_time[pin]) < 0) {
		error("setstat Z17_DEBOUNCE_TIME");
		return;
	}

	if (M_getstat(gpio->path, Z127_DEBOUNCE_TIME, &gpio->dber_time[pin]) < 0) {
		error("getstat Z17_DEBOUNCE_TIME");
		return;
	}
}
