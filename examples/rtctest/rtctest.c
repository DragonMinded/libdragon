#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <libdragon.h>

#define BLACK 0x000000FF
#define WHITE 0xFFFFFFFF

#define JOYSTICK_DEAD_ZONE 32

#define GLYPH_WIDTH  8
#define GLYPH_HEIGHT 8

#define LINE1 (8  * GLYPH_HEIGHT)
#define LINE2 (12 * GLYPH_HEIGHT)
#define LINE3 (14 * GLYPH_HEIGHT)
#define LINE4 (18 * GLYPH_HEIGHT)
#define LINE5 (20 * GLYPH_HEIGHT)

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

/* SCREEN_WIDTH_GUIDE:                 "----------------------------------------" */
static const char * MISSING_MESSAGE  = "     Real-time clock not detected.      ";
static const char * HELP_1_MESSAGE   = "     Double-check the settings for      ";
static const char * HELP_2_MESSAGE   = "      your emulator or flash cart.      ";
static const char * PROBING_MESSAGE  = "     Probing the real-time clock...     "; 
static const char * RUNNING_MESSAGE  = "       Reading time from the RTC:       ";
static const char * PAUSED_MESSAGE   = "      Adjust the current date/time:     ";
static const char * WRITING_MESSAGE  = "          Setting the clock...          ";
static const char * RTC_DATE_FORMAT  = "            YYYY-MM-DD (DoW)            ";
static const char * RTC_TIME_FORMAT  = "                HH:MM:SS                ";
static const char * ADJUST_MESSAGE   = "      Press A to adjust date/time       ";
static const char * CONFIRM_MESSAGE  = "        Press A to write to RTC         ";
static const char * RETEST_MESSAGE   = "      Press B to re-run write test      ";
static const char * NOSTATUS_MESSAGE = "         RTC status test failed!        ";
static const char * NOWRITE_MESSAGE  = "         RTC write test failed!         ";

