"""Convert an audio file into a clauduino-led Song note array.

Pipeline:
  1. Load audio via librosa (ffmpeg-backed, handles mp3).
  2. Estimate fundamental frequency (f0) per frame with pYIN.
  3. Quantize each voiced f0 to the nearest MIDI semitone (chromatic).
  4. Group consecutive same-note frames into notes.
  5. Merge runs shorter than MIN_NOTE_MS into their neighbor so we
     don't emit a swarm of 20 ms blips that sound like static.
  6. Print a C++ `Note[]` array suitable for pasting into songs.cpp.

Usage:
  python scripts/mp3_to_song.py <path_to_audio>
      [--song-name NAME]          (default: derived from filename)
      [--min-note-ms MS]          (default: 120)
      [--fmin HZ --fmax HZ]       (default: 80 .. 1200)
      [--rest-silence-ms MS]      (default: 80 — shorter than this = part of note)

Constraints:
  * Expects monophonic-ish content. Polyphonic audio still works but
    pYIN locks onto the dominant pitch (usually the melody vocal).
  * Outputs pitches roughly 100–4000 Hz (piezo-friendly).
"""

from __future__ import annotations

import argparse
import os
import re
import sys
from pathlib import Path

import librosa
import numpy as np


def midi_to_hz(m: int) -> int:
    return int(round(440.0 * (2.0 ** ((m - 69) / 12.0))))


def hz_to_midi(hz: float) -> int:
    return int(round(69 + 12 * np.log2(hz / 440.0)))


def _median_filter(x: np.ndarray, window: int) -> np.ndarray:
    if window <= 1:
        return x
    pad = window // 2
    xp = np.pad(x, pad, mode="edge")
    out = np.empty_like(x)
    for i in range(len(x)):
        out[i] = np.median(xp[i:i + window])
    return out


def extract_notes(
    audio_path: Path,
    min_note_ms: int,
    rest_silence_ms: int,
    fmin: float,
    fmax: float,
) -> list[tuple[int, int]]:
    """Returns a list of (freq_hz, duration_ms). freq_hz=0 means rest."""

    y, sr = librosa.load(str(audio_path), sr=22050, mono=True)

    # Try pYIN first (has voicing detection — cleaner on monophonic sources).
    # Fall back to plain yin if everything comes back unvoiced (polyphonic /
    # chorus / speech audio usually fails pYIN's voicing gate).
    f0, voiced_flag, _ = librosa.pyin(
        y,
        fmin=fmin,
        fmax=fmax,
        sr=sr,
        frame_length=2048,
    )
    hop_length = 512  # librosa pYIN default
    frame_ms = 1000 * hop_length / sr

    if int(np.sum(voiced_flag)) == 0:
        # Polyphonic fallback: plain yin gives a pitch for every frame.
        # Median-smooth to suppress frame-to-frame jitter on chorus audio.
        raw = librosa.yin(y, fmin=fmin, fmax=fmax, sr=sr, frame_length=2048)
        # Clamp crazy outliers before smoothing.
        raw = np.clip(raw, fmin, fmax)
        smoothed = _median_filter(raw, window=5)   # ~115 ms window at 22050/512
        f0 = smoothed
        voiced_flag = np.ones_like(smoothed, dtype=bool)

    # Per-frame MIDI (or -1 for unvoiced/silence).
    midi_per_frame: list[int] = []
    for f, voiced in zip(f0, voiced_flag):
        if voiced and f is not None and not np.isnan(f):
            midi_per_frame.append(hz_to_midi(float(f)))
        else:
            midi_per_frame.append(-1)

    # Group consecutive identical frames into runs.
    runs: list[tuple[int, int]] = []  # (midi or -1, frame_count)
    for m in midi_per_frame:
        if runs and runs[-1][0] == m:
            runs[-1] = (m, runs[-1][1] + 1)
        else:
            runs.append((m, 1))

    # Merge short runs (< min_note_ms for pitched, < rest_silence_ms for rests)
    # into the previous run. This kills pYIN's usual 1-2 frame flutter.
    def run_ms(r):
        return r[1] * frame_ms

    cleaned: list[tuple[int, int]] = []
    for m, frames in runs:
        dur_ms = frames * frame_ms
        threshold = rest_silence_ms if m == -1 else min_note_ms
        if dur_ms < threshold and cleaned:
            prev_m, prev_frames = cleaned[-1]
            cleaned[-1] = (prev_m, prev_frames + frames)
        else:
            cleaned.append((m, frames))

    # Collapse any adjacent same-midi runs that merging just created.
    collapsed: list[tuple[int, int]] = []
    for m, frames in cleaned:
        if collapsed and collapsed[-1][0] == m:
            collapsed[-1] = (m, collapsed[-1][1] + frames)
        else:
            collapsed.append((m, frames))

    # Convert to (freq_hz, duration_ms).
    notes: list[tuple[int, int]] = []
    for m, frames in collapsed:
        dur_ms = int(round(frames * frame_ms))
        if m == -1:
            notes.append((0, dur_ms))   # rest
        else:
            notes.append((midi_to_hz(m), dur_ms))

    # Drop leading/trailing rests (the buzzer silence bookending the song
    # is rarely interesting — commented out if you want to keep them).
    while notes and notes[0][0] == 0:
        notes.pop(0)
    while notes and notes[-1][0] == 0:
        notes.pop()

    return notes


