#!/usr/bin/env python3
"""
@file pybridge_poc.py
@brief Proof of concept client for the Python bridge

$LicenseInfo:firstyear=2024&license=viewerlgpl$
Second Life Viewer Source Code
Copyright (C) 2010, Linden Research, Inc.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation;
version 2.1 of the License only.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
$/LicenseInfo$
"""

import socket
import json
import base64
import numpy as np
import cv2

SOCKET_PATH = '/tmp/firestorm_pybridge.sock'

def connect_socket():
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(SOCKET_PATH)
    return s

sock = connect_socket()
buf = b""

def on_mouse(event, x, y, flags, param):
    if event == cv2.EVENT_MOUSEMOVE:
        sock.sendall(f"mouse move {x} {y} 0\n".encode())
    elif event == cv2.EVENT_LBUTTONDOWN:
        sock.sendall(f"mouse down {x} {y} 1\n".encode())
    elif event == cv2.EVENT_LBUTTONUP:
        sock.sendall(f"mouse up {x} {y} 1\n".encode())


cv2.namedWindow('frame')
cv2.setMouseCallback('frame', on_mouse)

try:
    while True:
        chunk = sock.recv(4096)
        if not chunk:
            sock.close()
            sock = connect_socket()
            buf = b""
            continue
        buf += chunk
        while b'\n' in buf:
            line, buf = buf.split(b'\n', 1)
            if not line:
                continue
            msg = json.loads(line.decode('utf-8'))
            if msg['type'] == 'frame':
                width = msg['width']
                height = msg['height']
            frame = np.frombuffer(base64.b64decode(msg['data']), dtype=np.uint8)
            frame = frame.reshape((height, width, 3))
            cv2.imshow('frame', frame)
            k = cv2.waitKey(1)
            if k == 27:
                break
            if k != -1:
                sock.sendall(f"key down {k} 0\n".encode())
                sock.sendall(f"key up {k} 0\n".encode())
        else:
            print(msg)
finally:
    sock.close()
