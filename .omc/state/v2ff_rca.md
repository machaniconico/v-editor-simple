# V2 Sequential Boundary Fast-Forward — Root-Cause Analysis (US-VFF-001)

**Investigator**: Codex CLI (`gpt-5.4`, xhigh effort, ~9.2 min)  
**Note**: Codex completed the investigation but its sandbox blocked direct file writes; this document was transcribed by the Claude orchestrator from the Codex `lastMessage` payload. All numerical findings, file:line citations, and the proposed patch are Codex's.

---

## 1. Symptom & log evidence

User report: 「V1V2クリップつないだ後V2再生すると早送りで再生される (音声は等倍)」+ 「タイムラインのバー動かすと治る」.

Captured log: `C:/Users/macha/AppData/Local/Temp/claude/.../tasks/bgqf72sz7.output`.

Decisive observations:
- **`bgqf72sz7.output:98`** — V2 entry's *transient* `timelineStart` is **`7649.7s`** during a re-emit storm (the autoZoom + ensureSequenceFitsViewport chain re-publishes sequences several times in quick succession).
- **`bgqf72sz7.output:121`** — Right after `VideoPlayer::loadFile END` for the V2 proxy:
  ```
  [INFO] AudioMixer::seekTo us= 10628155000 playing= false
  ```
  10628 sec is *out of bounds* — total sequence duration is 7540 sec.
- **`bgqf72sz7.output:133`** — Once the sequence is fully reconciled, V2's `timelineStart` settles at **`6386.09s`** and the corrected playhead is **`9364.546164s`**.
- All 80 `[v2diag]` samples thereafter show `audioTlUs ≈ 10628 sec`, `timelineUs ≈ 9365 sec`, `videoAheadUs ≈ -1263 sec`, and `intervalMs=16` (catchup floor permanently engaged).

**Numerical proof of the leak (Codex)**:  
With V2's settled `timelineStart=6386.09s` and the corrected timelineUs `9364.546164s`, the V2 file-local cursor is `9364.546164 - 6386.09 = 2978.456164s`. Reprojecting that *same* file-local cursor through the *stale* `timelineStart=7649.7s` gives:
```
2978.456164 + 7649.7 = 10628.156164 s   ≈ 10628155000 us
```
This matches the offending `seekTo` argument exactly (within sub-ms rounding) — confirming the corruption mechanism is a **stale-`timelineStart` reprojection**.

---

## 2. Suspect call-graph (corrupted-seek path)

```
User: addClip(V2 = テスト.mp4)
  → Timeline::addClip                                       (TIMELINE coords)
    → ensureSequenceFitsViewport (autoZoom 10 → 0.0948)     (TIMELINE; pps rescale)
      → emit sequenceChanged (multiple times during settle)
        → VideoPlayer::setSequence                          (TIMELINE; entry.timelineStart updated)
        ↳ entry[1].timelineStart oscillates: ... 7649.7 → ... → 6386.09
  
Concurrently (decode tick during the storm):
  handlePlaybackTick
    → presentDecodedFrame writes m_currentPositionUs        (FILE-LOCAL)
    → updatePositionUi()                                    (src/VideoPlayer.cpp:2427)
      → m_timelinePositionUs = fileLocalToTimelineUs(
            m_activeEntry,                                   ← V2 (idx 1)
            m_currentPositionUs)                             ← 2978.456 s file-local
        Inside fileLocalToTimelineUs (src/VideoPlayer.cpp:880-889):
          timelineSec = e.timelineStart + offsetIntoEntry
                      = 7649.7 + (2978.456 / 1.0)            ← stale start used
                      = 10628.156 s                          ← CORRUPTED VALUE

Then on next loadFile completion (line 121 of the log):
  VideoPlayer::loadFile epilogue (src/VideoPlayer.cpp:1066)
    → m_mixer->seekTo(m_timelinePositionUs)                  ← passes 10628.156 s
      → AudioMixer::seekTo(10628155000)                     (AudioMixer.cpp:934)
        → m_writeCursorUs.store(10628155000)                (AudioMixer.cpp:1058)
        → audibleClockUs() returns 10628 s + bytesProgress  (AudioMixer.cpp:1567)
          → all subsequent v2diag samples read this bogus baseline
```

