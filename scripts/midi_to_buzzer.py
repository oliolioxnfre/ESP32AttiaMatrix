#!/usr/bin/env python3
"""Convert a monophonic MIDI file into a C++ note/duration array for a passive buzzer.

Usage: python3 midi_to_buzzer.py <input.mid> <output.h> [array_name]
"""
import sys
import mido


def midi_note_to_freq(note):
    return round(440.0 * (2.0 ** ((note - 69) / 12.0)))


def convert(path):
    mid = mido.MidiFile(path)
    tempo = 500000  # default 120bpm, overridden by set_tempo if present

    # Flatten all tracks into one absolute-time stream of events
    events = []  # (abs_tick, msg)
    for track in mid.tracks:
        abs_tick = 0
        for msg in track:
            abs_tick += msg.time
            events.append((abs_tick, msg))
    events.sort(key=lambda e: e[0])

    notes = []  # (start_tick, end_tick, note)
    active_note = None
    active_start = 0
    last_tick = 0

    for abs_tick, msg in events:
        if msg.type == 'set_tempo':
            tempo = msg.tempo
        elif msg.type == 'note_on' and msg.velocity > 0:
            if active_note is not None and abs_tick > active_start:
                notes.append((active_start, abs_tick, active_note))
            active_note = msg.note
            active_start = abs_tick
        elif msg.type in ('note_off',) or (msg.type == 'note_on' and msg.velocity == 0):
            if active_note == msg.note:
                notes.append((active_start, abs_tick, active_note))
                active_note = None
        last_tick = abs_tick

    # Convert ticks -> ms using a single tempo (fine for constant-tempo songs like this one)
    us_per_tick = tempo / mid.ticks_per_beat
    entries = []  # (freq_hz, duration_ms, gap_ms)
    prev_end = 0
    for start, end, note in notes:
        gap_ticks = start - prev_end
        gap_ms = round(gap_ticks * us_per_tick / 1000)
        dur_ms = round((end - start) * us_per_tick / 1000)
        entries.append((midi_note_to_freq(note), dur_ms, gap_ms))
        prev_end = end

    return entries


def write_header(entries, out_path, array_name):
    with open(out_path, 'w') as f:
        f.write("#ifndef SONG_DATA_H\n#define SONG_DATA_H\n\n")
        f.write("#include <Arduino.h>\n\n")
        f.write("struct BuzzerNote { uint16_t freq; uint16_t durationMs; uint16_t gapMs; };\n\n")
        f.write(f"const BuzzerNote {array_name}[] PROGMEM = {{\n")
        for freq, dur, gap in entries:
            f.write(f"  {{{freq}, {dur}, {gap}}},\n")
        f.write("};\n\n")
        f.write(f"const size_t {array_name}_LEN = {len(entries)};\n\n")
        f.write("#endif // SONG_DATA_H\n")


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <input.mid> <output.h> [array_name]")
        sys.exit(1)
    in_path = sys.argv[1]
    out_path = sys.argv[2]
    array_name = sys.argv[3] if len(sys.argv) > 3 else 'songNotes'

    entries = convert(in_path)
    write_header(entries, out_path, array_name)
    total_ms = sum(d + g for _, d, g in entries)
    print(f"Wrote {len(entries)} notes to {out_path} (~{total_ms/1000:.1f}s playback)")
