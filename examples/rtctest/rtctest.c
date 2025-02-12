#include <stdio.h>
#include <sys/time.h>
#include <malloc.h>
#include <libdragon.h>

#include "constants.h"

static surface_t* disp = 0;
static joypad_inputs_t pad_inputs = {0};
static joypad_buttons_t pad_pressed = {0};
static int8_t joystick_x_direction = 0;
static struct tm rtc_tm = {0};
static uint16_t edit_mode = EDIT_NONE;

static void set_edit_color( uint16_t edit_mode_mask );
static void adjust_rtc_time( struct tm * t, int incr );
static void draw_rtc_time( void );
static void update_joystick_directions( void );

int main(void)
{
    debug_init_isviewer();
    debug_init_usblog();

    display_init( RESOLUTION_320x240, DEPTH_32_BPP, 2, GAMMA_NONE, FILTERS_RESAMPLE );
    joypad_init();
    timer_init();

    rtc_init();

    while(1)
    {
        if( !edit_mode )
        {
            /* Read the current timestamp from RTC subsystem */
            time_t now = time( NULL );
            /* Convert the timestamp into date/time struct */
            rtc_tm = *gmtime( &now );
        }

        disp = display_get();

        graphics_fill_screen( disp, BLACK );

        /* Line 1 */
        const char* line1_text;
        if( edit_mode )
        {
            line1_text = PAUSED_MESSAGE;
        }
        else
        {
            switch( rtc_get_source() ) {
                case RTC_SOURCE_JOYBUS:
                    line1_text = RUN_JOY_MESSAGE;
                    break;
                case RTC_SOURCE_DD:
                    line1_text = RUN_DD_MESSAGE;
                    break;
                case RTC_SOURCE_BB:
                    line1_text = RUN_BB_MESSAGE;
                    break;
                case RTC_SOURCE_NONE:
                default:
                    line1_text = RUN_SOFT_MESSAGE;
                    break;
            }
        }
        graphics_set_color( WHITE, BLACK );
        graphics_draw_text( disp, 0, LINE1, line1_text );

        /* Lines 2 & 3 */
        draw_rtc_time();

        /* Line 4 */
        const char* line4_text = edit_mode ? CONFIRM_MESSAGE : ADJUST_MESSAGE;
        graphics_set_color( WHITE, BLACK );
        graphics_draw_text( disp, 0, LINE4, line4_text );

        /* Line 5 */
        graphics_set_color( WHITE, BLACK );
        if( !rtc_is_persistent() )
        {
            graphics_draw_text( disp, 0, LINE5, NOWRITE_MESSAGE );
        }
        else if( rtc_get_source() == RTC_SOURCE_JOYBUS && rtc_is_source_available( RTC_SOURCE_DD ) )
        {
            graphics_draw_text( disp, 0, LINE5, SRC_DD_MESSAGE );
        }
        else if( rtc_get_source() == RTC_SOURCE_DD && rtc_is_source_available( RTC_SOURCE_JOYBUS ) )
        {
            graphics_draw_text( disp, 0, LINE5, SRC_JOY_MESSAGE );
        }

        if( sys_bbplayer() )
        {
            // TODO: Remove BBPlayer RTC internal API usage for debugging
            int bb_rtc_get_state( void *state, uint64_t *raw );
            int bb_rtc_get_time( time_t *out );

            uint64_t bb_state = 0;
            time_t bb_time = 0;
            int bb_result;
            char line_buf[41];

            bb_result = bb_rtc_get_state( NULL, &bb_state );
            sprintf( line_buf, " %016llx %21s", bb_state, rtc_error_str( bb_result ) );
            graphics_draw_text( disp, 0, LINE6, line_buf );

            bb_result = bb_rtc_get_time( &bb_time );
            sprintf( line_buf, " %016lld %21s", bb_time, rtc_error_str( bb_result ) );
            graphics_draw_text( disp, 0, LINE7, line_buf );
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
                struct timeval now = { .tv_sec = mktime( &rtc_tm ) };
                settimeofday( &now, NULL );
            }
            else
            {
                edit_mode = EDIT_YEAR;
            }
        }

        if( !edit_mode && pad_pressed.r )
        {
            /* Resync the time from the RTC source */
            rtc_set_source( rtc_get_source() );
        }

        if( !edit_mode && pad_pressed.l )
        {
            if( rtc_get_source() == RTC_SOURCE_JOYBUS )
            {
                rtc_set_source( RTC_SOURCE_DD );
            }
            else if( rtc_get_source() == RTC_SOURCE_DD )
            {
                rtc_set_source( RTC_SOURCE_JOYBUS );
            }
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

static void set_edit_color( uint16_t edit_mode_mask )
{
    if( edit_mode & edit_mode_mask ) graphics_set_color( BLACK, WHITE );
    else graphics_set_color( WHITE, BLACK );
}

static void adjust_rtc_time( struct tm * t, int incr )
{
    rtc_range_t rtc_range = rtc_get_supported_range();
    int year_min = TIMESTAMP_TO_YEAR(rtc_range.min);
    int year_max = TIMESTAMP_TO_YEAR(rtc_range.max + 1) - 1;

    switch( edit_mode )
    {
        case EDIT_YEAR:
            t->tm_year = WRAP( t->tm_year + incr, year_min - 1900, year_max - 1900 );
            break;
        case EDIT_MONTH:
            t->tm_mon = WRAP( t->tm_mon + incr, 0, 11 );
            break;
        case EDIT_DAY:
            t->tm_mday = WRAP( t->tm_mday + incr, 1, 31 );
            break;
        case EDIT_HOUR:
            t->tm_hour = WRAP( t->tm_hour + incr, 0, 23 );
            break;
        case EDIT_MIN:
            t->tm_min = WRAP( t->tm_min + incr, 0, 59 );
            break;
        case EDIT_SEC:
            t->tm_sec = WRAP( t->tm_sec + incr, 0, 59 );
            break;
    }
    // Recalculate day-of-week and day-of-year
    time_t timestamp = mktime( t );
    *t = *gmtime( &timestamp );
}

static void draw_rtc_time( void )
{
    rtc_range_t rtc_range = rtc_get_supported_range();
    int year_min = TIMESTAMP_TO_YEAR(rtc_range.min);
    int year_max = TIMESTAMP_TO_YEAR(rtc_range.max + 1) - 1;

    char year[sizeof("YYYY")];
    char month[sizeof("MM")];
    char day[sizeof("DD")];
    const char * dow;
    char hour[sizeof("HH")];
    char min[sizeof("MM")];
    char sec[sizeof("SS")];

    /* Format RTC date/time as strings */
    sprintf( year, "%04d", CLAMP(rtc_tm.tm_year + 1900, year_min, year_max) );
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

static void update_joystick_directions( void )
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
