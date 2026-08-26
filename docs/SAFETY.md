# Safety and Authorized Use

## Default safety boundary

The default `analyzer` environment defines:

```text
RF_LAB_TX_ENABLED=0
```

Active RF Test code is unavailable in that build. The Tools card shows `RX ONLY`, and Serial transmit commands return an unavailable message. This is the recommended firmware for analysis, demonstrations, development, and general diagnostics.

The same compile-time boundary applies to Sub-GHz raw Replay. Selecting an
ETSI/FCC policy does not enable TX in the default build.

## Controlled-lab profile

`authorized_rf_lab` compiles active RF test functionality. It is intended only for controlled work where all of the following are true:

- you own the equipment or have explicit written authorization;
- the test objective and affected devices are defined;
- the setup is inside an effective RF shield box or Faraday enclosure;
- no public, third-party, safety-critical, medical, emergency, industrial-control, or navigation system can be affected;
- operation complies with local spectrum and equipment regulations;
- a responsible operator can immediately stop the test.

Authorization to assess one device does not automatically authorize interference with every device using the same band.

The `RX ONLY`, `ETSI`, and `FCC` selections are firmware allowlists. They do
not certify the hardware, grant spectrum authorization, or automatically
enforce every local rule for power, duty cycle, occupied bandwidth, antenna,
or equipment approval.

## Pre-test checklist

1. Use the receive-only analyzer first to characterize the enclosure and intended setup.
2. Verify shielding with appropriate external monitoring equipment.
3. Inventory every device inside the enclosure.
4. Disconnect or isolate unintended radios.
5. Confirm stable power and radio cooling.
6. Define a maximum test duration and stop condition.
7. Keep physical access to power and the `B`/stop control.
8. Record the firmware commit, build profile, configuration, and authorization scope.

## During a test

- Keep the enclosure closed.
- Monitor unexpected resets, supply sag, and temperature.
- Stop immediately if energy is observed outside the intended enclosure.
- Do not use the device near operational networks or other spectrum users.
- Do not leave active RF Test unattended.

## After a test

1. Stop RF Test and verify the UI reports standby.
2. Power down or return to the `analyzer` build.
3. Preserve authorized test records without exposing sensitive third-party data.
4. Inspect hardware for overheating or supply damage.

## Measurement limitations and compliance

The nRF24-based analyzer is not a calibrated spectrum analyzer, compliance receiver, or certification instrument. Its carrier-hit percentages cannot establish regulatory emission levels. Use suitable calibrated equipment for conducted/radiated power, occupied bandwidth, spurious emissions, frequency accuracy, shielding effectiveness, and compliance decisions.

## Distribution recommendation

Distribute or flash the `analyzer` artifact by default. Treat `authorized_rf_lab` artifacts as restricted laboratory builds, label them clearly, and do not make the lab profile the default environment.
# RF Environment safety boundary

`RF_LAB_TX_ENABLED=0` remains the default. All Environment analysis is passive and remains available in that build; probe TX members and commands are compile-gated. Carrier hits are displayed only as occupancy/relative activity/interference score—never dBm, RSSI, exact power, or protocol detection.

The lab probe is limited to one channel, interval ≥20 ms, duration ≤60 s, packet count ≤1000, explicit start, responsive stop, and automatic RX restoration. It does not use `REUSE_TX_PL`, constant carrier, continuous CE-high operation, channel sweeping, or 100% duty cycle.
