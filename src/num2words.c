#include "num2words.h"

#include <string.h>
#include <stdio.h>

static const char *const ONES[] = {
	"zero",
	"one",
	"two",
	"three",
	"four",
	"five",
	"six",
	"seven",
	"eight",
	"nine"};

static const char *const TEENS[] = {
	"",
	"eleven",
	"twelve",
	"thirteen",
	"fourteen",
	"fifteen",
	"sixteen",
	"seventeen",
	"eighteen",
	"nineteen"};

static const char *const TEENS_SPLIT[][2] = {
	{"", ""},
	{"eleven", ""},
	{"twelve", ""},
	{"thirteen", ""},
	{"four", "teen"},
	{"fifteen", ""},
	{"sixteen", ""},
	{"seven", "teen"},
	{"eight", "teen"},
	{"nine", "teen"}};

static const char *const TENS[] = {
	"",
	"ten",
	"twenty",
	"thirty",
	"forty",
	"fifty",
	"sixty",
	"seventy",
	"eighty",
	"ninety"};

static const char *STR_OH_TICK = "o'";
static const char *STR_CLOCK = "clock";

void day_to_formal_words(int day, char *word)
{

	strcpy(word, "");

	if (day < 10)
	{
		strcat(word, ONES[day]);
		return;
	}
	if (day > 10 && day < 20)
	{
		strcat(word, TEENS[(day - 10)]);
		return;
	}

	strcat(word, TENS[day / 10 % 10]);

	int day_ones = day % 10;
	if (day_ones)
	{
		strcat(word, " ");
		strcat(word, ONES[day_ones]);
	}
}

// o'clock (0) and plain number words (10..)
void minute_to_formal_words(int minutes, char *first_word, char *second_word)
{
	// PBL_ASSERT(minutes >= 0 && minutes < 60, "Invalid number of minutes");

	strcpy(first_word, "");
	strcpy(second_word, "");

	if (minutes == 0)
	{
		strcat(first_word, STR_OH_TICK);
		strcat(first_word, STR_CLOCK);
		return;
	}
	if (minutes < 10)
	{
		strcat(first_word, ONES[minutes % 10]);
		return;
	}
	if (minutes > 10 && minutes < 20)
	{
		strcat(first_word, TEENS_SPLIT[(minutes - 10) % 10][0]);
		strcat(second_word, TEENS_SPLIT[(minutes - 10) % 10][1]);
		return;
	}

	strcat(first_word, TENS[minutes / 10 % 10]);

	int minute_ones = minutes % 10;
	if (minute_ones)
	{
		strcat(second_word, ONES[minute_ones]);
	}
}

void hour_to_12h_word(int hours, char *word)
{
	// PBL_ASSERT(hours >= 0 && hours < 24, "Invalid number of hours");
	hours = hours % 12;

	if (hours == 0)
	{
		hours = 12;
	}

	strcpy(word, "");

	int tens_val = hours / 10 % 10;
	int ones_val = hours % 10;

	if (tens_val > 0)
	{
		if (tens_val == 1 && hours != 10)
		{
			strcat(word, TEENS[ones_val]);
			return;
		}
		strcat(word, TENS[tens_val]);
		if (ones_val > 0)
		{
			strcat(word, " ");
		}
	}

	if (ones_val > 0 || hours == 0)
	{
		strcat(word, ONES[ones_val]);
	}
}

void hour_to_24h_word(int hours, char *first_word, char *second_word)
{
	// PBL_ASSERT(hours >= 0 && hours < 24, "Invalid number of hours");

	hours = hours % 24;

	strcpy(first_word, "");
	strcpy(second_word, "");

	if (hours < 10)
	{
		strcat(first_word, ONES[hours]);
		return;
	}
	if (hours > 10 && hours < 20)
	{
		strcat(first_word, TEENS_SPLIT[(hours - 10)][0]);
		strcat(second_word, TEENS_SPLIT[(hours - 10)][1]);
		return;
	}

	strcat(first_word, TENS[hours / 10 % 10]);

	int hour_ones = hours % 10;
	if (hour_ones)
	{
		strcat(second_word, ONES[hour_ones]);
	}
}
