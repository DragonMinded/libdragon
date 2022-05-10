#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <libdragon.h>

static volatile double t1 = 0.0; // increment every ms
static volatile double t2 = 0.0; // increment every .5 sec
static volatile double t3 = 0.0; // increment every 1 sec

static volatile int running = 1; // clear by one-shot after 30 sec

void one_msec(int ovfl)
{
    t1 += 0.001;
}

void half_sec(int ovfl)
{
    t2 += 0.5;
}

void one_sec(int ovfl)
{
    t3 += 1.0;
}

void one_shot(int ovfl)
{
    running = 0;
}

int main(void)
{
    timer_link_t *one_shot_t;
    long long start, end;

    /* Initialize peripherals */
    console_init();

    console_set_render_mode(RENDER_MANUAL);

    timer_init();
    start = timer_ticks();
    new_timer(TIMER_TICKS(1000), TF_CONTINUOUS, one_msec);
    new_timer(TIMER_TICKS(500000), TF_CONTINUOUS, half_sec);
    new_timer(TIMER_TICKS(1000000), TF_CONTINUOUS, one_sec);
    one_shot_t = new_timer(TIMER_TICKS(30000000), TF_ONE_SHOT, one_shot); // the only one we have to keep track of

    /* Main loop test */
    while(running)
    {
        console_clear();

        printf("Running on: %s\n", sys_bbplayer() ? "iQue" : "N64");
        printf( "\n Every msec    : %f", t1 );
        printf( "\n Every half sec: %f", t2 );
        printf( "\n Every sec     : %f", t3 );

        console_render();
    }
    end = timer_ticks();
    // one-shot timers have to be explicitly freed
    delete_timer(one_shot_t);
    timer_close();

    console_clear();

    printf("Running on: %s\n", sys_bbplayer() ? "iQue" : "N64");
    printf( "\n Every msec    : %f", t1 );
    printf( "\n Every half sec: %f", t2 );
    printf( "\n Every sec     : %f", t3 );
    printf( "\n\n Done in %f", (float)TIMER_MICROS_LL(end-start)/1000000.0f);

    console_render();

    while (1) ;
}
