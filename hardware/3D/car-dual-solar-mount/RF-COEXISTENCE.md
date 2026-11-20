# RF coexistence: two LoRa radios 20 cm apart

Technical note for the [dual Heltec V4 solar car-roof mount](README.md).

This mount carries **two independent LoRa radios**: one running Meshtastic and one running
MeshCore. Their antennas sit about **20 cm apart**. This document explains, from RF first
principles and published component data, how the two radios affect each other, what the real
risks are, how to reduce them and how to measure them on your own build.

> **Status of the reference build.** The author uses this exact build every day, with both
> radios on the default US presets, and **it works**: both radios send and receive normally
> and no hardware failure has been observed. The analysis below does not say the build is
> broken. It explains a physical limitation that every co-located dual-radio build has, so
> that you can make an informed decision, especially if you change transmit power, antennas,
> boards or spacing.

---

## Contents

1. [Summary](#1-summary)
2. [Reference configuration](#2-reference-configuration)
3. [Frequency plan: no channel overlap](#3-frequency-plan-no-channel-overlap)
4. [Antenna-to-antenna isolation at 20 cm](#4-antenna-to-antenna-isolation-at-20-cm)
5. [Power arriving at the other radio](#5-power-arriving-at-the-other-radio)
6. [Interference mechanisms](#6-interference-mechanisms)
7. [Risk to the hardware](#7-risk-to-the-hardware)
8. [What it means in practice: a time-domain problem](#8-what-it-means-in-practice-a-time-domain-problem)
9. [Secondary effects](#9-secondary-effects)
10. [Mitigation, ranked by effectiveness](#10-mitigation-ranked-by-effectiveness)
11. [Test procedure](#11-test-procedure)
12. [Assumptions and limits of this analysis](#12-assumptions-and-limits-of-this-analysis)
13. [References](#13-references)

---

## 1. Summary

| Question | Short answer |
| --- | --- |
| Do the two channels overlap? | **No.** About 3.5 MHz separate the channel edges, with different bandwidths and sync words. Neither radio can decode the other. |
| Do the radios interfere with each other? | **Yes, but only while one of them is transmitting.** At 20 cm the antennas are only about 13–15 dB apart in RF terms. The transmitting radio overloads (blocks) the receiver of the other one for the duration of each packet. |
| How much traffic is lost? | Roughly the fraction of time the *other* radio spends transmitting. In a quiet mesh this is a few percent and usually unnoticeable. In a busy mesh, or when a radio relays a lot of traffic, it becomes noticeable. |
| Can a filter fix it? | **Not in practice.** 3.65 MHz at 908 MHz is a 0.4 % separation. ISM-band SAW filters are about 26 MHz wide. Only bulky cavity filters are selective enough. |
| Can it damage the radios? | **It is a margin question, not a certainty.** With the Heltec V4 high-power amplifier at full power, the power reaching the other radio's antenna port is in the +7 to +14 dBm range, at or above the +10 dBm maximum RF input rating of the SX1262 itself. On the V4 that power first meets the external front-end module, which limits what reaches the SX1262. The reference build has shown no failures, but long-term stress at full power is not characterised. |
| What helps most? | Vertical separation of the antennas, then more horizontal distance, then lower transmit power. |

---

## 2. Reference configuration

| Item | Radio A | Radio B |
| --- | --- | --- |
| Board | Heltec WiFi LoRa 32 V4 (ESP32-S3 + SX1262 + external front-end module) | Heltec WiFi LoRa 32 V4 |
| Firmware | Meshtastic | MeshCore (companion) |
| Preset | US **LongFast** | **USA/Canada (Recommended)** |
| Centre frequency | **906.875 MHz** | **910.525 MHz** |
| Bandwidth | 250 kHz | 62.5 kHz |
| Spreading factor / coding rate | SF11 / 4:5 | SF7 / 4:5 |
| Maximum conducted TX power (hardware) | 28 ± 1 dBm (high-power V4) | 28 ± 1 dBm (high-power V4) |
| Antenna | 915 MHz whip, ≈ 2 dBi, vertical | 915 MHz whip, ≈ 2 dBi, vertical |
| Antenna spacing | ≈ 20 cm, side by side, same height | |

The Heltec V4 is not a bare SX1262 board. Its RF path goes through an external
front-end module (GC1109 on V4.2, KCT8103L on V4.3) that contains the transmit power
amplifier, a receive low-noise amplifier (LNA) and the TX/RX switch. That matters for
sections 5 to 7.

---

## 3. Frequency plan: no channel overlap

```
 906.750      907.000                          910.494  910.556 MHz
    |==== LongFast ====|                           |=MC=|
    |    250 kHz       |<------ ≈ 3.49 MHz ------->|62.5|
              906.875                             910.525
              <------------- 3.65 MHz ------------>
```

- Centre-to-centre separation: **3.65 MHz**.
- Edge-to-edge gap: **≈ 3.49 MHz**.
- Different bandwidths, spreading factors and sync words.

A LoRa receiver only demodulates chirps with its own bandwidth, spreading factor and sync word,
inside its own channel. So there is **no co-channel interference** and **no false packet
reception** between the two networks. If the two radios were far apart, they would not affect
each other at all.

The problem is not the frequency plan. It is the **distance** between the antennas.

---

## 4. Antenna-to-antenna isolation at 20 cm

### 4.1 Electrical distance

```
λ = c / f ≈ 299 792 458 / 908.7 MHz ≈ 0.330 m      (≈ 33 cm)
d / λ = 0.20 / 0.330 ≈ 0.61
```

The antennas are about **0.6 wavelengths** apart.

| Boundary | Formula | Value | Where the antennas are |
| --- | --- | --- | --- |
| Reactive near field | λ / 2π | ≈ 5.3 cm | Outside it |
| Far field (Fraunhofer) | 2D² / λ, D ≈ 17 cm | ≈ 18 cm | On the edge of it |

At 20 cm the antennas are in the transition between near and far field. Far-field formulas
are only approximate here, so three independent methods are used below. They agree well.

### 4.2 Method 1: Friis free-space path loss

```
FSPL = 20 · log10(4π · d / λ) = 20 · log10(7.61) ≈ 17.6 dB
Coupling ≈ −FSPL + G1 + G2 ≈ −17.6 + 2.15 + 2.15 ≈ −13.3 dB
```

### 4.3 Method 2: empirical co-site isolation formulas

Antenna-isolation formulas widely used in cellular co-siting practice (gains in dBi towards
each other, distances in metres):

```
Horizontal (side by side):  I_H ≈ 22 + 20 · log10(d / λ) − (G1 + G2)
Vertical (one above other): I_V ≈ 28 + 40 · log10(d_v / λ)
```

For this build:

```
I_H ≈ 22 + 20 · log10(0.61) − 4.3 ≈ 22 − 4.3 − 4.3 ≈ 13.4 dB
```

### 4.4 Method 3: mutual impedance of two parallel half-wave dipoles

The classic induced-EMF result (Carter; see Balanis, *Antenna Theory*) gives the mutual
impedance Z21 of two side-by-side half-wave dipoles. Building the two-port impedance matrix
with Z11 ≈ 73 Ω (resonant dipole) and converting to S-parameters in a 50 Ω system:

| Spacing | d / λ | Z21 (Ω) | \|S21\| (coupling) | Friis estimate | I_H formula |
| --- | --- | --- | --- | --- | --- |
| 10 cm | 0.30 | 28.5 − j34.7 | −10.8 dB | −7.3 dB | 7.3 dB |
| 15 cm | 0.45 | −4.8 − j34.4 | −13.4 dB | −10.8 dB | 10.9 dB |
| **20 cm** | **0.61** | **−23.7 − j14.9** | **−14.5 dB** | **−13.3 dB** | **13.4 dB** |
| 30 cm | 0.91 | −6.4 + j18.8 | −17.8 dB | −16.9 dB | 16.9 dB |
| 40 cm | 1.21 | 15.2 + j0.8 | −19.8 dB | −19.4 dB | 19.4 dB |
| 66 cm | 2.00 | 1.1 + j9.4 | −24.2 dB | −23.7 dB | 23.7 dB |
| 100 cm | 3.03 | 1.7 + j6.0 | −27.7 dB | −27.3 dB | 27.3 dB |

### 4.5 Result

All three methods converge: **isolation ≈ 13–15 dB at 20 cm**. Allowing for the real
antennas (not ideal dipoles), the car roof acting as a ground plane, the solar panel nearby
and the plastic cover, a realistic range is **12–18 dB**.

Every doubling of the side-by-side distance only adds about **6 dB**. In co-site terms, 13–15 dB
is very little isolation. Base stations that share a mast typically need 30–60 dB or more.

---

## 5. Power arriving at the other radio

```
P_victim = P_TX − isolation − cable losses
```

Pigtail and connector losses (u.FL to SMA) are about 0.5–1 dB per radio and are ignored below,
which makes the estimate slightly pessimistic.

| Aggressor transmitting | Conducted TX power | Isolation | Power at the victim's antenna port |
| --- | --- | --- | --- |
| Meshtastic, high-power V4 at full power | ≈ 27–28 dBm | 14 dB | **≈ +13 to +14 dBm** |
| MeshCore at 22 dBm | 22 dBm | 14 dB | **≈ +8 dBm** |
| Either radio at 20 dBm | 20 dBm | 14 dB | ≈ +6 dBm |
| Either radio at 14 dBm | 14 dBm | 14 dB | ≈ 0 dBm |

The transmit power depends on firmware and settings. Meshtastic's US region allows up to
30 dBm and uses the hardware maximum by default. MeshCore's Heltec V4 build has used a 22 dBm
cap in some releases. Check what your radios are actually set to.

For comparison, a strong signal from a nearby node over the air is typically −40 to −70 dBm,
and a weak but decodable one is −110 to −130 dBm. **The other radio of the same mount arrives
50 to 140 dB stronger than anything the receiver is designed to handle comfortably.**

---

## 6. Interference mechanisms

When radio A transmits, radio B is affected through three mechanisms. All of them stop as soon
as A stops transmitting.

### 6.1 Blocking (receiver overload)

Blocking is the loss of sensitivity caused by a strong signal **outside** the wanted channel.
Semtech specifies about **88 dB blocking immunity at 1 MHz offset** for the SX1261/2, measured
relative to the sensitivity, and higher values at larger offsets (low to mid 90s dB for
offsets of a few MHz, depending on the datasheet and test conditions).

| Victim | Approx. sensitivity | Blocking immunity at 3.65 MHz (assumed) | Blocker that starts to hurt | Actual blocker | Excess |
| --- | --- | --- | --- | --- | --- |
| MeshCore, SF7 / 62.5 kHz | ≈ −127 dBm | ≈ 92 dB | ≈ −35 dBm | ≈ +13 dBm | **≈ 48 dB** |
| Meshtastic, SF11 / 250 kHz | ≈ −131 dBm | ≈ 92 dB | ≈ −39 dBm | ≈ +8 dBm | **≈ 47 dB** |

A blocker 45–50 dB above the blocking limit does more than reduce sensitivity a little. The
LNA and mixer go into compression and the AGC is driven to its limit. **While the other radio
transmits, the victim is effectively deaf**: it cannot decode packets from distant nodes, and a
packet it was already receiving is corrupted.

On the Heltec V4 the external LNA in the front-end module adds gain before the SX1262. That
improves sensitivity in normal use, but it also means the blocker is amplified (until the LNA
itself compresses) before it reaches the transceiver. A board with a front-end LNA is generally
**more** susceptible to strong nearby transmitters, not less.

### 6.2 Transmitter noise and spectral regrowth

No transmitter is perfectly clean. Phase noise of the synthesiser, plus intermodulation in the
power amplifier, spread a small amount of energy several MHz away from the carrier. Part of that
energy falls **inside the victim's channel**, where no filter in the victim can remove it,
because it is on the victim's own frequency.

The receiver's thermal noise floor is very low:

```
Noise floor = −174 dBm/Hz + 10 · log10(BW) + NF
MeshCore   (62.5 kHz, NF ≈ 6 dB): ≈ −120 dBm
Meshtastic (250 kHz,  NF ≈ 6 dB): ≈ −114 dBm
```

Even a transmitter whose noise at 3.65 MHz offset is 120–130 dB below its carrier, after only
14 dB of isolation, can raise the victim's in-channel noise floor by tens of dB. The exact value
depends on the PA and synthesiser of the specific board and is best measured (see section 11).
In practice, blocking (6.1) and transmitter noise (6.2) happen at the same time and have the same
result: the victim cannot hear weak signals while the aggressor transmits.

### 6.3 No coordination between the two protocols

Meshtastic and MeshCore each have their own channel-access logic (listen-before-talk, CAD,
random back-off), but each one only looks at **its own** channel. Neither knows that the other
radio, 20 cm away, is about to transmit. The two radios therefore transmit and receive
independently, and collisions in time are random.

---

## 7. Risk to the hardware

| Component | Rating / behaviour | Situation in this build |
| --- | --- | --- |
| SX1262 RF input | **+10 dBm absolute maximum** (Semtech-based module datasheets) | The incoming +8 to +14 dBm is at or above this value at the antenna port. |
| V4 front-end module (GC1109 or KCT8103L) RX path | First component hit by the incoming power. Its absolute maximum RX input is given in the module's own datasheet and was **not verified** for this note. | Takes most of the stress. LNA compression limits what reaches the SX1262. |

What this means:

- The SX1262's +10 dBm limit applies to its own pin. On the V4, the antenna port is not the
  SX1262 pin, so the +10 dBm figure cannot be applied directly. That is one reason the
  reference build works.
- Exceeding an absolute maximum rating does not mean instant failure. It means the
  manufacturer no longer guarantees reliability. Possible long-term effects are gradual
  sensitivity loss or failure of the LNA or RF switch.
- The risk grows with transmit power and shrinks quickly with isolation. Each 6 dB of extra
  isolation or reduced power divides the incoming power by four.
- **Boards without an external front end, or with a different external PA (for example 30 dBm
  modules), behave differently.** A bare SX1262 board receiving a 28–30 dBm neighbour at
  20 cm would see its own pin above its rating. Do not assume that a result from one board
  applies to another.

**Practical rule:** keep the power arriving at any radio's antenna port at or below about
0 dBm (that is, isolation ≥ transmit power) if you want to stay inside conservative margins for
any board. In this mount, that means about 14 dBm TX power at 20 cm, or more spacing at higher
power. The reference build runs above that margin and works. That is a reasonable personal
choice, but it is a choice.

---

## 8. What it means in practice: a time-domain problem

Because the interference only exists while one radio transmits, its effect is a **packet loss
rate** on the other radio, not a permanent loss of range.

### 8.1 Airtime per packet

Calculated with the standard Semtech time-on-air formula (16-symbol preamble, explicit header,
CRC on):

| Payload | Meshtastic LongFast (SF11 / 250 kHz) | MeshCore (SF7 / 62.5 kHz) |
| --- | --- | --- |
| 20 bytes | ≈ 0.40 s | ≈ 0.13 s |
| 50 bytes | ≈ 0.64 s | ≈ 0.21 s |
| 100 bytes | ≈ 1.01 s | ≈ 0.37 s |
| 200 bytes | ≈ 1.75 s | ≈ 0.65 s |

### 8.2 Loss model

A packet arriving at the victim is lost if any part of it overlaps a transmission of the
aggressor. With the aggressor sending R packets per second, each lasting T_A, and victim packets
lasting T_B, for random timing:

```
P_loss ≈ 1 − exp(−R · (T_A + T_B)) ≈ R · (T_A + T_B)      (for small values)
```

| Scenario | Aggressor packets per hour | T_A | T_B | Expected loss on the victim |
| --- | --- | --- | --- | --- |
| Quiet: Meshtastic sends its own telemetry, MeshCore receives | ≈ 30 | 0.64 s | 0.21 s | **≈ 0.7 %** |
| Moderate: Meshtastic also relays a normal mesh | ≈ 120 | 0.64 s | 0.21 s | **≈ 2.8 %** |
| Busy: Meshtastic relays a busy city mesh | ≈ 600 | 0.64 s | 0.21 s | **≈ 13 %** |
| MeshCore companion sending, Meshtastic receives | ≈ 30 | 0.21 s | 0.64 s | **≈ 0.7 %** |

Notes:

- In Meshtastic, the default `CLIENT` role **relays** packets from other nodes. In a busy area
  the Meshtastic radio may transmit far more than its own messages. Its "Air Util TX" statistic
  shows the real share of time it spends transmitting.
- A MeshCore companion radio does not relay, so it transmits only when you send something, plus
  adverts and acknowledgements. It is usually the quieter of the two.
- Both protocols have retries and redundancy (multiple relays, acknowledgements), so a lost
  packet is often recovered by a repeat. A few percent of loss is usually invisible to the user.

This is consistent with what is seen on the reference build: both radios work normally in
everyday use.

---

## 9. Secondary effects

| Effect | Mechanism | Relevance here |
| --- | --- | --- |
| Pattern distortion | At 0.6 λ each antenna acts as a parasitic element for the other. The horizontal pattern gains ripple of roughly 1–3 dB, with slightly less gain towards the other antenna. | Low. Small range change in some directions. |
| Detuning (SWR change) | Mutual impedance changes the input impedance of each antenna. The dipole model gives S11 ≈ −15 dB at 20 cm, a good match. | Low. |
| Intermodulation | If both transmit at the same moment, some of each signal reaches the other PA and mixes: 2·f1 − f2 ≈ 903.2 MHz and 2·f2 − f1 ≈ 914.2 MHz. | Low. Rare, short, and weak compared with the carriers. |
| Received-signal statistics | Noise floor and RSSI readings taken while the other radio transmits look worse than they really are. | Keep in mind when reading diagnostics or doing site surveys. |
| Car roof and solar panel | The roof is a large ground plane close to the antennas. The panel's cells and wiring are conductive. Both change patterns and coupling slightly. | Moderate. The real isolation must be measured on the car (section 11). |

---

## 10. Mitigation, ranked by effectiveness

| # | Measure | Expected gain | Cost / trade-off |
| --- | --- | --- | --- |
| 1 | **Vertical separation.** Place one antenna directly above the other, collinear. Vertical whips have a null along their axis. | I_V ≈ 28 dB at 1 λ (33 cm), ≈ 40 dB at 2 λ (66 cm) | Needs a small mast. Taller mount. |
| 2 | **More horizontal distance.** For example, one antenna at the mount and the other at the far end of the solar panel. | +6 dB per doubling: ≈ 20 dB at 40 cm, ≈ 24 dB at 66 cm | Longer cable for one radio, small cable loss. |
| 3 | **Lower transmit power** on the busier radio (usually Meshtastic). | 1 dB per dB. 28 → 22 dBm = 6 dB less power at the other radio (one quarter). | Less range for that network. |
| 4 | **Reduce relayed traffic** (for example Meshtastic role or rebroadcast settings, if appropriate for your mesh). | Fewer transmissions = proportionally less loss on the other radio. | Less help to the mesh. Follow your community's guidance. |
| 5 | **Filtering.** | Not practical. 3.65 MHz apart at 908 MHz needs a cavity filter per radio. SAW filters for the 902–928 MHz band pass both channels. | Large, heavy, expensive. |
| 6 | **Separate the radios physically** (for example one on the roof, one inside the car). | Car body adds large isolation. | Loses the single-mount concept. |

Combining 2 and 3 (for example 40 cm and 22 dBm) already moves the power at the other radio from
about +14 dBm to about +2 dBm.

---

## 11. Test procedure

The theory above gives good estimates, but the car roof, solar panel, real antennas and real
boards change the numbers. These tests measure them. Fully charge both radios first.

### Test 1: antenna isolation with a vector network analyser (most valuable)

**Equipment:** NanoVNA or similar, two SMA cables, calibration kit.

1. Calibrate the VNA (SOLT, full two-port) from 900 to 920 MHz at the ends of the cables.
2. Install the mount on the car in its normal position, with the cover on.
3. Disconnect both antennas from the radios. Connect antenna A to port 1 and antenna B to port 2.
4. Read **S21** at 906.875 MHz and 910.525 MHz. This is the isolation in dB (as a negative number).
5. Also read **S11** and **S22**: they show whether each antenna is well matched in place.
6. Repeat for any alternative layout you are considering (more spacing, vertical stacking).

**Interpretation:**

```
P_at_victim = P_TX + S21
```

| P_at_victim | Meaning |
| --- | --- |
| ≤ 0 dBm | Conservative margin for any board. |
| 0 to +10 dBm | Above conservative margin. Works on boards with a front-end module, long-term stress uncertain. |
| > +10 dBm | Above the SX1262's own input rating. Reduce power or increase isolation, especially with boards without a front end. |

### Test 2: packet loss caused by the other radio

**Equipment:** the mount, a third radio of the same network as the victim, placed far enough
away to give a moderate signal (for example −90 to −110 dBm RSSI).

1. **Baseline.** Turn the aggressor radio off. From the third radio, send 100 messages to the
   victim at a fixed interval (for example every 10 s). Record how many arrive, with RSSI and SNR.
2. **With aggressor.** Turn the aggressor on and make it transmit regularly (for example send a
   message or a position every 5–10 s on its own network). Repeat the 100 messages.
3. **Swap roles.** Repeat steps 1 and 2 with the other radio as the victim.
4. Compare the delivery rates. The difference is the loss caused by the other radio.
5. Compare with the model in section 8.2 using the aggressor's real transmit rate.

### Test 3: real transmit duty cycle

Over a normal day of use, record:

- **Meshtastic:** "Air Util TX" and "Channel Utilization" from the device metrics.
- **MeshCore:** packets sent and airtime from the device statistics, if available.

The aggressor's transmit percentage is approximately the loss you should expect on the other
radio (section 8.2).

### Test 4: spectrum view (optional)

**Equipment:** RTL-SDR or another SDR, with a **20–30 dB attenuator** in front of it.

1. Put the SDR antenna near the mount.
2. While Meshtastic transmits, look at the spectrum around 910.525 MHz. You will see the noise
   skirt of the LongFast transmission reaching the MeshCore channel.
3. Do the same while MeshCore transmits, looking around 906.875 MHz.

An inexpensive SDR has limited dynamic range, so this test is qualitative: it shows the effect
but does not measure it precisely.

### Test 5: long-term check (optional)

Every few months, repeat Test 2 at the same place with the aggressor off. A gradual drop in the
victim's RSSI or SNR for the same distant node, with no other change, could indicate front-end
degradation.

---

## 12. Assumptions and limits of this analysis

- Antennas modelled as ideal vertical half-wave dipoles of about 2 dBi in free space. Real whips,
  the car roof and the solar panel change the numbers by a few dB. Test 1 gives the real value.
- Blocking and sensitivity values come from Semtech SX1261/2 public data. The Heltec V4's
  external front-end module changes both, and its absolute maximum ratings were not verified
  for this note.
- Transmitter noise at 3.65 MHz offset was not measured. Only the mechanism is described.
- Transmit power depends on firmware version, region and settings.
- The loss model assumes random, independent timing of the two radios.

This is an engineering estimate to help builders make informed decisions, not a certification
or a guarantee.

---

## 13. References

- Semtech, *SX1261/2 Long Range, Low Power, sub-GHz RF Transceiver* datasheet: sensitivity,
  88 dB blocking immunity at 1 MHz offset, absolute maximum ratings.
- Semtech, *LoRa Modem Design Guide* / AN1200.13: LoRa time-on-air formula.
- Heltec Automation, *WiFi LoRa 32 V4* product page, wiki and datasheet: 28 ± 1 dBm high-power
  version, GC1109 / KCT8103L front-end module with receive LNA.
- C. A. Balanis, *Antenna Theory: Analysis and Design*: mutual impedance of parallel dipoles,
  near-field and far-field boundaries.
- H. T. Friis, "A Note on a Simple Transmission Formula", *Proc. IRE*, 1946.
- Empirical horizontal and vertical antenna-isolation formulas used in cellular co-site
  engineering.
- Meshtastic documentation: LoRa configuration and modem presets (US LongFast, 906.875 MHz).
- MeshCore documentation and community guides: USA/Canada recommended preset
  (910.525 MHz, SF7, 62.5 kHz, CR 4:5).

---

Covered by the repository [LICENSE](../../../LICENSE).
