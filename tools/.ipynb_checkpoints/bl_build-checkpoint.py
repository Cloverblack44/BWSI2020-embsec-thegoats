#!/usr/bin/env python
"""
Bootloader Build Tool

This tool is responsible for building the bootloader from source and copying
the build outputs into the host tools directory for programming.
"""
import argparse
import os
import pathlib
import shutil
import subprocess

FILE_DIR = pathlib.Path(__file__).parent.absolute()


def copy_initial_firmware(binary_path):
    """
    Copy the initial firmware binary to the bootloader build directory
    Return:
        None
    """
    # Change into directory containing tools
    os.chdir(FILE_DIR)
    bootloader = FILE_DIR / '..' / 'bootloader'
    shutil.copy(binary_path, bootloader / 'src' / 'firmware.bin')


def make_bootloader():
    """
    Build the bootloader from source.

    Return:
        True if successful, False otherwise.
    """
    # Change into directory containing bootloader.
    bootloader = FILE_DIR / '..' / 'bootloader'
    os.chdir(bootloader)

    subprocess.call('make clean', shell=True)
    
    #make two keys: one for password for HKDF and one for HMACing the metadata
    myPass = Crypto.Random.get_random_bytes(16)
    myMetadataHMAC = Crypto.Random.get_random_bytes(16)
    
    status = subprocess.call(f'make PASSWORD={to_c_array(myPass)}', shell=True)
    status = subprocess.call(f'make METADATA_HMAC={to_c_array(myMetadataHMAC)}', shell=True)
    
    #write the keys to the text file
    with open('secret_build_output.txt', 'w') as fp:
        fp.write(myPass + "\n")
        fp.write(myMetadataHMAC + "\n")
    
    # Return True if make returned 0, otherwise return False.
    return (status == 0)

def to_c_array(binary_string):
	return "{" + ",".join([hex(c) for c in binary_string]) + "}"

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Bootloader Build Tool')
    parser.add_argument("--initial-firmware", help="Path to the the firmware binary.", default=None)
    args = parser.parse_args()
    if args.initial_firmware is None:
        binary_path = FILE_DIR / '..' / 'firmware' / 'firmware' / 'gcc' / 'main.bin'
    else:
        binary_path = os.path.abspath(pathlib.Path(args.initial_firmware))

    if not os.path.isfile(binary_path):
        raise FileNotFoundError(
            "ERROR: {} does not exist or is not a file. You may have to call \"make\" in the firmware directory.".format(
                binary_path))

    copy_initial_firmware(binary_path)
    make_bootloader()
