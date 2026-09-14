#!/usr/bin/env bash
# Builds dist/moku-trailer.mp4 from the game itself.
#
#   tools/make_trailer.sh                 # record a fresh take, then encode
#   SKIP_RECORD=1 tools/make_trailer.sh   # re-encode the frames already in tools/trailer/frames
#
# The take is one continuous run of the ROM under the mGBA harness (tools/trailer/trailer.txt), so
# the music never jumps a cut and every pixel on screen is a pixel the game drew. The captions live
# in a bar below the picture and are timed from the MARK lines the harness prints at each `echo`.
set -eu

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

frames="tools/trailer/frames"
out="dist/moku-trailer.mp4"

[ -f dist/moku.gba ] || { echo "no ROM: run make -j2 && make dist" >&2; exit 2; }
[ -x tests/rom/harness/mokurun ] || { echo "no harness: run make -C tests/rom/harness" >&2; exit 2; }

if [ "${SKIP_RECORD:-0}" != "1" ]; then
    rm -rf "$frames"
    tests/rom/harness/mokurun dist/moku.gba tools/trailer/trailer.txt | grep -E "^(MARK|RECORDED)" || true
fi

count=$(ls "$frames"/*.png 2>/dev/null | wc -l)
[ "$count" -gt 100 ] || { echo "no recorded frames in $frames" >&2; exit 2; }

read -r duration fade_out filter < <(python3 tools/trailer/captions.py "$count")
echo "encoding $count frames ($duration s)"

mkdir -p dist
ffmpeg -y -loglevel error -stats \
    -framerate 59.7275 -i "$frames/%06d.png" \
    -f s16le -ar 32768 -ac 2 -i "$frames/audio.raw" \
    -filter_complex "[0:v]${filter}[v]" \
    -map "[v]" -map 1:a \
    -af "afade=t=in:st=0:d=0.5,afade=t=out:st=${fade_out}:d=1.0,volume=2.2,alimiter=limit=0.95" \
    -c:v libx264 -preset slow -crf 20 -pix_fmt yuv420p -movflags +faststart \
    -c:a aac -b:a 192k -ar 48000 \
    -shortest "$out"

ls -l "$out"
ffprobe -v error -show_entries format=duration -show_entries stream=codec_name,width,height -of default=nw=1 "$out"
