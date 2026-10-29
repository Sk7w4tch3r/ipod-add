# ipod-add

`ipod-add` is a small Ubuntu command-line tool for adding music, audiobooks,
videos, and photos to a mounted iPod. It updates the iPod's Apple UI database
with libgpod and automatically converts media that the iPod cannot play.

## What it handles

- Audio: compatible files are copied directly; FLAC, OGG, and other formats
  are converted to 192 kb/s MP3.
- Video: incompatible video is converted to H.264 Baseline Level 3.0 at up to
  640x480 and 30 fps, with AAC stereo audio.
- Photos: JPEG, PNG, TIFF, BMP, and GIF are imported directly. WebP, HEIC,
  AVIF, and other recognized images are converted to JPEG.
- Clock: after a successful import, an optional restricted helper synchronizes
  the iPod clock to the computer's local time.

The tool is intended for iPods supported by libgpod and exposed as mounted
storage with an existing `iPod_Control/iTunes/iTunesDB` database.

## Install on Ubuntu

Ubuntu 22.04 and 24.04 provide the required libgpod development package.
Clone the repository and run:

```bash
./scripts/install-ubuntu.sh
```

The installer uses `apt`, builds the C programs from source, installs them
under `/usr/local`, and enables clock synchronization. To install media import
without the privileged clock helper:

```bash
./scripts/install-ubuntu.sh --without-clock-sync
```

The required Ubuntu packages are `build-essential`, `pkg-config`,
`libgpod-dev`, `libgpod-common`, `libglib2.0-dev`, `ffmpeg`, and `imagemagick`.
MPV is optional and can serve as a fallback decoder when FFmpeg cannot decode
a video.

For a manual installation:

```bash
sudo apt install build-essential pkg-config libgpod-dev libgpod-common \
  libglib2.0-dev ffmpeg imagemagick
make
make check
sudo make install
sudo make enable-clock-sync  # optional
```

## Usage

Mount the iPod normally, then run:

```bash
ipod-add song.flac
ipod-add --audiobook --title "The Odyssey" audiobook.ogg
ipod-add --title "Official Trailer" trailer.mkv
ipod-add --album "Vacation" photo.webp
```

The iPod is auto-detected below `/media/$USER`. Use `--mount` when it is
mounted somewhere else:

```bash
ipod-add --mount /mnt/ipod song.mp3
```

Run `ipod-add --help` for all options.

Back up the iPod database before first use on a new device. Do not disconnect
the iPod during a transfer, and unmount it cleanly afterward.

## Clock helper security

Clock synchronization needs raw write access to the iPod's SCSI device. The
installed set-user-ID helper does not accept a path from the caller: it scans
Linux sysfs for a whole disk whose vendor and model are exactly `Apple` and
`iPod`, executes the fixed `/usr/bin/ipod-time-sync` program supplied by
Ubuntu's `libgpod-common` package, and refuses to run when no matching device
is present. It also clears the caller's environment before executing the
root process so dynamic-loader and library settings cannot be injected.

If you do not want a set-user-ID helper, use `--without-clock-sync`. Media
imports still work; the command prints a warning instead of changing the
clock.

## Development

```bash
make
make check
make clean
```

`bin/ipod-add` is the conversion and compatibility wrapper.
`src/ipod-add.c` is the libgpod database importer. The restricted clock
launcher is also under `src/`.

To uninstall:

```bash
sudo make uninstall
```

## License

LGPL-2.1-or-later.
