# Python Bridge Plugin

This repository now includes a small proof-of-concept plugin which exposes
viewer information to an external Python process.  The plugin lives under
`indra/media_plugins/pybridge` and communicates with Python using WebRTC.
Signaling is performed with a simple HTTP POST request.

## Building

The plugin is compiled on Linux automatically when the viewer is built.
It produces a shared library named `media_plugin_pybridge` which is loaded
through the normal plugin framework.

## Usage

1. Launch the viewer so that the `pybridge` plugin is loaded.
2. Optionally set `PYBRIDGE_SCRIPT` to the path of a Python script. When the
   plugin initializes it will launch this script automatically. If the
   environment variable `PYBRIDGE_VENV` points to a virtualenv directory, the
   interpreter `${PYBRIDGE_VENV}/bin/python` will be used instead of the system
   `python3`.
3. Run `scripts/pybridge_poc.py` manually or rely on `PYBRIDGE_SCRIPT`.  The
   script starts an HTTP server on `localhost:8080` and waits for the viewer's
   offer.
4. Frames are pushed from the viewer to Python via the WebRTC data channel.
   The viewer captures the back buffer each frame using its regular snapshot
   routine so the image matches what you see on screen. Key and mouse events
   typed in the Python window are forwarded back to the viewer.

Each JSON packet ends with a newline and is carried over the WebRTC data
channel.

Frames are transferred using the existing shared memory mechanism and are
sent to Python base64 encoded.  The proof-of-concept script reconstructs
the frames using NumPy and OpenCV.
