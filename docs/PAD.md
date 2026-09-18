# EF2Pad

EF2Pad is EF2SDK's controller layer.

The first implementation intentionally does not wrap PS2SDK libpad or the
ROM PADMAN service. An embedded `ef2pad.irx` owns the controller SIO2
transport and exposes a compact private RPC protocol to the freestanding EE
runtime.

## Goals

- behave consistently across retail fat and slim PS2 revisions;
- avoid indefinite waits when a controller or transport misbehaves;
- recover automatically after unplug/replug;
- keep the EE API independent from PADMAN's old/new shared-memory ABIs;
- expose enough raw state to diagnose unusual controllers and clones.

## Alpha.18 baseline

The first backend supports both physical controller ports and polls SIO2
synchronously when the EE requests a snapshot.

Supported response layouts include:

- digital/PS1-style 5-byte reports;
- 9-byte analog reports;
- 21-byte DualShock 2 pressure reports.

The controller ID determines the expected packet length. The backend accepts
the standard `0x42` poll protocol and keeps neutral analog values for devices
that only expose digital data.

### Hardware compatibility measures

The SIO2 transfer layer has a bounded timeout instead of waiting forever on a
missing interrupt.

Each port tracks the SIO2 STAT70 framing bit. When that framing is active the
reply is shifted in the same way used by established PS2 pad drivers.

A second timing profile is attempted automatically if the normal timing
profile fails. This covers the alternate SIO2 timing used by some analog
controller modes without forcing every controller onto one profile.

Disconnects are normal state transitions. A later successful poll restores
the port automatically and increments a reconnect counter.

## Public API

`ef2_pad_init()` installs the embedded IOP service.

`ef2_pad_poll()` reads one physical port, while `ef2_pad_poll_all()`
updates both ports with one RPC call.

Button bits are active-high in EF2Pad: a set bit means the button is pressed.
The public state also exposes edge-triggered `pressed` and `released`
masks, analog axes, pressure values, raw controller ID, timing profile and
error/reconnect counters.

## Validation plan

Emulator validation is useful for protocol regressions, but it is not treated
as sufficient proof of controller compatibility.

Real-hardware validation should cover at minimum:

- early fat PS2;
- later fat PS2;
- SCPH-700xx slim;
- SCPH-750xx/770xx slim;
- SCPH-790xx slim;
- SCPH-900xx slim;
- official DualShock 2;
- original digital/PS1 controller where available;
- at least a few common third-party/clone pads;
- unplug/replug during active polling.

Multitap and rumble are deliberately deferred until the two native ports have
been validated on real hardware.


## Alpha.19 input usability

Alpha.19 adds convenience helpers for the three common button states:

- `ef2_pad_is_held()` for continuous/held input;
- `ef2_pad_was_pressed()` for the rising edge;
- `ef2_pad_was_released()` for the falling edge.

`ef2_pad_axis_deadzone()` converts a raw 0..255 stick axis to a signed
-32768..32767 value and removes center noise with a caller-selected deadzone.
Capability helpers report whether the current packet contains analog axes or
DualShock 2 pressure values.

The IOP backend also attempts a minimal DualShock configuration sequence once
per connection. Known digital/analog DualShock IDs are asked to enter config
mode, switch to locked analog mode, enable the twelve pressure channels when
supported, and leave config mode. Every step remains bounded by the SIO2
timeout. Failure is non-fatal: a digital-only controller or clone continues
using the mode in which it already responds.

The alpha.19 smoke test exercises held input by letting D-pad Up/Down repeat
volume changes while held. Moving either analog stick changes the framebuffer
color: left X/Y control red/green, right X controls blue, and right Y controls
brightness. Returning both sticks to their deadzones restores the normal
audio-state color.


## Alpha.20 rumble

EF2Pad now configures the standard DualShock actuator alignment directly with
SIO2: actuator 0 is mapped to the small on/off motor and actuator 1 to the
large 0..255 motor. The command is best-effort and does not make controller
initialization fail when a digital pad or clone does not support actuators.

`ef2_pad_set_rumble()` stores the requested motor values in the IOP service.
They are folded into the next normal `0x42` pad poll, so rumble does not add a
second SIO2 transaction to every input frame. `ef2_pad_stop_rumble()` clears
both motors.

The public pad state exposes whether actuator alignment succeeded and the
currently requested motor values.

The alpha.20 smoke test uses held R1 for the small motor and held R2 for the
large motor. On a pressure-capable DualShock 2, R2 pressure controls the large
motor intensity; otherwise R2 falls back to full intensity.


## Alpha.21 dual-port and Multitap topology

EF2Pad now keeps independent state for two physical ports and up to four slots
per port. The original `ef2_pad_poll(port)` API remains source-compatible and
maps to slot zero.

New APIs expose Multitap-aware topology and slots:

- `ef2_pad_refresh_topology()`;
- `ef2_pad_get_slot_count()`;
- `ef2_pad_poll_slot()`;
- `ef2_pad_set_rumble_slot()`;
- `ef2_pad_stop_rumble_slot()`.

The direct SIO2 backend implements the small Multitap management protocol
without MTAPMAN/XSIO2MAN runtime dependencies. It probes slot count through
the logical SIO2 endpoint `port | 2` and selects a slot before a normal pad
poll. Ports with no detected Multitap fall back to exactly one native slot.

Slot selection is retried and a failure triggers topology rediscovery, so
removing a Multitap can fall back to the native controller port rather than
leaving a stale four-slot topology.

This implementation is CI/build validated only. Multitap behavior is not
marked hardware-validated until it can be tested with a real Multitap and
multiple controllers.
