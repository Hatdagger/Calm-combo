"""Calm Combo audio for the Neo Geo CD (milestone 4).

    python3 tools/make_audio.py              placeholders for anything missing, then build
    python3 tools/make_audio.py --force      remake every placeholder (overwrites your audio!)
    python3 tools/make_audio.py --build      only build the sound-chip files from sfx/*.wav

What it makes, under src/assets/:
  cdda/track02_lakeside.wav  track03_shoreline.wav  track04_rain.wav
      The three scene songs, 44.1 kHz stereo, with the ambience mixed in.  They
      are rendered from the song data of the original browser prototype (chords,
      tempo, 32-bar form, motifs), kept below.
  sfx/*.wav
      The 16 sound effects at 18,500 Hz mono, again copying the prototype's
      synth sounds.
  sounds/calm.pcm  sounds/calm.z80  ../sound_ids.h
      The build step.  Every sfx/*.wav (yours or a placeholder) is encoded to
      the YM2610's ADPCM-A format and packed into calm.pcm.  calm.z80 is the
      NGFX SoundBuilder Z80 driver (from NeoCore's soundFX_poly sample, MIT)
      with its sample table rewritten: each sound gets three commands, for
      hard left, centre and hard right.

Your own audio: drop a WAV with the same name over a placeholder.  Placeholders
are only made for missing files, so your files are never touched without
--force.  Needs numpy and scipy.
"""
import os, sys, wave
import numpy as np
from scipy.signal import lfilter, fftconvolve

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.normpath(os.path.join(HERE, '..', 'src'))
ASSETS = os.path.join(SRC, 'assets')

SFX = ['grab', 'no_match'] + ['clear_%d' % i for i in range(1, 10)] + ['lever', 'moves', 'stones', 'tick', 'menu']
TRACKS = ['track02_lakeside', 'track03_shoreline', 'track04_rain']
SFX_RATE = 18500
CD_RATE = 44100
CMD_BASE = 0x20            # first sound command of the NGFX driver
TABLE = 0x1000             # its sample table in Z80 memory, 16 bytes per entry
PANS = [(0x80, 'L'), (0xc0, 'C'), (0x40, 'R')]   # ADPCM-A L/R bits; volume 31 is or-ed in

# ---------------------------------------------------------------- prototype data
PENTA = [72, 74, 76, 79, 81, 84, 86, 88, 91]
SONGS = [
    dict(bpm=72, lead='sine', seed=11, scale=[72, 74, 76, 79, 81, 84],
         A=[[53, 57, 60, 64], [52, 55, 59, 62], [50, 53, 57, 60], [48, 52, 55, 59]],
         B=[[46, 50, 53, 57], [45, 48, 52, 55], [43, 46, 50, 53], [48, 53, 55, 58]]),
    dict(bpm=80, lead='triangle', seed=23, scale=[74, 76, 78, 81, 83, 86],
         A=[[50, 54, 57, 61], [47, 50, 54, 57], [43, 47, 50, 54], [45, 49, 52, 54]],
         B=[[52, 55, 59, 62], [54, 57, 61, 64], [43, 47, 50, 54], [45, 50, 52, 57]]),
    dict(bpm=64, lead='sine', seed=37, scale=[69, 72, 74, 76, 79, 81],
         A=[[45, 48, 52, 55], [41, 45, 48, 52], [48, 52, 55, 59], [40, 43, 47, 50]],
         B=[[50, 53, 57, 60], [43, 47, 50, 53], [48, 52, 55, 59], [45, 48, 52, 55]])]
FORM = [('A', 0), ('A', 1), ('A', 2), ('B', 1), ('B', 2), ('A', 2), ('A', 1), ('A', 0)]
# 32-bar loops per track: 5:20 lake, 6:24 shore, 6:00 rain.  Every track is at
# least 5 minutes, the length of a scene, so it never has to loop on the disc.
LOOPS = [3, 4, 3]
BEACH_PHASE = 3.0          # the shore animation is 3 s into its 9 s wave when the beach scene starts


