# libfourier
This is the repository for Fourier-related routines used by my code.

There is a lot of "templating" happening so I can compile for SSE/AVX/FMA/whatever using the same code.

## Motivation

It's fun to work on this rather than just use off-the-shelf components.

Sure, I could maybe get better performance using heavily-tested libraries, but it really was more fun to roll my own.

## Contents

* DCT (type II and IV)
* MDCT+MDST and IMDCT
* Centered FFTs/iFFTs of real data
* Specialized routines used by some of my projects
* etc.

## License
All code here is released under the Unlicense terms - go wild with it.
