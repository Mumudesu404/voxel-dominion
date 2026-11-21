"""Author original, deterministic mono weapon effects using only Python's standard library."""
import json
import math
from pathlib import Path
import random
import struct
import wave

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'SourceArt' / 'Audio'
RATE = 44100


def synthesize(kind, variation):
    rng = random.Random(8191 + variation * 101 + (2000 if kind == 'CannonBurst' else 0))
    duration = .245 if kind == 'CannonBurst' else .36
    samples, low, body, phase = [], 0., 0., 0.
    tuning = 1 + (variation - 1) * .035
    for i in range(round(duration * RATE)):
        t = i / RATE
        noise = rng.uniform(-1, 1)
        low += .085 * (noise - low)
        body += .022 * (noise - body)
        if kind == 'CannonBurst':
            value = 0.
            # Three short reports accompany the existing decorative rotary-gun burst.
            for offset, weight in ((0, 1), (.051, .83), (.104, .72)):
                u = t - offset
                if u < 0:
                    continue
                attack = 1 - math.exp(-u / .00045)
                phase = math.tau * tuning * (73 * u + 46 * .012 * (1 - math.exp(-u / .012)))
                value += weight * attack * (
                    .78 * (noise - low) * math.exp(-u / .0065)
                    + 1.5 * low * math.exp(-u / .020)
                    + .48 * math.sin(phase) * math.exp(-u / .029)
                    + .16 * math.sin(phase * 2.17) * math.exp(-u / .011))
            for offset in (.139, .181):
                u = t - offset
                if u >= 0:
                    value += .045 * (math.sin(math.tau * 2160 * tuning * u)
                                     + .5 * math.sin(math.tau * 3270 * u)) * math.exp(-u / .012)
        else:
            attack = 1 - math.exp(-t / .0008)
            value = attack * (
                .27 * (noise - low) * math.exp(-t / .007)
                + 1.6 * body * math.exp(-t / .044)
                + .24 * math.sin(math.tau * 173 * tuning * t) * math.exp(-t / .036))
            # Wooden stock, snapping bowstring, then a small airy release.
            for frequency, weight in ((287, .25), (573, .13), (911, .065), (1487, .025)):
                value += weight * math.sin(math.tau * frequency * tuning * t) * math.exp(-t / .073) * attack
            value += .5 * low * (t / .017) * math.exp(-t / .026)
            u = t - .027
            if u > 0:
                value += .065 * (noise - low) * math.exp(-u / .009) * (1 - math.exp(-u / .001))
        fade = min(1, max(0, (duration - t) / .025))
        samples.append(math.tanh(value * 1.35) * fade)
    dc = sum(samples) / len(samples)
    samples = [x - dc for x in samples]
    peak = max(abs(x) for x in samples)
    gain = (.70 if kind == 'CannonBurst' else .58) / peak
    # Smooth both ends after DC removal; no discontinuity when voices are reused.
    return [x * gain * min(1, i / 32, (len(samples) - 1 - i) / 200)
            for i, x in enumerate(samples)]


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    manifest = {'authorship': 'Original procedural synthesis; no recordings or external samples.',
                'sample_rate': RATE, 'channels': 1, 'bits': 16, 'files': []}
    for kind in ('CannonBurst', 'ArrowRelease'):
        for variation in range(3):
            name = f'S_{kind}_{variation+1:02}'
            samples = synthesize(kind, variation)
            path = OUT / f'{name}.wav'
            with wave.open(str(path), 'wb') as handle:
                handle.setparams((1, 2, RATE, 0, 'NONE', 'not compressed'))
                handle.writeframes(struct.pack('<' + 'h' * len(samples),
                                              *(round(x * 32767) for x in samples)))
            manifest['files'].append({'name': name, 'duration': len(samples) / RATE,
                                      'peak': max(abs(x) for x in samples),
                                      'rms': math.sqrt(sum(x*x for x in samples) / len(samples))})
    (OUT / 'manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    print(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    main()
