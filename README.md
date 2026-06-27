# mp3

A small music player you fill from your phone. No store, no streaming, no account.
You upload your own files, build playlists, and it just plays them.

Part of [marmota](https://github.com/gustavosilveiragss/marmota).

## The idea

The player has almost no interface of its own. When you want to manage it, you open
a connection from your phone, upload tracks and arrange playlists, then disconnect
and the device goes back to being a quiet music box.

## Plan

* Audio out through a DAC, driven by an ESP32.
* A connectable interface (Bluetooth or WiFi, only on demand) to upload files and
  edit playlists from the phone.
* Local storage for the library, playback controlled by a few physical buttons.

## Principles

Battery is the boss. Radios (Bluetooth, WiFi) only turn on when you ask to manage
the library, never during playback. The CPU runs as slow and as asleep as the audio
allows. Same rule across all marmota devices.

## Status

Idea stage. No code yet.
