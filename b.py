#!/usr/bin/env python3

import sys
import os

# os.environ["BLEAK_LOGGING"] = "True"

import asyncio
import bleak

print(bleak.__file__)

address = "dc:99:65:b8:f1:f6"
# MODEL_NBR_UUID = "00002a24-0000-1000-8000-00805f9b34fb"
name_uuid =      "00002a00-0000-1000-8000-00805f9b34fb"

# must use lowercase hex digits
tx_uuid = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
rx_uuid = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

#        await client.write_gatt_char(tx_uuid, "\n".encode("utf8"))

async def do_await(coro):
    return await coro

def getval(coro):
    return asyncio.get_event_loop().run_until_complete(do_await(coro))

def conn():
    client = BleakClient(address, None, timeout=0.5)
    getval(client.connect())
    return client
    

async def run(address, loop):
    async with bleak.BleakClient(address, loop=loop, timeout=0.01) as client:
        print("getting name")
        name_raw = await client.read_gatt_char(name_uuid)
        name = name_raw.decode('utf8')
        print(name)
        await client.write_gatt_char(tx_uuid, "foo\n".encode("utf8"), False)
#        os._exit(1)


if __name__ == "__main__":
    loop = asyncio.get_event_loop()
    loop.run_until_complete(run(address, loop))
