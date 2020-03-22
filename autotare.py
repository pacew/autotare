#!/usr/bin/env python3

import sys

from bluetooth.ble import GATTRequester

# GATTRequester(addr, do_connect=True, device="hci0")
req = GATTRequester("DC:99:65:B8:F1:F6", False)

# connect(wait=False,channel_type="public",security_level="low", psm=0, mtu=0)
print("connecting...")
req.connect(True, channel_type="random")
print("ok")
val = req.read_by_uuid("00002a00-0000-1000-8000-00805f9b34fb")
print(val)

# req.write_cmd(0x13, bytes([0xa]))
# print("write done")
