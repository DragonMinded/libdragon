#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "libdragon.h"
#include "regsinternal.h"

#define TV_TYPE_LOC   0x80000300 // uint32 => 0 = PAL, 1 = NTSC, 2 = MPAL

#define AI_NTSC_DACRATE 48681812
#define AI_PAL_DACRATE  49656530
#define AI_MPAL_DACRATE 48628316

#define AI_STATUS_BUSY  ( 1 << 30 )
#define AI_STATUS_FULL  ( 1 << 31 )

#define NUM_BUFFERS     4
#define CALC_BUFFER(x)  ( ( ( ( x ) / 25 ) >> 3 ) << 3 )

static int _frequency = 0;
static int _num_buf = NUM_BUFFERS;
static int _buf_size = 0;
static short **buffers = NULL;

static volatile int now_playing = 0;
static volatile int now_writing = 0;
static volatile int buf_full = 0;


static volatile struct AI_regs_s * const AI_regs = (struct AI_regs_s *)0xa4500000;

static volatile inline int __busy()
{
    return AI_regs->status & AI_STATUS_BUSY;
}

static volatile inline int __full()
{
    return AI_regs->status & AI_STATUS_FULL;
}

/* Called whenever internal buffers are running low */
static void audio_callback()
{
    /* Do not copy more data if we've freed the audio system */
    if(!buffers)
    {
        return;
    }

    /* Copy in as many buffers as can fit (up to 2) */
    while(!__full())
    {
        /* check if next buffer is full */
        int next = (now_playing + 1) % _num_buf;
        if (!(buf_full & (1<<next)))
        {
            break;
        }

        /* clear buffer full flag */
        buf_full &= ~(1<<next);

        /* Set up DMA */
        now_playing = next;

        AI_regs->address = UncachedAddr( buffers[now_playing] );
        AI_regs->length = (_buf_size * 2 * 2 ) & ( ~7 );

         /* Start DMA */
        AI_regs->control = 1;
    }
}

void audio_init(const int frequency, int numbuffers)
{
    int clockrate;

    switch (*(unsigned int*)TV_TYPE_LOC)
    {
        case 0:
            /* PAL */
            clockrate = AI_PAL_DACRATE;
            break;
        case 2:
            /* MPAL */
            clockrate = AI_MPAL_DACRATE;
            break;
        case 1:
        default:
            /* NTSC */
            clockrate = AI_NTSC_DACRATE;
            break;
    }

    /* Remember frequency */
    AI_regs->dacrate = ((2 * clockrate / frequency) + 1) / 2 - 1;
    AI_regs->samplesize = 15;

    /* Real frequency */
    _frequency = 2 * clockrate / ((2 * clockrate / frequency) + 1);

    /* Set up hardware to notify us when it needs more data */
    register_AI_handler(audio_callback);
    set_AI_interrupt(1);

    /* Set up buffers */
    _buf_size = CALC_BUFFER(_frequency);
    _num_buf = (numbuffers > 1) ? numbuffers : NUM_BUFFERS;
    buffers = malloc(_num_buf * sizeof(short *));

    for(int i = 0; i < _num_buf; i++)
    {
        /* Stereo buffers, interleaved */
        buffers[i] = malloc(sizeof(short) * 2 * _buf_size);
        memset(buffers[i], 0, sizeof(short) * 2 * _buf_size);
    }

    /* Set up ring buffer pointers */
    now_playing = 0;
    now_writing = 0;
    buf_full = 0;
}

void audio_close()
{
    if(buffers)
    {
        for(int i = 0; i < _num_buf; i++)
        {
            /* Nuke anything that isn't freed */
            if(buffers[i])
            {
                free(buffers[i]);
                buffers[i] = 0;
            }
        }

        /* Nuke master array */
        free(buffers);
        buffers = 0;
    }

    _frequency = 0;
    _buf_size = 0;
}

void audio_write(const short * const buffer)
{
    if(!buffers)
    {
        return;
    }

    disable_interrupts();

    /* check for empty buffer */
    int next = (now_writing + 1) % _num_buf;
    while (buf_full & (1<<next))
    {
        // buffers full
        audio_callback();
        enable_interrupts();
        disable_interrupts();
    }

    /* Copy buffer into local buffers */
    buf_full |= (1<<next);
    now_writing = next;
    memcpy(UncachedShortAddr(buffers[now_writing]), buffer, _buf_size * 2 * sizeof(short));
    audio_callback();
    enable_interrupts();
}

void audio_write_silence()
{
    if(!buffers)
    {
        return;
    }

    disable_interrupts();

    /* check for empty buffer */
    int next = (now_writing + 1) % _num_buf;
    while (buf_full & (1<<next))
    {
        // buffers full
        audio_callback();
        enable_interrupts();
        disable_interrupts();
    }

    /* Copy silence into local buffers */
    buf_full |= (1<<next);
    now_writing = next;
    memset(UncachedShortAddr(buffers[now_writing]), 0, _buf_size * 2 * sizeof(short));
    audio_callback();
    enable_interrupts();
}

volatile int audio_can_write()
{
    if(!buffers)
    {
        return 0;
    }

    /* check for empty buffer */
    int next = (now_writing + 1) % _num_buf;
    return (buf_full & (1<<next)) ? 0 : 1;
}

int audio_get_frequency()
{
    return _frequency;
}

int audio_get_buffer_length()
{
    return _buf_size;
}
