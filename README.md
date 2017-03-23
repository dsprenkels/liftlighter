# LiftLighter

I once found an old lift indicator in the FNWI building (credits to Luuk van
Summeren). If you don't know what I mean, [here is an exampe][photo].

The indicator has ten lights: down, 1, 2, 3, 4, 5, B, S, K and up. This
repository contains the KiCad files for the circuit board and the software
for the _Atmega8_ microcontroller.

## Components

The board (as you can see in the schematic) is reasonably modular. If you are
going to build your own setup, I recommend doing so with a 3.3V power supply
with LEDs instead of 12V lights.

For a breadboard-proof-of-concept:
- 1x Atmega8 microcontroller
- 11x LED + resistor
- 1x 32.768kHz crystal
- 2x 22 pF capacitors (for use with crystal)
- 1x pull-up resistor (30-80kΩ) for reset
- 1x control button

For a very complete setup, add:
- 1x reset button
- 1x switch or pin header w/ jumper for PD5.
- 12V setup
  * 1x 3.3V voltage regulator
  * 1x 100nF capacitor (for use with voltage regulator)
  * 1x 10μF capacitor (for use with voltage regulator)
  * 10x 12V led light + transistor + resistor (instead of LEDs)
  * 1x fuse <!-- TODO calculate threshold value -->
  * 1x 3.3V backup battery
  * 1x diode (for use with backup battery)
  * 1x diode (for prevention of backflow current)
- Pin headers (whichever amount you are going to need)

## Getting started

> If you are interested in this project, ask me personally about it. Although
  I try to keep my documentation complete and readable, I think it may be
  hard to get your setup correctly. Anyway, here goes:

I use a Raspberri Pi to flash my microcontrollers, for this I use a customized
version of the [`avrdude`] tool which uses `linuxspi` ([more info about
this][Raspberri Pi as an AVR Programmer]). If you have your own methods to
flash your microcontrollers you can just use them. In this README I assume you
will be using my method.

1. Install [`KiCad`] (I used version 4.0.3)
2. Install `gcc-avr`, `avr-libc` and **Kevin Cuzner's** [`avrdude`] tool on
   your Raspberri Pi.
3. `git clone https://github.com/dsprenkels/liftlighter.git`
4. `cd liftlighter`
5. Wire up the microcontroller and all the components on a breadboard
   according to the KiCad `liftlighter.sch` file. I recommend tweaking the
   setup, using LEDs instead of 12V lights (which allows you to ditch all the
   transistors). Parts like the 32.768kHz crystal _are_ important!
6. Connect the Raspberri Pi's GPIO pins to the microcontroller (see below).
7. `make flash`
8. Send me an e-mail, because something did not work as expected.

If all went well, the Atmega8 microcontroller should now be executing the
liftlighter


## Wire connections

All the wiring used during the normal operation of the program can be viewed
in `pcb/liftlighter.sch` using KiCad. Furthermore, a PCB layout can be found
in `pcb/liftlighter.kicad_pcb`.

To flash the microcontroller, I use my Raspberri Pi and Kevin Cuzner's
[`avrdude`] tool. When using this setup, you will have to connect the following
(physically numbered) pins:

| type | Pi  | Atmega8  |
| ---- | --- | -------- |
| Vcc  | 17  |  7 & 2   |
| GND  | 20  |  8 & 22  |
| RST  | 22  |  1 (PC6) |
| MOSI | 19  | 17 (PB3) |
| MISO | 21  | 18 (PB4) |
| SCLK | 23  | 19       |

You can actually use any free GPIO pin for reset, as long as you tell `arvdude`
which one you are using [in the Makefile][reset pin].

`make fuse` calls `avrdude` and the Pi should immediately see the Atmega8. If
it does not work, check your connections. More reading about using a Raspberri
Pi to flash a microcontroller can be found [on Cuzner's blog][Raspberri Pi as
an AVR programmer]. Additionally, you may find the [datasheet for the
Atmega8][atmega8] helpful. You can also write me an email.

## Questions

Feel free to send me an email on my Github associated e-mail address.

[photo]: http://farm4.static.flickr.com/3210/3149843161_4fa5ab7734.jpg
[`KiCad`]: http://kicad-pcb.org/
[`avrdude`]: https://github.com/kcuzner/avrdude
[Raspberri Pi as an AVR programmer]: http://kevincuzner.com/2013/05/27/raspberry-pi-as-an-avr-programmer/
[reset pin]: https://github.com/dsprenkels/liftlighter/blob/master/Makefile#L9
[atmega8]: http://www.atmel.com/Images/Atmel-2486-8-bit-AVR-microcontroller-ATmega8_L_datasheet.pdf
