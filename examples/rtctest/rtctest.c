#include <stdio.h>
#include <sys/time.h>
#include <malloc.h>
#include <libdragon.h>

#define BLACK 0x000000FF
#define WHITE 0xFFFFFFFF

#define JOYSTICK_DEAD_ZONE 32

#define MAX(a,b)  ({ typeof(a) _a = a; typeof(b) _b = b; _a > _b ? _a : _b; })
#define MIN(a,b)  ({ typeof(a) _a = a; typeof(b) _b = b; _a < _b ? _a : _b; })
#define CLAMP(x, min, max) (MIN(MAX((x), (min)), (max)))

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

#define YEAR_MIN 1996
#define YEAR_MAX 2095

/* SCREEN_WIDTH_GUIDE:                "----------------------------------------" */
static const char* MISSING_MESSAGE  = "     Real-time clock not detected.      ";
static const char* HELP_1_MESSAGE   = "     Double-check the settings for      ";
static const char* HELP_2_MESSAGE   = "      your emulator or flash cart.      ";
static const char* PROBING_MESSAGE  = "     Probing the real-time clock...     "; 
static const char* RUNNING_MESSAGE  = "       Reading time from the RTC:       ";
static const char* SOFTWARE_MESSAGE = "      Simulating clock in software:     ";
static const char* PAUSED_MESSAGE   = "      Adjust the current date/time:     ";
static const char* WRITING_MESSAGE  = "          Setting the clock...          ";
static const char* RTC_DATE_FORMAT  = "            YYYY-MM-DD (DoW)            ";
static const char* RTC_TIME_FORMAT  = "                HH:MM:SS                ";
static const char* ADJUST_MESSAGE   = "      Press A to adjust date/time       ";
static const char* CONFIRM_MESSAGE  = "        Press A to write to RTC         ";
static const char* RETEST_MESSAGE   = "      Press B to re-run write test      ";
static const char* NOSTATUS_MESSAGE = "         RTC status test failed!        ";
static const char* NOWRITE_MESSAGE  = "         RTC write test failed!         ";
static const char* CONTINUE_MESSAGE = "           Press A to continue.         ";

