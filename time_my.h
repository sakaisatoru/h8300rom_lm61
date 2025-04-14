#ifndef TIME_MY_H
#define TIME_MY_H
#include <stdint.h>
typedef struct {
			int16_t Year;
			int8_t Month;
			int8_t Day;
			int8_t Weekday;
			char *WeekdayName;
			int8_t Hour;
			int8_t Minute;
			int8_t Second;
		} MYTIME;
		
int IsLeapYear(int16_t y);
int16_t DayOfWeek(int16_t y, int16_t m, int16_t d);
void UnixToMYTIME(int64_t uni, int64_t loc, MYTIME *mt);
void IncTime(MYTIME *mt);
#endif
