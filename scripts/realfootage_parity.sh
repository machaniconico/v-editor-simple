#!/usr/bin/env bash
# realfootage_parity.sh — multi-codec real-footage parity harness (TM-7)
#
# Usage: realfootage_parity.sh <footage_dir> [binary_path]
#
# Runs the general parity selftest (VEDITOR_PARITY_SELFTEST=1) and the
# track-matte SSOT parity selftest (--selftest-trackmatte-parity /
# VEDITOR_TRACKMATTE_PARITY_SELFTEST=1) against every video file found in
# <footage_dir>.  Prints a summary table and exits non-zero if any test fails.
#
# Exit codes:
#   0  — every file passed every test
#   1  — at least one file failed at least one test
#   2  — footage_dir missing, not a directory, or contains no video files

set -euo pipefail

FOOTAGE_DIR="${1:-}"
BINARY="${2:-build_win/Release/v-simple-editor.exe}"
TIMEOUT_SECS=300

# ── helpers ─────────────────────────────────────────────────────────────────

usage() {
    echo "Usage: $(basename "$0") <footage_dir> [binary_path]" >&2
    exit 2
}

# Return codec string via ffprobe; degrade to '?' if ffprobe absent or fails.
probe_codec() {
    local file="$1"
    if command -v ffprobe &>/dev/null; then
        ffprobe -v error -select_streams v:0 \
            -show_entries stream=codec_name \
            -of default=noprint_wrappers=1:nokey=1 \
            "$file" 2>/dev/null || echo "?"
    else
        echo "?"
    fi
}

# Return WxH resolution string; degrade to '?' if ffprobe absent or fails.
probe_res() {
    local file="$1"
    if command -v ffprobe &>/dev/null; then
        ffprobe -v error -select_streams v:0 \
            -show_entries stream=width,height \
            -of csv=s=x:p=0 \
            "$file" 2>/dev/null || echo "?"
    else
        echo "?"
    fi
}

# Run one selftest invocation.  Returns the exit code (0=pass, non-zero=fail,
# 124=timeout via GNU timeout / 142 on BSD).
run_test() {
    local label="$1"   # for error output only
    local file="$2"
    shift 2
    local -a cmd=("$@")
    local rc=0
    timeout "$TIMEOUT_SECS" "${cmd[@]}" \
        >/dev/null 2>&1 || rc=$?
    # GNU timeout exits 124; some BSD/WSL variants exit 142.
    if [[ $rc -eq 124 || $rc -eq 142 ]]; then
        echo "  TIMEOUT (${TIMEOUT_SECS}s) for ${label}: $(basename "$file")" >&2
    fi
    return $rc
}

# ── validate inputs ──────────────────────────────────────────────────────────

if [[ -z "$FOOTAGE_DIR" ]]; then
    usage
fi

if [[ ! -d "$FOOTAGE_DIR" ]]; then
    echo "ERROR: '${FOOTAGE_DIR}' is not a directory." \
         "Place real clips (diverse codecs/resolutions) there and re-run." >&2
    exit 2
fi

# Collect video files (case-insensitive glob via find).
mapfile -d '' VIDEO_FILES < <(
    find "$FOOTAGE_DIR" -maxdepth 1 \
        \( -iname '*.mp4' -o -iname '*.mov' -o -iname '*.mkv' \
           -o -iname '*.webm' -o -iname '*.avi' -o -iname '*.m4v' \) \
        -print0 2>/dev/null | sort -z
)

if [[ ${#VIDEO_FILES[@]} -eq 0 ]]; then
    echo "ERROR: no footage found in '${FOOTAGE_DIR}'." \
         "Place real clips (diverse codecs/resolutions) there and re-run." >&2
    exit 2
fi

if [[ ! -x "$BINARY" ]]; then
    echo "ERROR: binary not found or not executable: '${BINARY}'" >&2
    exit 2
fi

# ── run tests ────────────────────────────────────────────────────────────────

# Table columns: FILE | CODEC | RES | parity | trackmatte-parity | RESULT
printf "%-45s %-8s %-10s %-7s %-18s %s\n" \
    "FILE" "CODEC" "RES" "parity" "trackmatte-parity" "RESULT"
printf '%s\n' "$(printf '─%.0s' {1..100})"

overall_rc=0

for file in "${VIDEO_FILES[@]}"; do
    fname="$(basename "$file")"
    codec="$(probe_codec "$file")"
    res="$(probe_res "$file")"

    # --- general parity selftest (env-var only) ---
    parity_rc=0
    VEDITOR_E2E_CLIP="$file" \
    VEDITOR_PARITY_SELFTEST=1 \
        run_test "parity" "$file" "$BINARY" || parity_rc=$?

    parity_label="PASS"
    [[ $parity_rc -eq 0 ]] || { parity_label="FAIL"; overall_rc=1; }

    # --- track-matte SSOT parity selftest (argv + env-var) ---
    tm_rc=0
    VEDITOR_E2E_CLIP="$file" \
    VEDITOR_TRACKMATTE_PARITY_SELFTEST=1 \
        run_test "trackmatte-parity" "$file" \
            "$BINARY" --selftest-trackmatte-parity || tm_rc=$?

    tm_label="PASS"
    [[ $tm_rc -eq 0 ]] || { tm_label="FAIL"; overall_rc=1; }

    if [[ $parity_rc -eq 0 && $tm_rc -eq 0 ]]; then
        row_result="PASS"
    else
        row_result="FAIL"
    fi

    # Truncate filename for display if too long.
    if [[ ${#fname} -gt 44 ]]; then
        fname="${fname:0:41}..."
    fi

    printf "%-45s %-8s %-10s %-7s %-18s %s\n" \
        "$fname" "$codec" "$res" "$parity_label" "$tm_label" "$row_result"
done

printf '%s\n' "$(printf '─%.0s' {1..100})"

if [[ $overall_rc -eq 0 ]]; then
    echo "ALL TESTS PASSED"
else
    echo "ONE OR MORE TESTS FAILED" >&2
fi

exit $overall_rc
