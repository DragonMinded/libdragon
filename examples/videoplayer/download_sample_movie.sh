#!/bin/bash
set -euo pipefail

# Download and convert the free short movie Caminandes (by Blender Foundation)
if [ ! -f caminandes.ogv ]; then
	wget -O caminandes.ogv https://archive.org/download/CaminandesLlamigos/Caminandes_%20Llamigos-1080p.ogv
fi
ffmpeg -y -i caminandes.ogv -vb 800K -vf 'scale=320x176' -r 20 filesystem/caminandes.m1v
ffmpeg -y -i caminandes.ogv -vn -acodec pcm_s16le -ar 32000 -ac 1 caminandes.wav
"$N64_INST/bin/audioconv64" -o filesystem caminandes.wav 
