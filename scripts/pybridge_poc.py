#!/usr/bin/env python3
"""
Proof-of-concept Python client for the pybridge plugin using WebRTC
signaling over HTTP.
"""

import asyncio
import base64
import json
from aiohttp import web
from aiortc import RTCPeerConnection, RTCSessionDescription
import cv2
import numpy as np

pc = RTCPeerConnection()
channel = None

async def index(request):
    return web.Response(text="pybridge signaling")

async def offer(request):
    params = await request.json()
    offer = RTCSessionDescription(sdp=params["sdp"], type="offer")
    await pc.setRemoteDescription(offer)
    answer = await pc.createAnswer()
    await pc.setLocalDescription(answer)
    return web.json_response({"sdp": pc.localDescription.sdp})

@pc.on("datachannel")
def on_datachannel(dc):
    global channel
    channel = dc
    dc.on("message", on_message)

buf = ""

def on_mouse(event, x, y, flags, param):
    if channel is None:
        return
    if event == cv2.EVENT_MOUSEMOVE:
        channel.send(f"mouse move {x} {y} 0")
    elif event == cv2.EVENT_LBUTTONDOWN:
        channel.send(f"mouse down {x} {y} 1")
    elif event == cv2.EVENT_LBUTTONUP:
        channel.send(f"mouse up {x} {y} 1")


def on_message(message):
    global buf
    if isinstance(message, bytes):
        return
    buf += message
    while "\n" in buf:
        line, buf = buf.split("\n", 1)
        if not line:
            continue
        msg = json.loads(line)
        if msg["type"] == "frame":
            frame = np.frombuffer(base64.b64decode(msg["data"]), dtype=np.uint8)
            frame = frame.reshape((msg["height"], msg["width"], 3))
            cv2.imshow("frame", frame)
            k = cv2.waitKey(1)
            if k == 27:
                exit(0)
            if k != -1 and channel:
                channel.send(f"key down {k} 0")
                channel.send(f"key up {k} 0")
        else:
            print(msg)

async def main():
    app = web.Application()
    app.add_routes([web.get('/', index), web.post('/offer', offer)])
    runner = web.AppRunner(app)
    await runner.setup()
    site = web.TCPSite(runner, '127.0.0.1', 8080)
    await site.start()
    cv2.namedWindow('frame')
    cv2.setMouseCallback('frame', on_mouse)
    while True:
        await asyncio.sleep(1)

if __name__ == '__main__':
    asyncio.run(main())
