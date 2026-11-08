"""Make the three pitched scream files for the DFPlayer version (PumpkinLevels).

    python tools/make_scream_mp3s.py scream.mp3 E:/      (needs ffmpeg on PATH)

Writes mp3/0001.mp3 (original pitch), 0002.mp3 and 0003.mp3 (higher) under the SD
card root, or under any folder you then copy onto the card. The sketch plays level N
with playMp3Folder(N), which goes by these file names rather than SD copy order.
"""
import argparse
import os
import subprocess

RATE = 44100


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("source", help="scream recording (mp3, wav, ...)")
    parser.add_argument("sd_root", help="SD card root, or a folder to copy onto the card")
    parser.add_argument("--semitones", type=float, nargs=3, default=[0, 4, 8], metavar="N",
                        help="pitch shift for levels 1-3 (default: 0 4 8)")
    parser.add_argument("--keep-length", action="store_true",
                        help="raise the pitch without also speeding the scream up")
    args = parser.parse_args()

    out_dir = os.path.join(args.sd_root, "mp3")
    os.makedirs(out_dir, exist_ok=True)
    for level, semitones in enumerate(args.semitones, start=1):
        ratio = 2 ** (semitones / 12)
        # Trim leading silence so the scream starts as soon as the DFPlayer gets the command,
        # then play it back faster: higher and shorter, like a sped-up tape.
        filters = ["silenceremove=start_periods=1:start_threshold=-45dB",
                   f"aresample={RATE}", f"asetrate={RATE * ratio:.0f}", f"aresample={RATE}"]
        if args.keep_length:
            filters.append(f"atempo={1 / ratio:.4f}")
        out_path = os.path.join(out_dir, f"{level:04d}.mp3")
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", args.source, "-vn",
                        "-af", ",".join(filters), "-b:a", "128k", out_path], check=True)
        print(f"Level {level}: {semitones:+g} semitones -> {out_path}")


if __name__ == "__main__":
    main()
