/* Shared library add-on to iptables to add TIME matching support. */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <iptables.h>
#include <linux/netfilter_ipv4/ipt_time.h>
#include <time.h>

/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"TIME v%s options:\n"
" --timestart value --timestop value --days listofdays\n"
"          timestart value : HH:MM\n"
"          timestop  value : HH:MM\n"
"          listofdays value: a list of days to apply -> ie. Mon,Tue,Wed,Thu,Fri. Case sensitive\n",
NETFILTER_VERSION);
}

static struct option opts[] = {
	{ "timestart", 1, 0, '1' },
	{ "timestop", 1, 0, '2' },
	{ "days", 1, 0, '3'},
	{0}
};

/* Initialize the match. */
static void
init(struct ipt_entry_match *m, unsigned int *nfcache)
{
	/* caching not yet implemented */
        *nfcache |= NFC_UNKNOWN;
}


/**
 * param: part1, a pointer on a string 2 chars maximum long string, that will contain the hours.
 * param: part2, a pointer on a string 2 chars maximum long string, that will contain the minutes.
 * param: str_2_parse, the string to parse.
 * return: 1 if ok, 0 if error.
 */
static int
split_time(char **part1, char **part2, const char *str_2_parse)
{
	unsigned short int i,j;
	char *rpart1 = *part1;
	char *rpart2 = *part2;

	/* Check the length of the string */
	if (strlen(str_2_parse) > 5)
		return 0;
	/* parse the first part until the ':' */
	for (i=0; i<2; i++)
	{
		if (str_2_parse[i] != ':') {
			rpart1[i] = str_2_parse[i];
		}
	}
	if (str_2_parse[i] != ':')
		++i;
	/* parse the second part */
	j=++i;
	for (i=j; i<5; i++)
	{
		rpart2[i-j] = str_2_parse[i];
	}
	/* if we are here, format should be ok. */
	return 1;
}

static void
parse_time_string(int *hour, int *minute, const char *time)
{
	char *hours;
	char *minutes;

	hours = (char *)malloc(3);
	minutes = (char *)malloc(3);
	bzero((void *)hours, 3);
	bzero((void *)minutes, 3);

	if (split_time(&hours, &minutes, time) == 1)
	{
		*hour   = string_to_number(hours, 0, 23);
		*minute = string_to_number(minutes, 0, 59);
	}
	if ((*hour != (-1)) && (*minute != (-1))) {
		free(hours);
		free(minutes);
		return;
	}

	/* If we are here, there was a problem ..*/
	exit_error(PARAMETER_PROBLEM,
		   "invalid time `%s' specified, should be HH:MM format", time);
}

/* return 1->ok, return 0->error */
static int
parse_day(int *days, int from, int to, const char *string)
{
	char *dayread;
	char *days_str[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
	unsigned short int days_of_week[7] = {64, 32, 16, 8, 4, 2, 1};
	unsigned int i;

	dayread = (char *)malloc(4);
	bzero(dayread, 4);
	if ((to-from) != 3) {
		free(dayread);
		return 0;
	}
	for (i=from; i<to; i++)
		dayread[i-from] = string[i];
	for (i=0; i<7; i++)
		if (strcmp(dayread, days_str[i]) == 0)
		{
			*days |= days_of_week[i];
			free(dayread);
			return 1;
		}
	/* if we are here, we didn't read a valid day */
	free(dayread);
	return 0;
}

static void
parse_days_string(int *days, const char *daystring)
{
	int len;
	int i=0;
	char *err = "invalid days `%s' specified, should be Sun,Mon,Tue... format";

	len = strlen(daystring);
	if (len < 3)
		exit_error(PARAMETER_PROBLEM, err, daystring);	
	while(i<len)
	{
		if (parse_day(days, i, i+3, daystring) == 0)
			exit_error(PARAMETER_PROBLEM, err, daystring);
		i += 4;
	}
}

#define IPT_TIME_START 0x01
#define IPT_TIME_STOP  0x02
#define IPT_TIME_DAYS  0x04


/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry,
      unsigned int *nfcache,
      struct ipt_entry_match **match)
{
	struct ipt_time_info *timeinfo = (struct ipt_time_info *)(*match)->data;
	int hours, minutes, days;

