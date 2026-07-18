### Overview
***

Each block is coded sequentially, with each channel also coded sequentially, resulting in the following structure:

    Block {
     Header
     Chan0 {
      SubBlock0
      SubBlock1
      SubBlock2
      ...
     }
     Chan1 {
      SubBlock0
      SubBlock1
      SubBlock2
      ...
     }
    }

The encoding tools align each block (that is, including all channels) to byte boundaries, but this is not strictly necessary; any alignment will do.

These blocks code MDCT coefficients that have been pre-normalized. So to decode, one must simply apply an unnormalized IMDCT to return the decoded PCM stream.

Each block always starts with a window control code before any coefficients are coded (this is considered the header). This controls the window lengths and shapes, and is explained in the `Block header` section.

NB: The encoder is expected to handle all scaling, such that the inverse transform needs no scaling whatsoever (not even MDCT normalization). The result is that all coefficients are in the range ```|x| <= 4/Pi``` (where 4/Pi is the p=1 limit of the MDCT matrix as N approaches infinity).

### Block header
***

Each block starts with a nybble that encodes overlap scaling, with the high bit being a decimation (window switch) toggle.

| Nybble sequence    | Window 0 | Window 1 | Window 2 | Window 3 |
| ------------------ | -------- | -------- | -------- | -------- |
| ```0h..7h```       | N*       | -        | -        | -        |
| ```8h..Fh,0010b``` | N/2*     | N/2      | -        | -        |
| ```8h..Fh,0011b``` | N/2      | N/2*     | -        | -        |
| ```8h..Fh,0100b``` | N/4*     | N/4      | N/2      | -        |
| ```8h..Fh,0101b``` | N/4      | N/4*     | N/2      | -        |
| ```8h..Fh,0110b``` | N/2      | N/4*     | N/4      | -        |
| ```8h..Fh,0111b``` | N/2      | N/4      | N/4*     | -        |
| ```8h..Fh,1000b``` | N/8*     | N/8      | N/4      | N/2      |
| ```8h..Fh,1001b``` | N/8      | N/8*     | N/4      | N/2      |
| ```8h..Fh,1010b``` | N/4      | N/8*     | N/8      | N/2      |
| ```8h..Fh,1011b``` | N/4      | N/8      | N/8*     | N/2      |
| ```8h..Fh,1100b``` | N/2      | N/8*     | N/8      | N/4      |
| ```8h..Fh,1101b``` | N/2      | N/8      | N/8*     | N/4      |
| ```8h..Fh,1110b``` | N/2      | N/4      | N/8*     | N/8      |
| ```8h..Fh,1111b``` | N/2      | N/4      | N/8      | N/8*     |

Overlap scaling (bit0..2 of the first nybble) is applied to the \[sub]block denoted with an asterisk, and results in an overlap of ```50% * 2^-Scale``` (that is, ```SubBlockSize * 2^-Scale``` samples of overlap) for that \[sub]block blending with the last.

Note that the transient subblock index can be easily obtained using a population count (POPCNT) on bit1..3 of the second nybble (minus 1 for 0-based indexing).

### Block syntax
***

| Nybble sequence         | Explanation         | Effect                                          |
| ----------------------- | ------------------- | ----------------------------------------------- |
| ```-7h..-2h,+2h..+7h``` | Normal coefficient  | Insert ```Coef[n++] = Nybble*Abs[Nybble] * Quantizer``` |
| ```0h,0h..Fh```         | Zeros run (short)   | Insert zeros                                    |
| ```1h,Yh,Xh```          | Zeros run (long)    | Insert zeros                                    |
| ```8h,Zh,Yh,Xh```       | Noise-fill          | Insert noise                                    |
| ```Fh,0h..Dh```         | Quantizer change    | Set ```Quantizer = 2^-(5+X)```                  |
| ```Fh,Eh,0h..Ch```      | Quantizer change    | Set ```Quantizer = 2^-(5+14+X)```               |
| ```Fh,Eh,Dh```          | *Unallocated*       | N/A                                             |
| ```Fh,Eh,Eh```          | *Unallocated*       | N/A                                             |
| ```Fh,Eh,Fh```          | Stop                | Stop reading coefficients; fill rest with zeros |
| ```Fh,Fh,Zh,Yh,Xh```    | Stop (noise)        | Stop reading coefficients; fill rest with noise |

#### ```-7h..-2h, +2h..+7h```: Normal coefficient

This is a quantized coefficient. To unpack, take the signed squared value (ie. ```x*ABS(x)```), scale by the current quantizer, and move to the next coefficient.

If we've received as many coefficients as there are in a transform block (eg. 2048 coefficients), this channel's coefficients are finished, and we can move on to inverse transforming.

