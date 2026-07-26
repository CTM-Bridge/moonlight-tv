# Moonlight TV (CTM Bridge)

> This is a fork of [Moonlight TV](https://github.com/mariotaku/moonlight-tv) by mariotaku,
> extended with my **[CTM Bridge](https://github.com/CTM-Bridge/CTM-USBIP)**:
> controllers paired to the TV — and the Magic Remote as a pointer + keyboard —
> show up on the gaming PC as native USB devices, with full input, rumble and
> controller audio.

## Support

One person, late nights: controllers were just the start — native AMF
streaming, a custom low-latency codec and a bigger webOS app are in the pipe.
If CTM Bridge saved you some hassle, coffee speeds them up.

[![Support me on Ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/ciprianteodormisaila)

Moonlight TV is a community version of [Moonlight GameStream Client](https://moonlight-stream.org/),
made for large screens, running on LG webOS TVs.

## What the CTM fork adds

* **Controllers over CTM Bridge** — DualShock 4 / DualSense / Xbox / Steam Puck
  paired to the TV appear on the PC as native USB devices: input, rumble, LEDs
  and controller audio (DS4/DS5 speaker + headset, with headphone-jack
  auto-route).
* **Magic Remote → PC mouse + keyboard** while streaming: pointer, click,
  wheel, and D-pad/OK as arrow/enter keys — single input authority, no double
  cursor.
* **Mice & keyboards** connected to the TV (Bluetooth or USB dongle) bridge and
  auto-plug the same way.
* **On-stream CTM panel** — plug/unplug devices, per-controller audio modes and
  volumes, live bridge status.
* **Auto-reconnect** on network hiccups, with bridged controllers kept alive
  through the retry.

Requires the [CTM Bridge Windows service](https://github.com/CTM-Bridge/CTM-USBIP)
on the gaming PC.

## Download

Grab `com.limelight.webos_<version>.ipk` from the
[combined CTM Bridge release](https://github.com/CTM-Bridge/releases/releases/latest)
and install with [dev-manager-desktop](https://github.com/webosbrew/dev-manager-desktop)
or `ares-install`.

## Building

Clone [ctm-bridge-webos](https://github.com/CTM-Bridge/ctm-bridge-webos) as a
**sibling directory** of this repo — the embedded `ctmbridge` lib compiles the
bridge core straight from it (`-DCTM_BRIDGE_DIR=<path>` overrides the
location). Then build as usual for webOS.

## Upstream

Everything else — general features, platform support, documentation — is
upstream [mariotaku/moonlight-tv](https://github.com/mariotaku/moonlight-tv)
([wiki](https://github.com/mariotaku/moonlight-tv/wiki)). Credits to
[moonlight-embedded](https://github.com/irtimmer/moonlight-embedded) for the
original libgamestream and decoder components.
