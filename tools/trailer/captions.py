"""Builds the ffmpeg filter chain for the trailer: upscale, caption bar, captions, fades.

The frame numbers come from the MARK lines tools/trailer/trailer.txt makes the harness print, so a
caption always lands on the beat it describes. Prints "DURATION FADE_OUT FILTER" for the shell.
"""
import sys

FPS = 59.7275
SCALE = 5                 # 240x160 -> 1200x800, nearest neighbour so no pixel is ever blurred
BAR = 128                 # the caption bar under the picture
FONT = "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf"

# (first frame, last frame, line, text). Line 0 alone is centred in the bar; a 0 and a 1 stack.
CAPTIONS = [
    (  40,  250, 0, "MOKU - Tactics of Go"),
    (  40,  250, 1, "Go, taught and played, on a Game Boy Advance"),
    ( 380,  630, 0, "Twenty missions, one idea each"),
    ( 670,  880, 0, "Master Sen explains. You play."),
    (1150, 1330, 0, "Pause - undo, hint, pass, estimate, resign"),
    (1370, 1600, 0, "Preview the stone, then commit it"),
    (1800, 2000, 0, "Ranked S to C, starred for a clean solve"),
    (2090, 2320, 0, "PUP - a backyard lawn and all the juice"),
    (2360, 2590, 0, "DINO - obsidian, amber and a volcano"),
    (2630, 2900, 0, "Five opponents, one for each level of the AI"),
    (2950, 3200, 0, "19x19 against the old master"),
    (3240, 3430, 0, "L+R+SELECT - what the AI is thinking"),
    (3500, 3780, 0, "Every screen in English and French"),
    (3830, 3995, 0, "Twenty missions. Five opponents. Three skins."),
    (3830, 3995, 1, "617 KB of Game Boy Advance cartridge"),
]


def main():
    count = int(sys.argv[1])
    duration = count / FPS
    fade_out = max(duration - 1.0, 0.1)

    width, height = 240 * SCALE, 160 * SCALE
    paired = {c[0] for c in CAPTIONS if c[2] == 1}

    parts = [
        f"scale={width}:{height}:flags=neighbor",
        f"pad={width}:{height + BAR}:0:0:color=0x12121a",
    ]

    for start, end, line, text in CAPTIONS:
        if start in paired:
            y = height + (30 if line == 0 else 76)
            size = 44 if line == 0 else 30
        else:
            y = height + 44
            size = 36

        safe = text.replace("\\", "").replace("'", "").replace(":", " -")
        parts.append(
            f"drawtext=fontfile={FONT}:text='{safe}'"
            f":x=(w-text_w)/2:y={y}:fontsize={size}:fontcolor=0xf2efe6"
            f":enable='between(n\\,{start}\\,{end})'")

    parts.append("fade=t=in:st=0:d=0.8")
    parts.append(f"fade=t=out:st={fade_out:.3f}:d=1.0")

    print(f"{duration:.3f} {fade_out:.3f} {','.join(parts)}")


main()
