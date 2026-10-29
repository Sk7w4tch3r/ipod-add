#!/usr/bin/env bash
# SPDX-License-Identifier: LGPL-2.1-or-later
set -euo pipefail

if [[ -n ${IPOD_ADD_EXPECT_SUFFIX:-} ]]; then
    input=${!#}
    [[ -f $input ]] || {
        printf 'fake backend: input does not exist: %s\n' "$input" >&2
        exit 1
    }
    [[ $input == *"$IPOD_ADD_EXPECT_SUFFIX" ]] || {
        printf 'fake backend: expected suffix %s, got %s\n' \
            "$IPOD_ADD_EXPECT_SUFFIX" "$input" >&2
        exit 1
    }
fi

printf 'backend'
printf ' <%s>' "$@"
printf '\n'
