# ulc-codec
ulc-codec (Ultra-Low-Complexity Codec) is intended to be a low-bitrate MDCT audio codec, providing ultra-low-complexity decoding.

## Getting started

### Prerequisites
None (so far). Perhaps GCC if building from source.

### Installing
Run ```make all``` to build the file-based encoding and decoding tools (```ulcencode``` and ```ulcdecode```).

You could also ```make encodetool``` or ```make decodetool```.

## Usage
The encoding/decoding tools work with WAV files for simplicity, and to avoid external dependencies.

Additionally, the core encoding/decoding routines can theoretically work with any data they are fed, allowing for easier integration with non-file-based blocks of audio in the future.

### Encoding
```ulcencodetool Input.wav Output.ulc RateKbps[,AvgComplexity]|-Quality [-blocksize:2048]```

This will take ```Input.wav``` and encode it into the output file ```Output.ulc```, at a coding rate of ```RateKbps``` (with ```AvgComplexity``` being passed, this uses ABR mode); alternatively, passing a negative value between -1 and -100 will encode in VBR mode (```-1``` corresponds to Quality=1, ```-100``` corresponds to Quality=100). ```-blocksize:X``` sets the size of each block (ie. the number of coefficients per block). The input file must be 8-bit, 16-bit, 24-bit, or 32-bit float.

Encoding in any mode will display the actual average bitrate, maximum bitrate, and an 'average complexity' parameter. The latter doesn't have much real meaning (perhaps 'how difficult the file is to encode', or 'Quality parameter needed to achieve full transparency'), but can be passed to the encoder in ABR mode to achieve a desired average bitrate.

### Decoding
```ulcdecodetool Input.ulc Output.wav [-format:PCM16]```

This will take ```Input.ulc``` and output ```Output.wav``` in the specified format. Accepted values are PCM8, PCM16, PCM24, and FLOAT32.

## Possible issues
* Syntax is flexible enough to cause buffer overflows.
* No block synchronization (if an encoded file is damaged, there is no way to detect where the next block lies)
* The psychoacoustic model used is somewhat bare-bones, so as to avoid extra complexity and memory usage. As an example, blocks are processed with no memory of prior blocks, which could cause some inefficiency in coding (such as not taking advantage of temporal masking effects). However, it does appear to work very well for what it *does* do.
* Noise fill can leak on transients that are followed by a sharp drop in amplitude.
    * Because noise-fill is not coupled to the L/R signal, noise will leak to both channels when used.
* Encode/decode tools compile with SSE+SSE2/AVX+AVX2/FMA enabled by default. If the encoder crashes/doesn't work, change these flags in the ```Makefile```.
* The codec is VBR in the way it operates; CBR and ABR are faked by adjusting quality until reaching the desired bitrate, reducing encoding speed.
* Transient detection/window selection is an ongoing area of research, especially as it also affects noise fill.

## Technical details
* Target bitrate: 32..256kbps+ (44.1kHz, M/S stereo)
    * CBR, ABR, and VBR (Quality = 1..100) modes available
    * No hard limits on playback rate or coding bitrate
* MDCT-based encoding (using sine window)
    * Window switching is combined with so-called 'overlap scaling', the latter of which varies the size of the overlap segment of transient \[sub]blocks. The idea is to center the transient within a window transition region, at which point overlap scaling takes over to clamp down on its leakage without having to switch to use small windows for the entire block, overall resulting in improved quality compared to the more-common '1 long block or N short blocks' strategy.
* Non-linear coefficient quantization for greater control over dynamic range
* Noise-fill mode for coefficients that aren't directly coded (similar to PNS)
* Extremely simple nybble-based syntax (no entropy-code lookups needed)

## Authors
* **Ruben Nunez** - *Initial work* - [Aikku93](https://github.com/Aikku93)

## License
ulc-codec is released under the Unlicense terms - do whatever you want with it.

## Acknowledgements
* Huge thanks to Dennis K (`DekuTree64`) and `musicalman` for listening to (and for playing programming-rubber-ducky to) all my rants and thought processes as I worked through understanding audio codec design, as well as testing out experimental builds and offering immensely helpful critiques. Without them, this codec would never have gotten as far as it has.

## Gameboy Advance and Nintendo DS player

As a proof of concept of the decoding complexity, a Gameboy Advance demonstration may be found in the [ulcplayer-gba](https://github.com/Aikku93/ulcplayer-gba) repository.

While not as challenging as the GBA version, [there is also a Nintendo DS version](https://github.com/Aikku93/ulcplayer-nds).
