#!/usr/bin/env bash
# SPDX-License-Identifier: LGPL-2.1-or-later
set -euo pipefail

repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
wrapper=$repo_root/bin/ipod-add
backend=$repo_root/tests/fake-backend.sh
tmp_dir=$(mktemp -d /tmp/ipod-add-test.XXXXXX)
trap 'rm -rf -- "$tmp_dir"' EXIT HUP INT TERM

help=$($wrapper --help)
grep -q '^Usage:' <<<"$help"
grep -q -- '--audiobook' <<<"$help"

if $wrapper --title >/dev/null 2>"$tmp_dir/error"; then
    printf 'expected a missing --title value to fail\n' >&2
    exit 1
fi
grep -q -- '--title requires a value' "$tmp_dir/error"

ffmpeg -hide_banner -loglevel error -y \
    -f lavfi -i 'sine=frequency=440:duration=0.05' \
    -c:a pcm_s16le "$tmp_dir/tone.wav"

result=$(IPOD_ADD_REAL=$backend IPOD_TIME_SYNC_HELPER=/bin/true \
    "$wrapper" --music --title Tone "$tmp_dir/tone.wav")
grep -q 'backend <--music> <--title> <Tone>' <<<"$result"
grep -q 'tone\.wav>' <<<"$result"

ffmpeg -hide_banner -loglevel error -y \
    -f lavfi -i 'sine=frequency=660:duration=0.05' \
    -c:a flac "$tmp_dir/tone.flac"

converted=$(IPOD_ADD_REAL=$backend IPOD_TIME_SYNC_HELPER=/bin/true \
    IPOD_ADD_EXPECT_SUFFIX=.mp3 "$wrapper" "$tmp_dir/tone.flac")
grep -Eq '/tmp/ipod-add\.[^>]+\.mp3>' <<<"$converted"

printf 'wrapper tests passed\n'
