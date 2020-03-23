import asyncio
import importlib

def reload(mod):
    importlib.reload(mod)

async def sleeper():
    print(1)
    await asyncio.sleep(.1)
    print(2)
    return 123

async def do_await(coro):
    return await coro

def val(coro):
    return asyncio.get_event_loop().run_until_complete(do_await(coro))
