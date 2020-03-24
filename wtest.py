#!/usr/bin/env python3

import gattlib
import struct
import posix
import sys

if posix.getuid() != 0:
    print("must be root")
    sys.exit(1)

req = gattlib.GATTRequester("DC:99:65:B8:F1:F6", False)
req.connect(True, "random")
val = req.read_by_uuid("00002a00-0000-1000-8000-00805f9b34fb")
print(val)

def add_cksum(pkt):
    cksum = ~sum(pkt) & 0xff
    return pkt + bytes((cksum,))

def mkpkt(button, pressed):
    fmt = '<2sss'
    hdr = b'!B'
    
    button_byte = button.encode('utf8')
    if pressed:
        pressed_byte = b'1'
    else:
        pressed_byte = b'0'

    pkt = struct.pack(fmt, hdr, button_byte, pressed_byte)
    pkt = add_cksum(pkt)
    print(pkt)
    return(pkt)


req.write_cmd(0x13, mkpkt('1', True))
print("write done")
