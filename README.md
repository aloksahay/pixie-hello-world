Firefly Hello World
===================

This a very quick demo application to get you up and running, writing
firmware for Firefly. You can clone this project an jump in and just
start hacking away.


Installing the ESP-IDF
----------------------

The Firefly is based on the ESP32-C3 MCU, so the toolchain provided
from Espressif (the creators of the ESP32 processor family) is the
first thing you'll need to install.

See [Espressif's Getting Started](https://docs.espressif.com/projects/esp-idf/en/v6.0.1/esp32c3/get-started/index.html)
documentation for your platform.


Where to begin?
---------------

Start by cloning this repository, incuding all its submodules recursively:

```
/home/ricmoo> git clone --recursive https://github.com/firefly/pixie-hello-world.git
```

The Firefly framework is called **Hollows**, which is designed to be simple
(for C) to write your firmware in.

It provides core APIs to:

- The scene graph of hierarchal graphic nodes for showing images, displaying text, etc.
- Getting device input from the keypad
- Receive and send messages over BLE from a Firefly web application

Check out the `main/main.c` for the entry point into Hollows.


License
-------

MIT license.
