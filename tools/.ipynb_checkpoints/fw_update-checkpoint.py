#!/usr/bin/env python
"""
Firmware Updater Tool

A frame consists of two sections:
1. Two bytes for the length of the data section
2. A data section of length defined in the length section

[ 0x02 ]  [ variable ]
--------------------
| Length | Data... |
--------------------

In our case, the data is from one line of the Intel Hex formated .hex file

We write a frame to the bootloader, then wait for it to respond with an
OK message so we can write the next frame. The OK message in this case is
just a zero
"""

import argparse
import struct
import time

from serial import Serial

RESP_OK = b'\x00'
FRAME_SIZE = 16


def send_metadata(ser, metadata, debug=False):
    version, size, iv , salt, MAC= struct.unpack_from('<HH16s32s32s', metadata)
    print(f'Version: {version}\nSize: {size} bytes\n')

    # Handshake for update
    ser.write(b'U')
    
    print('Waiting for bootloader to enter update mode...')
    while ser.read(1).decode() != 'U':
        pass

    # Send size and version to bootloader.
    if debug:
        print(metadata)

    ser.write(metadata)

    # Wait for an OK from the bootloader.
    resp = ser.read()
    print(resp)
    if resp == 3:
        raise RuntimeError('Frame did not verify')
    
    if resp != RESP_OK:
        raise RuntimeError("ERROR: Bootloader responded with {}".format(repr(resp)))
    ser.read()

def send_frame(ser, frame, debug=False):
    print("sending frame")
    ser.write(frame)  # Write the frame...

    if debug:
        print(frame)

    resp = ser.read()  # Wait for an OK from the bootloader
    print(resp)
    time.sleep(0.1)
    
    if resp == 3:
        while resp != RESP_OK:
            print("in while loop")
            ser.write(frame)
            resp = ser.read()
            time.sleep(0.2)
    
    if resp != RESP_OK:
        raise RuntimeError("ERROR: Bootloader responded with {}".format(repr(resp)))

    if debug:
        print("Resp: {}".format(ord(resp)))


def main(ser, infile, debug):
    # Open serial port. Set baudrate to 115200. Set timeout to 2 seconds.
    idv = 0
    with open(infile, 'rb') as fp:
        # write the metadata to teh stellaris
        metadata = fp.read(84)
        fp.read(1)

        send_metadata(ser, metadata, debug=debug)
        while True:
            # write 16 byte frames to the stellaris
            idv += 1
            data = fp.read(16)
            fp.read(1)
            if data == '\0':
                ser.write(0, 0)
                break
            # get length of data
            length = int(data[:2])
            frame_fmt = '>H48s'
            data = data[2:]
            # Construct frame.
            frame = struct.pack(frame_fmt, length, data)

            if debug:
                print("Writing frame {} ({} bytes)...".format(idx, len(frame)))

            send_frame(ser, frame, debug=debug)

#         for idx, frame_start in enumerate(range(0, len(firmware), FRAME_SIZE)):
#             data = firmware[frame_start: frame_start + FRAME_SIZE]

#             # Get length of data.
#             length = len(data)
#             frame_fmt = '>H{}s'.format(length)

#             # Construct frame.
#             frame = struct.pack(frame_fmt, length, data)

#             if debug:
#                 print("Writing frame {} ({} bytes)...".format(idx, len(frame)))

#             send_frame(ser, frame, debug=debug)

        print("Done writing firmware.")

        # Send a zero length payload to tell the bootlader to finish writing it's page.
        ser.write(struct.pack('>H', 0x0000))

        return ser


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Firmware Update Tool')

    parser.add_argument("--port", help="Serial port to send update over.",
                        required=True)
    parser.add_argument("--firmware", help="Path to firmware image to load.",
                        required=True)
    parser.add_argument("--debug", help="Enable debugging messages.",
                        action='store_true')
    args = parser.parse_args()

    print('Opening serial port...')
    ser = Serial(args.port, baudrate=115200, timeout=2)
    main(ser=ser, infile=args.firmware, debug=args.debug)


