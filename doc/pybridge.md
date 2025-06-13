# Python Bridge Plugin

This repository now includes a small proof-of-concept plugin which exposes
viewer information to an external Python process.  The plugin lives under
`indra/media_plugins/pybridge` and communicates with Python via a UNIX
socket.

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
   script connects to the socket at `/tmp/firestorm_pybridge.sock`.
4. Frames are pushed from the viewer to Python.  Key and mouse events typed
   in the Python window are forwarded back to the viewer.  The script
   accumulates incoming data and splits messages on newline characters so
   partial socket reads are handled correctly.

The plugin writes each JSON packet followed by a newline and will accept a new
connection if the client disconnects.

Frames are transferred using the existing shared memory mechanism and are
sent to Python base64 encoded.  The proof-of-concept script reconstructs
the frames using NumPy and OpenCV.