def mulberry32(a):          # exact port of the prototype's generator
    st = [a & 0xffffffff]
    def imul(x, y): return (x * y) & 0xffffffff
    def r():
        st[0] = (st[0] + 0x6D2B79F5) & 0xffffffff
        a = st[0]
        t = imul(a ^ (a >> 15), 1 | a)
        t = ((t + imul(t ^ (t >> 7), 61 | t)) & 0xffffffff) ^ t
        return ((t ^ (t >> 14)) & 0xffffffff) / 4294967296.0
    return r


def make_motif(seed, scale):
    r = mulberry32(seed)
    out = []
    idx = int(r() * len(scale))
    for _ in range(16):
        if r() < .45: out.append(None); continue
        idx = max(0, min(len(scale) - 1, idx + int(np.floor(r() * 3)) - 1))
        out.append(scale[idx])
    return out

# ---------------------------------------------------------------- tiny WebAudio
def mtof(m): return 440.0 * 2 ** ((m - 69) / 12.0)


def biquad(kind, f, rate, q=0.7071):
    w = 2 * np.pi * f / rate
    c, s = np.cos(w), np.sin(w)
    al = s / (2 * q)
    if kind == 'lowpass':  b = [(1 - c) / 2, 1 - c, (1 - c) / 2]
    elif kind == 'highpass': b = [(1 + c) / 2, -(1 + c), (1 + c) / 2]
    else: b = [al, 0, -al]   # bandpass, peak gain 1
    a = [1 + al, -2 * c, 1 - al]
    return np.array(b) / a[0], np.array(a) / a[0]


def filt(x, kind, f, rate, q=0.7071):
    b, a = biquad(kind, f, rate, q)
    return lfilter(b, a, x, axis=0)


class Mix:
    """A mono buffer that voices are added into (a bus)."""
    def __init__(self, seconds, rate):
        self.rate = rate
        self.buf = np.zeros(int(seconds * rate) + 1)

    def add(self, t, sig):
        i = int(round(t * self.rate))
        if i >= len(self.buf): return
        n = min(len(sig), len(self.buf) - i)
        self.buf[i:i + n] += sig[:n]

    def env(self, dur, vol, attack):
        """gain: 0 -> vol in `attack` (linear), then exponential to 1e-4 at `dur`"""
        n = max(1, int((dur + .05) * self.rate))
        t = np.arange(n) / self.rate
        g = np.where(t < attack, vol * t / max(attack, 1e-6), 0)
        late = t >= attack
        span = max(dur - attack, 1e-3)
        g[late] = vol * (1e-4 / vol) ** np.minimum((t[late] - attack) / span, 1.0)
        g[t > dur] = 0
        return t, g

    def voice(self, m, t0, dur, typ, vol, attack=.01):
        t, g = self.env(dur, vol, attack)
        ph = mtof(m) * t
        if typ == 'sine': w = np.sin(2 * np.pi * ph)
        else: w = 1 - 4 * np.abs(((ph + .25) % 1) - .5)       # triangle
        self.add(t0, w * g)

    def noise_hit(self, t0, dur, f, vol, kind='bandpass', rng=None):
        n = int((dur + .05) * self.rate)
        x = rng.uniform(-1, 1, n)
        x = filt(x, kind, min(f, self.rate * .45), self.rate, 1.0)
        t = np.arange(n) / self.rate
        g = vol * (1e-4 / vol) ** np.minimum(t / dur, 1.0)
        g[t > dur] = 0
        self.add(t0, x * g)

    def kick(self, t0):
        n = int(.35 * self.rate)
        t = np.arange(n) / self.rate
        f = np.where(t < .18, 110 * (42 / 110.) ** (t / .18), 42)
        ph = np.cumsum(f) / self.rate
        g = .28 * (1e-4 / .28) ** np.minimum(t / .3, 1)
        g[t > .3] = 0
        self.add(t0, np.sin(2 * np.pi * ph) * g)