static const char* const DAYS_OF_WEEK[7] =
    { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

static char year[sizeof("YYYY")];
static char month[sizeof("MM")];
static char day[sizeof("DD")];
static const char * dow;
static char hour[sizeof("HH")];
static char min[sizeof("MM")];
static char sec[sizeof("SS")];

static const char* line1_text;
static const char* line4_text;

static surface_t* disp = 0;
static joypad_inputs_t pad_inputs = {0};
static joypad_buttons_t pad_pressed = {0};
static int8_t joystick_x_direction = 0;
static struct tm rtc_tm = {0};
static bool rtc_detected = false;
static bool rtc_persistent = false;
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

void adjust_rtc_time( struct tm * t, int incr )
{
    switch( edit_mode )
    {
        case EDIT_YEAR:
            t->tm_year = wrap( t->tm_year + incr, YEAR_MIN - 1900, YEAR_MAX - 1900 );
            break;
        case EDIT_MONTH:
            t->tm_mon = wrap( t->tm_mon + incr, 0, 11 );
            break;
        case EDIT_DAY:
            t->tm_mday = wrap( t->tm_mday + incr, 1, 31 );
            break;
        case EDIT_HOUR:
            t->tm_hour = wrap( t->tm_hour + incr, 0, 23 );
            break;
        case EDIT_MIN:
            t->tm_min = wrap( t->tm_min + incr, 0, 59 );
            break;
        case EDIT_SEC:
            t->tm_sec = wrap( t->tm_sec + incr, 0, 59 );
            break;
    }
    // Recalculate day-of-week and day-of-year
    time_t timestamp = mktime( t );
    *t = *gmtime( &timestamp );
}

void draw_rtc_time( void )
{
    /* Format RTC date/time as strings */
    sprintf( year, "%04d", CLAMP(rtc_tm.tm_year + 1900, YEAR_MIN, YEAR_MAX) );
    sprintf( month, "%02d", CLAMP(rtc_tm.tm_mon + 1, 1, 12) );
    sprintf( day, "%02d", CLAMP(rtc_tm.tm_mday, 1, 31) );
    dow = DAYS_OF_WEEK[CLAMP(rtc_tm.tm_wday, 0, 6)];
    sprintf( hour, "%02d", CLAMP(rtc_tm.tm_hour, 0, 23) );
    sprintf( min, "%02d", CLAMP(rtc_tm.tm_min, 0, 59) );
    sprintf( sec, "%02d", CLAMP(rtc_tm.tm_sec, 0, 59) );

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
    disp = display_get();

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
    disp = display_get();

    graphics_fill_screen( disp, BLACK );

    graphics_set_color( WHITE, BLACK );
    graphics_draw_text( disp, 0, LINE1, PROBING_MESSAGE );

    display_show(disp);

    rtc_persistent = rtc_is_persistent();
    if( !rtc_persistent ) line4_text = NOWRITE_MESSAGE;
}

void update_joystick_directions( void )
{
    /* Treat the X direction as a d-pad single button press */
    if( (pad_inputs.stick_x < -JOYSTICK_DEAD_ZONE) )
    {
        if( joystick_x_direction == 0 ) pad_pressed.d_left = true;
        joystick_x_direction = -1;
    }
    else if ( pad_inputs.stick_x > +JOYSTICK_DEAD_ZONE )
    {
        if( joystick_x_direction == 0 ) pad_pressed.d_right = true;
        joystick_x_direction = +1;
    }
    else joystick_x_direction = 0;

    /* Treat the Y direction as a d-pad button hold */
    if( pad_inputs.stick_y > +JOYSTICK_DEAD_ZONE )
    {
        pad_inputs.btn.d_up = true;
    }
    else if ( pad_inputs.stick_y < -JOYSTICK_DEAD_ZONE )
    {
        pad_inputs.btn.d_down = true;
    }
}

int main(void)
{
    debug_init_isviewer();
    debug_init_usblog();

    display_init( RESOLUTION_320x240, DEPTH_32_BPP, 2, GAMMA_NONE, FILTERS_RESAMPLE );
    joypad_init();
    timer_init();

    rtc_detected = rtc_init();

    if( rtc_detected )
    {
        /* Determine if the RTC is writable */
        run_rtc_write_test();
    }
    else
    {
        disp = display_get();

        graphics_fill_screen( disp, BLACK );

        graphics_set_color( WHITE, BLACK );

        graphics_draw_text( disp, 0, LINE1, MISSING_MESSAGE );
        graphics_draw_text( disp, 0, LINE2, HELP_1_MESSAGE );
        graphics_draw_text( disp, 0, LINE3, HELP_2_MESSAGE );
        graphics_draw_text( disp, 0, LINE4, NOSTATUS_MESSAGE );
        graphics_draw_text( disp, 0, LINE5, CONTINUE_MESSAGE );

        display_show( disp );

        while(1) {
            /* Deadloop until A is pressed */ 
            joypad_poll();
            if( joypad_get_buttons_pressed(JOYPAD_PORT_1).a ) break;
        }
    }

    while(1) 
    {
        if( !edit_mode )
        {
            time_t now = time( NULL );
            rtc_tm = *gmtime( &now );
        }

        disp = display_get();

        graphics_fill_screen( disp, BLACK );

        /* Line 1 */
        if( edit_mode ) line1_text = PAUSED_MESSAGE;
        else if( rtc_detected ) line1_text = RUNNING_MESSAGE;
        else line1_text = SOFTWARE_MESSAGE;

        graphics_set_color( WHITE, BLACK );
        graphics_draw_text( disp, 0, LINE1, line1_text );

        /* Lines 2 & 3 */
        draw_rtc_time();

        /* Line 4 */
        if( !rtc_detected || rtc_persistent )
        {
            line4_text = edit_mode ? CONFIRM_MESSAGE : ADJUST_MESSAGE;
        }
        graphics_set_color( WHITE, BLACK );
        graphics_draw_text( disp, 0, LINE4, line4_text );

        /* Line 5 */
        if( rtc_detected && !edit_mode )
        {
            graphics_set_color( WHITE, BLACK );
            graphics_draw_text( disp, 0, LINE5, RETEST_MESSAGE );
        }

        display_show(disp);
       
        joypad_poll();
        pad_inputs = joypad_get_inputs(JOYPAD_PORT_1);
        pad_pressed = joypad_get_buttons_pressed(JOYPAD_PORT_1);
        update_joystick_directions();

        /* Toggle edit mode */
        if( pad_pressed.a )
        {
            if( edit_mode )
            {
                edit_mode = EDIT_NONE;
                draw_writing_message();

                struct timeval now = { .tv_sec = mktime( &rtc_tm ) };
                settimeofday( &now, NULL );
            }
            else
            {
                edit_mode = EDIT_YEAR;
            }
        }

        if( rtc_detected && !edit_mode && pad_pressed.b )
        {
            run_rtc_write_test();
        }

        if( !edit_mode && pad_pressed.r )
        {
            rtc_resync_time();
        }

        if( edit_mode )
        {
            /* Move between fields */
            if( pad_pressed.d_left )
            {
                edit_mode = edit_mode << 1;
                if( edit_mode > EDIT_YEAR ) edit_mode = EDIT_SEC;
            }
            else if( pad_pressed.d_right )
            {
                edit_mode = edit_mode >> 1;
                if( edit_mode < EDIT_SEC ) edit_mode = EDIT_YEAR;
            }

            /* Adjust date/time */
            if( pad_inputs.btn.d_up )
            {
                adjust_rtc_time( &rtc_tm, +1 );
                /* Add a delay so you can just hold the direction */
                wait_ms( 100 );
            }
            else if( pad_inputs.btn.d_down )
            {
                adjust_rtc_time( &rtc_tm, -1 );
                /* Add a delay so you can just hold the direction */
                wait_ms( 100 );
            }
        }
    }
}
