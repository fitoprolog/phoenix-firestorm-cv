#!/usr/bin/env python3
import socket
import json
import base64
import numpy as np
import cv2

SOCKET_PATH = '/tmp/firestorm_pybridge.sock'

sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.connect(SOCKET_PATH)

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
