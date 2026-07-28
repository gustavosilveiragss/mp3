# mp3

A small music player you fill from your phone. No store, no streaming, no account.
You upload your own files, build playlists, and it just plays them, over a cable or
to Bluetooth headphones.

Part of [marmota](https://github.com/gustavosilveiragss/marmota).

## The idea

The player has almost no interface of its own. When you want to manage it, you raise
its WiFi on demand, upload tracks and arrange playlists from the phone, then it goes
back to being a quiet music box. Day to day you drive it with a few buttons and a
small screen.

## Audio out

Two ways to listen:

* **Wired:** a PCM5102A I2S DAC into a dedicated headphone amp, out to a 3.5mm jack.
  Clean and cheap on battery, radio stays off.
* **Bluetooth:** the player streams to wireless headphones over A2DP. This is a core
  feature, so the board is a classic dual core ESP32 (the C3 cannot do A2DP audio).

Bluetooth breaks the usual marmota rule of radios off, on purpose. Wireless is worth
a radio. Pick wired when you want the longest battery life, Bluetooth when you want
no cable. Your choice, every time.

## Storage

A microSD card holds the library. Music is big, so you upload files onto the card
from the phone over WiFi. Easy to grow, easy to swap.

## Control

* Buttons: play and pause, next, previous, volume up and down.
* A small 0.96 inch OLED shows the track and playlist. Simplest display that does the
  job, same SSD1306 driver as the rest of the family.
* One LED for status.

## Manage (WiFi on demand)

Press the config button, the player raises an access point, you open the page on your
phone, upload tracks and edit playlists, then the radio sleeps. Same pattern as the pin.

## Power

* Single 18650 with a holder, recycled where possible.
* TP4056 USB-C charger with protection.
* Wired playback sips current. Bluetooth costs more, as expected.

## BOM and cost

Rough per unit, in BRL. Modules on perfboard, hand assembled, no custom PCB. Nothing
is removed from the earlier spec. The cost drop comes from sourcing the cheap parts on
AliExpress in one consolidated order. Batteries stay local. Import tax can nibble at
the savings, so these are the good case.

| Part | Note | Price |
|---|---|---|
| ESP32 classic devkit | dual core, needs A2DP, sourced cheap | 25 |
| PCM5102A DAC module | I2S audio out | 12 |
| Headphone amp module | NS8002 or similar | 5 |
| microSD module | library storage | 4 |
| microSD 8 GB card | reuse an old card if possible | 5 |
| OLED SSD1306 0.96 I2C | track and playlist | 10 |
| Buttons | play, next, prev, volume x2 | 5 |
| 3.5mm jack | headphone out | 3 |
| 18650 cell | recycled where possible | 5 |
| 18650 holder | | 3 |
| TP4056 USB-C | charger | 2 |
| Power switch | slide | 2 |
| LED | status | 1 |
| Passives and wires | | 6 |
| Perfboard | | 4 |
| Case | 3D print, commissioned, screw-openable | 18 |
| Packaging and stickers | simple box, small labels | 5 |

Component cost is about **R$115** per unit, down from R$160 by cheaper sourcing alone.

Price is cost plus assembly time plus a light 25% markup to cover fees, failures and
the odd warranty. About R$115 in parts, 3 hours at R$25 an hour, then +25%:
**around R$240**.

## Principles

Battery first and friction on purpose: your files, your playlists, no feed. Radios
only when you ask, with Bluetooth as the one deliberate exception for wireless sound.

## Build

Hand soldered on perfboard. Follow the marmota
[build standard](https://github.com/gustavosilveiragss/marmota#build-standard):
minimal solder, strain relief, panel-mount hardware, board screwed to standoffs.
Strain relief the battery, the headphone jack and the display wiring.

## Status

Idea stage, decisions locked. No code yet. Board is a classic ESP32 for A2DP.
