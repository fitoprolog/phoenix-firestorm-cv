#!/usr/bin/env python3
import socket
import json
import base64
import numpy as np
import cv2

SOCKET_PATH = '/tmp/firestorm_pybridge.sock'

sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.connect(SOCKET_PATH)

try:
    while True:
        data = b''
        while not data.endswith(b'\n'):
            chunk = sock.recv(4096)
            if not chunk:
                raise SystemExit
            data += chunk
        msg = json.loads(data.decode('utf-8'))
        if msg['type'] == 'frame':
            width = msg['width']
            height = msg['height']
            frame = np.frombuffer(base64.b64decode(msg['data']), dtype=np.uint8)
            frame = frame.reshape((height, width, 3))
            cv2.imshow('frame', frame)
            if cv2.waitKey(1) == 27:
                break
        else:
            print(msg)
finally:
    sock.close()