def default_song_name(path: Path) -> str:
    # hidup-jokowi.mp3 -> HIDUP_JOKOWI
    stem = path.stem
    stem = re.sub(r"[^A-Za-z0-9]+", "_", stem).strip("_").upper()
    if not stem:
        stem = "SONG"
    return stem


def format_cpp(name: str, notes: list[tuple[int, int]], source: str) -> str:
    array_name = f"{name}_NOTES"
    lines = []
    lines.append(f"// Auto-generated from {source} by scripts/mp3_to_song.py")
    lines.append("// Paste into arduino/clauduino_led/songs.cpp (inside the")
    lines.append("// anonymous namespace for the note array; put the Song")
    lines.append("// struct at file scope so it has external linkage).")
    lines.append("")
    lines.append(f"const Note {array_name}[] = {{")
    for freq, dur in notes:
        lines.append(f"  {{ {freq:>4}, {dur:>4} }},")
    lines.append("};")
    lines.append("")
    lines.append(f"const Song {name} = {{")
    lines.append(f"  {array_name},")
    lines.append(f"  sizeof({array_name}) / sizeof(Note),")
    lines.append(f'  "{name}"')
    lines.append("};")
    return "\n".join(lines)


def main(argv: list[str]) -> int:
    p = argparse.ArgumentParser()
    p.add_argument("audio", help="Path to mp3/wav/flac")
    p.add_argument("--song-name", default=None)
    p.add_argument("--min-note-ms", type=int, default=120)
    p.add_argument("--rest-silence-ms", type=int, default=80)
    p.add_argument("--fmin", type=float, default=80.0)
    p.add_argument("--fmax", type=float, default=1200.0)
    args = p.parse_args(argv)

    audio_path = Path(args.audio).expanduser().resolve()
    if not audio_path.exists():
        print(f"audio file not found: {audio_path}", file=sys.stderr)
        return 2

    name = args.song_name or default_song_name(audio_path)

    notes = extract_notes(
        audio_path,
        min_note_ms=args.min_note_ms,
        rest_silence_ms=args.rest_silence_ms,
        fmin=args.fmin,
        fmax=args.fmax,
    )

    if not notes:
        print("No voiced content detected — try widening --fmin/--fmax "
              "or lowering --min-note-ms.", file=sys.stderr)
        return 1

    total_ms = sum(d for _, d in notes)
    non_rest = [n for n in notes if n[0] != 0]
    if non_rest:
        freqs = [f for f, _ in non_rest]
        print(f"# {len(notes)} note(s), {len(non_rest)} pitched, "
              f"{len(notes) - len(non_rest)} rests, "
              f"total {total_ms} ms "
              f"(pitch range {min(freqs)}..{max(freqs)} Hz)",
              file=sys.stderr)
    else:
        print(f"# {len(notes)} note(s), all rests (pyIn found no voiced "
              "content).", file=sys.stderr)

    print(format_cpp(name, notes, audio_path.name))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
