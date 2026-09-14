#!/usr/bin/env python3
"""
MOKU - Tactics of Go : audio asset generator.

Synthesizes *everything* with numpy - no downloaded samples, no external audio
libraries.  Produces:

    audio/*.wav   sound effects, 8-bit unsigned mono PCM (maxmod converts them
                  to 8-bit signed samples in the soundbank, so the WAV byte
                  count is also the ROM byte count).
    audio/*.mod   4-channel ProTracker modules written by the MOD writer below,
                  with synthesized instruments (pulse/square, triangle, sine
                  bass, plucked koto, bells, noise and drum one-shots).

Butano/maxmod turn every file name into an item name:
    audio/music_title.mod  ->  bn::music_items::music_title
    audio/sfx_stone.wav    ->  bn::sound_items::sfx_stone

Usage:
    python3 tools/gen_audio.py                 # write audio/ and validate
    python3 tools/gen_audio.py --render DIR    # also render each MOD to WAV
    python3 tools/gen_audio.py --list          # print the generated item names

Mixing head-room (measured from butano/hw/3rd_party/maxmod/src/gba/mixer_asm.s):
each mixer channel contributes  sample * fvol / 256  to a signed 8-bit output
that is hard-clamped to +-127.  A MOD channel at volume 64 with the module
volume at 1.0 ends up with fvol = 128, i.e. it contributes half of the output
range on its own.  The head-room metric used here is therefore

    M(t) = sum_over_channels |sample(t)| * channel_volume / 64      (sample in [-1,1])

and the output clips when M >= 2.0.  Every module is normalized so that
peak(M) <= MIX_TARGET_PEAK (1.15), which leaves room for two simultaneous
sound effects.  See docs/decisions/audio.md.
"""

from __future__ import annotations

import argparse
import math
import os
import struct
import sys
import wave
from dataclasses import dataclass, field

import numpy as np

# --------------------------------------------------------------------------------------
# constants
# --------------------------------------------------------------------------------------

#: Amiga NTSC clock: playback rate = AMIGA_CLOCK / (2 * period).  mmutil tunes MOD
#: samples to mid-C = 8363 Hz (period 428), i.e. the NTSC constant, so the renderer
#: below uses the same one and our instrument tuning lands within 2 cents of A=440.
AMIGA_CLOCK = 7159090.5

#: Butano's default Direct Sound mixing rate (BN_AUDIO_MIXING_RATE_16_KHZ).
GBA_MIX_RATE = 15768

#: peak of the head-room metric M we normalize modules to (clipping starts at 2.0).
MIX_TARGET_PEAK = 1.15

#: loudness target (RMS in the same units), so sparse and dense tracks match.
MIX_TARGET_RMS = 0.28

#: ProTracker period table, finetune 0, notes C-1 .. B-3 (tracker octaves 1..3).
PERIODS = [
    856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
    428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,
    214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113,
]

NOTE_NAMES = ['C-', 'C#', 'D-', 'D#', 'E-', 'F-', 'F#', 'G-', 'G#', 'A-', 'A#', 'B-']

# MIDI note helpers (C4 = 60 = middle C, A4 = 69 = 440 Hz).


