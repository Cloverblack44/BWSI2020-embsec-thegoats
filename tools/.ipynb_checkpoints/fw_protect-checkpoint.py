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
from Crypto.Protocol.KDF import HKDF
<<<<<<< HEAD
from Crypto.Hash import HMAC, SHA256
=======
from Crypto.Hash import SHA512
>>>>>>> 436372af25b45cb1aaabb04a74ed889b02dfdf42
from Crypto.Random import get_random_bytes


def generate_keys_hkdf(mySalt):
    #Crypto.Protocol.KDF.HKDF(master, key_len, salt, hashmod, num_keys=1, context=None)
    with open('secret_build_output.txt', 'rb') as fp:
        password = fp.read()
    key1, key2 = HKDF(password, 16, mySalt, SHA512, 2)
    print(key1)
    print(key2)
    return key1, key2


def protect_firmware(infile, outfile, version, message):
    # Load firmware binary from infile
    with open(infile, 'rb') as fp:
        firmware = fp.read()
    # generate keys
    salt = get_random_bytes(32)
    AESkey, HMACkey = generate_keys_hkdf(salt)
    
    # generate cipher
    cipher = AES.new(AESkey, AES.MODE_CBC)
    # send metadata
    length = len(firmware)
    # [ version #] | [firmware size] | [ cipher iv] | [salt]
    # [0x02]       | [0x02]          | [0x10]       | [0x20] in bytes
    metadata = struct.pack('<HH16x32x', version, length, cipher.iv, salt)
    with open(outfile, 'wb+') as out
        outfile.write(metadata + '\n')
    # writes to the file 16 bytes at a time
    firmware_blob = open(outfile, 'ab')
    while length > 16:
        with open(infile, 'rb') as fp:
            firmware = cipher.encrypt(pad(fp.read(16), 16))
            # I have to reset the HMAC each time unfort
            MACkey = HMAC.new(HMACkey, digestmod=SHA256)
            MACkey.update(firmware)
            bigMAC = MACkey.digest()
            length -= 16
            firmware_blob.write('16' + firmware+'\n')

    firmware = cipher.encrypt(pad(fp.read(length), 16))
    MACkey = HMAC.new(HMACkey, digestmod=SHA256)
    MACkey.update(firmware)
    bigMAC = MACkey.digest()
    firmware_blob.write(f'{length}'+ firmware+'\n')
    # null terminator
    firmware_blob.write("\0\n")
    
    
# ---------------------------trash code -------------------------------
#     # Append null-terminated message to end of firmware
    
#     # generate keys
#     salt = get_random_bytes(32)
#     AESkey, HMACkey = generate_keys_hkdf(salt)
    
#     length = len(firmware)
#     # Creates the ciphertext
#     cipher = AES.new(AESkey, AES.MODE_CBC)
#     firmware = cipher.encrypt(pad(firmware, 16))
    
#     # MAC generation
#     MACkey = HMAC.new(HMACkey, digestmod=SHA256)
#     MACkey.update(firmware)
#     bigMAC = MACkey.digest()
    
#     # message is not encrypted
#     firmware_and_message = firmware + bigMAC + message.encode() + b'\00'
#     # Pack version and size into two little-endian shorts
#     # [ version #] | [firmware size] | [ cipher iv] | [salt]
#     # [0x02]       | [0x02]          | [0x10]       | [0x20] in bytes
#     metadata = struct.pack('<HH16x32x', version, length, cipher.iv, salt)
    
#     # Append firmware and message to metadata
#     firmware_blob = metadata + firmware_and_message

#     # Write firmware blob to outfile
#     with open(outfile, 'wb+') as outfile:
#         outfile.write(firmware_blob)
# -------------------------------------------------------------------------------

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Firmware Update Tool')
    parser.add_argument("--infile", help="Path to the firmware image to protect.", required=True)
    parser.add_argument("--outfile", help="Filename for the output firmware.", required=True)
    parser.add_argument("--version", help="Version number of this firmware.", required=True)
    parser.add_argument("--message", help="Release message for this firmware.", required=True)
    args = parser.parse_args()

    protect_firmware(infile=args.infile, outfile=args.outfile, version=int(args.version), message=args.message)
