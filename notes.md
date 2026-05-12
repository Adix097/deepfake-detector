- audio is a sequence of amplitude over times
- mp3 is the audio but computers do not know audio they know numbers
- PCM is a standard method for digitally representing uncompressed raw analog audio signals4
- mp3 is compressed -> not good, we need non compressed raw files popular ones are pcm/wav

## sample rate: 
The sameple rate refers to how many times a second a mesurement is taken of the magnitude of the analogue audio signal.
A common sample rate is 44.1 Khz
Professional videos mostly use 48 Khz
to reduce quality loss during production sample rates of 88.1 Khz and 96 Khz are common
Even 176.4 Khz and 192 Khz are sometimes used although they are rarely needed

Most audio formats sample data using "pulse code modulation" (PCM)

## Codec & Containers:
Codec stands for coder-decoder, and refers to the algorithm used to digitally encode and then decode an audio file.
A container is a digital box or "wrapper" that is used to store encoded audio data.
MP3 is in effect both a codec and a comntainer
File extensions refer to the container of that audio. For example: .m4a refers to Apple's "MPEG-4 Audio" container, which may contain audio compressed with the ALAC or the AAC codec.
Not all audio file formats can be identified by their file extensions

## Non compressed audios: .wav
wav was created by IBM and microsoft and has a conatiner with the .wav file extension.
EBU created an extension to WAV called brodcast wave format or BWF
.wav support sample rates up to 192 Khz, in up to a 32-bit float bit depth.

"So, how does a microphone turn differences in air pressure into something a computer can work with? It is actually pretty easy, at least if you know the principle of electromagnetic induction. A microphone has a membrane which will vibrate in the same frequency as the audio signal it is picking up. These vibrations are transferred to a permanent magnet, wich will move inside of a coil and thus generate an analog signal representing the audio waves." - [text](https://steemit.com/programming/@targodan/decoding-audio-files-with-ffmpeg)

Say:
    Sample frequency: 44100 Hz
    Sample depth: 16 bit
    Song length: 4 Minutes

    Space required: 44100 * (16/8) * (4 * 60) bytes or 17.169 Mbs

17 Mbs for a audio file is too much thats why in order to compress audio data a so called codec is used which does some fancy things in order to reduce the file size without sacrificing too much sound quality.

## Spectrogram
axes:
    x -> time
    y -> frequency
    brightness -> strength

## THE PLAN
1. Decode Audio (.mp3) -> raw waveform numbers
2. resample it because different audio files have different sample rates limit them to say 16Khz
3. model cannot easily see hidden voice patterns from numbers convert it to frequency -> frequency is much informative than waveforms
4. can't do entire audio at once chunk it up say 25ms window and 10ms hop
5. repeat FFT -> STFT this becomes a spectrogram




