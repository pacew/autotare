#! /usr/bin/env python3

import struct
import binascii

def add_cksum(pkt):
    cksum = ~sum(pkt) & 0xff
    return pkt + bytes((cksum,))

def mkpkt():
    fmt = '<2sss'
    hdr = b'!B'
    
    button = b'1'
    pressed = b'1'

    pkt = struct.pack(fmt, hdr, button, pressed)
    pkt = add_cksum(pkt)
    print(pkt)
    return(pkt)

def sendpkt(pkt):
    addr = "DC:99:65:B8:F1:F6"
    cmd = "gatttool -b {} -t random --char-write-req --handle=0x13 --value={}".format(
        addr, binascii.hexlify(pkt).decode('utf8'))
    print(cmd)

sendpkt(mkpkt())

