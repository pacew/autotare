#!/usr/bin/env python3

import asyncio
from bleak import BleakClient

address = "DC:99:65:B8:F1:F6"
# MODEL_NBR_UUID = "00002a24-0000-1000-8000-00805f9b34fb"
name_uuid =      "00002a00-0000-1000-8000-00805f9b34fb"

async def run(address, loop):
    async with BleakClient(address, loop=loop) as client:
        name = await client.read_gatt_char(name_uuid)
        print(name)
        print("name: {0}".format("".join(map(chr, name))))

loop = asyncio.get_event_loop()
loop.run_until_complete(run(address, loop))
