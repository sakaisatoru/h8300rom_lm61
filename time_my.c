#include "time_my.h"

static int8_t md[13] = {0,  31,28,31,30,  31,30,31,31,  30,31,30,31};
static char *weekday[] = {"Sun", "Mon", "Tue", "Wed", "Thr", "Fri", "Sat"};

int IsLeapYear(int16_t y)
{
    int i = 0;
    if (y % 4 == 0) {			// ４で割れる年はうるう年		
		if (y % 100 == 0) {		// 但し１００で割れる年はうるう年ではない
			if (y % 400 == 0) {	// 但し４００で割れる年はうるう年
				i = 1;
			}
	    } else {
		    i = 1;
	    }
    }
    return i;
}

int16_t DayOfWeek(int16_t y, int16_t m, int16_t d)
{
    // ツェラーの公式で曜日を求める 0 日曜日 ... 6 土曜日
    // 負数には対応していないことに注意
    if (m < 3) {
        y--;
        m += 12;	// １月と２月は前年の１３月と１４月に変換
    }
    return (y + y/4 - y/100 + y/400 + (13*m + 8)/5 + d) % 7;
}

/*
 * 時刻を１秒進める
 */
void IncTime(MYTIME *mt)
{
	mt->Second++;
	if (mt->Second <= 59) return;
	
	mt->Second = 0;
	mt->Minute++;
	if (mt->Minute <= 59) return;
	
	mt->Minute = 0;
	mt->Hour++;
	if (mt->Hour <= 23) return;
	
	mt->Hour = 0;
	mt->Day++;
	if (mt->Day <= md[mt->Month]) return;

	if (mt->Month == 2) {
		if (IsLeapYear(mt->Year)) {
			if (mt->Day <= 29) return;
		}
	}

	mt->Day = 1;
	mt->Month++;
	if (mt->Month <= 12) return;

	mt->Month = 1;
	mt->Year++;
}

/*
 * Unix時間を変換して返す
 * unix : unix時間
 * loc : 時差 (jst : 9*60*60)
 * mt : 変換結果格納先
 */
void UnixToMYTIME(int64_t uni, int64_t loc, MYTIME *mt)
{
	uni += loc;

	int32_t days = (int32_t)(uni / 86400);
	int16_t years = 1970 -1;
	int16_t month = 1 -1;
	
	int leapyear;
	
	for (;;) {
		years++;
		leapyear = IsLeapYear(years);
		int32_t tmp = days;
		days -= (leapyear)? 366:365;
		if (days < 1) {
			days = tmp;
			break;
		}
	}
	for (;;) {
		month++;
		int32_t tmp = days;
		days -= md[month];
		if (leapyear) days--;
		if (days < 1) {
			days = tmp;
			break;
		}
	}
	days++;

	mt->Year = years;
	mt->Month = (int8_t)month;
	mt->Day = (int8_t)days;
	int32_t dd = (int32_t)(uni % 86400);
	mt->Hour = (int8_t)(dd / 3600);
	mt->Minute = (int8_t)((dd % 3600) / 60);
	mt->Second = (int8_t)((dd % 3600) % 60);
	mt->Weekday = DayOfWeek((int16_t)mt->Year, (int16_t)mt->Month, (int16_t)mt->Day);
	mt->WeekdayName = weekday[mt->Weekday];
}
