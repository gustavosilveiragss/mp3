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

Rough per unit, in BRL, Brazilian shops. Modules on perfboard, hand assembled. No
custom PCB. Kept cheap with a smaller display, a recycled cell and a printed case.

| Part | Note | Price |
|---|---|---|
| ESP32 classic devkit | dual core, needs A2DP | 40 |
| PCM5102A DAC module | I2S audio out | 18 |
| Headphone amp module | NS8002 or similar | 8 |
| microSD module | library storage | 8 |
| microSD 8 GB card | reuse an old card if possible | 8 |
| OLED SSD1306 0.96 I2C | track and playlist | 12 |
| Buttons | play, next, prev, volume x2 | 8 |
| 3.5mm jack | headphone out | 3 |
| 18650 cell | recycled where possible | 5 |
| 18650 holder | | 4 |
| TP4056 USB-C | charger | 2 |
| Power switch | slide | 3 |
| LED | status | 1 |
| Passives and wires | | 8 |
| Perfboard | | 5 |
| Case | 3D print or reused | 5 |

Component cost is about **R$140** per unit.

Price is just cost plus time, no profit markup. About R$140 in parts plus roughly
3 hours of assembly at R$25 an hour: **around R$210**.

## Principles

Battery first and friction on purpose: your files, your playlists, no feed. Radios
only when you ask, with Bluetooth as the one deliberate exception for wireless sound.

## Status

Idea stage, decisions locked. No code yet. Board is a classic ESP32 for A2DP.
