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
* A small 0.91 inch OLED shows the track and playlist. Simplest display that does the
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

Rough per unit, in BRL, for a batch on a custom PCB. Bare chips soldered to our own
board (JLCPCB) replace the modules, which is the main saving at volume.

| Part | Note | Price |
|---|---|---|
| ESP32-WROOM-32 module | bare, dual core for A2DP | 25 |
| PCM5102A chip | I2S DAC on the PCB | 10 |
| Headphone amp chip | NS8002 or similar | 3 |
| microSD socket | on the PCB | 3 |
| microSD 8 GB card | reuse an old card if possible | 8 |
| OLED SSD1306 0.91 128x32 | track and playlist | 16 |
| Buttons | play, next, prev, volume x2 | 8 |
| 3.5mm jack | headphone out | 3 |
| 18650 cell | recycled where possible | 5 |
| 18650 holder | | 4 |
| TP4056 chip | charger on the PCB | 3 |
| Power switch | slide | 3 |
| LED | status | 1 |
| Passives | on the PCB | 6 |
| Custom PCB | JLCPCB, per unit at volume | 8 |
| Case | 3D print | 5 |

Component cost is about **R$100** per unit, down from R$165 in the module spec.

Suggested selling price, with build time, failures, packaging and margin (about 2x
to 2.5x the BOM): **R$220 to R$280**.

## Principles

Battery first and friction on purpose: your files, your playlists, no feed. Radios
only when you ask, with Bluetooth as the one deliberate exception for wireless sound.

## Status

Idea stage, decisions locked. No code yet. Board is a classic ESP32 for A2DP.