def midi(name: str) -> int:
    """'A#3' / 'Bb3' / 'C4' -> MIDI note number."""
    step = {'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11}[name[0].upper()]
    i = 1
    while i < len(name) and name[i] in '#b':
        step += 1 if name[i] == '#' else -1
        i += 1
    return (int(name[i:]) + 1) * 12 + step


def hz_to_midi(f: float) -> float:
    return 12.0 * math.log2(f / 440.0) + 69.0


def period_rate(period: int) -> float:
    """Playback rate in Hz of a sample played at `period`."""
    return AMIGA_CLOCK / (2.0 * period)


# --------------------------------------------------------------------------------------
# small DSP toolbox (numpy only)
# --------------------------------------------------------------------------------------

def rng(seed: int) -> np.random.Generator:
    return np.random.default_rng(seed)


def biquad(x: np.ndarray, b: tuple, a: tuple) -> np.ndarray:
    """Direct form I biquad. a[0] is assumed to be 1."""
    b0, b1, b2 = b
    a1, a2 = a
    y = np.empty_like(x)
    x1 = x2 = y1 = y2 = 0.0
    for n in range(x.shape[0]):
        xn = x[n]
        yn = b0 * xn + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        y[n] = yn
        x2, x1 = x1, xn
        y2, y1 = y1, yn
    return y


def _rbj(kind: str, f0: float, sr: float, q: float, gain: float = 1.0):
    w0 = 2.0 * math.pi * max(1.0, min(f0, sr * 0.49)) / sr
    cw, sw = math.cos(w0), math.sin(w0)
    alpha = sw / (2.0 * q)
    if kind == 'lp':
        b = ((1 - cw) / 2, 1 - cw, (1 - cw) / 2)
        a0 = 1 + alpha
        a = (-2 * cw, 1 - alpha)
    elif kind == 'hp':
        b = ((1 + cw) / 2, -(1 + cw), (1 + cw) / 2)
        a0 = 1 + alpha
        a = (-2 * cw, 1 - alpha)
    elif kind == 'bp':  # constant skirt gain, peak gain = q
        b = (q * alpha, 0.0, -q * alpha)
        a0 = 1 + alpha
        a = (-2 * cw, 1 - alpha)
    else:
        raise ValueError(kind)
    b = tuple(v * gain / a0 for v in b)
    a = tuple(v / a0 for v in a)
    return b, a


def lowpass(x, f0, sr, q=0.707):
    b, a = _rbj('lp', f0, sr, q)
    return biquad(x, b, a)


def highpass(x, f0, sr, q=0.707):
    b, a = _rbj('hp', f0, sr, q)
    return biquad(x, b, a)


def bandpass(x, f0, sr, q=4.0, gain=1.0):
    b, a = _rbj('bp', f0, sr, q, gain)
    return biquad(x, b, a)


def env_exp(n: int, sr: float, attack: float, decay: float, floor: float = 0.0) -> np.ndarray:
    """Fast attack + exponential decay envelope, `decay` = time constant (s)."""
    t = np.arange(n) / sr
    a = np.clip(t / max(attack, 1e-6), 0.0, 1.0)
    d = np.exp(-t / max(decay, 1e-6))
    return a * (d * (1.0 - floor) + floor)


def env_ad(n: int, sr: float, attack: float, hold: float, release: float) -> np.ndarray:
    t = np.arange(n) / sr
    total = attack + hold + release
    e = np.ones(n)
    e = np.where(t < attack, t / max(attack, 1e-9), e)
    rel = np.clip((total - t) / max(release, 1e-9), 0.0, 1.0)
    e = np.minimum(e, rel)
    return np.clip(e, 0.0, 1.0)


def fade_edges(x: np.ndarray, sr: float, fin: float = 0.0015, fout: float = 0.004) -> np.ndarray:
    """Guarantee the sample starts and ends at zero (no DAC click)."""
    n = x.shape[0]
    ni = max(1, min(int(fin * sr), n // 2))
    no = max(2, min(int(fout * sr), n // 2))
    x = x.copy()
    x[:ni] *= np.linspace(0.0, 1.0, ni)
    x[n - no:] *= np.linspace(1.0, 0.0, no)
    x[-1] = 0.0
    return x


def normalize(x: np.ndarray, peak: float) -> np.ndarray:
    m = float(np.max(np.abs(x))) if x.size else 0.0
    if m < 1e-9:
        return x
    return x * (peak / m)


def dc_block(x: np.ndarray) -> np.ndarray:
    return x - float(np.mean(x))


def soft_clip(x: np.ndarray) -> np.ndarray:
    """tanh-ish saturation, keeps |y| < 1 without hard edges."""
    return np.tanh(x * 1.15) / math.tanh(1.15)


def periodic_noise(n: int, sr: float, lo: float, hi: float, seed: int, tilt: float = 0.0) -> np.ndarray:
    """Band-limited noise that loops perfectly (built in the frequency domain)."""
    g = rng(seed)
    spec = np.zeros(n // 2 + 1, dtype=complex)
    freqs = np.arange(n // 2 + 1) * sr / n
    mag = np.ones_like(freqs)
    mag[freqs < lo] = 0.0
    mag[freqs > hi] = 0.0
    with np.errstate(divide='ignore'):
        shape = np.where(freqs > 0, (freqs / max(lo, 1.0)) ** tilt, 0.0)
    mag = mag * shape
    # soft edges so the band does not ring
    edge = np.clip((freqs - lo) / max(lo * 0.5, 1.0), 0.0, 1.0)
    edge *= np.clip((hi - freqs) / max(hi * 0.25, 1.0), 0.0, 1.0)
    mag = mag * edge
    phase = g.uniform(0.0, 2.0 * math.pi, spec.shape[0])
    spec = mag * np.exp(1j * phase)
    spec[0] = 0.0
    y = np.fft.irfft(spec, n)
    return normalize(y, 1.0)


def white(n: int, seed: int) -> np.ndarray:
    return rng(seed).uniform(-1.0, 1.0, n)


def sweep(n: int, sr: float, f0: float, f1: float, curve: float = 1.0, phase0: float = 0.0) -> np.ndarray:
    """Sine with an exponential-ish frequency sweep from f0 to f1."""
    t = np.arange(n) / sr
    k = (t / max(t[-1], 1e-9)) ** curve if n > 1 else np.zeros(1)
    f = f0 * (f1 / f0) ** k
    ph = phase0 + 2.0 * math.pi * np.cumsum(f) / sr
    return np.sin(ph)


def harmonic_amps(kind: str, nharm: int, duty: float = 0.5) -> np.ndarray:
    """Amplitudes (sine series) of an ideal waveform, DC removed."""
    k = np.arange(1, nharm + 1)
    if kind == 'saw':
        a = 2.0 / (math.pi * k)
    elif kind == 'square':
        a = np.where(k % 2 == 1, 4.0 / (math.pi * k), 0.0)
    elif kind == 'pulse':
        a = (4.0 / (math.pi * k)) * np.sin(math.pi * k * duty)
    elif kind == 'triangle':
        a = np.where(k % 2 == 1, 8.0 / (math.pi ** 2 * k ** 2), 0.0)
        a = a * np.where((k // 2) % 2 == 0, 1.0, -1.0)
    elif kind == 'sine':
        a = np.where(k == 1, 1.0, 0.0)
    else:
        raise ValueError(kind)
    return a


def lanczos(nharm: int) -> np.ndarray:
    k = np.arange(1, nharm + 1)
    return np.sinc(k / (nharm + 1.0))


def cycle_wave(length: int, kind: str, nharm: int = 0, duty: float = 0.5,
               tilt: float = 0.0, extra: dict | None = None) -> np.ndarray:
    """One band-limited cycle of `length` samples, peak-normalized, no DC."""
    if nharm <= 0:
        nharm = max(1, length // 2 - 1)
    nharm = min(nharm, max(1, length // 2 - 1))
    a = harmonic_amps(kind, nharm, duty) * lanczos(nharm)
    if tilt:
        k = np.arange(1, nharm + 1)
        a = a * (k ** -tilt)
    if extra:
        for k_i, mul in extra.items():
            if 1 <= k_i <= nharm:
                a[k_i - 1] *= mul
    n = np.arange(length)
    ph = 2.0 * math.pi * n / length
    y = np.zeros(length)
    for i, amp in enumerate(a, start=1):
        if amp:
            y += amp * np.sin(i * ph)
    return normalize(y, 1.0)


# --------------------------------------------------------------------------------------
# WAV output (8-bit unsigned mono - the format maxmod stores 1:1 in the soundbank)
# --------------------------------------------------------------------------------------

def quantize_u8(x: np.ndarray) -> np.ndarray:
    q = np.rint(np.clip(x, -1.0, 127.0 / 128.0) * 128.0) + 128.0
    return np.clip(q, 0, 255).astype(np.uint8)


def quantize_s8(x: np.ndarray) -> np.ndarray:
    q = np.rint(np.clip(x, -1.0, 127.0 / 128.0) * 127.0)
    return np.clip(q, -128, 127).astype(np.int8)


def write_wav_u8(path: str, x: np.ndarray, sr: int) -> int:
    data = quantize_u8(x).tobytes()
    with wave.open(path, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(1)
        w.setframerate(int(sr))
        w.writeframes(data)
    return os.path.getsize(path)


def write_wav_s16(path: str, x: np.ndarray, sr: int) -> int:
    d = np.rint(np.clip(x, -1.0, 1.0) * 32000.0).astype('<i2').tobytes()
    with wave.open(path, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(int(sr))
        w.writeframes(d)
    return os.path.getsize(path)


# --------------------------------------------------------------------------------------
# ProTracker MOD writer
# --------------------------------------------------------------------------------------

@dataclass(eq=False)
class Instrument:
    """A MOD sample plus the musical metadata needed to write notes for it."""
    name: str
    data: np.ndarray                 # float samples in [-1, 1]
    volume: int = 48                 # default channel volume when the note is triggered
    loop_start: int = -1             # in samples (-1: one-shot)
    loop_len: int = 0                # in samples
    base_midi: int | None = None     # real pitch of tracker note index 0 (C-1)
    fixed_index: int | None = None   # percussion: always play at this tracker index
    cents: float = 0.0               # tuning error of base_midi (informative)

    def tracker_index(self, m: int | None) -> int:
        if self.fixed_index is not None:
            return self.fixed_index
        if self.base_midi is None:
            raise ValueError(f'{self.name}: percussion instrument needs fixed_index')
        idx = int(m) - self.base_midi
        if not 0 <= idx < len(PERIODS):
            raise ValueError(f'{self.name}: MIDI {m} maps to tracker index {idx} (out of range; '
                             f'instrument covers MIDI {self.base_midi}..{self.base_midi + 35})')
        return idx


def tonal(name: str, cycle: np.ndarray, repeats: int = 1, attack: np.ndarray | None = None,
          volume: int = 48) -> Instrument:
    """Looping instrument: optional attack section followed by a looped cycle."""
    length = cycle.shape[0]
    body = np.tile(cycle, repeats)
    if attack is not None:
        assert attack.shape[0] % length == 0, 'attack must be a whole number of cycles'
        data = np.concatenate([attack, body])
        loop_start = attack.shape[0]
    else:
        data = body
        loop_start = 0
    m = hz_to_midi(period_rate(PERIODS[0]) / length)
    base = int(round(m))
    cents = (m - base) * 100.0
    return Instrument(name=name, data=data, volume=volume, loop_start=loop_start,
                      loop_len=length * repeats, base_midi=base, cents=cents)


def percussive(name: str, data: np.ndarray, note: str = 'C-2', volume: int = 48) -> Instrument:
    """One-shot instrument; `note` is the tracker note it is designed to be played at."""
    idx = NOTE_NAMES.index(note[:2]) + (int(note[2]) - 1) * 12
    return Instrument(name=name, data=data, volume=volume, loop_start=-1, loop_len=0,
                      base_midi=None, fixed_index=idx)


def perc_rate(note: str = 'C-2') -> float:
    idx = NOTE_NAMES.index(note[:2]) + (int(note[2]) - 1) * 12
    return period_rate(PERIODS[idx])


class Module:
    """A 4-channel ProTracker module under construction."""

    CHANNELS = 4
    ROWS = 64

    def __init__(self, name: str, rows: int, speed: int = 6, tempo: int = 125,
                 title: str | None = None):
        self.name = name
        self.title = (title or name)[:20]
        self.speed = speed
        self.tempo = tempo
        self.nrows = rows
        self.instruments: list[Instrument] = []
        self._slots: dict[int, int] = {}
        self._timed = False
        # cells[row][channel] = [sample, period, cmd, param]
        self.cells = [[[0, 0, 0, 0] for _ in range(self.CHANNELS)] for _ in range(rows)]
        self.restart = 0

    # -- instruments ---------------------------------------------------------------
    def use(self, inst: Instrument) -> int:
        slot = self._slots.get(id(inst))
        if slot is None:
            if len(self.instruments) >= 31:
                raise ValueError('MOD supports at most 31 samples')
            self.instruments.append(inst)
            slot = len(self.instruments)
            self._slots[id(inst)] = slot
        return slot

    # -- writing cells -------------------------------------------------------------
    def _cell(self, ch: int, row: int):
        if not 0 <= row < self.nrows:
            raise IndexError(f'{self.name}: row {row} outside 0..{self.nrows - 1}')
        return self.cells[row][ch]

    def note(self, ch: int, row: int, inst: Instrument, m: int | None = None,
             vol: int | None = None, cmd: int = 0, param: int = 0) -> None:
        slot = self.use(inst)
        c = self._cell(ch, row)
        if c[1] or c[0]:
            raise ValueError(f'{self.name}: note collision at ch{ch} row{row}')
        c[0] = slot
        c[1] = PERIODS[inst.tracker_index(m)]
        if vol is not None:
            if cmd:
                raise ValueError('cannot set volume and another effect on the same cell')
            cmd, param = 0xC, max(0, min(64, int(vol)))
        c[2], c[3] = cmd, param

    def effect(self, ch: int, row: int, cmd: int, param: int, force: bool = False) -> None:
        c = self._cell(ch, row)
        if c[2] and not force:
            raise ValueError(f'{self.name}: effect collision at ch{ch} row{row}')
        c[2], c[3] = cmd, max(0, min(255, int(param)))

    def setvol(self, ch: int, row: int, v: int) -> None:
        self.effect(ch, row, 0xC, max(0, min(64, int(v))))

    def decay(self, ch: int, row: int, rows: int, rate: int, start: int = 1) -> None:
        """A0x volume slide on `rows` rows starting `start` rows after the note."""
        for r in range(row + start, min(row + start + rows, self.nrows)):
            c = self._cell(ch, r)
            if c[0] == 0 and c[1] == 0 and (c[2] == 0 or c[2] == 0xA):
                c[2], c[3] = 0xA, max(1, min(15, int(rate)))

    def play(self, ch: int, row: int, inst: Instrument, m: int, length: int,
             vol: int | None = None, dec: int | None = None, cut: bool = True,
             cmd: int = 0, param: int = 0, dec_start: int = 1) -> None:
        """Note + optional exponential-ish decay + optional note cut at the end."""
        self.note(ch, row, inst, m, vol=vol, cmd=cmd, param=param)
        if dec:
            self.decay(ch, row, length - dec_start, dec, start=dec_start)
        if cut and row + length < self.nrows:
            c = self._cell(ch, row + length)
            if c[1] == 0 and (c[2] == 0 or c[2] == 0xA):
                c[2], c[3] = 0xC, 0

    # -- encoding ------------------------------------------------------------------
    def _patterns(self):
        if self.nrows % self.ROWS:
            raise ValueError(f'{self.name}: {self.nrows} rows is not a multiple of 64')
        raw = []
        for p in range(self.nrows // self.ROWS):
            blob = bytearray()
            for r in range(p * self.ROWS, (p + 1) * self.ROWS):
                for ch in range(self.CHANNELS):
                    s, period, cmd, param = self.cells[r][ch]
                    blob.append((s & 0xF0) | ((period >> 8) & 0x0F))
                    blob.append(period & 0xFF)
                    blob.append(((s & 0x0F) << 4) | (cmd & 0x0F))
                    blob.append(param & 0xFF)
            raw.append(bytes(blob))
        uniq: list[bytes] = []
        order: list[int] = []
        for blob in raw:
            if blob in uniq:
                order.append(uniq.index(blob))
            else:
                uniq.append(blob)
                order.append(len(uniq) - 1)
        return uniq, order

    def write_timing(self) -> None:
        """MOD has no initial speed/tempo field: emit Fxx commands on row 0."""
        if self._timed:
            return
        self._timed = True
        wanted = []
        if self.speed != 6:
            wanted.append(self.speed)
        if self.tempo != 125:
            wanted.append(self.tempo)
        for value in wanted:
            for ch in range(self.CHANNELS):
                c = self.cells[0][ch]
                if c[2] == 0 and c[3] == 0:
                    c[2], c[3] = 0xF, value
                    break
            else:
                raise ValueError(f'{self.name}: no free effect slot on row 0 for Fxx')

    def encode(self) -> bytes:
        self.write_timing()
        uniq, order = self._patterns()
        out = bytearray()
        out += self.title.encode('ascii', 'replace').ljust(20, b'\0')
        for i in range(31):
            if i < len(self.instruments):
                inst = self.instruments[i]
                data = quantize_s8(inst.data)
                if data.shape[0] % 2:
                    data = np.append(data, np.int8(0))
                words = data.shape[0] // 2
                if words > 65535:
                    raise ValueError(f'{inst.name}: sample too long')
                if inst.loop_start >= 0 and inst.loop_len > 0:
                    rep = inst.loop_start // 2
                    replen = inst.loop_len // 2
                    if inst.loop_start % 2 or inst.loop_len % 2:
                        raise ValueError(f'{inst.name}: loop points must be even')
                    if replen < 1:
                        raise ValueError(f'{inst.name}: loop too short')
                else:
                    rep, replen = 0, 1
                out += inst.name.encode('ascii', 'replace')[:22].ljust(22, b'\0')
                out += struct.pack('>H', words)
                out += bytes([0, max(0, min(64, inst.volume))])
                out += struct.pack('>HH', rep, replen)
            else:
                out += b'\0' * 22 + struct.pack('>H', 0) + bytes([0, 0]) + struct.pack('>HH', 0, 1)
        if len(order) > 128:
            raise ValueError(f'{self.name}: {len(order)} pattern positions (max 128)')
        out += bytes([len(order), self.restart & 0x7F])
        out += bytes(order).ljust(128, b'\0')
        out += b'M.K.'
        for blob in uniq:
            out += blob
        for inst in self.instruments:
            data = quantize_s8(inst.data)
            if data.shape[0] % 2:
                data = np.append(data, np.int8(0))
            if inst.loop_start < 0:
                # ProTracker convention: a non-looping sample starts with two zero bytes.
                data = data.copy()
                data[0] = 0
                data[1] = 0
            out += data.tobytes()
        return bytes(out)

    # -- info ----------------------------------------------------------------------
    def row_seconds(self) -> float:
        return self.speed * 2.5 / self.tempo

    def duration(self) -> float:
        return self.nrows * self.row_seconds()

    def unique_patterns(self) -> int:
        return len(self._patterns()[0])

    def positions(self) -> int:
        return len(self._patterns()[1])


# --------------------------------------------------------------------------------------
# instrument bank - every sample below is synthesized from scratch
# --------------------------------------------------------------------------------------

def evolving_attack(length: int, cycles: int, start: np.ndarray, end: np.ndarray,
                    taus: np.ndarray) -> np.ndarray:
    """`cycles` cycles morphing from the `start` spectrum to the `end` spectrum."""
    total = length * cycles
    n = np.arange(total)
    ph = 2.0 * math.pi * n / length
    y = np.zeros(total)
    for i in range(start.shape[0]):
        a0, a1, tau = start[i], end[i], taus[i]
        amp = a1 + (a0 - a1) * np.exp(-n / (length * max(tau, 1e-6)))
        y += amp * np.sin((i + 1) * ph)
    return y


def plucked_instrument(name: str, length: int, pluck: float, nharm: int, damp_end: float,
                       bright: float, cycles: int, volume: int) -> Instrument:
    """Bright attack settling into a mellow looped cycle (koto / harp / bass pluck)."""
    k = np.arange(1, nharm + 1)
    end = (1.0 / k) * np.abs(np.sin(math.pi * k * pluck))
    end = end * np.exp(-((k - 1) / max(damp_end, 1e-6)) ** 1.3)
    start = (1.0 / k ** 0.55) * np.abs(np.sin(math.pi * k * pluck)) * bright
    lz = lanczos(nharm)
    end, start = end * lz, start * lz
    taus = 0.9 + 3.5 / k                      # high partials die first
    atk = evolving_attack(length, cycles, start, end, taus)
    loop = np.zeros(length)
    n = np.arange(length)
    ph = 2.0 * math.pi * n / length
    for i, amp in enumerate(end, start=1):
        loop += amp * np.sin(i * ph)
    peak = max(float(np.max(np.abs(atk))), float(np.max(np.abs(loop))), 1e-9)
    return tonal(name, loop / peak, repeats=1, attack=atk / peak, volume=volume)


def bell_cycle(length: int) -> np.ndarray:
    """Metallic, slightly hollow spectrum (integer partials only, so it can loop)."""
    amps = {1: 0.30, 2: 1.00, 3: 0.70, 4: 0.45, 5: 0.12, 6: 0.38, 7: 0.22, 9: 0.16, 11: 0.10}
    n = np.arange(length)
    ph = 2.0 * math.pi * n / length
    y = np.zeros(length)
    for k, a in amps.items():
        if k <= length // 2 - 1:
            y += a * np.sin(k * ph + (0.37 * k))
    return normalize(y, 1.0)


# --- tonal ------------------------------------------------------------------------------

def make_bank() -> dict:
    b: dict[str, Instrument] = {}

    # koto: 32-sample cycle -> tracker note C-1 sounds as MIDI 48 (C3), range C3..B5.
    b['koto'] = plucked_instrument('koto', 32, 0.22, 15, 2.6, 1.0, 10, volume=46)
    b['harp'] = plucked_instrument('harp', 32, 0.30, 15, 3.4, 0.9, 8, volume=44)
    b['bassp'] = plucked_instrument('bass pluck', 128, 0.18, 24, 3.0, 0.8, 6, volume=48)

    b['pulse50'] = tonal('square', cycle_wave(32, 'square', nharm=12), volume=40)
    b['pulse25'] = tonal('pulse 25%', cycle_wave(32, 'pulse', nharm=12, duty=0.25), volume=38)
    b['pulse12'] = tonal('pulse 12%', cycle_wave(32, 'pulse', nharm=13, duty=0.125), volume=36)
    b['tri'] = tonal('triangle', cycle_wave(32, 'triangle', nharm=15), volume=46)
    b['sine'] = tonal('sine', cycle_wave(32, 'sine'), volume=44)
    b['flute'] = tonal('flute', cycle_wave(32, 'triangle', nharm=9, extra={3: 0.55, 5: 0.4}),
                       volume=44)
    b['bell'] = tonal('bell', bell_cycle(32), volume=40)

    # basses: 128-sample cycle -> tracker note C-1 sounds as MIDI 24 (C1), range C1..B3.
    b['bass'] = tonal('sine bass', cycle_wave(128, 'sine', extra={2: 0.28, 3: 0.10}), volume=50)
    b['bass2'] = tonal('wood bass', cycle_wave(128, 'triangle', nharm=10, extra={2: 0.45, 3: 0.3}),
                       volume=48)
    b['drone'] = tonal('drone', cycle_wave(128, 'sine', extra={2: 0.18, 3: 0.22, 5: 0.08}),
                       volume=42)

    # --- percussion (designed at the playback rate of the tracker note they use) ---------
    sr = perc_rate('C-1')                                  # 4143 Hz - low drums
    n = int(0.150 * sr)
    body = sweep(n, sr, 150.0, 46.0, curve=0.35) * env_exp(n, sr, 0.0008, 0.045)
    click = white(n, 11) * env_exp(n, sr, 0.0002, 0.004) * 0.5
    click = lowpass(click, 900.0, sr)
    kick = soft_clip(body * 1.15 + click)
    b['kick'] = percussive('kick', fade_edges(normalize(kick, 0.95), sr), 'C-1', volume=52)

    n = int(0.190 * sr)
    tom = sweep(n, sr, 210.0, 96.0, curve=0.5) * env_exp(n, sr, 0.001, 0.075)
    tomn = lowpass(white(n, 12), 1400.0, sr) * env_exp(n, sr, 0.0005, 0.010) * 0.35
    b['tom'] = percussive('tom', fade_edges(normalize(tom + tomn, 0.92), sr), 'C-1', volume=50)

    n = int(0.230 * sr)
    tomlo = sweep(n, sr, 150.0, 62.0, curve=0.5) * env_exp(n, sr, 0.001, 0.095)
    tomlo += lowpass(white(n, 13), 900.0, sr) * env_exp(n, sr, 0.0005, 0.012) * 0.3
    b['tomlo'] = percussive('floor tom', fade_edges(normalize(tomlo, 0.92), sr), 'C-1', volume=50)

    sr = perc_rate('C-2')                                  # 8287 Hz - bright percussion
    n = int(0.130 * sr)
    sn = white(n, 21)
    sn = bandpass(sn, 1900.0, sr, q=0.8, gain=1.0) * env_exp(n, sr, 0.0006, 0.030)
    sn += np.sin(2 * math.pi * 190.0 * np.arange(n) / sr) * env_exp(n, sr, 0.0006, 0.022) * 0.55
    sn += np.sin(2 * math.pi * 285.0 * np.arange(n) / sr) * env_exp(n, sr, 0.0006, 0.016) * 0.3
    b['snare'] = percussive('snare', fade_edges(normalize(soft_clip(sn), 0.92), sr), 'C-2',
                            volume=46)

    n = int(0.038 * sr)
    hat = highpass(white(n, 22), 3400.0, sr) * env_exp(n, sr, 0.0004, 0.008)
    b['hat'] = percussive('hat', fade_edges(normalize(hat, 0.70), sr), 'C-2', volume=32)

    n = int(0.150 * sr)
    ohat = highpass(white(n, 23), 3000.0, sr) * env_exp(n, sr, 0.0006, 0.050)
    b['ohat'] = percussive('open hat', fade_edges(normalize(ohat, 0.72), sr), 'C-2', volume=30)

    n = int(0.028 * sr)
    rim = bandpass(white(n, 24), 2300.0, sr, q=2.5) * env_exp(n, sr, 0.0003, 0.005)
    rim += np.sin(2 * math.pi * 880.0 * np.arange(n) / sr) * env_exp(n, sr, 0.0002, 0.004) * 0.6
    b['rim'] = percussive('rim', fade_edges(normalize(rim, 0.80), sr), 'C-2', volume=38)

    n = int(0.045 * sr)
    t = np.arange(n) / sr
    wood = (np.sin(2 * math.pi * 1180.0 * t) * 1.0 + np.sin(2 * math.pi * 2450.0 * t) * 0.45)
    wood = wood * env_exp(n, sr, 0.0004, 0.009)
    wood += bandpass(white(n, 25), 1700.0, sr, q=1.5) * env_exp(n, sr, 0.0002, 0.004) * 0.5
    b['wood'] = percussive('woodblock', fade_edges(normalize(wood, 0.85), sr), 'C-2', volume=40)

    n = int(0.070 * sr)
    shk = bandpass(white(n, 26), 5200.0, sr, q=0.7) * env_ad(n, sr, 0.006, 0.008, 0.050)
    b['shaker'] = percussive('shaker', fade_edges(normalize(shk, 0.66), sr), 'C-2', volume=34)

    n = int(0.420 * sr)
    t = np.arange(n) / sr
    cr = highpass(white(n, 27), 2600.0, sr)
    for f in (3100.0, 4300.0, 5700.0, 7100.0):
        cr += np.sin(2 * math.pi * f * t + f) * 0.10
    cr = cr * env_exp(n, sr, 0.0015, 0.130)
    b['crash'] = percussive('crash', fade_edges(normalize(cr, 0.80), sr), 'C-2', volume=40)

    # looping wind/air bed for the meditative track (perfectly periodic noise)
    wind = periodic_noise(2048, perc_rate('C-2'), 240.0, 2400.0, seed=31, tilt=-0.7)
    wind = normalize(wind, 0.85)
    b['wind'] = Instrument('wind', wind, volume=18, loop_start=0, loop_len=2048,
                           base_midi=None, fixed_index=NOTE_NAMES.index('C-') + 12)
    return b


# --------------------------------------------------------------------------------------
# MOD renderer - a small ProTracker mixer used to validate what we wrote
# --------------------------------------------------------------------------------------

class _Chan:
    __slots__ = ('inst', 'pos', 'period', 'note_idx', 'vol', 'active',
                 'vib_pos', 'vib_speed', 'vib_depth', 'porta_target', 'porta_speed')

    def __init__(self):
        self.inst = None
        self.pos = 0.0
        self.period = 0
        self.note_idx = 0
        self.vol = 0
        self.active = False
        self.vib_pos = 0
        self.vib_speed = 0
        self.vib_depth = 0
        self.porta_target = 0
        self.porta_speed = 0


_VIB_SINE = np.rint(255.0 * np.sin(2.0 * math.pi * np.arange(64) / 64.0)).astype(int)


class ModRenderer:
    """Renders a Module to floating point 'mixer units': 1.0 == one full-scale
    channel at volume 64, i.e. half of the GBA output range.  maxmod clips at 2.0."""

    def __init__(self, mod: Module, sr: int = GBA_MIX_RATE):
        self.mod = mod
        self.sr = sr

    def render(self, loops: int = 1) -> np.ndarray:
        mod = self.mod
        mod.write_timing()
        chans = [_Chan() for _ in range(mod.CHANNELS)]
        speed, tempo = 6, 125
        out = []
        frac = 0.0
        for _ in range(loops):
            for row in range(mod.nrows):
                cells = mod.cells[row]
                # --- tick 0: notes and one-shot effects ---------------------------
                for ci, c in enumerate(cells):
                    ch = chans[ci]
                    s, period, cmd, param = c
                    if s:
                        ch.inst = mod.instruments[s - 1]
                        ch.vol = ch.inst.volume
                    if period:
                        if cmd == 0x3:
                            ch.porta_target = period
                            if param:
                                ch.porta_speed = param
                        else:
                            ch.period = period
                            ch.note_idx = PERIODS.index(period) if period in PERIODS else -1
                            ch.pos = 0.0
                            ch.active = ch.inst is not None
                            ch.vib_pos = 0
                    if cmd == 0xC:
                        ch.vol = min(64, param)
                    elif cmd == 0xF:
                        if param < 0x20:
                            speed = max(1, param)
                        else:
                            tempo = param
                    elif cmd == 0x4:
                        if param >> 4:
                            ch.vib_speed = param >> 4
                        if param & 0xF:
                            ch.vib_depth = param & 0xF
                    elif cmd == 0x3 and param:
                        ch.porta_speed = param
                # --- ticks --------------------------------------------------------
                for tick in range(speed):
                    for ci, c in enumerate(cells):
                        ch = chans[ci]
                        cmd, param = c[2], c[3]
                        if tick > 0:
                            if cmd == 0xA:
                                up, down = param >> 4, param & 0xF
                                ch.vol = max(0, min(64, ch.vol + up - down))
                            elif cmd == 0x1:
                                ch.period = max(113, ch.period - param)
                            elif cmd == 0x2:
                                ch.period = min(856, ch.period + param)
                            elif cmd == 0x3 and ch.porta_target:
                                if ch.period < ch.porta_target:
                                    ch.period = min(ch.porta_target, ch.period + ch.porta_speed)
                                else:
                                    ch.period = max(ch.porta_target, ch.period - ch.porta_speed)
                    n = (self.sr * 2.5 / tempo) + frac
                    ns = int(n)
                    frac = n - ns
                    block = np.zeros(ns)
                    for ci, c in enumerate(cells):
                        ch = chans[ci]
                        cmd, param = c[2], c[3]
                        period = ch.period
                        if cmd == 0x0 and param and ch.note_idx >= 0:
                            off = (0, param >> 4, param & 0xF)[tick % 3]
                            period = PERIODS[min(len(PERIODS) - 1, ch.note_idx + off)]
                        elif cmd == 0x4 and ch.vib_depth:
                            delta = (_VIB_SINE[ch.vib_pos & 63] * ch.vib_depth) // 128
                            period = max(113, min(856, period + int(delta)))
                            ch.vib_pos = (ch.vib_pos + ch.vib_speed) & 63
                        self._mix(ch, block, period)
                    out.append(block)
        return np.concatenate(out) if out else np.zeros(0)

    def _mix(self, ch: _Chan, block: np.ndarray, period: int) -> None:
        if not ch.active or ch.inst is None or ch.vol <= 0 or period <= 0:
            if ch.active and ch.inst is not None and period > 0:
                ch.pos += (period_rate(period) / self.sr) * block.shape[0]
                length = ch.inst.data.shape[0]
                if ch.inst.loop_len > 0:
                    ls, ll = ch.inst.loop_start, ch.inst.loop_len
                    if ch.pos >= ls + ll:
                        ch.pos = ls + math.fmod(ch.pos - ls, ll)
                elif ch.pos >= length:
                    ch.active = False
            return
        data = ch.inst.data
        length = data.shape[0]
        step = period_rate(period) / self.sr
        idx = ch.pos + step * np.arange(block.shape[0])
        end = ch.pos + step * block.shape[0]
        if ch.inst.loop_len > 0:
            ls, ll = ch.inst.loop_start, ch.inst.loop_len
            over = idx >= ls + ll
            if over.any():
                idx = np.where(over, ls + np.mod(idx - ls, ll), idx)
            if end >= ls + ll:
                end = ls + math.fmod(end - ls, ll)
        else:
            over = idx >= length
            if over.any():
                idx = np.where(over, 0.0, idx)
                ch.active = False
            if end >= length:
                end = float(length)
        gather = data[np.clip(idx.astype(np.int64), 0, length - 1)]
        if ch.inst.loop_len <= 0 and over.any():
            gather = np.where(over, 0.0, gather)
        block += gather * (ch.vol / 64.0)
        ch.pos = end


@dataclass
class MixStats:
    seconds: float
    peak: float
    rms: float
    clipped: int
    max_gap: float


def analyse(x: np.ndarray, sr: int) -> MixStats:
    if x.size == 0:
        return MixStats(0.0, 0.0, 0.0, 0, 0.0)
    peak = float(np.max(np.abs(x)))
    rms = float(np.sqrt(np.mean(x * x)))
    clipped = int(np.count_nonzero(np.abs(x) >= 2.0))
    win = max(1, sr // 20)
    n = (x.shape[0] // win) * win
    env = np.max(np.abs(x[:n].reshape(-1, win)), axis=1)
    gap, best = 0, 0
    for v in env:
        gap = gap + 1 if v < 0.02 else 0
        best = max(best, gap)
    return MixStats(x.shape[0] / sr, peak, rms, clipped, best * win / sr)


# --------------------------------------------------------------------------------------
# composition helpers
# --------------------------------------------------------------------------------------

Q = 4        # rows per beat
BAR = 16     # rows per 4/4 bar

# note-name shortcuts used by the songs below
N = {n: midi(n) for n in
     [f'{p}{o}' for o in range(0, 7)
      for p in ('C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B')]}


def phrase(mod: Module, ch: int, inst: Instrument, start: int, notes, vol=None,
           dec: int = 0, cut: bool = False, q: int = Q) -> None:
    """notes = [(beat, midi, beats), ...] placed `start` rows into the module."""
    for beat, m, dur in notes:
        row = start + int(round(beat * q))
        length = max(1, int(round(dur * q)))
        mod.play(ch, row, inst, m, length, vol=vol, dec=dec, cut=cut)


def echo(mod: Module, dst_ch: int, inst: Instrument, start: int, notes,
         delay: int = 2, vol: int = 16, dec: int = 2, q: int = Q) -> None:
    """Soft repeat of a phrase a couple of rows later (koto-style shimmer)."""
    for beat, m, dur in notes:
        if dur < 0.5:
            continue
        row = start + int(round(beat * q)) + delay
        if row >= mod.nrows:
            continue
        c = mod.cells[row][dst_ch]
        if c[0] or c[1]:
            continue
        length = max(1, int(round(dur * q)))
        mod.play(dst_ch, row, inst, m, length, vol=vol, dec=dec, cut=False)


def hits(mod: Module, ch: int, inst: Instrument, start: int, pat: str, vol: int | None = None,
         accent: int = 8, soft: int = 14) -> None:
    """Drum row-string: 'x' hit, 'X' accent, 'o' ghost, '-' silence, ' ' ignored."""
    base = vol if vol is not None else inst.volume
    col = 0
    for chunk in pat.split():
        for chpos in chunk:
            if chpos != '-':
                v = base + accent if chpos == 'X' else (base - soft if chpos == 'o' else base)
                mod.note(ch, start + col, inst, vol=max(1, min(64, v)))
            col += 1


def chord(mod: Module, ch: int, inst: Instrument, row: int, root: int, intervals: tuple,
          length: int, vol: int | None = None, dec: int = 0) -> None:
    """One-channel chord using the MOD arpeggio effect (0xy)."""
    a, b = intervals
    mod.note(ch, row, inst, root, cmd=0x0, param=((a & 0xF) << 4) | (b & 0xF))
    if vol is not None:
        # volume has to be set one row earlier because 0xy owns the effect slot
        c = mod.cells[row - 1][ch] if row > 0 else None
        if c is not None and c[2] == 0 and c[3] == 0:
            c[2], c[3] = 0xC, max(0, min(64, vol))
    for r in range(row + 1, min(row + length, mod.nrows)):
        c = mod.cells[r][ch]
        if c[0] == 0 and c[1] == 0 and c[2] == 0:
            c[2], c[3] = (0xA, dec) if dec else (0x0, ((a & 0xF) << 4) | (b & 0xF))
    if row + length < mod.nrows:
        c = mod.cells[row + length][ch]
        if c[0] == 0 and c[1] == 0 and c[2] in (0, 0xA, 0x0):
            c[2], c[3] = 0xC, 0


def fade(mod: Module, ch: int, row0: int, row1: int, v0: int, v1: int,
         every: int = 2, curve: float = 1.8) -> None:
    """Smooth Cxx volume ramp - finer than A0x, which can only move 5 units per row."""
    span = max(1, row1 - row0)
    for r in range(row0, min(row1 + 1, mod.nrows), every):
        k = (r - row0) / span
        v = v1 + (v0 - v1) * ((1.0 - k) ** curve)
        c = mod.cells[r][ch]
        if c[0] == 0 and c[1] == 0 and c[2] in (0, 0xA, 0xC):
            c[2], c[3] = 0xC, max(0, min(64, int(round(v))))


def normalize_module(mod: Module, target: float = MIX_TARGET_PEAK,
                     rms_target: float = MIX_TARGET_RMS,
                     sr: int = GBA_MIX_RATE) -> tuple[np.ndarray, MixStats, float]:
    """Scale every volume so the mix peaks at `target` and lands near `rms_target`.

    Peak normalization alone would leave sparse pieces much quieter than dense ones,
    so the gain is whichever of the two limits binds first."""
    x = ModRenderer(mod, sr).render()
    peak = float(np.max(np.abs(x))) if x.size else 0.0
    rms = float(np.sqrt(np.mean(x * x))) if x.size else 0.0
    gain = 1.0 if peak <= 1e-6 else min(target / peak, rms_target / max(rms, 1e-6))
    if abs(gain - 1.0) > 0.015:
        for inst in mod.instruments:
            inst.volume = max(1, min(64, int(round(inst.volume * gain))))
        for row in mod.cells:
            for c in row:
                if c[2] == 0xC and c[3] > 0:
                    c[3] = max(1, min(64, int(round(c[3] * gain))))
                elif c[2] == 0xA:
                    up, down = c[3] >> 4, c[3] & 0xF
                    up = min(15, max(1, int(round(up * gain)))) if up else 0
                    down = min(15, max(1, int(round(down * gain)))) if down else 0
                    c[3] = (up << 4) | down
        x = ModRenderer(mod, sr).render()
    return x, analyse(x, sr), gain


# --------------------------------------------------------------------------------------
# the songs
# --------------------------------------------------------------------------------------

def song_title() -> Module:
    """Calm koto piece, D hirajoshi (D E F A Bb).  ~80 s loop."""
    b = make_bank()
    koto, bassp, bell, pad = b['koto'], b['bassp'], b['bell'], b['sine']
    koto.volume, bassp.volume, bell.volume, pad.volume = 44, 40, 26, 16
    m = Module('music_title', 32 * BAR, speed=6, tempo=96, title='MOKU - sunlit goban')

    D4, E4, F4, A4, Bb4 = N['D4'], N['E4'], N['F4'], N['A4'], N['A#4']
    D5, E5, F5 = N['D5'], N['E5'], N['F5']
    A3, Bb3, F3 = N['A3'], N['A#3'], N['F3']

    pA1 = [(0.0, D4, 1.5), (1.5, F4, 0.5), (2.0, E4, 1.0), (3.0, D4, 1.0),
           (4.0, A3, 2.0),
           (8.0, F4, 1.0), (9.0, A4, 1.0), (10.0, Bb4, 0.5), (10.5, A4, 0.5), (11.0, F4, 1.0),
           (12.0, E4, 2.0), (14.0, D4, 2.0)]
    pA2 = [(0.0, D4, 1.5), (1.5, F4, 0.5), (2.0, E4, 1.0), (3.0, D4, 1.0),
           (4.0, A3, 1.5), (5.5, Bb3, 0.5), (6.0, A3, 1.5),
           (8.0, A4, 1.0), (9.0, D5, 1.0), (10.0, E5, 0.5), (10.5, D5, 0.5), (11.0, Bb4, 1.0),
           (12.0, A4, 3.0)]
    pB = [(0.0, D5, 1.0), (1.0, E5, 1.0), (2.0, F5, 2.0),
          (4.0, E5, 1.0), (5.0, D5, 1.0), (6.0, Bb4, 1.0), (7.0, A4, 1.0),
          (8.0, F4, 1.0), (9.0, A4, 1.0), (10.0, D5, 2.0),
          (12.0, Bb4, 2.0), (14.0, A4, 2.0)]
    pC = [(0.0, F4, 2.0), (2.0, E4, 2.0),
          (4.0, D4, 3.5),
          (8.0, A3, 2.0), (10.0, Bb3, 1.0), (11.0, A3, 1.0),
          (12.0, D4, 4.0)]
    pD = [(0.0, A4, 1.0), (1.0, Bb4, 1.0), (2.0, A4, 1.0), (3.0, F4, 1.0),
          (4.0, E4, 2.0), (6.0, D4, 2.0),
          (8.0, D4, 0.5), (8.5, E4, 0.5), (9.0, F4, 1.0), (10.0, A4, 2.0),
          (12.0, F4, 1.0), (13.0, E4, 1.0), (14.0, D4, 2.0)]

    plan = [(0, pA1), (4, pA2), (8, pB), (12, pC), (16, pA1), (20, pD), (24, pB), (28, pC)]
    for bar0, notes in plan:
        start = bar0 * BAR
        phrase(m, 0, koto, start, notes, dec=1)
        echo(m, 1, koto, start, notes, delay=2, vol=13, dec=2)

    # bass: one long note per bar, following the mode
    roots = {0: ['D2', 'D2', 'F2', 'A1'], 4: ['D2', 'D2', 'A1', 'A1'],
             8: ['A#1', 'F2', 'D2', 'A1'], 12: ['D2', 'A1', 'A#1', 'D2'],
             16: ['D2', 'D2', 'F2', 'A1'], 20: ['F2', 'A1', 'D2', 'D2'],
             24: ['A#1', 'F2', 'D2', 'A1'], 28: ['D2', 'A1', 'A#1', 'D2']}
    for bar0, names in roots.items():
        for i, nm in enumerate(names):
            m.play(2, (bar0 + i) * BAR, bassp, N[nm], BAR, dec=1, cut=False, dec_start=7)

    # bell at the head of each section, quiet sine pad through the B sections
    for bar0 in (0, 8, 16, 24):
        m.play(3, bar0 * BAR, bell, N['D4'] if bar0 % 16 == 0 else N['A4'], BAR, dec=1, cut=False)
    for bar0 in (8, 24):
        m.play(3, (bar0 + 2) * BAR, pad, N['A4'], BAR * 2, dec=1, cut=True,
               cmd=0x4, param=0x18)
    return m


def song_map() -> Module:
    """Gentle travelling theme, G major pentatonic.  ~86 s loop."""
    b = make_bank()
    lead, harp, bass, shk, rim = b['flute'], b['harp'], b['bass2'], b['shaker'], b['rim']
    lead.volume, harp.volume, bass.volume, shk.volume, rim.volume = 38, 30, 42, 22, 24
    m = Module('music_map', 40 * BAR, speed=6, tempo=112, title='MOKU - the long road')

    G3, A3, B3, D4, E4 = N['G3'], N['A3'], N['B3'], N['D4'], N['E4']
    G4, A4, B4, D5, E5, G5 = N['G4'], N['A4'], N['B4'], N['D5'], N['E5'], N['G5']

    mel_A = [(0.0, D4, 1.0), (1.0, G4, 1.0), (2.0, A4, 1.5), (3.5, G4, 0.5),
             (4.0, E4, 2.0), (6.0, D4, 2.0),
             (8.0, E4, 1.0), (9.0, G4, 1.0), (10.0, B4, 1.5), (11.5, A4, 0.5),
             (12.0, G4, 3.0),
             (16.0, D4, 1.0), (17.0, G4, 1.0), (18.0, A4, 1.5), (19.5, B4, 0.5),
             (20.0, D5, 2.0), (22.0, B4, 2.0),
             (24.0, A4, 1.0), (25.0, G4, 1.0), (26.0, E4, 2.0),
             (28.0, D4, 4.0)]
    mel_B = [(0.0, B4, 1.0), (1.0, D5, 1.0), (2.0, E5, 2.0),
             (4.0, D5, 1.0), (5.0, B4, 1.0), (6.0, A4, 2.0),
             (8.0, G4, 1.0), (9.0, A4, 1.0), (10.0, B4, 1.0), (11.0, D5, 1.0),
             (12.0, E5, 2.0), (14.0, D5, 2.0),
             (16.0, G5, 1.5), (17.5, E5, 0.5), (18.0, D5, 2.0),
             (20.0, B4, 1.0), (21.0, A4, 1.0), (22.0, G4, 2.0),
             (24.0, E4, 1.0), (25.0, G4, 1.0), (26.0, A4, 1.0), (27.0, B4, 1.0),
             (28.0, G4, 4.0)]
    mel_C = [(0.0, G4, 0.5), (0.5, A4, 0.5), (1.0, B4, 1.0), (2.0, D5, 1.0), (3.0, B4, 1.0),
             (4.0, A4, 2.0), (6.0, G4, 2.0),
             (8.0, E4, 1.0), (9.0, G4, 1.0), (10.0, A4, 1.0), (11.0, G4, 1.0),
             (12.0, E4, 2.0), (14.0, D4, 2.0),
             (16.0, D4, 0.5), (16.5, E4, 0.5), (17.0, G4, 1.0), (18.0, A4, 2.0),
             (20.0, B4, 1.5), (21.5, A4, 0.5), (22.0, G4, 2.0),
             (24.0, A4, 1.0), (25.0, B4, 1.0), (26.0, D5, 2.0),
             (28.0, G4, 4.0)]

    phrase(m, 0, lead, 8 * BAR, mel_A, dec=0, cut=True)
    phrase(m, 0, lead, 16 * BAR, mel_B, dec=0, cut=True)
    phrase(m, 0, lead, 24 * BAR, mel_C, dec=0, cut=True)
    phrase(m, 0, lead, 32 * BAR, mel_A, dec=0, cut=True)

    # harp arpeggio through the whole piece, following the bass roots
    prog = ['G', 'G', 'C', 'C', 'G', 'G', 'D', 'D',
            'G', 'G', 'C', 'C', 'G', 'Em', 'D', 'D',
            'C', 'C', 'G', 'G', 'Em', 'Em', 'D', 'D',
            'G', 'G', 'C', 'C', 'G', 'Em', 'D', 'D',
            'G', 'G', 'C', 'C', 'G', 'Em', 'D', 'G']
    voicing = {'G': [N['G3'], N['B3'], N['D4'], N['B3']],
               'C': [N['C4'], N['E4'], N['G4'], N['E4']],
               'D': [N['D4'], N['F#4'], N['A4'], N['F#4']],
               'Em': [N['E4'], N['G4'], N['B4'], N['G4']]}
    roots = {'G': N['G1'], 'C': N['C2'], 'D': N['D2'], 'Em': N['E1']}
    for bar_i, name in enumerate(prog):
        start = bar_i * BAR
        v = voicing[name]
        sparse = bar_i < 4 or 24 <= bar_i < 28          # intro and the quiet section
        steps = (0, 2, 4, 6) if sparse else (0, 2, 4, 6, 8, 10, 12, 14)
        for si, step in enumerate(steps):
            up = 12 if (not sparse and si >= 4 and bar_i % 2) else 0
            m.play(1, start + step, harp, v[si % 4] + up, 2, dec=3, cut=False)
        r = roots[name]
        m.play(2, start, bass, r, 8, dec=2, cut=True)
        m.play(2, start + 8, bass, r + 7, 6, dec=2, cut=True)

    # light percussion: shaker on the off-beats, rim on 2 and 4, resting in section C
    for bar_i in range(40):
        start = bar_i * BAR
        quiet = 24 <= bar_i < 28
        if bar_i >= 4 and not quiet:
            hits(m, 3, shk, start, '--o- --x- --o- --x-')
        if bar_i >= 8 and not quiet:
            for r in (4, 12):
                c = m.cells[start + r][3]
                if c[0] == 0:
                    m.note(3, start + r, rim, vol=rim.volume)
    return m


def song_play_classic() -> Module:
    """Sparse, meditative board music - A kumoi (A B C E F).  ~76 s loop."""
    b = make_bank()
    koto, bell, drone, wind = b['koto'], b['bell'], b['drone'], b['wind']
    koto.volume, bell.volume, drone.volume, wind.volume = 40, 22, 26, 10
    m = Module('music_play_classic', 24 * BAR, speed=6, tempo=76,
               title='MOKU - stone and shadow')

    A3, B3, C4, E4, F4 = N['A3'], N['B3'], N['C4'], N['E4'], N['F4']
    A4, B4, C5, E5 = N['A4'], N['B4'], N['C5'], N['E5']

    phrases = [
        [(0.0, A4, 2.0), (4.0, E4, 1.5), (8.0, F4, 2.0), (12.0, E4, 3.0)],
        [(0.0, C5, 1.5), (2.0, B4, 1.0), (5.0, A4, 3.0), (10.0, E4, 2.0), (14.0, C4, 2.0)],
        [(0.0, E4, 2.0), (4.0, F4, 1.0), (5.0, E4, 1.0), (8.0, C4, 2.0), (13.0, B3, 2.0)],
        [(0.0, A3, 3.0), (6.0, C4, 1.5), (8.0, E4, 2.0), (12.0, A4, 4.0)],
        [(0.0, E5, 2.0), (4.0, C5, 1.5), (7.0, B4, 1.0), (9.0, A4, 3.0), (14.0, F4, 2.0)],
        [(0.0, E4, 2.0), (4.0, C4, 2.0), (9.0, A3, 3.0), (14.0, E4, 2.0)],
    ]
    for i, notes in enumerate(phrases):
        start = i * 4 * BAR
        phrase(m, 0, koto, start, notes, dec=1)
        echo(m, 1, koto, start, notes, delay=3, vol=11, dec=2)

    for barno, name in ((0, 'A4'), (8, 'E5'), (16, 'A4'), (20, 'C5')):
        r = barno * BAR
        if m.cells[r][1][0] == 0:
            m.play(1, r, bell, N[name], BAR * 2, dec=1, cut=False)

    # breathing drone: explicit volume steps (A0x is too coarse for a slow swell)
    for start_bar, name in ((0, 'A1'), (8, 'E2'), (12, 'F1'), (16, 'A1')):
        start = start_bar * BAR
        m.play(2, start, drone, N[name], BAR * 4, vol=6, cut=False)
        for k, v in enumerate((10, 16, 22, 26, 24, 18, 12, 8)):
            m.setvol(2, start + 6 + k * 7, v)

    # air bed: one looped noise sample shaped with slow volume steps
    m.note(3, 0, wind, vol=4)
    swell = [3, 6, 9, 12, 10, 7, 5, 8, 11, 14, 12, 9, 6, 4, 6, 9,
             12, 15, 13, 10, 7, 5, 4, 6, 9, 12, 14, 11, 8, 6, 4, 3]
    for k, v in enumerate(swell):
        row = 4 + k * 12
        if row < m.nrows:
            m.setvol(3, row, v)
    return m


def song_play_pup() -> Module:
    """Bouncy major-key board music for the PUP skin.  ~87 s loop."""
    b = make_bank()
    lead, keys, bass = b['pulse25'], b['pulse50'], b['bass2']
    kick, snare, hat = b['kick'], b['snare'], b['hat']
    lead.volume, keys.volume, bass.volume = 34, 22, 40
    kick.volume, snare.volume, hat.volume = 44, 36, 22
    m = Module('music_play_pup', 48 * BAR, speed=6, tempo=132, title='MOKU - paws on the board')

    C4, D4, E4, F4, G4, A4, B4 = (N['C4'], N['D4'], N['E4'], N['F4'], N['G4'], N['A4'], N['B4'])
    C5, D5, E5 = N['C5'], N['D5'], N['E5']

    melA = [(0.0, G4, 0.5), (0.5, A4, 0.5), (1.0, C5, 1.0), (2.0, G4, 1.0), (3.0, E4, 1.0),
            (4.0, G4, 2.0), (6.0, E4, 1.0), (7.0, D4, 1.0),
            (8.0, C4, 0.5), (8.5, E4, 0.5), (9.0, G4, 1.0), (10.0, A4, 1.0), (11.0, G4, 1.0),
            (12.0, E4, 2.0), (14.0, D4, 2.0),
            (16.0, E4, 0.5), (16.5, F4, 0.5), (17.0, G4, 1.0), (18.0, C5, 1.0), (19.0, B4, 1.0),
            (20.0, A4, 2.0), (22.0, G4, 2.0),
            (24.0, F4, 1.0), (25.0, E4, 1.0), (26.0, D4, 1.0), (27.0, E4, 1.0),
            (28.0, C4, 3.0)]
    melB = [(0.0, A4, 1.0), (1.0, C5, 1.0), (2.0, B4, 1.0), (3.0, A4, 1.0),
            (4.0, G4, 2.0), (6.0, E4, 2.0),
            (8.0, F4, 1.0), (9.0, A4, 1.0), (10.0, C5, 2.0),
            (12.0, B4, 2.0), (14.0, G4, 2.0),
            (16.0, C5, 0.5), (16.5, B4, 0.5), (17.0, A4, 1.0), (18.0, G4, 1.0), (19.0, E4, 1.0),
            (20.0, D4, 2.0), (22.0, G4, 2.0),
            (24.0, E4, 1.0), (25.0, G4, 1.0), (26.0, A4, 1.0), (27.0, C5, 1.0),
            (28.0, G4, 3.0)]
    melC = [(0.0, C5, 0.5), (0.5, C5, 0.5), (1.0, A4, 1.0), (2.0, G4, 1.0),
            (4.0, D5, 0.5), (4.5, D5, 0.5), (5.0, B4, 1.0), (6.0, A4, 1.0),
            (8.0, E5, 0.5), (8.5, D5, 0.5), (9.0, C5, 1.0), (10.0, A4, 1.0), (11.0, G4, 1.0),
            (12.0, E4, 2.0),
            (16.0, G4, 0.5), (16.5, A4, 0.5), (17.0, B4, 1.0), (18.0, C5, 1.0), (19.0, D5, 1.0),
            (20.0, E5, 2.0), (22.0, C5, 2.0),
            (24.0, D5, 1.0), (25.0, C5, 1.0), (26.0, B4, 1.0), (27.0, A4, 1.0),
            (28.0, G4, 3.0)]

    for start_bar, mel in ((4, melA), (12, melB), (24, melA), (32, melC)):
        phrase(m, 0, lead, start_bar * BAR, mel, dec=0, cut=True)

    prog = ['C', 'C', 'F', 'G', 'C', 'Am', 'F', 'G']
    triad = {'C': (N['C4'], (4, 7)), 'F': (N['F3'], (4, 7)), 'G': (N['G3'], (4, 7)),
             'Am': (N['A3'], (3, 7)), 'Dm': (N['D4'], (3, 7))}
    root = {'C': N['C2'], 'F': N['F1'], 'G': N['G1'], 'Am': N['A1'], 'Dm': N['D2']}
    for bar_i in range(48):
        start = bar_i * BAR
        name = prog[bar_i % 8]
        note_root, iv = triad[name]
        if bar_i >= 4 and not (20 <= bar_i < 24):
            for beat in (1, 3):                       # off-beat stabs
                chord(m, 1, keys, start + beat * Q, note_root, iv, 3, dec=6)
        r = root[name]
        if bar_i >= 2:
            steps = [(0, r), (2, r + 12), (4, r + 7), (6, r + 12),
                     (8, r), (10, r + 12), (12, r + 7), (14, r + 12)]
            for row, mnote in steps:
                m.play(2, start + row, bass, mnote, 2, dec=4, cut=True)
        else:
            m.play(2, start, bass, r, 8, dec=2, cut=True)
        fill = (bar_i % 8) == 7 and bar_i >= 8
        hits(m, 3, kick, start, 'x--- ---- x--- ----' if not fill else 'x--- ---- ---- ----')
        hits(m, 3, snare, start, '---- x--- ---- x---' if not fill else
             '---- x--- x-x- xoxo')
        hits(m, 3, hat, start, '--o- --o- --o- --o-' if not fill else '--o- --o- ---- ----')
    return m


def song_play_dino() -> Module:
    """Tribal drums + E minor pentatonic riff for the DINO skin.  ~74 s loop."""
    b = make_bank()
    tomlo, tom, kick = b['tomlo'], b['tom'], b['kick']
    shaker, wood, bass, lead = b['shaker'], b['wood'], b['bass2'], b['flute']
    tomlo.volume, tom.volume, kick.volume = 44, 38, 46
    shaker.volume, wood.volume, bass.volume, lead.volume = 22, 30, 42, 34
    m = Module('music_play_dino', 32 * BAR, speed=6, tempo=104, title='MOKU - bone valley')

    E2, G2, A2, B2, D3, E3 = N['E2'], N['G2'], N['A2'], N['B2'], N['D3'], N['E3']
    E4, G4, A4, B4, D5, E5 = N['E4'], N['G4'], N['A4'], N['B4'], N['D5'], N['E5']

    riffs = [
        [(0.0, E2, 0.5), (0.5, E2, 0.5), (1.0, G2, 0.5), (1.5, E2, 0.5),
         (2.0, A2, 0.5), (2.5, E2, 0.5), (3.0, B2, 1.0)],
        [(0.0, E2, 0.5), (0.5, E2, 0.5), (1.0, D3, 0.5), (1.5, B2, 0.5),
         (2.0, A2, 1.0), (3.0, G2, 0.5), (3.5, E2, 0.5)],
        [(0.0, G2, 0.5), (0.5, G2, 0.5), (1.0, A2, 0.5), (1.5, B2, 0.5),
         (2.0, D3, 1.0), (3.0, B2, 0.5), (3.5, A2, 0.5)],
        [(0.0, E2, 1.0), (1.0, E3, 0.5), (1.5, D3, 0.5), (2.0, B2, 0.5), (2.5, A2, 0.5),
         (3.0, G2, 0.5), (3.5, E2, 0.5)],
    ]
    calls = [
        [(0.0, E4, 1.0), (1.0, G4, 1.0), (2.0, B4, 2.0)],
        [(0.0, D5, 1.0), (1.0, B4, 1.0), (2.0, A4, 1.0), (3.0, G4, 1.0)],
        [(0.0, B4, 0.5), (0.5, A4, 0.5), (1.0, G4, 1.0), (2.0, E4, 2.0)],
        [(0.0, E5, 1.5), (1.5, D5, 0.5), (2.0, B4, 2.0)],
        [(0.0, G4, 1.0), (1.0, A4, 1.0), (2.0, B4, 1.0), (3.0, D5, 1.0)],
        [(0.0, E5, 2.0), (2.0, B4, 1.0), (3.0, G4, 1.0)],
    ]

    call_bars = [4, 6, 12, 14, 20, 22, 26, 28]
    for i, bar_i in enumerate(call_bars):
        phrase(m, 3, lead, bar_i * BAR, calls[i % len(calls)], dec=2, cut=True,
               vol=lead.volume)
        for row in range(bar_i * BAR + 2, bar_i * BAR + 6):
            c = m.cells[row][3]
            if c[0] == 0 and c[2] == 0:
                c[2], c[3] = 0x4, 0x27
                break

    for bar_i in range(32):
        start = bar_i * BAR
        heavy = bar_i % 4 == 3
        if bar_i >= 2:
            phrase(m, 2, bass, start, riffs[(bar_i // 2) % len(riffs)], dec=5, cut=True)
        hits(m, 0, kick, start, 'X--- ---- --X- ----')
        hits(m, 0, tomlo, start, '---- x--- ---- x-x-' if heavy else '---- x--- ---- x---')
        hits(m, 0, tom, start, '--x- --x- x--- ---x' if bar_i >= 8 else '--x- ---- x--- ----')
        if bar_i >= 4:
            hits(m, 1, shaker, start, 'x-o- x-o- x-o- x-o-')
        if bar_i >= 8 and bar_i % 2 == 1:
            hits(m, 1, wood, start, '---x ---- ---x ----')
    return m


def song_clear() -> Module:
    """Six second victory fanfare (non-looping)."""
    b = make_bank()
    lead, keys, bass = b['pulse25'], b['pulse50'], b['bass']
    snare, crash, kick = b['snare'], b['crash'], b['kick']
    lead.volume, keys.volume, bass.volume = 40, 24, 44
    snare.volume, crash.volume, kick.volume = 34, 34, 44
    m = Module('music_clear', BAR * 4, speed=6, tempo=150, title='MOKU - rank fanfare')

    G4, C5, D5, E5, F5, G5, A5 = (N['G4'], N['C5'], N['D5'], N['E5'], N['F5'], N['G5'], N['A5'])
    mel = [(0.0, G4, 0.5), (0.5, C5, 0.5), (1.0, E5, 0.5), (1.5, G5, 2.5),
           (4.0, F5, 0.5), (4.5, E5, 0.5), (5.0, D5, 0.5), (5.5, E5, 1.5), (7.0, G5, 1.0),
           (8.0, A5, 1.0), (9.0, G5, 1.0), (10.0, E5, 1.0), (11.0, C5, 1.0),
           (12.0, G5, 4.0)]
    phrase(m, 0, lead, 0, mel, cut=True)
    m.effect(0, 12 * Q + 2, 0x4, 0x28)
    fade(m, 0, 52, 63, lead.volume, 0, every=1, curve=2.4)   # ring the last note out

    chord(m, 1, keys, 0, N['C4'], (4, 7), 15, dec=2)
    chord(m, 1, keys, 16, N['G3'], (4, 7), 15, dec=2)
    chord(m, 1, keys, 32, N['F3'], (4, 7), 7, dec=2)
    chord(m, 1, keys, 40, N['G3'], (4, 7), 7, dec=2)
    chord(m, 1, keys, 48, N['C4'], (4, 7), 16, dec=1)

    for row, name in ((0, 'C2'), (16, 'G1'), (32, 'F1'), (40, 'G1'), (48, 'C2')):
        m.play(2, row, bass, N[name], 8, dec=2, cut=True)
    m.play(2, 56, bass, N['C1'], 8, dec=1, cut=False)

    hits(m, 3, kick, 0, 'x--- ---- x--- ----')
    hits(m, 3, snare, 16, '---- x--- ---- x---')
    hits(m, 3, snare, 32, 'x-x- x-x- xxxx xxxx')
    m.note(3, 48, crash, vol=crash.volume)
    return m


def song_lose() -> Module:
    """Short sad sting (non-looping)."""
    b = make_bank()
    koto, pad, bass, drum = b['koto'], b['sine'], b['bass'], b['tomlo']
    koto.volume, pad.volume, bass.volume, drum.volume = 40, 16, 40, 34
    m = Module('music_lose', BAR * 4, speed=6, tempo=160, title='MOKU - fading light')

    # A4 - G4 - F4 - E4, the last note sagging out of tune as it fades
    for row, name, tail in ((0, 'A4', 7), (8, 'G4', 7), (16, 'F4', 11)):
        m.note(0, row, koto, N[name])
        fade(m, 0, row + 1, row + tail, 34, 6, every=2)
    m.note(0, 28, koto, N['E4'])
    fade(m, 0, 29, 62, 40, 0, every=2)
    m.effect(0, 46, 0x2, 0x06, force=True)
    m.effect(0, 48, 0x2, 0x0A, force=True)
    m.effect(0, 50, 0x2, 0x0E, force=True)

    for row, name, ln in ((0, 'C4', 8), (8, 'A#3', 8), (16, 'A3', 12), (28, 'E3', 30)):
        m.note(1, row, pad, N[name], vol=pad.volume)
        fade(m, 1, row + 2, row + ln, pad.volume, 0, every=3)
    for row, name in ((0, 'A1'), (16, 'F1'), (28, 'E1')):
        m.note(2, row, bass, N[name], vol=bass.volume)
        fade(m, 2, row + 6, row + (14 if row < 28 else 32), bass.volume, 0, every=3)
    m.note(3, 0, drum, vol=drum.volume)
    m.note(3, 28, drum, vol=drum.volume - 8)
    return m


SONGS = {
    'music_title': song_title,
    'music_map': song_map,
    'music_play_classic': song_play_classic,
    'music_play_pup': song_play_pup,
    'music_play_dino': song_play_dino,
    'music_clear': song_clear,
    'music_lose': song_lose,
}


# --------------------------------------------------------------------------------------
# sound effects
# --------------------------------------------------------------------------------------

SFX_RATE = 16000


def _phase(f, sr: float, n: int) -> np.ndarray:
    f = np.full(n, float(f)) if np.isscalar(f) else np.asarray(f, dtype=float)
    return 2.0 * math.pi * np.cumsum(f) / sr


def bl_tone(f, sr: float, n: int, kind: str = 'saw', duty: float = 0.5,
            nharm: int = 16) -> np.ndarray:
    """Band-limited oscillator; harmonics above 0.45*sr are dropped, not aliased."""
    ph = _phase(f, sr, n)
    fmax = float(np.max(f)) if not np.isscalar(f) else float(f)
    amps = harmonic_amps(kind, nharm, duty)
    y = np.zeros(n)
    for k in range(1, nharm + 1):
        if amps[k - 1] == 0.0:
            continue
        if k * fmax > 0.45 * sr:
            break
        y += amps[k - 1] * np.sin(k * ph)
    return y


def finish(y: np.ndarray, sr: float, peak: float, fout: float = 0.004):
    """DC-block, guarantee silent edges, then set the balanced peak level."""
    return normalize(fade_edges(dc_block(y), sr, fout=fout), peak), sr


def sfx_stone(sr=SFX_RATE):
    """Wooden click of a stone landing on the goban."""
    n = int(0.080 * sr)
    t = np.arange(n) / sr
    y = np.sin(2 * math.pi * 780.0 * t) * env_exp(n, sr, 0.0004, 0.018) * 0.9
    y += np.sin(2 * math.pi * 1950.0 * t) * env_exp(n, sr, 0.0003, 0.010) * 0.45
    y += np.sin(2 * math.pi * 2960.0 * t) * env_exp(n, sr, 0.0002, 0.005) * 0.20
    y += np.sin(2 * math.pi * 176.0 * t) * env_exp(n, sr, 0.0008, 0.026) * 0.55
    y += bandpass(white(n, 101), 3100.0, sr, q=1.1) * env_exp(n, sr, 0.0002, 0.0035) * 0.8
    return finish(soft_clip(y), sr, 0.6)


def sfx_capture(sr=SFX_RATE):
    """Bubbly pop when a group is lifted off the board."""
    n = int(0.110 * sr)
    y = sweep(n, sr, 300.0, 1150.0, curve=0.55) * env_exp(n, sr, 0.0012, 0.024)
    y += sweep(n, sr, 600.0, 2300.0, curve=0.55) * env_exp(n, sr, 0.0008, 0.010) * 0.35
    y += bandpass(white(n, 102), 2200.0, sr, q=1.4) * env_exp(n, sr, 0.0003, 0.004) * 0.45
    tail = np.sin(2 * math.pi * 1150.0 * np.arange(n) / sr) * env_exp(n, sr, 0.030, 0.030) * 0.25
    return finish(soft_clip(y + tail), sr, 0.68)


def sfx_atari(sr=SFX_RATE):
    """Two-note alert: something is in danger."""
    n1, gap = int(0.055 * sr), int(0.018 * sr)
    n2 = int(0.085 * sr)
    a = bl_tone(1046.5, sr, n1, 'pulse', duty=0.35) * env_ad(n1, sr, 0.002, 0.030, 0.023)
    b = bl_tone(1568.0, sr, n2, 'pulse', duty=0.35) * env_ad(n2, sr, 0.002, 0.040, 0.043)
    y = np.concatenate([a, np.zeros(gap), b])
    return finish(y, sr, 0.64)


def sfx_menu_move(sr=SFX_RATE):
    n = int(0.038 * sr)
    y = bl_tone(880.0, sr, n, 'pulse', duty=0.25) * env_exp(n, sr, 0.0008, 0.011)
    return finish(y, sr, 0.34)


def sfx_menu_ok(sr=SFX_RATE):
    n1 = int(0.055 * sr)
    n2 = int(0.100 * sr)
    a = bl_tone(659.3, sr, n1, 'pulse', duty=0.3) * env_ad(n1, sr, 0.001, 0.035, 0.019)
    b = bl_tone(987.8, sr, n2, 'pulse', duty=0.3) * env_exp(n2, sr, 0.001, 0.035)
    return finish(np.concatenate([a, b]), sr, 0.52)


def sfx_menu_back(sr=SFX_RATE):
    n1 = int(0.055 * sr)
    n2 = int(0.090 * sr)
    a = bl_tone(493.9, sr, n1, 'triangle') * env_ad(n1, sr, 0.001, 0.035, 0.019)
    b = bl_tone(329.6, sr, n2, 'triangle') * env_exp(n2, sr, 0.001, 0.032)
    return finish(np.concatenate([a, b]), sr, 0.36)


def sfx_type(sr=SFX_RATE):
    """Typewriter tick for the dialogue box."""
    n = int(0.026 * sr)
    t = np.arange(n) / sr
    y = bandpass(white(n, 103), 2700.0, sr, q=1.0) * env_exp(n, sr, 0.0002, 0.0032)
    y += np.sin(2 * math.pi * 3400.0 * t) * env_exp(n, sr, 0.0002, 0.0022) * 0.5
    y += np.sin(2 * math.pi * 1250.0 * t) * env_exp(n, sr, 0.0003, 0.0035) * 0.35
    return finish(y, sr, 0.3, fout=0.003)


def sfx_bark(sr=SFX_RATE):
    """PUP helper: a short friendly bark."""
    n = int(0.200 * sr)
    t = np.arange(n) / sr
    f = 250.0 * np.exp(-t * 3.2) + 165.0
    src = bl_tone(f, sr, n, 'saw', nharm=24) * 0.8 + white(n, 104) * 0.25
    op = np.clip(1.0 - t / 0.13, 0.0, 1.0)              # mouth opening -> closing
    y = (bandpass(src, 640.0, sr, q=2.2) * (0.55 + 0.45 * op)
         + bandpass(src, 1320.0, sr, q=2.6) * (0.85 - 0.35 * op)
         + bandpass(src, 2550.0, sr, q=3.0) * 0.30)
    env = env_ad(n, sr, 0.006, 0.055, 0.135) * (1.0 - 0.25 * np.sin(2 * math.pi * 22.0 * t))
    return finish(soft_clip(y * env * 1.3), sr, 0.68)


def sfx_roar(sr=SFX_RATE):
    """DINO helper: a rumbling roar."""
    n = int(0.850 * sr)
    t = np.arange(n) / sr
    f = 92.0 * np.exp(-t * 0.9) + 58.0 + 3.0 * np.sin(2 * math.pi * 5.5 * t)
    src = bl_tone(f, sr, n, 'saw', nharm=28)
    src += white(n, 105) * 0.30
    growl = 0.72 + 0.28 * np.sin(2 * math.pi * 27.0 * t)
    y = (bandpass(src, 230.0, sr, q=1.6) * 1.00
         + bandpass(src, 760.0, sr, q=2.2) * 0.55
         + bandpass(src, 1850.0, sr, q=2.6) * 0.22
         + bandpass(white(n, 106), 1250.0, sr, q=0.8) * 0.18)
    env = env_ad(n, sr, 0.090, 0.430, 0.330)
    return finish(soft_clip(y * env * growl * 1.4), sr, 0.74)


def sfx_bite(sr=SFX_RATE):
    """DINO capture: a snapping chomp."""
    n = int(0.170 * sr)
    t = np.arange(n) / sr
    y = np.zeros(n)
    for off, amp, f in ((0.0, 1.0, 2600.0), (0.052, 0.75, 2100.0)):
        s = int(off * sr)
        m = n - s
        snap = bandpass(white(m, 107 + s), f, sr, q=1.1) * env_exp(m, sr, 0.0004, 0.006)
        y[s:] += snap * amp
    y += sweep(n, sr, 420.0, 120.0, curve=0.5) * env_exp(n, sr, 0.001, 0.045) * 0.65
    y += np.sin(2 * math.pi * 88.0 * t) * env_exp(n, sr, 0.002, 0.055) * 0.45
    return finish(soft_clip(y), sr, 0.7)


def sfx_cheer(sr=SFX_RATE):
    """Small crowd cheering on a mission clear."""
    n = int(1.100 * sr)
    t = np.arange(n) / sr
    g = rng(108)
    y = bandpass(white(n, 109), 1150.0, sr, q=0.5) * 0.9
    y += bandpass(white(n, 110), 2600.0, sr, q=0.7) * 0.45
    wob = 1.0 + 0.35 * np.sin(2 * math.pi * 3.1 * t + 0.4) + 0.2 * np.sin(2 * math.pi * 5.7 * t)
    y *= wob
    for k in range(6):                                   # a few voices on top
        f = float(g.choice([392.0, 440.0, 523.3, 587.3, 659.3, 784.0]))
        vib = 1.0 + 0.02 * np.sin(2 * math.pi * float(g.uniform(4.0, 7.0)) * t + k)
        start = float(g.uniform(0.0, 0.25))
        e = env_ad(n, sr, 0.12 + start, 0.45, 0.45) * float(g.uniform(0.10, 0.20))
        y += bl_tone(f * vib, sr, n, 'triangle') * e
    env = env_ad(n, sr, 0.200, 0.400, 0.500)
    return finish(soft_clip(y * env), sr, 0.68)


def sfx_stamp(sr=SFX_RATE):
    """Rank stamp slamming onto the score card."""
    n = int(0.290 * sr)
    t = np.arange(n) / sr
    y = sweep(n, sr, 190.0, 44.0, curve=0.35) * env_exp(n, sr, 0.0008, 0.052) * 1.1
    y += bandpass(white(n, 111), 1750.0, sr, q=0.9) * env_exp(n, sr, 0.0004, 0.009) * 0.85
    y += bandpass(white(n, 112), 520.0, sr, q=1.2) * env_exp(n, sr, 0.001, 0.030) * 0.5
    y += lowpass(white(n, 113), 2200.0, sr) * env_exp(n, sr, 0.004, 0.075) * 0.14
    return finish(soft_clip(y), sr, 0.78)


def sfx_confetti(sr=SFX_RATE):
    """Paper burst plus a sparkle cascade."""
    n = int(0.780 * sr)
    y = np.zeros(n)
    y[:int(0.06 * sr)] += (bandpass(white(int(0.06 * sr), 114), 4200.0, sr, q=0.6)
                           * env_exp(int(0.06 * sr), sr, 0.001, 0.016) * 0.9)
    g = rng(115)
    pitches = [1046.5, 1318.5, 1568.0, 2093.0, 2637.0, 3136.0]
    for k in range(11):
        start = int((0.02 + 0.062 * k + float(g.uniform(0.0, 0.02))) * sr)
        if start >= n:
            break
        m = min(int(0.16 * sr), n - start)
        f = float(g.choice(pitches))
        ping = (np.sin(2 * math.pi * f * np.arange(m) / sr)
                + 0.3 * np.sin(2 * math.pi * 2 * f * np.arange(m) / sr))
        y[start:start + m] += ping * env_exp(m, sr, 0.0008, 0.030) * float(g.uniform(0.35, 0.6))
    return finish(soft_clip(y), sr, 0.56)


def sfx_error(sr=SFX_RATE):
    """Illegal move buzz - firm but not harsh."""
    n1 = int(0.085 * sr)
    gap = int(0.035 * sr)
    t1 = np.arange(n1) / sr
    f = 168.0 - 22.0 * t1 / max(t1[-1], 1e-9)
    a = bl_tone(f, sr, n1, 'square', nharm=10) * env_ad(n1, sr, 0.003, 0.055, 0.027)
    y = np.concatenate([a, np.zeros(gap), a * 0.85])
    return finish(soft_clip(y * 1.1), sr, 0.4)


def sfx_unlock(sr=SFX_RATE):
    """Something new became available."""
    n = int(0.640 * sr)
    y = np.zeros(n)
    for k, f in enumerate((523.3, 659.3, 784.0, 1046.5)):
        start = int(0.075 * k * sr)
        m = n - start
        tone = (np.sin(2 * math.pi * f * np.arange(m) / sr) * 1.0
                + 0.35 * np.sin(2 * math.pi * 2.0 * f * np.arange(m) / sr)
                + 0.12 * np.sin(2 * math.pi * 3.0 * f * np.arange(m) / sr))
        y[start:] += tone * env_exp(m, sr, 0.002, 0.105 + 0.05 * k) * (0.55 + 0.1 * k)
    t = np.arange(n) / sr
    shimmer = (np.sin(2 * math.pi * 3136.0 * t) * (0.5 + 0.5 * np.sin(2 * math.pi * 17.0 * t)))
    y += shimmer * env_ad(n, sr, 0.18, 0.10, 0.30) * 0.10
    return finish(soft_clip(y), sr, 0.62)


def sfx_pass(sr=SFX_RATE):
    """A player passes: a soft breath of air."""
    n = int(0.260 * sr)
    t = np.arange(n) / sr
    noise = white(n, 116)
    y = (bandpass(noise, 1900.0, sr, q=0.7) * np.clip(1.2 - t / 0.12, 0.0, 1.0)
         + bandpass(noise, 700.0, sr, q=0.8) * np.clip(t / 0.12, 0.0, 1.0))
    y *= env_ad(n, sr, 0.020, 0.070, 0.170)
    y += np.sin(2 * math.pi * 261.6 * t) * env_ad(n, sr, 0.030, 0.040, 0.150) * 0.22
    return finish(y, sr, 0.48)


def sfx_think(sr=SFX_RATE):
    """Quiet tick played on a loop while the AI is thinking."""
    n = int(0.042 * sr)
    t = np.arange(n) / sr
    y = np.sin(2 * math.pi * 1300.0 * t) * env_exp(n, sr, 0.0006, 0.0075)
    y += np.sin(2 * math.pi * 2100.0 * t) * env_exp(n, sr, 0.0004, 0.0035) * 0.3
    y += bandpass(white(n, 117), 1800.0, sr, q=1.5) * env_exp(n, sr, 0.0002, 0.0022) * 0.35
    return finish(y, sr, 0.26, fout=0.006)


def sfx_hint(sr=SFX_RATE):
    """Helper suggestion: a gentle bell ting."""
    n = int(0.460 * sr)
    t = np.arange(n) / sr
    y = np.sin(2 * math.pi * 1174.7 * t) * env_exp(n, sr, 0.0015, 0.105)
    y += np.sin(2 * math.pi * 1760.0 * t) * env_exp(n, sr, 0.0012, 0.070) * 0.55
    y += np.sin(2 * math.pi * 2637.0 * t) * env_exp(n, sr, 0.0008, 0.028) * 0.22
    y += np.sin(2 * math.pi * 587.3 * t) * env_exp(n, sr, 0.002, 0.130) * 0.30
    return finish(y, sr, 0.56)


SFX = {
    'sfx_stone': sfx_stone,
    'sfx_capture': sfx_capture,
    'sfx_atari': sfx_atari,
    'sfx_menu_move': sfx_menu_move,
    'sfx_menu_ok': sfx_menu_ok,
    'sfx_menu_back': sfx_menu_back,
    'sfx_type': sfx_type,
    'sfx_bark': sfx_bark,
    'sfx_roar': sfx_roar,
    'sfx_bite': sfx_bite,
    'sfx_cheer': sfx_cheer,
    'sfx_stamp': sfx_stamp,
    'sfx_confetti': sfx_confetti,
    'sfx_error': sfx_error,
    'sfx_unlock': sfx_unlock,
    'sfx_pass': sfx_pass,
    'sfx_think': sfx_think,
    'sfx_hint': sfx_hint,
}


# --------------------------------------------------------------------------------------
# build / validation entry point
# --------------------------------------------------------------------------------------

#: looping tracks must land inside this window (seconds)
LOOP_RANGE = (60.0, 120.0)

ONESHOT_RANGE = {'music_clear': (5.0, 8.0), 'music_lose': (3.0, 7.0)}


def seam_delta(mod: Module, sr: int = GBA_MIX_RATE) -> tuple[float, float]:
    """Loop-point click test.

    Restarting the module always produces the same downbeat transient as starting it
    from silence; a *click* is the extra jump you get when a voice that was still
    ringing is chopped off.  Returns (step across the loop point, step at the start),
    which are equal when the loop is clean."""
    x = ModRenderer(mod, sr).render(loops=2)
    if x.size < 8:
        return 0.0, 0.0
    seam = x.shape[0] // 2
    step = float(abs(x[seam] - x[seam - 1]))
    start = float(max(abs(x[0]), abs(x[1] - x[0]), abs(x[2] - x[1])))
    return step, start


def build(out_dir: str, render_dir: str | None = None, mmutil: str | None = None,
          verbose: bool = True) -> dict:
    os.makedirs(out_dir, exist_ok=True)
    report = {'music': [], 'sfx': [], 'errors': []}

    if verbose:
        print('sound effects')
        print('  {:<16s} {:>8s} {:>8s} {:>6s} {:>6s}'.format('item', 'ms', 'bytes', 'peak', 'rms'))
    for name, fn in SFX.items():
        x, sr = fn()
        path = os.path.join(out_dir, name + '.wav')
        size = write_wav_u8(path, x, sr)
        peak = float(np.max(np.abs(x)))
        rms = float(np.sqrt(np.mean(x * x)))
        if peak > 0.95:
            report['errors'].append(f'{name}: peak {peak:.2f} too hot')
        if x.shape[0] > sr * 1.5:
            report['errors'].append(f'{name}: {x.shape[0] / sr:.2f}s is not a short effect')
        if abs(float(x[0])) > 1e-6 or abs(float(x[-1])) > 1e-6:
            report['errors'].append(f'{name}: does not start/end at zero')
        report['sfx'].append((name, x.shape[0] / sr, size, peak, rms, sr))
        if verbose:
            print(f'  {name:<16s} {x.shape[0] / sr * 1000:8.1f} {size:8d} {peak:6.2f} {rms:6.3f}')
        if render_dir:
            write_wav_s16(os.path.join(render_dir, name + '.wav'), x, sr)

    if verbose:
        print('music')
        print('  {:<20s} {:>7s} {:>4s} {:>4s} {:>7s} {:>6s} {:>6s} {:>5s}'.format(
            'item', 's', 'pat', 'pos', 'bytes', 'peak', 'rms', 'gap'))
    for name, fn in SONGS.items():
        mod = fn()
        x, stats, gain = normalize_module(mod)
        blob = mod.encode()
        path = os.path.join(out_dir, name + '.mod')
        with open(path, 'wb') as f:
            f.write(blob)
        lo, hi = ONESHOT_RANGE.get(name, LOOP_RANGE)
        if not lo <= mod.duration() <= hi:
            report['errors'].append(f'{name}: {mod.duration():.1f}s outside {lo}-{hi}s')
        if stats.clipped:
            report['errors'].append(f'{name}: {stats.clipped} clipped samples')
        if stats.peak > 1.45:
            report['errors'].append(f'{name}: mix peak {stats.peak:.2f} leaves no head-room')
        if stats.max_gap > 1.5:
            report['errors'].append(f'{name}: {stats.max_gap:.1f}s of silence')
        if stats.rms < 0.08:
            report['errors'].append(f'{name}: too quiet (rms {stats.rms:.3f})')
        step, start = (seam_delta(mod) if name not in ONESHOT_RANGE else (0.0, 0.0))
        if step > start * 1.5 + 0.25:
            report['errors'].append(
                f'{name}: click at the loop point (step {step:.2f} vs {start:.2f} at the start)')
        report['music'].append((name, mod.duration(), mod.unique_patterns(), mod.positions(),
                                len(blob), stats.peak, stats.rms, stats.max_gap, gain,
                                step, start))
        if verbose:
            print(f'  {name:<20s} {mod.duration():7.1f} {mod.unique_patterns():4d} '
                  f'{mod.positions():4d} {len(blob):7d} {stats.peak:6.2f} {stats.rms:6.3f} '
                  f'{stats.max_gap:5.2f}')
        if render_dir:
            write_wav_s16(os.path.join(render_dir, name + '.wav'),
                          np.clip(x / 2.0, -1.0, 1.0), GBA_MIX_RATE)

    total = sum(s[2] for s in report['sfx']) + sum(m[4] for m in report['music'])
    report['total'] = total
    if total > 400 * 1024:
        report['errors'].append(f'total audio {total / 1024:.1f} KB over the 400 KB budget')
    if verbose:
        print(f'total: {total} bytes ({total / 1024:.1f} KB) in {out_dir}')

    if mmutil:
        report['soundbank'] = run_mmutil(out_dir, mmutil, verbose, report)
    return report


def run_mmutil(out_dir: str, mmutil: str, verbose: bool, report: dict) -> int:
    import subprocess
    import tempfile
    files = sorted(f for f in os.listdir(out_dir) if f.endswith(('.mod', '.wav')))
    with tempfile.TemporaryDirectory() as tmp:
        bin_path = os.path.join(tmp, 'soundbank.bin')
        hdr_path = os.path.join(tmp, 'soundbank.h')
        cmd = [mmutil] + [os.path.join(out_dir, f) for f in files] + \
              ['-o' + bin_path, '-h' + hdr_path]
        proc = subprocess.run(cmd, capture_output=True, text=True)
        if proc.returncode != 0:
            report['errors'].append(f'mmutil failed: {proc.stdout} {proc.stderr}')
            return 0
        names = set()
        with open(hdr_path) as f:
            for line in f:
                parts = line.split()
                if len(parts) >= 2 and parts[0] == '#define':
                    names.add(parts[1])
        for name in SONGS:
            if 'MOD_' + name.upper() not in names:
                report['errors'].append(f'{name}: missing from the soundbank')
        for name in SFX:
            if 'SFX_' + name.upper() not in names:
                report['errors'].append(f'{name}: missing from the soundbank')
        size = os.path.getsize(bin_path)
        if verbose:
            print(f'mmutil soundbank: {size} bytes ({size / 1024:.1f} KB), '
                  f'{len(SONGS)} modules + {len(SFX)} effects')
        if size > 400 * 1024:
            report['errors'].append(f'soundbank {size / 1024:.1f} KB over budget')
        return size


def main(argv=None) -> int:
    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ap = argparse.ArgumentParser(description='generate MOKU audio assets')
    ap.add_argument('--out', default=os.path.join(here, 'audio'))
    ap.add_argument('--render', default=None,
                    help='also write 16-bit WAV renders of every module there')
    ap.add_argument('--mmutil', default='/opt/devkitpro/tools/bin/mmutil')
    ap.add_argument('--list', action='store_true', help='print Butano item names and exit')
    ap.add_argument('--check', action='store_true',
                    help='exit 1 if the committed assets differ from a fresh generation')
    ap.add_argument('--quiet', action='store_true')
    args = ap.parse_args(argv)

    if args.list:
        for name in SONGS:
            print('bn::music_items::' + name)
        for name in SFX:
            print('bn::sound_items::' + name)
        return 0

    if args.check:
        import filecmp
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            report = build(tmp, None, None, False)
            expected = sorted(os.listdir(tmp))
            found = sorted(f for f in os.listdir(args.out) if f.endswith(('.mod', '.wav')))
            stale = [f for f in expected if f not in found or
                     not filecmp.cmp(os.path.join(tmp, f), os.path.join(args.out, f), shallow=False)]
            extra = [f for f in found if f not in expected]
            for f in stale:
                print('stale: ' + f, file=sys.stderr)
            for f in extra:
                print('unexpected: ' + f, file=sys.stderr)
            for err in report['errors']:
                print('ERROR: ' + err, file=sys.stderr)
            if not stale and not extra and not report['errors']:
                print(f'{len(expected)} audio assets up to date')
                return 0
            return 1

    if args.render:
        os.makedirs(args.render, exist_ok=True)
    mmutil = args.mmutil if args.mmutil and os.path.exists(args.mmutil) else None
    report = build(args.out, args.render, mmutil, not args.quiet)
    for err in report['errors']:
        print('ERROR: ' + err, file=sys.stderr)
    return 1 if report['errors'] else 0


if __name__ == '__main__':
    sys.exit(main())
