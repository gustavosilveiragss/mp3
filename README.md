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

Bluetooth only. The player streams to wireless headphones over A2DP, so the board is a
classic dual core ESP32 (the C3 cannot do A2DP audio). No DAC, no amp, no headphone jack:
fewer parts, smaller board, lower cost.

Bluetooth breaks the usual marmota rule of radios off, on purpose. Wireless sound is worth
a radio. The WiFi portal and Bluetooth never run together: opening the portal to upload
tracks turns Bluetooth off, and the other way around.

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

Rough per unit, in BRL, retail Brazilian prices. Modules on perfboard, hand assembled, no custom PCB.
Ordering the cheap parts from AliExpress in bulk cuts about 20-30% off.

| Part | Note | Price |
|---|---|---|
| ESP32 classic devkit | dual core, needs A2DP | 35 |
| microSD module | library storage | 6 |
| microSD 8 GB card | reuse an old card if possible | 8 |
| OLED SSD1306 0.96 I2C | track and playlist | 20 |
| MT3608 boost | 3.7 V to 5 V for the ESP | 10 |
| Buttons | play, next, prev, volume x2, 6x6 tact as in the pin | 2 |
| 18650 cell | | 15 |
| 18650 holder | | 4 |
| TP4056 USB-C | charger | 5 |
| Power switch | SS-12D00 slide, as in the pin | 2 |
| LED | status | 1 |
| Passives, diode, wires | | 8 |
| Perfboard | | 6 |
| Female headers | ESP socket | 5 |
| Case | own printer, PETG filament | 4 |

Component cost is about **R$130** per unit (range R$105-160).

Price is cost plus assembly time plus a light 25% markup to cover fees, failures and
the odd warranty. About R$130 in parts, 3 hours at R$25 an hour, then +25%:
**around R$255**.

## Principles

Battery first and friction on purpose: your files, your playlists, no feed. Radios
only when you ask, with Bluetooth as the one deliberate exception for wireless sound.

## Build

Prototype first: wire everything on a breadboard with jumpers (almost no soldering) and iterate
on pins, firmware, power and heat. Then measure every part (`medidas/`). Perfboard and case come
last, once the prototype is validated. The battery runs warm, so the case is vented.

Then hand solder on perfboard. Follow the marmota
[build standard](https://github.com/gustavosilveiragss/marmota#build-standard):
minimal solder, strain relief. The board is never screwed and never held by foam: it slides into
printed rails in the case and is locked with hot glue. Strain relief the battery and the display
wiring.

## Status

Idea stage, decisions locked. Firmware scaffold compiles. Next step: breadboard prototype. Board is a classic ESP32 for A2DP.