static const char * const DAYS_OF_WEEK[7] =
    { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

static char year[sizeof("YYYY")];
static char month[sizeof("MM")];
static char day[sizeof("DD")];
static const char * dow;
static char hour[sizeof("HH")];
static char min[sizeof("MM")];
static char sec[sizeof("SS")];

static const char * line1_text;
static const char * line4_text;

static const resolution_t res = RESOLUTION_320x240;
static const bitdepth_t bit = DEPTH_32_BPP;
static display_context_t disp = 0;
static struct controller_data keys_pressed;
static struct controller_data keys_down;
static int8_t joystick_x_direction = 0;
static rtc_time_t rtc_time;
static bool rtc_writable;
static uint16_t edit_mode = EDIT_NONE;

void set_edit_color( uint16_t mask )
{
    if( edit_mode & mask ) graphics_set_color( BLACK, WHITE );
    else graphics_set_color( WHITE, BLACK );
}

int wrap( int val, int min, int max )
{
    if( val < min ) return max;
    if( val > max ) return min;
    return val;
}

void adjust_rtc_time( rtc_time_t * t, int incr )
{
    uint8_t expected_day = 0;
    switch( edit_mode )
    {
        case EDIT_YEAR:
            /* TODO Figure out what the max supported year is */
            t->year = wrap( (int)t->year + incr, 1996, 2037 );
            break;
        case EDIT_MONTH:
            t->month = wrap( (int)t->month + incr, 0, 11 );
            break;
        case EDIT_DAY:
            t->day = wrap( (int)t->day + incr, 1, 31 );
            expected_day = t->day;
            break;
        case EDIT_HOUR:
            t->hour = wrap( (int)t->hour + incr, 0, 23 );
            break;
        case EDIT_MIN:
            t->min = wrap( (int)t->min + incr, 0, 59 );
            break;
        case EDIT_SEC:
            t->sec = wrap( (int)t->sec + incr, 0, 59 );
            break;
    }
    rtc_normalize_time( t );
    /* Handle wrap-around for normalized day of month */
    if( expected_day && expected_day != t->day && incr > 0 ) t->day = 1;
}

void draw_rtc_time( void )
{
    /* Format RTC date/time as strings */
    sprintf( year, "%04d", rtc_time.year % 10000 );
    sprintf( month, "%02d", (rtc_time.month % 12) + 1 );
    sprintf( day, "%02d", rtc_time.day % 32 );
    dow = DAYS_OF_WEEK[rtc_time.week_day % 7];
    sprintf( hour, "%02d", rtc_time.hour % 24 );
    sprintf( min, "%02d", rtc_time.min % 60 );
    sprintf( sec, "%02d", rtc_time.sec % 60 );

    /* Line 2 */
    graphics_draw_text( disp, 0, LINE2, RTC_DATE_FORMAT );
    set_edit_color( EDIT_YEAR );
    graphics_draw_text( disp, YEAR_X, LINE2, year );
    set_edit_color( EDIT_MONTH );
    graphics_draw_text( disp, MONTH_X, LINE2, month );
    set_edit_color( EDIT_DAY );
    graphics_draw_text( disp, DAY_X, LINE2, day );
    graphics_set_color( WHITE, BLACK );
    graphics_draw_text( disp, DOW_X, LINE2, dow );

    /* Line 3 */
    graphics_draw_text( disp, 0, LINE3, RTC_TIME_FORMAT );
    set_edit_color( EDIT_HOUR );
    graphics_draw_text( disp, HOUR_X, LINE3, hour );
    set_edit_color( EDIT_MIN );
    graphics_draw_text( disp, MIN_X, LINE3, min );
    set_edit_color( EDIT_SEC );
    graphics_draw_text( disp, SEC_X, LINE3, sec );
}

void draw_writing_message( void )
{
    while( !(disp = display_lock()) ) { /* Spinloop */ }

    graphics_fill_screen( disp, BLACK );

    /* Line 1 */
    graphics_set_color( WHITE, BLACK );
    graphics_draw_text( disp, 0, LINE1, WRITING_MESSAGE );

    /* Lines 2 & 3 */
    draw_rtc_time();

    display_show( disp );
}

void run_rtc_write_test( void )
{
    while( !(disp = display_lock()) ) { /* Spinloop */ }

    graphics_fill_screen( disp, BLACK );

    graphics_set_color( WHITE, BLACK );
    graphics_draw_text( disp, 0, LINE1, PROBING_MESSAGE );

    display_show(disp);

    rtc_writable = rtc_is_writable();
    if( !rtc_writable ) line4_text = NOWRITE_MESSAGE;
}

void update_joystick_directions( void )
{
    /* Treat the X direction as a d-pad single button press */
    if( (keys_pressed.c[0].x < -JOYSTICK_DEAD_ZONE) )
    {
        if( joystick_x_direction == 0 ) keys_down.c[0].left = true;
        joystick_x_direction = -1;
    }
    else if ( keys_pressed.c[0].x > +JOYSTICK_DEAD_ZONE )
    {
        if( joystick_x_direction == 0 ) keys_down.c[0].right = true;
        joystick_x_direction = +1;
    }
    else joystick_x_direction = 0;

    /* Treat the Y direction as a d-pad button hold */
    if( keys_pressed.c[0].y > +JOYSTICK_DEAD_ZONE )
    {
        keys_pressed.c[0].up = true;
    }
    else if ( keys_pressed.c[0].y < -JOYSTICK_DEAD_ZONE )
    {
        keys_pressed.c[0].down = true;
    }
}

int main(void)
{
    display_init( res, bit, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE );
    controller_init();
    timer_init();

    if( !rtc_init() )
    {
        while( !(disp = display_lock()) ) { /* Spinloop */ }

        graphics_fill_screen( disp, BLACK );

        graphics_set_color( WHITE, BLACK );

        graphics_draw_text( disp, 0, LINE1, MISSING_MESSAGE );
        graphics_draw_text( disp, 0, LINE2, HELP_1_MESSAGE );
        graphics_draw_text( disp, 0, LINE3, HELP_2_MESSAGE );
        graphics_draw_text( disp, 0, LINE4, NOSTATUS_MESSAGE );

        display_show( disp );

        while(1) { /* Deadloop forever */ }
    }

    /* Determine if the RTC is writable */
    run_rtc_write_test();

    while(1) 
    {
        if( !edit_mode ) rtc_get( &rtc_time );

        while( !(disp = display_lock()) ) { /* Spinloop */ }

        graphics_fill_screen( disp, BLACK );

        /* Line 1 */
        line1_text = edit_mode ? PAUSED_MESSAGE : RUNNING_MESSAGE;
        graphics_set_color( WHITE, BLACK );
        graphics_draw_text( disp, 0, LINE1, line1_text );

        /* Lines 2 & 3 */
        draw_rtc_time();

        /* Line 4 */
        if( rtc_writable )
        {
            line4_text = edit_mode ? CONFIRM_MESSAGE : ADJUST_MESSAGE;
        }
        graphics_set_color( WHITE, BLACK );
        graphics_draw_text( disp, 0, LINE4, line4_text );

        /* Line 5 */
        if( !edit_mode )
        {
            graphics_set_color( WHITE, BLACK );
            graphics_draw_text( disp, 0, LINE5, RETEST_MESSAGE );
        }

        display_show(disp);
       
        controller_scan();
        keys_pressed = get_keys_pressed();
        keys_down = get_keys_down();
        update_joystick_directions();

        /* Toggle edit mode */
        if( rtc_writable && keys_down.c[0].A )
        {
            if( edit_mode )
            {
                edit_mode = EDIT_NONE;
                draw_writing_message();
                rtc_set( &rtc_time );
            }
            else
            {
                edit_mode = EDIT_YEAR;
            }
        }

        if( !edit_mode && keys_down.c[0].B )
        {
            run_rtc_write_test();
        }

        if( edit_mode )
        {
            /* Move between fields */
            if( keys_down.c[0].left )
            {
                edit_mode = edit_mode << 1;
                if( edit_mode > EDIT_YEAR ) edit_mode = EDIT_SEC;
            }
            else if( keys_down.c[0].right )
            {
                edit_mode = edit_mode >> 1;
                if( edit_mode < EDIT_SEC ) edit_mode = EDIT_YEAR;
            }

            /* Adjust date/time */
            if( keys_pressed.c[0].up )
            {
                adjust_rtc_time( &rtc_time, +1 );
                /* Add a delay so you can just hold the direction */
                wait_ms( 100 );
            }
            else if( keys_pressed.c[0].down )
            {
                adjust_rtc_time( &rtc_time, -1 );
                /* Add a delay so you can just hold the direction */
                wait_ms( 100 );
            }
        }
    }
}
