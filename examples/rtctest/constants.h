#ifndef RTCTEST_CONSTANTS_H
#define RTCTEST_CONSTANTS_H

#define BLACK 0x000000FF
#define WHITE 0xFFFFFFFF

#define JOYSTICK_DEAD_ZONE 32

#define MAX(a,b)  ({ typeof(a) _a = a; typeof(b) _b = b; _a > _b ? _a : _b; })
#define MIN(a,b)  ({ typeof(a) _a = a; typeof(b) _b = b; _a < _b ? _a : _b; })
#define CLAMP(x, min, max) (MIN(MAX((x), (min)), (max)))
#define WRAP(x, min, max)  ({ \
    typeof(x) _x = x; typeof(min) _min = min; typeof(max) _max = max; \
    _x < _min ? _max : _x > _max ? _min : _x; \
})

#define SECONDS_PER_YEAR (60.0f * 60.0f * 24.0f * 365.2422f)
#define TIMESTAMP_TO_YEAR(ts) CLAMP((int)(roundf((ts) / SECONDS_PER_YEAR)) + 1970, 1900, 2199)

#define GLYPH_WIDTH  8
#define GLYPH_HEIGHT 8

#define LINE1 (8  * GLYPH_HEIGHT)
#define LINE2 (12 * GLYPH_HEIGHT)
#define LINE3 (14 * GLYPH_HEIGHT)
#define LINE4 (18 * GLYPH_HEIGHT)
#define LINE5 (20 * GLYPH_HEIGHT)
#define LINE6 (22 * GLYPH_HEIGHT)
#define LINE7 (24 * GLYPH_HEIGHT)

/* Line 2 */
#define YEAR_X  (12 * GLYPH_WIDTH)
#define MONTH_X (YEAR_X + (5 * GLYPH_WIDTH))
#define DAY_X   (MONTH_X + (3 * GLYPH_WIDTH))
#define DOW_X   (DAY_X + (4 * GLYPH_WIDTH))
/* Line 3 */
#define HOUR_X  (16 * GLYPH_WIDTH)
#define MIN_X   (HOUR_X + (3 * GLYPH_WIDTH))
#define SEC_X   (MIN_X + (3 * GLYPH_WIDTH))

#define EDIT_YEAR  0x0020
#define EDIT_MONTH 0x0010
#define EDIT_DAY   0x0008
#define EDIT_HOUR  0x0004
#define EDIT_MIN   0x0002
#define EDIT_SEC   0x0001
#define EDIT_NONE  0x0000


/* SCREEN_WIDTH_GUIDE:                "----------------------------------------" */
static const char* RUN_SOFT_MESSAGE = "        RTC source detected: None       ";
static const char* RUN_JOY_MESSAGE  = "       RTC source detected: Joybus      ";
static const char* RUN_DD_MESSAGE   = "        RTC source detected: 64DD       ";
static const char* RUN_BB_MESSAGE   = "        RTC source detected: iQue       ";
static const char* PAUSED_MESSAGE   = "      Adjust the current date/time:     ";
static const char* RTC_DATE_FORMAT  = "            YYYY-MM-DD (DoW)            ";
static const char* RTC_TIME_FORMAT  = "                HH:MM:SS                ";
static const char* ADJUST_MESSAGE   = "      Press A to adjust date/time       ";
static const char* CONFIRM_MESSAGE  = "        Press A to write to RTC         ";
static const char* NOWRITE_MESSAGE  = "      RTC source is not persistent!     ";
static const char* SRC_JOY_MESSAGE  = "    Press L to switch to Joybus RTC     ";
static const char* SRC_DD_MESSAGE   = "     Press L to switch to 64DD RTC      ";

static const char* const DAYS_OF_WEEK[7] =
    { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

#endif
