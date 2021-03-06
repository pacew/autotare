#!/usr/bin/env python3

import asyncio
from bleak import BleakClient

address = "DC:99:65:B8:F1:F6"
# MODEL_NBR_UUID = "00002a24-0000-1000-8000-00805f9b34fb"
name_uuid =      "00002a00-0000-1000-8000-00805f9b34fb"

tx_uuid = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
rx_uuid = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

#        await client.write_gatt_char(tx_uuid, "\n".encode("utf8"))

async def run(address, loop):
    async with BleakClient(address, loop=loop) as client:
        print("getting name")
        name_raw = await client.read_gatt_char(name_uuid)
        name = name_raw.decode('utf8')
        print(name)
#        await client.write_gatt_descriptor(0x13, "\n".encode("utf8"))
#        print("written")

if __name__ == "__main__":
    loop = asyncio.get_event_loop()
    loop.run_until_complete(run(address, loop))
