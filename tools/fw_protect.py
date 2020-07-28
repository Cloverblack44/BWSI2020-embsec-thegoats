"""
Firmware Bundle-and-Protect Tool
 
"""
import argparse
import struct
from embsec import Serial
import random
import Crypto.Random
from Crypto.Cipher import AES
from Crypto.Protocol.KDF import HKDF
from Crypto.Util.Padding import pad
from Crypto.Hash import HMAC, SHA256, SHA512
from Crypto.Random import get_random_bytes
 

def generate_keys_hkdf(mySalt):
    """reads in salt from secre_build_output.txt and produces two keys"""
 
    with open('secret_build_output.txt', 'rb') as fp:
        password = fp.read()
    key1, key2 = HKDF(password, 16, mySalt, SHA512, 2)
    return key1, key2
 

def protect_firmware(infile, outfile, version, message):
    """
    This is the function responsible for encrypting and protecting the firmware data.
 
    Two keys are derived using HKDF in RFC 5869 and in NIST SP-800 56C using the salt generated from
    generate_keys_hkdf(). HMAC of the metadata is calculated using SHA256 with the following format:
 
                                 metadata
    [ version #] | [firmware size] | [ cipher iv] | [salt] 
    [0x02]       | [0x02]          | [0x10]       | [0x20] 
 
    Both the HMAC of the metadata and the metadata are written to the outfile followed by the encrypted 
    firmware which is first encrypted, then hashed with HMAC. Once the entire firmware has been sent,
    "\x00\x00" is written to signal the end of firmware data.
 
    Parameters
    ----------
    infile: String
        unprotected_firmware_filename stores the unprotected firmware data 
    outfile: String
        protected_firmware_output_filename stores firmware ciphertext
    version: int
        specifies firmware version
    message: String
        message to be displayed during boot
 
    """
 
    # Load firmware binary from infile
    with open(infile, 'rb') as fp:
        firmware = fp.read()
        length = len(firmware)
 
    # generate keys
    passwords = open("/home/jovyan/design-challenge-t-h-g-o-a-t-s/bootloader/secret_build_output.txt", 'rb')
    salt = passwords.read(32)
    passwords.read(1)
    HMACkey1 = passwords.read(16)
    passwords.read(1)
    AESkey, HMACkey = generate_keys_hkdf(salt)
 
    # generate cipher
    cipher = AES.new(AESkey, AES.MODE_CBC)
 
    MACkey = HMAC.new(HMACkey1, digestmod=SHA256)
    MACkey.update(struct.pack('<HH16s32s', version, length, cipher.iv, salt))
    bigMAC = MACkey.digest()
 
    metadata = struct.pack('<HH16s32s32s', version, length, cipher.iv, salt, bigMAC)
 
    #writes metadata along with encrypted and hashed firmware data to outfile 
    with open(outfile, 'wb+') as out:
        out.write(metadata + b'\n')
    # writes to the file 16 bytes at a time
    firmware_blob = open(outfile, 'ab')
    HMACkey = b'0000000000000000' #specifies the length of the HMAC key
    while length >= 16:
        fp = open(infile, 'rb')
        firmware = cipher.encrypt(fp.read(16))
        # I have to reset the HMAC each time unfort
        MACkey = HMAC.new(HMACkey, digestmod=SHA256)
        MACkey.update(firmware)
        bigMAC = MACkey.digest()
        length -= 16
        firmware_blob.write(struct.pack('>h', 16) + firmware + bigMAC + b'\n')
    if not(length == 0):
        firmware = cipher.encrypt(pad(fp.read(length), 16))
        MACkey = HMAC.new(HMACkey, digestmod=SHA256)
        MACkey.update(firmware)
        bigMAC = MACkey.digest()
        firmware_blob.write(struct.pack('>h', length) + firmware+ bigMAC + b'\n')
    # null terminator
    firmware_blob.write(b"\x00\x00")
 
 
if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Firmware Update Tool')
    parser.add_argument("--infile", help="Path to the firmware image to protect.", required=True)
    parser.add_argument("--outfile", help="Filename for the output firmware.", required=True)
    parser.add_argument("--version", help="Version number of this firmware.", required=True)
    parser.add_argument("--message", help="Release message for this firmware.", required=True)
    args = parser.parse_args()
 
    protect_firmware(infile=args.infile, outfile=args.outfile, version=int(args.version), message=args.message)
 