# ---------------------------------------------------------------- songs
def render_song(k):
    S = SONGS[k]
    st = 60.0 / S['bpm'] / 2                    # one step = an eighth note
    steps = 32 * 8 * LOOPS[k]
    length = steps * st
    tail = 8.0
    rng = np.random.default_rng(1000 + k)
    bus = Mix(length + tail, CD_RATE)
    mA, mB = make_motif(S['seed'], S['scale']), make_motif(S['seed'] + 7, S['scale'])
    for i in range(steps):
        t = i * st + (st * .16 if i % 2 else 0)  # swing
        bar, s = i // 8, i % 8
        sec = FORM[(bar // 4) % len(FORM)]
        ch = S[sec[0]][bar % 4]
        dens = sec[1]
        if s == 0:
            for m in ch:
                bus.voice(m, t, st * 8.6, 'triangle', .032 if dens else .04, .8)
                bus.voice(m + 12, t, st * 8.6, 'sine', .011, 1.1)
            if dens > 0: bus.voice(ch[0] - 12, t, st * 7, 'sine', .15, .05)
        if dens > 0:
            if s == 5: bus.voice(ch[0] - 5, t, st * 2.5, 'sine', .08, .05)
            if s in (0, 5): bus.kick(t)
            if s in (2, 6): bus.noise_hit(t, .12, 1800, .04, rng=rng)
            if dens == 2 and s % 2: bus.noise_hit(t, .04, 7000, .011, 'highpass', rng=rng)
        if dens == 2:
            mo = mA if sec[0] == 'A' else mB
            n = mo[(bar % 2) * 8 + s]
            if n:
                bus.voice(n, t, 1.1, S['lead'], .05, .005)
                bus.voice(n + 12, t, .4, 'triangle', .01, .005)
        elif rng.random() < (.2 if dens else .1):
            pool = [m + 12 for m in ch] + S['scale'][:4]
            bus.voice(pool[int(rng.random() * len(pool))], t, 1.3, 'sine', .045, .005)

    music = bus.buf
    n = len(music)
    tt = np.arange(n) / CD_RATE
    # ambience, as in the prototype: wind / wave / rain = low-passed noise beds
    def bed(f, seed):
        r = np.random.default_rng(seed)
        return filt(r.uniform(-1, 1, (n, 2)), 'lowpass', f, CD_RATE, 1.0)
    gust = .75 + .25 * np.sin(2 * np.pi * tt / 23.0) * np.sin(2 * np.pi * tt / 7.3)
    if k == 0:
        amb = bed(420, 1) * (.022 * gust)[:, None] + bed(1600, 2) * .004
    elif k == 1:
        wp = .5 + .5 * np.sin(2 * np.pi * (tt + BEACH_PHASE) / 9.0)   # in step with shore.png
        amb = bed(420, 1) * .008 + bed(750, 3) * (.012 + .05 * wp)[:, None]
    else:
        amb = bed(1600, 2) * .07 + bed(400, 4) * .02
    mix = (music[:, None] * np.ones(2) + amb * 1.6) * .55 * .7

    # soft room so the mono synth sits in stereo, then the prototype's 3 kHz master low-pass
    r = np.random.default_rng(7)
    ir_t = np.arange(int(2.2 * CD_RATE)) / CD_RATE
    ir = r.standard_normal((len(ir_t), 2)) * np.exp(-ir_t / .55)[:, None]
    ir = filt(ir, 'lowpass', 4000, CD_RATE)
    ir /= np.sqrt((ir ** 2).sum(axis=0))
    wet = np.stack([fftconvolve(mix[:, c], ir[:, c])[:n] for c in range(2)], axis=1)
    out = mix + wet * .45
    out = filt(out, 'lowpass', 3000, CD_RATE)

    # fold the tail (reverb, last chords) onto the start so the track loops cleanly
    L = int(round(length * CD_RATE))
    out[:n - L] += out[L:]
    out = out[:L]
    out *= .89 / np.abs(out).max()
    return out


def write_wav(path, x, rate):
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    x = np.clip(np.round(x * 32767), -32768, 32767).astype('<i2')
    w = wave.open(path, 'wb')
    w.setnchannels(1 if x.ndim == 1 else x.shape[1])
    w.setsampwidth(2)
    w.setframerate(rate)
    w.writeframes(x.tobytes())
    w.close()


def read_wav(path):
    w = wave.open(path)
    ch, sw, fr, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
    raw = w.readframes(n)
    w.close()
    if sw == 2: x = np.frombuffer(raw, '<i2').astype(float) / 32768
    elif sw == 1: x = (np.frombuffer(raw, 'u1').astype(float) - 128) / 128
    elif sw == 3:
        b = np.frombuffer(raw, 'u1').reshape(-1, 3).astype(np.int32)
        x = ((b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16)) << 8 >> 8) / 8388608.0
    else: x = np.frombuffer(raw, '<i4').astype(float) / 2147483648.0
    return x.reshape(-1, ch).mean(axis=1), fr

# ---------------------------------------------------------------- sound effects
def render_sfx(name):
    """The prototype's sfx (sfx bus .6 x master .7), mono at the chip's rate."""
    b = Mix(2.0, SFX_RATE)
    if name == 'grab':
        b.voice(84, 0, .12, 'sine', .07, .005)
    elif name == 'no_match':
        b.voice(45, 0, .25, 'sine', .14, .01)
    elif name.startswith('clear_'):
        m = PENTA[int(name[6:]) - 1]
        b.voice(m, 0, .9, 'sine', .12, .005)
        b.voice(m + 7, .06, .8, 'triangle', .04, .005)
    elif name == 'lever':
        for i, m in enumerate([88, 91, 96]): b.voice(m, i * .06, .4, 'sine', .05, .005)
    elif name == 'moves':
        b.voice(79, 0, .5, 'sine', .09, .005)
        b.voice(84, .12, .7, 'sine', .09, .005)
    elif name == 'stones':
        b.voice(40, 0, .3, 'triangle', .18, .005)
        b.voice(47, .08, .3, 'triangle', .12, .005)
    elif name == 'tick':
        b.voice(96, 0, .05, 'sine', .03, .002)
    elif name == 'menu':                       # not in the prototype: a soft two-note blip
        b.voice(81, 0, .09, 'sine', .05, .004)
        b.voice(88, .04, .14, 'sine', .035, .004)
    # x10 keeps the prototype's balance between effects but uses the chip's
    # 12-bit range (the loudest, stones, peaks near 0.7); the tick gets a bit extra
    x = filt(b.buf * .6 * .7 * 10 * (1.8 if name == 'tick' else 1), 'lowpass', 3000, SFX_RATE)
    nz = np.nonzero(np.abs(x) > 1e-4)[0]
    return x[:nz[-1] + 1] if len(nz) else x[:1]

# ---------------------------------------------------------------- ADPCM-A
STEPS = [16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130,
         143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
         876, 963, 1060, 1166, 1282, 1411, 1552]
INC = [-1, -1, -1, -1, 2, 5, 7, 9]
# delta for (step index, nibble)
DELTA = [[((2 * (n & 7) + 1) * s // 8) * (-1 if n & 8 else 1) for n in range(16)] for s in STEPS]


def adpcma_encode(x):
    """x in -1..1 at the chip rate -> bytes (high nibble first), padded to 256 bytes."""
    target = np.clip(np.round(x * 2047), -2048, 2047).astype(int).tolist()
    target += [0] * ((-len(target)) % 512)            # pad with silence: 512 nibbles = 256 bytes
    acc, idx, nibs = 0, 0, []
    for v in target:
        best, bn = None, 0
        row = DELTA[idx]
        for n in range(16):
            a = acc + row[n]
            if a > 2047 or a < -2048: continue        # the chip would wrap round
            e = abs(v - a)
            if best is None or e < best: best, bn = e, n
        acc += row[bn]
        idx = min(48, max(0, idx + INC[bn & 7]))
        nibs.append(bn)
    return bytes((nibs[i] << 4) | nibs[i + 1] for i in range(0, len(nibs), 2))


def adpcma_decode(data):
    acc, idx, out = 0, 0, []
    for b in data:
        for n in (b >> 4, b & 15):
            acc = ((acc + DELTA[idx][n] + 2048) & 0xfff) - 2048
            idx = min(48, max(0, idx + INC[n & 7]))
            out.append(acc)
    return np.array(out) / 2048.0


def resample(x, fr, to):
    if fr == to: return x
    n = int(round(len(x) * to / float(fr)))
    if fr > to: x = filt(x, 'lowpass', to * .45, fr)
    return np.interp(np.arange(n) * (fr / float(to)), np.arange(len(x)), x)


def build_chip():
    pcm = bytearray()
    entries = []
    report = []
    for name in SFX:
        path = os.path.join(ASSETS, 'sfx', name + '.wav')
        x, fr = read_wav(path)
        x = resample(x, fr, SFX_RATE)
        if len(x) > 2 * SFX_RATE:
            print('  note: %s.wav is %.1f s, cut to 2 s' % (name, len(x) / float(SFX_RATE)))
            x = x[:2 * SFX_RATE] * np.minimum(1, (2 * SFX_RATE - np.arange(2 * SFX_RATE)) / 400.0)
        data = adpcma_encode(x)
        start = len(pcm) // 256
        pcm += data
        end = len(pcm) // 256 - 1
        entries.append((start, end))
        y = adpcma_decode(data)[:len(x)]
        snr = 10 * np.log10((x ** 2).sum() / max(((x - y) ** 2).sum(), 1e-12))
        report.append('  %-10s %5.2f s  %6d bytes  SNR %4.1f dB' % (name, len(x) / float(SFX_RATE), len(data), snr))
    if len(pcm) > 512 * 1024:
        print('WARNING: sound effects use %d KB, over the 512 KB budget' % (len(pcm) // 1024))
    if len(pcm) > 1024 * 1024:
        sys.exit('ERROR: sound effects are over the 1 MB of sound memory')

    z80 = bytearray(open(os.path.join(ASSETS, 'sounds', 'ngfx_driver.z80'), 'rb').read())
    for a in range(TABLE, TABLE + 0x800): z80[a] = 0                  # drop the sample's old table
    k = 0
    for (start, end) in entries:
        for bits, _ in PANS:
            e = TABLE + 16 * k
            z80[e] = 1                                                 # ADPCM-A, any free channel
            z80[e + 1] = bits | 0x1f                                   # pan + volume 31
            z80[e + 6:e + 8] = bytes([start & 255, start >> 8])
            z80[e + 8:e + 10] = bytes([end & 255, end >> 8])
            k += 1
    os.makedirs(os.path.join(ASSETS, 'sounds'), exist_ok=True)
    open(os.path.join(ASSETS, 'sounds', 'calm.pcm'), 'wb').write(bytes(pcm))
    open(os.path.join(ASSETS, 'sounds', 'calm.z80'), 'wb').write(bytes(z80))

    h = ['/* Generated by tools/make_audio.py - do not edit. */',
         '#ifndef CC_SOUND_IDS_H', '#define CC_SOUND_IDS_H', '',
         '/* sound effects, in the order of the driver table (3 commands each: L, C, R) */',
         'enum {']
    h += ['    SND_%s,' % n.upper() for n in SFX]
    h += ['    SND_COUNT', '};', '', '#define SND_CMD_BASE 0x%02x' % CMD_BASE,
          '#define SND_CMD(id, pan) ((BYTE)(SND_CMD_BASE + (id) * 3 + (pan)))   /* pan 0 left, 1 centre, 2 right */',
          '#define SND_NAMES ' + ', '.join('"%s"' % n for n in SFX) + '   /* file names, for the PC harness */',
          '', '#endif', '']
    open(os.path.join(SRC, 'sound_ids.h'), 'w', newline='\n').write('\n'.join(h))
    print('\n'.join(report))
    print('  sound memory: %d KB of 512 KB budget, %d driver commands (0x%02x-0x%02x)'
          % (len(pcm) // 1024, k, CMD_BASE, CMD_BASE + k - 1))


def main():
    force = '--force' in sys.argv
    if '--build' not in sys.argv:
        for name in SFX:
            p = os.path.join(ASSETS, 'sfx', name + '.wav')
            if force or not os.path.exists(p):
                write_wav(p, render_sfx(name), SFX_RATE)
                print('  made sfx/%s.wav' % name)
        for k, name in enumerate(TRACKS):
            p = os.path.join(ASSETS, 'cdda', name + '.wav')
            if force or not os.path.exists(p):
                print('  rendering cdda/%s.wav ...' % name)
                sys.stdout.flush()
                write_wav(p, render_song(k), CD_RATE)
    print('Building sound-chip files')
    build_chip()


if __name__ == '__main__':
    main()