	switch (c)
	{
		/* timestart */
	case '1':
		if (invert)
			exit_error(PARAMETER_PROBLEM,
                                   "unexpected '!' with --timestart");
		if (*flags & IPT_TIME_START)
                        exit_error(PARAMETER_PROBLEM,
                                   "Can't specify --timestart twice");
		parse_time_string(&hours, &minutes, optarg);
		timeinfo->hour_start = hours;
		timeinfo->minute_start = minutes;
		*flags |= IPT_TIME_START;
		break;
		/* timestop */
	case '2':
		if (invert)
			exit_error(PARAMETER_PROBLEM,
                                   "unexpected '!' with --timestop");
		if (*flags & IPT_TIME_STOP)
                        exit_error(PARAMETER_PROBLEM,
                                   "Can't specify --timestop twice");
		parse_time_string(&hours, &minutes, optarg);
		timeinfo->hour_stop = hours;
		timeinfo->minute_stop = minutes;
		*flags |= IPT_TIME_STOP;
		break;

		/* days */
	case '3':
		if (invert)
			exit_error(PARAMETER_PROBLEM,
                                   "unexpected '!' with --days");
		if (*flags & IPT_TIME_DAYS)
                        exit_error(PARAMETER_PROBLEM,
                                   "Can't specify --days twice");
		parse_days_string(&days, optarg);
		timeinfo->days_match = days;
		*flags |= IPT_TIME_DAYS;
		break;
	default:
		return 0;
	}
	return 1;
}

/* Final check; must have specified --time-start --time-stop --days. */
static void
final_check(unsigned int flags)
{
	if (flags != (IPT_TIME_START | IPT_TIME_STOP | IPT_TIME_DAYS))
		exit_error(PARAMETER_PROBLEM,
			   "TIME match: You must specify `--time-start --time-stop and --days'");
}


static void
print_days(int daynum)
{
	char *days[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
	unsigned short int days_of_week[7] = {64, 32, 16, 8, 4, 2, 1};
	unsigned short int i, nbdays=0;

	for (i=0; i<7; i++) {
		if ((days_of_week[i] & daynum) == days_of_week[i])
		{
			if (nbdays>0)
				printf(",%s", days[i]);
			else
				printf("%s", days[i]);
			++nbdays;
		}
	}
}

/* Prints out the matchinfo. */
static void
print(const struct ipt_ip *ip,
      const struct ipt_entry_match *match,
      int numeric)
{
	struct ipt_time_info *time = ((struct ipt_time_info *)match->data);
	printf(" TIME from %d:%d to %d:%d on ",
	       time->hour_start, time->minute_start,
	       time->hour_stop, time->minute_stop);
	print_days(time->days_match);
	printf(" ");
}

/* Saves the data in parsable form to stdout. */
static void
save(const struct ipt_ip *ip, const struct ipt_entry_match *match)
{
	struct ipt_time_info *time = ((struct ipt_time_info *)match->data);

	printf(" --timestart %.2d:%.2d --timestop %.2d:%.2d --days ",
	       time->hour_start, time->minute_start,
	       time->hour_stop, time->minute_stop);
	print_days(time->days_match);
	printf(" ");
}

struct iptables_match timestruct
= { NULL,
    "time",
    NETFILTER_VERSION,
    IPT_ALIGN(sizeof(struct ipt_time_info)),
    IPT_ALIGN(sizeof(struct ipt_time_info)),
    &help,
    &init,
    &parse,
    &final_check,
    &print,
    &save,
    opts
};

void _init(void)
{
	register_match(&timestruct);
}