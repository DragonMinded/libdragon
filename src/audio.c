#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "libdragon.h"
#include "regsinternal.h"

#define AI_NTSC_DACRATE 48681812
#define AI_STATUS_BUSY  ( 1 << 30 )
#define AI_STATUS_FULL  ( 1 << 31 )

#define NUM_BUFFERS     4
#define CALC_BUFFER(x)  ( ( ( ( x ) / 25 ) >> 3 ) << 3 )

static int _frequency = 0;
static int _buf_size = 0;
static short **buffers = NULL;

static volatile int now_playing = 0;
static volatile int now_writing = 0;

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
    if(!buffers) { return; }

    /* Copy in as many buffers as can fit (up to 2) */
    while(!__full()) 
    {
        /* Set up DMA */
        now_playing = (now_playing + 1) % NUM_BUFFERS;

        AI_regs->address = UncachedAddr( buffers[now_playing] );
        AI_regs->length = (_buf_size * 2 * 2 ) & ( ~7 );

         /* Start DMA */
        AI_regs->control = 1;
    }
}

void audio_init(const int frequency)
{
    /* Remember frequency */
    AI_regs->dacrate = AI_NTSC_DACRATE / frequency;
    AI_regs->samplesize = 15;

    /* Real frequency */
    _frequency = AI_NTSC_DACRATE / (AI_NTSC_DACRATE / frequency);

    /* Set up hardware to notify us when it needs more data */
    register_AI_handler(audio_callback);
    set_AI_interrupt(1);

    /* Set up buffers */
    _buf_size = CALC_BUFFER(_frequency);
    buffers = malloc(sizeof(short *) * NUM_BUFFERS);

    for(int i = 0; i < NUM_BUFFERS; i++)
    {
        /* Stereo buffers, interleaved */
        buffers[i] = malloc(sizeof(short) * 2 * _buf_size);
        memset(buffers[i], 0, sizeof(short) * 2 * _buf_size);
    }

    /* Set up ring buffer pointers */
    now_playing = 0;
    now_writing = 0;
}

void audio_close()
{
    if(buffers)
    {
        for(int i = 0; i < NUM_BUFFERS; i++)
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
    if(!buffers) { return; }

    /* Wait until there is a buffer to write to */
    while(now_playing == now_writing) 
    {
        if(!__busy()) audio_callback();
    }

    /* Copy buffer into local buffers */
    memcpy(UncachedShortAddr(buffers[now_writing]), buffer, _buf_size * 2 * sizeof(short));
    now_writing = (now_writing + 1) % NUM_BUFFERS;
}

void audio_write_silence()
{
    if(!buffers) { return; }

    /* Wait until there is a buffer to write to */
    while(now_playing == now_writing) 
    {
        if(!__busy()) audio_callback();
    }

    /* Copy silence into local buffers */
    memset(UncachedShortAddr(buffers[now_writing]), 0, _buf_size * 2 * sizeof(short));
    now_writing = (now_writing + 1) % NUM_BUFFERS;
}

volatile int audio_can_write()
{
    return !(now_playing == now_writing);
}

int audio_get_frequency()
{
    return _frequency;
}

int audio_get_buffer_length()
{
    return _buf_size;
}

