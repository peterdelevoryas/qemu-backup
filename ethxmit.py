#!/usr/bin/env python3.6

from socket import *
from scapy.all import Ether
import sys

fd = socket(AF_INET, SOCK_DGRAM)
fd.bind(("", 6666))

i2c = "20 0F 19 65 01 00 00 C8 00 00 03 00 D0 7A 22 C5 16 8D EB 11 80 00 B8 CE F6 66 70 A4 1C"
i2c = bytes([int(s, 16) for s in i2c.split()])

for b in i2c:
    sys.stdout.write(f"{b:02x} ")
sys.stdout.write('\n')

fd.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
fd.sendto(i2c, ("127.0.0.1", 7777))