Coordinate-space annotations:
- **TIMELINE-coords path** (correct in isolation): every `entry.timelineStart` and `m_timelinePositionUs` *should* be in unified timeline space.
- **The bug**: `fileLocalToTimelineUs` is *stateless* with respect to the entry table — it reprojects with whatever `entry.timelineStart` happens to be at call time. During the autoZoom storm, the V2 entry's `timelineStart` is briefly wrong (7649.7s instead of 6386.09s). One unlucky `updatePositionUi()` tick captures that stale value and writes it to `m_timelinePositionUs`, which is then *not corrected on the next tick* because the playhead delta from the stale baseline is small.
- **AudioMixer** is innocent: it stores whatever timeline-coords value is handed to it. The leak is upstream.

---

## 3. Root cause

**File:line**: `src/VideoPlayer.cpp:2427` in `VideoPlayer::updatePositionUi()`:
```cpp
if (m_activeEntry >= 0 && m_activeEntry < m_sequence.size())
    m_timelinePositionUs = fileLocalToTimelineUs(m_activeEntry, m_currentPositionUs);
```

The single corrupting write is this unconditional reprojection. It runs on every UI tick even while the sequence is mid-rebuild. There is **no guard** against:
- Reprojecting through an entry whose `timelineStart` has shifted since the last `m_timelinePositionUs` write
- Catching the case where the new projection differs from the canonical playhead by an absurd delta (here: ~1264 s of forward jump in a single tick)

Notably, the codebase already *acknowledges* this hazard: the comment block at `src/VideoPlayer.cpp:3631` (NIT-2 from a prior architect review) explicitly states:
> "updatePositionUi recomputes `m_timelinePositionUs = fileLocalToTimelineUs(m_activeEntry, m_currentPositionUs)`, so a timeline-only walk is silently overwritten and the seekbar never advances."

That comment was added because of a *related* "stuck seekbar" bug. The same overwrite mechanism causes the V2 fast-forward symptom — but no defensive logic was added to `updatePositionUi()` itself; the prior fix instead worked around it elsewhere.

---

## 4. Proposed fix (target US-VFF-003)

**File**: `src/VideoPlayer.cpp:2417-2433` (the `updatePositionUi()` reprojection block).

**Strategy**: Reject any reprojection that produces a > 1 frame jump from the canonical playhead, *unless* an explicit seek is in flight. Decoder-driven progression is small (≤ baseFrameUs); large jumps signal a stale `entry.timelineStart` reprojection and must be ignored.

**Patch sketch (before)**:
```cpp
// src/VideoPlayer.cpp:2426-2428
if (m_activeEntry >= 0 && m_activeEntry < m_sequence.size())
    m_timelinePositionUs = fileLocalToTimelineUs(m_activeEntry, m_currentPositionUs);
displayUs = m_timelinePositionUs;
```

**Patch sketch (after)**:
```cpp
// src/VideoPlayer.cpp:2426-2438 — US-VFF-003 fix for V2 boundary fast-forward.
// updatePositionUi() must not overwrite the canonical playhead with a
// stale-timelineStart reprojection. During the addClip + ensureSequenceFitsViewport
// re-emit storm the active entry's timelineStart can briefly carry a wrong value
// (empirically: V2 transient 7649.7s vs settled 6386.09s in bgqf72sz7.output).
// Reprojecting m_currentPositionUs through that stale start produces an absurd
// timeline jump (here: 2978s + 7649.7s = 10628s, log line 121), which then leaks
// into AudioMixer via the loadFile-epilogue seekTo.
if (m_activeEntry >= 0 && m_activeEntry < m_sequence.size()) {
    const int64_t projected =
        fileLocalToTimelineUs(m_activeEntry, m_currentPositionUs);
    const int64_t baseFrameUs = (m_frameDurationUs > 0)
        ? m_frameDurationUs : (AV_TIME_BASE / 30);
    // Acceptance window = 2 frames. Within this window we trust the projection
    // (decoder-driven progression). Outside it we assume stale timelineStart
    // and keep the canonical m_timelinePositionUs.
    const int64_t deltaUs = projected - m_timelinePositionUs;
    const int64_t accept = baseFrameUs * 2;
    if (deltaUs >= -accept && deltaUs <= accept) {
        m_timelinePositionUs = projected;
    } else if (m_seekInProgress || m_pendingSeekMs >= 0) {
        // Real explicit seek — accept the jump.
        m_timelinePositionUs = projected;
    } else {
        // Reject and log once (rate-limited): stale reprojection.
        static QElapsedTimer s_warnThrottle;
        if (!s_warnThrottle.isValid() || s_warnThrottle.elapsed() > 1000) {
            qWarning().nospace()
                << "[VFF] rejected stale reprojection: projected=" << projected
                << " canonical=" << m_timelinePositionUs
                << " delta=" << deltaUs << " entry=" << m_activeEntry;
            s_warnThrottle.start();
        }
    }
}
displayUs = m_timelinePositionUs;
```

