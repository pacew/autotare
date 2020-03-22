#!/usr/bin/env python3

import gattlib

req = gattlib.GATTRequester("DC:99:65:B8:F1:F6", False)
req.connect(True, "random")
val = req.read_by_uuid("00002a00-0000-1000-8000-00805f9b34fb")
print(val)

req.write_cmd(0x13, bytes([0xa]))
print("write done")
