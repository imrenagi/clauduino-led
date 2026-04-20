# Hardware wiring & rationale

This doc covers how to wire the WS2812 LED strip to the Arduino Uno R4 WiFi, and **why** each passive component is there. If something flickers, resets, or the "first LED is always wrong", come back here — 95% of the time it's one of these details.

## Bill of materials

| Part | Value | Notes |
|---|---|---|
| Arduino | Uno R4 WiFi | 3.3 V I/O on digital pins |
| LED strip | WS2812 / WS2812B, 46 LEDs | Data on D6 |
| 5 V PSU | ≥ 3 A | 46 LEDs at full white ≈ 2.8 A — USB won't do it |
| Series resistor | 100–470 Ω (220 Ω is fine) | On the data line, close to the strip |
| Electrolytic cap | 470–2200 µF (1000 µF nominal) | Across the strip's 5V/GND, near LED #1 |
| (Optional) level shifter | 74AHCT125 or similar | 3.3 V → 5 V buffer for the data line, for long runs |

## Diagram

```
   Arduino UNO R4 WiFi                 WS2812B strip (46 LEDs)
   ┌───────────────────┐               ┌──────────────────────┐
   │                   │               │                      │
   │  D6  ───┬─────[220Ω]─────────────►│ DIN                  │
   │         │         │               │                      │
   │  GND ───┼─────────┼─────┬─────────┤ GND                  │
   │         │         │     │         │                      │
   └─────────┼─────────┼─────┼─────────┤ +5V                  │
             │         │     │         │                      │
             │         │     │         └──────────────────────┘
             │         │     │                    ▲
             │         │     │                    │
             │         │     │   ┌────────────────┘
             │         │     │   │  ─── 5V PSU (≥3A) ───────┐
             │         │     │   │  │                       │
             │         │     │   │  │   +5V ──┬────────┐    │
             │         │     │   └──┤         │        │    │
             │         │     │      │         │     1000µF  │
             │         │     │      │         │     (+ on   │
             │         │     │      │         │      5V,    │
             │         │     └──────┤  GND ───┼──────-on    │
             │         │            │         │      GND)   │
             │         │            └─────────┴────────┘    │
             │         │                                    │
             │         └────────────────────────────────────┘
             │         (optional: put cap leads directly across
             │          strip's +5V/GND pads, near 1st LED)
             │
             └── share this GND with the Arduino (already wired above)
```

## Three critical rules

Even more important than the exact diagram:

1. **Resistor in series on the data line.** Arduino `D6` → resistor → strip `DIN`. Place the resistor **close to the strip**, not close to the Arduino — reflections happen at the load end.

2. **Common ground.** Arduino `GND` and PSU `GND` *must* be tied together. Without a shared reference, the data signal floats relative to the strip's view of ground, and you'll get flicker, dropped bits, or worse.

3. **Capacitor polarity.** 1000 µF electrolytic is polarized — **long lead = `+5V`, short lead (striped side) = `GND`**. Reverse it and it will pop. Place the cap directly across the strip's 5V/GND pads near LED #1.

## Why the series resistor?

WS2812 data edges are *fast* — about 5–10 ns rise time. Any wire longer than a couple of centimetres starts behaving like a short transmission line: edges bounce back and forth between the Arduino (low impedance source) and the strip's DIN (high impedance load).

Two things go wrong without damping:

1. **Ringing / overshoot.** The reflected edge overlays on the next bit. The first LED sees voltage spikes outside the WS2812 logic spec, so it misreads the data stream. Classic symptoms:
   - "First LED lights a wrong color, rest of the strip is fine"
   - Random flicker on LED 0
   - The strip seems to work, then cuts out when you change brightness
2. **Fault protection.** If the first LED ever latches up (brown-out, ESD event, mis-wire) the data pin can become a near-short to rail. The series resistor limits the Arduino pin's fault current to ~15 mA (`3.3 V / 220 Ω`) instead of potentially amps — the pin survives.

The resistor is a rough source-termination too; the "right" value to match typical hookup wire impedance would be ~100 Ω, but 220–470 Ω works well in practice because it dominates over the wire impedance.

**Placement matters:** reflections originate at the load end (strip DIN). Put the resistor physically close to the strip, not in-line at the Arduino header.

## Why the 1000 µF capacitor?

When a big chunk of LEDs switches from off to white at once, the strip's current demand can jump by hundreds of milliamps in a few microseconds. The wire between the PSU and strip is not a perfect conductor — it has tiny inductance. Fast current changes through that inductance produce voltage dips at the strip end, by the simple relationship:

```
V_dip = L · (di/dt)
```

A dip big enough to drop the 5 V rail below the WS2812's logic threshold (~3.5 V) causes the strip to see corrupted data and either reset mid-frame or latch garbage. Symptoms:

- Strip flickers heavily on sudden-bright animations, but is stable on dim / gradual ones
- First few LEDs "glitch" when you switch colors abruptly
- Strip resets to weird state when you hit it with a full-white command

The cap is a local energy reservoir. When the strip suddenly needs 1 A, it pulls most of it from the cap (which has a very short electrical path to the strip) instead of from the PSU (which is down a long, slightly-inductive wire). The PSU then slowly tops up the cap. You've effectively cached current near the consumer.

Rule-of-thumb sizing: ~10 µF per 10 LEDs minimum; 1000 µF is a comfortable default up to ~120 LEDs. For 46 LEDs, anywhere between 470 µF and 2200 µF is fine.

## Analogy for developers

- **Series resistor** = a combined rate-limiter and circuit breaker on the data line. Tames fast edges, and shuts down damage if the downstream endpoint misbehaves.
- **Local electrolytic cap** = a cache/buffer very close to a fast consumer, so short-duration demand spikes don't back-pressure the origin server (PSU) through a laggy connection (wire inductance).

You can technically run without either and it'll often *look* fine on a short, lightly-loaded strip at low brightness. They're the difference between "works on the bench" and "works reliably when I drive all 46 LEDs to full white."

## Logic level (3.3 V → 5 V)

Uno R4 drives the data line at 3.3 V. WS2812 datasheets say DIN wants ≥ 0.7 · VCC = 3.5 V. In practice, most strips accept 3.3 V over short runs (< 30 cm), and just work. If you see unstable behavior specifically on longer runs or at the very start of the strip, add a 74AHCT125 (or similar) 3.3 V → 5 V buffer between D6's resistor and the strip's DIN. The chip is cheap, the fix is rock-solid, and it's worth having in the parts drawer.

## Troubleshooting quick reference

| Symptom | Likely cause |
|---|---|
| First LED is always the wrong color, rest is fine | Data-line reflection — missing/oversized series resistor, or resistor at the wrong end |
| Random flicker when strip goes bright | Missing bulk cap, or cap too small / too far from strip |
| Whole strip resets mid-animation | Brownout on the 5 V rail — undersized PSU, long thin wires, or missing cap |
| Nothing lights up at all | Missing common ground (Arduino GND ↔ PSU GND) |
| Colors are swapped (e.g. red shows as green) | Strip is RGB-ordered, firmware has GRB — flip the FastLED template parameter |
| Works on USB power but fails on PSU | Bad common-ground connection, or PSU ground/hot swapped |
