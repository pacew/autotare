#! /usr/bin/env python3

import struct
import binascii
import subprocess

def add_cksum(pkt):
    cksum = ~sum(pkt) & 0xff
    return pkt + bytes((cksum,))

def mkpkt():
    fmt = '<2sss'
    hdr = b'!B'
    
    button = b'x'
    pressed = b'0'

    pkt = struct.pack(fmt, hdr, button, pressed)
    pkt = add_cksum(pkt)
    print(pkt)
    return(pkt)

def sendpkt(pkt):
    addr = "DC:99:65:B8:F1:F6"
    cmd = ["gatttool",
           "-b", addr,
           "-t", "random",
           "--char-write-req",
           "--handle=0x13",
           "--value={}".format(binascii.hexlify(pkt).decode('utf8'))
           ]
    print(cmd)
    subprocess.run(cmd)

sendpkt(mkpkt())

