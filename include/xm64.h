#ifndef XM64_H
#define XM64_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xm_context_s xm_context_t;
typedef struct waveform_s waveform_t;

/**
 * xm64player_t is a player of the .XM64 file format, which is based on the
 * Fast Tracker II .XM module format.
 * 
 * The playback relies on the mixer (mixer.h), as it uses it to perform the
 * actual channel playing and mixing. It will use one mixer channel per each XM
 * channel. You need to initialize the mixer (via mixer_init) before using
 * xm64player_t.
 * 
 * The actual XM player is based on libxm (https://github.com/Artefact2/libxm),
 * a very fast and accurate XM player ibrary that has been adapted for usage
 * in libdragon on N64. The main changes are:
 * 
 *   * Usage of the custom XM64 format. This formats is a serialization of the
 *     internal libxm context (xm_context_t), and allows us to do some required
 *     preprocessing (such as unrolling ping-pong loops which are not supported
 *     by the mixer), and to load the whole file with one single memory allocation
 *     to avoid heap fragmentation.
 *   * Loading of XM patterns is done on the fly rather than all in advance,
 *     to save RAM. Patterns are also recompressed using a RLE derivative which
 *     is more efficient and still fast to decompress.
 *   * Waveforms ("samples" in XM) are not preloaded, but they are streamed
 *     off disk when necessary, thanks to the mixer API that allows to do it
 *     very easily. This saves a lot of RAM.
 *   * The actual sample generation code of libxm is not used, as the playback
 *     code is based on the mixer. 
 *   * XM64 contains also the precalculated amount of sample buffer memory
 *     required for playing back, per each channel. This allows for precise
 *     memory allocations even within the mixer.
 */
typedef struct xm64player_s {
	xm_context_t *ctx;        // libxm context
	waveform_t *waves;        // array of all waveforms (one per XM "sample")
	int nwaves;               // number of wavers (XM "samples")
	FILE *fh;                 // open handle of XM64 file
	int first_ch;             // first channel used in the mixer
	bool playing;             // playing flag
	struct {
		int patidx, row, tick;
	} seek;                   // seeking to be performed
} xm64player_t;

/**
 * @brief Open a XM64 module file and prepare for playback.
 * 
 * This function requires the mixer to have been already initialized
 * (via mixer_init).
 * 
 * @param player Pointer to the xm64player_t player structure to use
 * @param fn     Filename of the XM64 (with filesystem prefix). Currently,
 *               only files on DFS ("rom:/") are supported.
 */
void xm64player_open(xm64player_t *player, const char *fn);

/**
 * @brief Get the number of channels in the XM64 file
 * 
 * Notice that the player needs to use one mixer channel per each XM64 channel.
 */
int xm64player_num_channels(xm64player_t *player);

/**
 * @brief Start playing the XM64 module.
 * 
 * Notice that the player needs to use one mixer channel per each XM64 channel.
 *
 * @param player 	XM64 player
 * @param fist_ch 	Index of the first mixer channel to use for playback.
 */
void xm64player_play(xm64player_t *player, int first_ch);

/**
 * @brief Stop XM playback.
 * 
 * The XM module will keep the current position. Use xmplayer_play to continue
 * playback.
 */
void xm64player_stop(xm64player_t *player);

/**
 * @brief Read the current position of the XM module.
 * 
 * The function returns the current position expressed as
 * pattern/row (internal XM position), and also expressed
 * as number of seconds. You can pass NULL to information
 * that you are not interested in receiving.
 * 
 * @param player        XM64 player
 * @param[out] patidx   Index of the XM pattern
 * @param[out] row      Row within the pattern
 * @param[out] secs     Total number of seconds
 */
void xm64player_tell(xm64player_t *player, int *patidx, int *row, float *secs);

/**
 * @brief Seek to a specific position of the XM module.
 * 
 * Seeking in XM module is "broken by design". What this function does
 * is to move the playback cursor to the specified position, but
 * it doesn't take into effect what samples / effects should be active
 * at the seeking point.
 * 
 * @param player 		XM64 player
 * @param patidx 		Index of the XM pattern to seek to
 * @param row 			Row within the pattern to seek to
 * @param tick 			Tick within the row to seek to
 */
void xm64player_seek(xm64player_t *player, int patidx, int row, int tick);

/**
 * @brief Change the volume of the player.
 * 
 * This allows to tune the volume of playback. The default volume is 1.0; smaller
 * values will lower the volume, higher values will amplificate (but may clip).
 */
void xm64player_set_vol(xm64player_t *player, float volume);

/**
 * @brief Close and deallocate the XM64 player.
 */
void xm64player_close(xm64player_t *player);

#ifdef __cplusplus
}
#endif

#endif
