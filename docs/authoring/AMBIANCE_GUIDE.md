# Ambiance GLB Authoring Guide

Back to the [Authoring Guide](../AUTHORING_GUIDE.md). For exact syntax and
constraints, use the [Ambiance Authoring Reference](AMBIANCE_REFERENCE.md).

This guide is for authors creating standalone Ambiance GLB. Converting a small
original AMB first provides a useful example of track ordering, key placement,
and automation helpers.

Ambiance GLB uses empties and external audio sidecars. Audio is not embedded.
The default scale is 10 Arx units per GLB unit.

## Root and Tracks

Create one root named `arx_ambiance__MASTER_4__forest`. `MASTER_4` selects track
ordinal 4 as the master. Add two direct track children:

- `TRACK_001__sfx/birds.wav__birds` for a positioned bird track.
- `TRACK_004__sfx/wind__gust.wav__wind` for a panned wind track. The sample path
  contains `__`; its final `__wind` component remains the required label.

The Ambiance root defines its coordinate frame. Moving, rotating, or scaling
the complete root does not change imported data. Transforms below the root
affect their descendants normally.

[Exact root and track syntax](AMBIANCE_REFERENCE.md#ambiance-layout)

## Keys and Automation

Under the bird track, place this key at `(2,1,-3)` GLB units:

```text
KEY_000__PLAY_COUNT_2__START_500__birds_start
```

At the default scale, its positioned center is `(20,-10,30)` Arx units. Add two
direct automation helpers:

```text
VOLUME__VAL_0.8__RANGE_0.1__INTERVAL_2000__INTERPOLATED__volume
X__RANGE_0.5__INTERVAL_1500__RANDOM_INTERPOLATED__x
```

Volume varies from `0.7` to `0.9`. X varies by 5 Arx units around the key's X
position.

Under the wind track, place `KEY_000__key` to the listener's left and add:

```text
PAN__VAL_-1.5__RANGE_0.2__RANDOM_STEP__pan
```

`VAL_-1.5` preserves a pan center outside the visible `[-1,1]` arc. The key's
position shows the clamped direction.

The complete structure is:

```text
arx_ambiance__MASTER_4__forest
+-- TRACK_001__sfx/birds.wav__birds
|  `-- KEY_000__PLAY_COUNT_2__START_500__birds_start
|     +-- VOLUME__VAL_0.8__RANGE_0.1__INTERVAL_2000__INTERPOLATED__volume
|     `-- X__RANGE_0.5__INTERVAL_1500__RANDOM_INTERPOLATED__x
`-- TRACK_004__sfx/wind__gust.wav__wind
   `-- KEY_000__key
      `-- PAN__VAL_-1.5__RANGE_0.2__RANDOM_STEP__pan
```

Track and key ordinals define order and may have gaps. A track without key
children is a positioned-track shortcut: one default key is created at the
track position.

[Exact key and automation rules](AMBIANCE_REFERENCE.md#keys-and-automation)

## Audio Sidecars

Track sample paths are relative to the GLB. For a GLB written as
`project/forest.glb`, the example requires:

```text
project/sfx/birds.wav
project/sfx/wind__gust.wav
```

WAV, MP3, and Ogg Vorbis input can be carried through Ambiance authoring. Native
AMB output converts available audio to PCM16 WAV and spatial playback to mono.
Keep the master's shortest possible duration at least as long as every other
track's longest possible duration. Native export warns when attached audio
shows that a track can outlast the master. Use `--trim-to-master` to reduce
trailing play counts and keys to that limit when every compared audio file is
available.
See [Fidelity and Limitations](../LIMITATIONS.md#ambiance-and-audio) before
choosing source audio.

## Reference Model

Use `--reference-model` when positional sound should be authored around a
character or object. The reference mesh appears beside the Ambiance root and is
visual context only.

The first `view_attach` action point places the Ambiance root at the expected
camera attachment. Without one, the Model origin is used. The reference mesh is
ignored when the GLB is imported back into Ambiance.

[Exact reference-Model behavior](AMBIANCE_REFERENCE.md#reference-model)

## Export Checklist

- Export exactly one `arx_ambiance` root.
- Preserve hierarchy and semantic node names.
- Keep track and key ordinals unique; gaps are allowed.
- Keep the master long enough to cover every other track.
- Keep automation helpers directly below their key.
- Keep referenced audio files relative to the GLB at their sample paths.
- Use key translation for spatial position; helper rotation and scale carry no
  automation meaning.

[Back to the Authoring Guide](../AUTHORING_GUIDE.md)
