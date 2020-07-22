"""
Firmware Bundle-and-Protect Tool

"""
import argparse
import struct
from embsec import Serial
import struct
from Crypto.Util.Padding import pad
import Crypto.Random
from Crypto.Cipher import AES
import random

def protect_firmware(infile, outfile, version, message):
    # Load firmware binary from infile
    with open(infile, 'rb') as fp:
        firmware = fp.read()
        
    # Append null-terminated message to end of firmware
    # ----- Ingrid's random number generator ----------
    
    # -------------------------------------------------
    # Creates the ciphertext
#     cipher = AES.new(key, AES.MODE_CBC)
#     firmware = cipher.encrypt(pad(firmware, 16))
    
    # ------ Andrew's HMAC code ------------------------
    
    # --------------------------------------------------
    # message is not encrypted
    firmware_and_message = firmware + message.encode() + b'\00'
             
    # Pack version and size into two little-endian shorts
    # [ key generation iteration] | [ version #] | [firmware size] | [HMAC size] | [ cipher iv]
    # [0x10] | [0x10] | [0x10] | [0x10]
    metadata = struct.pack('<HHH16s', version, len(firmware))
    
    # Append firmware and message to metadata
    firmware_blob = metadata + firmware_and_message

    # Write firmware blob to outfile
    with open(outfile, 'wb+') as outfile:
        outfile.write(firmware_blob)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Firmware Update Tool')
    parser.add_argument("--infile", help="Path to the firmware image to protect.", required=True)
    parser.add_argument("--outfile", help="Filename for the output firmware.", required=True)
    parser.add_argument("--version", help="Version number of this firmware.", required=True)
    parser.add_argument("--message", help="Release message for this firmware.", required=True)
    args = parser.parse_args()

    protect_firmware(infile=args.infile, outfile=args.outfile, version=int(args.version), message=args.message)
