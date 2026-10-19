# Gapless FLAC Fixture

`gapless-silence.flac` is a generated 16-second stereo 44.1 kHz / 16-bit signal:
1 second of silence, 13.5 seconds of a 660 Hz sine wave, and 1.5 seconds of silence.
It contains no third-party audio. Tests serve it over chunked HTTP Range responses
and exercise analysis, head trimming, hand-off, and decoding through EOF.

Regenerate with:

```sh
ffmpeg -f lavfi -i 'sine=frequency=660:sample_rate=44100:duration=13.5' \
  -af 'adelay=1000:all=1,apad=pad_dur=1.5' -ac 2 -sample_fmt s16 \
  -c:a flac -map_metadata -1 gapless-silence.flac
```

`embedded-metadata.flac` is the source project's generated 20-second 440Hz tone,
Chinese title/artist/album, LRC tag and one-pixel PNG attachment (36a80899).
No third-party recording/artwork. Reader extraction/export must not read or seek again.

The source fixture's 1px PNG IDAT had an invalid CRC. The Qt copy replaces only
the attached image with a generated valid 1px RGBA PNG using ffmpeg stream copy;
the generated tone and title/artist/album/LRC tags remain unchanged. Actual Qt and
MPNowPlaying image decoding is tested, rather than ignoring the decoding error.
