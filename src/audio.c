/**
 * @file audio.c
 * @brief Audio Subsystem
 * @ingroup audio
 */
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "libdragon.h"
#include "regsinternal.h"

/**
 * @defgroup audio Audio Subsystem
 * @ingroup libdragon
 * @brief Interface to the N64 audio hardware.
 *
 * The audio subsystem handles queueing up chunks of audio data for
 * playback using the N64 audio DAC.  The audio subsystem handles
 * DMAing chunks of data to the audio DAC as well as audio callbacks
 * when there is room for another chunk to be written.  Buffer size
 * is calculated automatically based on the requested audio frequency.
 * The audio subsystem accomplishes this by interfacing with the audio
 * interface (AI) registers.
 *
 * Because the audio DAC is timed off of the master clock of the N64,
 * the audio subsystem needs to know what region the N64 is from.  This
 * is due to the fact that the master clock is timed differently for
 * PAL, NTSC and MPAL regions.  This is handled automatically by the
 * audio subsystem based on settings left by the bootloader.
 *
 * Code attempting to output audio on the N64 should initialize the
 * audio subsystem at the desired frequency and with the desired number
 * of buffers using #audio_init.  More audio buffers allows for smaller
 * chances of audio glitches but means that there will be more latency
 * in sound output.  When new data is available to be output, code should
 * check to see if there is room in the output buffers using 
 * #audio_can_write.  Code can probe the current frequency and buffer
 * size using #audio_get_frequency and #audio_get_buffer_length respectively.
 * When there is additional room, code can add new data to the output
 * buffers using #audio_write.  Be careful as this is a blocking operation,
 * so if code doesn't check for adequate room first, this function will
 * not return until there is room and the samples have been written.
 * When all audio has been written, code should call #audio_close to shut
 * down the audio subsystem cleanly.
 * @{
 */

/**
 * @brief Memory location to read which determines the TV type
 *
 * Values read include 0 for PAL, 1 for NTSC and 2 for MPAL
 */
#define TV_TYPE_LOC   0x80000300 // uint32 => 0 = PAL, 1 = NTSC, 2 = MPAL

/**
 * @name DAC rates for different regions
 * @{
 */
/** @brief NTSC DAC rate */
#define AI_NTSC_DACRATE 48681812
/** @brief PAL DAC rate */
#define AI_PAL_DACRATE  49656530
/** @brief MPAL DAC rate */
#define AI_MPAL_DACRATE 48628316
/** @} */

/**
 * @name AI Status Register Values
 * @{
 */
/** @brief Bit representing that the AI is busy */
#define AI_STATUS_BUSY  ( 1 << 30 )
/** @brief Bit representing that the AI is full */
#define AI_STATUS_FULL  ( 1 << 31 )
/** @} */

/** @brief Number of buffers the audio subsytem allocates and manages */
#define NUM_BUFFERS     4
/** 
 * @brief Macro that calculates the size of a buffer based on frequency
 *
 * @param[in] x 
 *            Frequency the AI is running at
 *
 * @return The size of the buffer in bytes rounded to an 8 byte boundary
 */
#define CALC_BUFFER(x)  ( ( ( ( x ) / 25 ) >> 3 ) << 3 )

/** @brief The actual frequency the AI will run at */
static int _frequency = 0;
/** @brief The number of buffers currently allocated */
static int _num_buf = NUM_BUFFERS;
/** @brief The buffer size in bytes for each buffer allocated */
static int _buf_size = 0;
/** @brief Array of pointers to the allocated buffers */
static short **buffers = NULL;

/** @brief Index of the current playing buffer */
static volatile int now_playing = 0;
/** @brief Index pf the currently being written buffer */
static volatile int now_writing = 0;
/** @brief Bitmask of buffers indicating which buffers are full */
static volatile int buf_full = 0;

/** @brief Structure used to interact with the AI registers */
static volatile struct AI_regs_s * const AI_regs = (struct AI_regs_s *)0xa4500000;

/**
 * @brief Return whether the AI is currently busy
 *
 * @return nonzero if the AI is busy, zero otherwise
 */
static volatile inline int __busy()
{
    return AI_regs->status & AI_STATUS_BUSY;
}

/**
 * @brief Return whether the AI is currently full
 *
 * @return nonzero if the AI is full, zero otherwise
 */
static volatile inline int __full()
{
    return AI_regs->status & AI_STATUS_FULL;
}

/**
 * @brief Send next available chunks of audio data to the AI
 *
 * This function is called whenever internal buffers are running low.  It will
 * send as many buffers as possible to the AI until the AI is full.
 */
static void audio_callback()
{
    /* Do not copy more data if we've freed the audio system */
    if(!buffers)
    {
        return;
    }

    /* Disable interrupts so we don't get a race condition with writes */
    disable_interrupts();

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

    /* Safe to enable interrupts here */
    enable_interrupts();
}

/**
 * @brief Initialize the audio subsystem
 *
 * This function will set up the AI to play at a given frequency and 
 * allocate a number of back buffers to write data to.
 *
 * @note Before re-initializing the audio subsystem to a new playback
 *       frequency, remember to call #audio_close.
 *
 * @param[in] frequency
 *            The frequency in Hz to play back samples at
 * @param[in] numbuffers
 *            The number of buffers to allocate internally
 */
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

    /* Make sure we don't choose too many buffers */
    if( numbuffers > (sizeof(buf_full) * 8) )
    {
        /* This is a bit mask, so we can only have as many
         * buffers as we have bits. */
        numbuffers = sizeof(buf_full) * 8;
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

/**
 * @brief Close the audio subsystem
 *
 * This function closes the audio system and cleans up any internal
 * memory allocated by #audio_init.
 */
void audio_close()
{
    set_AI_interrupt(0);
    unregister_AI_handler(audio_callback);

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

/**
 * @brief Write a chunk of audio data
 *
 * This function takes a chunk of audio data and writes it to an internal
 * buffer which will be played back by the audio system as soon as room
 * becomes available in the AI.  The buffer should contain stereo interleaved
 * samples and be exactly #audio_get_buffer_length stereo samples long.
 *
 * @note This function will block until there is room to write an audio sample.
 *       If you do not want to block, check to see if there is room by calling
 *       #audio_can_write.
 *
 * @param[in] buffer
 *            Buffer containing stereo samples to be played
 */
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

/**
 * @brief Write a chunk of silence
 *
 * This function will write silence to be played back by the audio system.
 * It writes exactly #audio_get_buffer_length stereo samples.
 *
 * @note This function will block until there is room to write an audio sample.
 *       If you do not want to block, check to see if there is room by calling
 *       #audio_can_write.
 */
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

/**
 * @brief Return whether there is an empty buffer to write to
 *
 * This function will check to see if there are any buffers that are not full to
 * write data to.  If all buffers are full, wait until the AI has played back
 * the next buffer in its queue and try writing again.
 */
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

/**
 * @brief Return actual frequency of audio playback
 *
 * @return Frequency in Hz of the audio playback
 */
int audio_get_frequency()
{
    return _frequency;
}

/**
 * @brief Get the number of stereo samples that fit into an allocated buffer
 *
 * @note To get the number of bytes to allocate, multiply the return by 
 *       2 * sizeof( short )
 *
 * @return The number of stereo samples in an allocated buffer
 */
int audio_get_buffer_length()
{
    return _buf_size;
}

/** @} */ /* audio */
