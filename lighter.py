#!/usr/bin/env python3

import atexit
import collections
import datetime
import random
import time

from RPi import GPIO

PINS = [26, 23, 21, 19, 15, 13, 11, 7, 5, 3]
FREQ = 30


class Light(object):
    def __init__(self, cn):
        self.state = "unset"
        self.cn = cn

    def __repr__(self):
        return "pin '{}'".format(self.cn)


def decide_state_K(light, d):
    """Decide the current state for the K (random) light"""
    if light.state == "unset":
        light.state = "on" if random.randint(0, 1) else "off"
    if random.randint(1, FREQ*60*60) == 1:
        # change once per hour on avarage
        light.state = "on" if light.state == "off" else "off"


def decide_state_S(light, d):
    """Decide the current state for the S (southern canteen) light"""
    # we are currently not in de southern canteen
    light.state = "off"


def decide_state_B(light, d):
    """Decide the current state for the B (time for beer) light"""
    if 16 <= d.hour < 21:
        # it is time for beer
        light.state = "on"
    else:
        light.state = "off"


def decide_state_1(light, d):
    """Decide the current state for the 1 (first time frame) light"""
    if ((d.hour == 9) or (d.hour == 8 and d.minute >= 30) or
            (d.hour == 10 and d.minute < 30)):
        light.state = "on"
    else:
        light.state = "off"


def decide_state_2(light, d):
    """Decide the current state for the 2 (second time frame) light"""
    if ((d.hour == 11) or
            (d.hour == 10 and d.minute >= 30) or
            (d.hour == 12 and d.minute < 30)):
        light.state = "on"
    else:
        light.state = "off"


def decide_state_3(light, d):
    """Decide the current state for the 3 (third time frame) light"""
    if ((d.hour == 14) or
            (d.hour == 13 and d.minute >= 30) or
            (d.hour == 15 and d.minute < 30)):
        light.state = "on"
    else:
        light.state = "off"


def decide_state_4(light, d):
    """Decide the current state for the 4 (fourth time frame) light"""
    if ((d.hour == 16) or
            (d.hour == 14 and d.minute >= 30) or
            (d.hour == 17 and d.minute < 30)):
        light.state = "on"
    else:
        light.state = "off"


def decide_state_5(light, d):
    """Decide the current state for the 5 (fifth time frame) light"""
    if ((d.hour == 18) or
            (d.hour == 16 and d.minute >= 30) or
            (d.hour == 19 and d.minute < 30)):
        light.state = "on"
    else:
        light.state = "off"


def decide_state(light, d):
    """Decide the current state for any light"""
    noop = lambda light, d: None
    {
        "down": noop,
        "K": decide_state_K,
        "S": decide_state_S,
        "B": decide_state_B,
        "1": decide_state_1,
        "2": decide_state_2,
        "3": decide_state_3,
        "4": decide_state_4,
        "5": decide_state_5,
        "up": noop
    }[light.cn](light, d)


def main():
    cns = ["down", "K", "S", "B", "1", "2", "3", "4", "5", "up"]
    lights = collections.OrderedDict(zip(PINS, [Light(cn) for cn in cns]))

    try:
        GPIO.setmode(GPIO.BOARD)
        GPIO.setup(PINS, GPIO.OUT)
        while 1:
            d = datetime.datetime.now()
            for pin, light in lights.items():
                decide_state(light, d)
            onoff = [lights[pin].state == "on" for pin in PINS]
            GPIO.output(PINS, onoff)
            time.sleep(1.0 / FREQ)
    finally:
        GPIO.cleanup()


if __name__ == "__main__":
    main()