Note that this range excludes the coefficients +/-1. This is by design, as in testing, it was found that this coefficient level was generally used less than 0.05% of the time, no matter the bitrate.

#### ```0h,0h..Fh```: Zeros run (short)

This inserts coefficients with a value of 0. As these are fairly common after quantization (especially without noise fill), we use a compact format to code runs of them.

To unpack: ```n = 1 + [second nybble]```. This allows 1..16 zeros to be coded with two nybbles.

#### ```1h,Yh,Xh```: Zeros run (long)

Further refinement on short zeros runs. This in particular is more common at lower bitrates, where quantization becomes too aggressive and/or we must cull large swathes of frequency bands in order to meet the target bitrate.

Because a short run can already code up to 32 zeros at once in the 4 nybbles this command uses, a long run codes 33..288 zeros at once.

To unpack: ```n = (Y<<4 | X) + 33```

#### ```8h,Zh,Yh,Xh```: Noise-fill

Because removing a larger number of coefficients can result in holes in the spectrum, this command allows filling the holes with noise data.

To unpack: ```n = (Z<<5 | Y<<1 | (X&1)) + 16``` and ```Level = ((X>>1) + 1)^2 * Quantizer/4```. Then store ```n``` coefficients generated by randomly cycling +/-```Level```.

#### ```Fh,0h..Dh```, ```Fh,Eh,0h..Ch```: Quantizer change

This modifies the current quantizer.

When the third nybble is between ```0h..Dh```, this signals a normal quantizer change; at average audio levels, these are the most common quantization scalers.

```Quantizer = 2^-(5 + [third nybble])```

(Note the bias of 5)

When the nybble is ```Eh```, however, this signals that the quantizer needs to be smaller still, and so is followed by an extra nybble to afford a so-called 'extended-precision quantizer', allowing a maximum quantization scale of 2^-31.

```Quantizer = 2^-(5 + 14 + [fourth nybble])```

This can be considered to result in a scientific notation containing a sign bit, 3 bits of significand, and 5 bits of exponent, with the minimum possible coefficient value being ±1.0×2^-31.

Each channel begins with a nybble specifying the initial quantizer, akin to a silent ```Fh``` at the start (extended-precision quantizers are also allowed for this first quantizer band).

#### ```Fh,Eh,Fh```, ```Fh,Fh,Zh,Yh,Xh```: Stop

```Fh,Eh,Fh``` signals that the remaining coefficients for this channel are all zeros. This completes this channel's data.

```Fh,Fh,Zh,Yh,Xh``` signals that the remaining coefficients should be filled with exponentially-decaying noise.

A channel's block may begin with ```[Fh,]Eh,Fh```; this means that the channel is silent.

A channel's block cannot begin with ```[Fh],Fh,Zh,Yh,Xh```; no quantizer has been set at this point, rendering the expression meaningless.

##### Tail-end noise-fill

Unpack Amplitude and Decay as follows:

    Amplitude = (Z+1)^2 * Quantizer/16
    Decay     = 1 - 2^-19*(Y<<4 | X)^2

For each of the remaining coefficients in the block, noise is used in place of encoded coefficients as follows:

    Coef[n++] = Amplitude * [randomly generated +/-1]
    Amplitude = Amplitude * Decay

The quantizer is scaled by 1/16, as this was found to give consistently good results with very minimal overload/saturation and underload/collapse.

### Inverse transform process
***

Inverse transform takes the above-decoded coefficients and applies the IMDCT to give the output stream.

#### IMDCT

The IMDCT used in this codec follows the normal IMDCT formula found on any maths book, but without any scaling factor in front (as it is moved to the MDCT in the encoder) and with a shifted basis that results in phase inversion (as this results in programming optimizations), ie.

    y[n] =  Sum[X[k]*Cos[(n+1/2 + N/2 + N*2)(k+1/2)*Pi/N], {k,0,N-1}]
         = -Sum[X[k]*Cos[(n+1/2 + N/2      )(k+1/2)*Pi/N], {k,0,N-1}]

The windowing function used is a sine window, with the overlap amount based on the block size scaled by the nybble at the start of the block (see the `Overview` section). To use a different window, the MDCT and IMDCT functions of the source code must be modified to accomodate such (this is not too difficult, and only involves loading the sine/cosine values with appropriate data). Note that using a sine window allows reuse of the DCT coefficients table, whereas a different window cannot reuse these coefficients and so needs double the storage space.

When using window switching, it is important to note that a subblock's overlap may be larger than allowed by the last subblock. When this happens, the number of overlap samples must be clipped to the size of the previous, smaller subblock. This unfortunately results in an additional block delay for decoding (on top of the MDCT delay), as the encoder must have knowledge about the next \[sub]block to account for this.
