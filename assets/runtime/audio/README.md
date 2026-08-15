# Wii U runtime audio

`car_honk.pcm` is the Map 2 car-warning sound prepared for the native sndcore2 path.

Runtime format:

- source: `Goofy ahh car honk sound effect.mp3` supplied for We Beast;
- playback starts from approximately **00:01** of the source clip;
- 16 kHz;
- mono;
- signed 16-bit PCM;
- big-endian sample bytes;
- non-looping.

The Wii U audio code resamples it through AX/sndcore2 to the active renderer rate and sends the horn to both TV and GamePad.
