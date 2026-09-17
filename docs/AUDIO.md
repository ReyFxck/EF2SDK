# EF2Audio design

EF2Audio is designed around a simple rule:

> The application's native sample rate is application data, not a hardware
> restriction.

The PlayStation 2 output backend may ultimately feed the SPU2 at 48 kHz, but
an emulator, tracker, game or media player must not be forced to synthesize at
48 kHz just to satisfy the SDK.

## Source-rate-agnostic streams

A stream describes what the producer actually generates:

```c
ef2_audio_stream_config snes = {
    .sample_rate = 32000,
    .channels = 2,
    .format = EF2_AUDIO_SAMPLE_S16,
    .resampler = EF2_AUDIO_RESAMPLE_LINEAR,
};

ef2_audio_stream_config tracker = {
    .sample_rate = 44100,
    .channels = 2,
    .format = EF2_AUDIO_SAMPLE_S16,
    .resampler = EF2_AUDIO_RESAMPLE_LINEAR,
};
```

Rates such as 22050, 32000, 32768, 44100, 48000 or unusual core-specific
values are not special cases. A non-zero rate is accepted by the converter.

## Current alpha.3 foundation

The first audio layer is deliberately independent from SIF, IOP and SPU2 so
it can be tested before the hardware backend exists.

It currently provides:

- S16 mono/stereo stream descriptions;
- nearest-neighbour and linear rate conversion;
- Q32 phase accumulation to avoid long-term rate drift from low-precision
  stepping;
- freestanding integer implementation with no libc and no floating point;
- a saturating Q15-gain S16 mixer helper;
- host-side regression tests in CI.

The 64/32 division used to calculate the Q32 step is implemented locally so
the freestanding EE build does not gain a hidden libgcc dependency.

## Planned pipeline

```text
producer A (32 kHz) ---- rate converter ----\
producer B (44.1 kHz) -- rate converter -----+--> bus mixer --> IOP ring --> SPU2
producer C (48 kHz) ---- zero-cost copy -----/
```

The hardware backend will be separate from the producer API. Exact 48 kHz
streams bypass interpolation.

## Planned buses

The intended mixer model is:

```text
Master
├── Game
├── Music
├── SFX
└── UI
```

Pausing or changing one bus must not require stopping every audio producer.

## Planned IOP/SPU2 backend

The IOP side should keep enough queued PCM to continue feeding SPU2 while the
EE briefly performs expensive work such as filesystem access, ROM loading or
texture conversion.

The backend is expected to expose useful diagnostics instead of hiding them:

- queued frames / milliseconds;
- underrun and overrun counters;
- measured output latency;
- producer rate and backend rate;
- per-stream and per-bus state.

## Resampler roadmap

The initial modes are intentionally small:

- nearest: useful for diagnostics and extremely cheap paths;
- linear: baseline real-time converter.

Higher-quality modes such as cubic and band-limited/sinc conversion can be
added behind the same stream API. Applications should be able to choose the
quality/cost tradeoff without rewriting their audio backend.

## Why this belongs in the SDK

Ports should not each need their own 32 kHz -> 48 kHz converter, private ring
buffer, underrun workaround and mixer policy. Those are platform services.

EF2Audio treats the SPU2 rate as a backend detail and keeps emulator/core
timing at its native rate.