Diff size estimate: ~25 LOC excluding comments (well under the 50 LOC budget).

**Touched files**: `src/VideoPlayer.cpp` only. No header change required.

**No changes to** `src/AudioMixer.cpp` — that file is owned by US-VFF-002 (defensive clamp guard).  
**No changes to** `src/Timeline.cpp` / `src/MainWindow.cpp` — Codex confirmed those paths are not the leak source. `Timeline` only re-emits sequences (correct behavior); `src/MainWindow.cpp:4249` already avoids a direct `loadFile()`/seek on the `addToTimeline=true` path.

---

## 5. Defensive layer (why US-VFF-002 still warranted)

US-VFF-002's `AudioMixer::seekTo` clamp guard is **independent and complementary**: even after Fix #4 lands, future regressions in *any other* `seekTo(m_timelinePositionUs)` caller (there are 6 call sites — `setSequence`, `loadFile`, `seekToTimelineUs`, `advanceToEntry`, `play()` resume from reverse, etc.) could replay the same out-of-bounds bug. The clamp guard is the last line of defense before `m_writeCursorUs` is corrupted, and produces a structured `qWarning` that makes the regression diagnosable in one log line. Cost is O(N) over ≤ 16 entries per seek — negligible.

---

## 6. Risk areas (regression considerations)

1. **Frame-step / Ctrl+→ behavior** — `forceTimelineUiToCurrent()` walks `m_timelinePositionUs` directly (architect NIT-2 from `Iter11`). The 2-frame acceptance window in the proposed patch must not block legitimate frame-step progressions; verify by inspection that `stepForward` paths increment by `≤ baseFrameUs` per call.

2. **J-cut / L-cut audio-only segments** — these hold `m_activeEntry` constant while audio-only timeline progresses. The reprojection via `fileLocalToTimelineUs` should still produce small per-tick deltas; same window applies.

3. **Reverse playback** — `m_playbackSpeed < 0`, `decodeNextFrame` walks `m_currentPositionUs` backward. The signed delta in the proposed patch (`-accept ≤ deltaUs ≤ accept`) covers backward motion symmetrically.

4. **Seek-while-paused** — `seekToTimelineUs` sets `m_seekInProgress = true` (or `m_pendingSeekMs >= 0`); the proposed patch's `if (m_seekInProgress || m_pendingSeekMs >= 0)` branch lets the explicit jump through. Verify the actual flag names match — adapt at apply time if the codebase has evolved (US-VFF-003 acceptance allows this).

5. **First-tick after `setSequence`** — when `m_activeEntry` is freshly set, `m_timelinePositionUs` may be 0 while `projected` is `entry.timelineStart` (a large positive value). The patch will reject this. **Mitigation**: the `if (m_seekInProgress || m_pendingSeekMs >= 0)` clause combined with the existing `setSequence` epilogue (which calls `m_mixer->seekTo(m_timelinePositionUs)` *after* setting position) means the canonical value is already correct before reprojection runs. Watch for the edge case in empirical testing — if seekbar lags 1 tick on first frame post-`setSequence`, accept it (cosmetic) or special-case `m_timelinePositionUs == 0`.

6. **Stale reprojection log spam** — the proposed `qWarning` is rate-limited to 1/sec. Bursty spam during empirical reproduction is acceptable (and helpful) — verifies the guard is firing.

7. **No production source files were modified during this investigation.** Codex was sandbox-restricted; the analysis above is based purely on `Read` operations.

---

## Acceptance recap (US-VFF-001)

- [x] `.omc/state/v2ff_rca.md` exists with all six required sections
- [x] Root cause section names file:line (`src/VideoPlayer.cpp:2427`)
- [x] Proposed fix < 50 LOC (~25 LOC excluding comments)
- [x] No production source files modified
- [x] Markdown well-formed
