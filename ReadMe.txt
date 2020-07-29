T̶̀͛h̴̆̀ȇ̸͝ ̵̃̽G̶̈́̏O̵̓͂A̶̍̕T̵̂͝S̴͑̈́

Four Tools:
1) bl_build.py: provisions two keys
2) fw_protect.py: protects firmware
3) fw_update.py: sends firmware in packets
4) bootloader.c: flashes the firmware

-----bl_build.py-----
Provisions two keys to secret_build_output.txt file: 
1) password to HKDF function
2) key for HMACing metadata

-----fw_protect.py-----
generate_keys_hkdf(mySalt) function:
 - PURPOSE: creating two random keys for AES and HMAC
 - reads in password from secret_build_output.txt file
 - takes in a salt through its parameters 
 - creates two 16 byte keys from the password and salt using SHA512
 - first key is an AES encryption key
 - second key is a HMAC key for HMACing the firmware
 - RETURNS the two keys

protect_firmware(infile, outfile, version, message) function:
 - PURPOSE: encrypting and protecting firmware
 - load firmware binary from infile and generate keys using generate_keys_hkdf function
 - generate AES cipher (to be used on this entire firmware binary) and encode release message to bytes
 - create an HMAC of all our metadata(version, length, len(release message), cipher.iv, salt, release message)
 - create metadata variable of metadata+HMAC and write it to outfile
 - write encrypted 16 byte chunks to file with a new HMAC each time 
 - send a null terminator firmware chunk
 - RETURNS nothing

-----fw_update.py-----
send_metadata(ser, metadata, message_length, debug=False) function:
 - PURPOSE: take the file metadata and send it as a frame to the bootloader
 - unpacks metadata to: version, size, length, iv , salt, message, MAC
 - handshakes for update and sends size and version to bootloader
 - writes metadata and waits for an OK from the bootloader
 - RETURNS nothing

send_frame(ser, frame, debug=False) function:
 - PURPOSE: send each frame to bootloader
 - writes the frame through the Serial
 - waits for an OK from the bootloader and throws an error if we do not receive the OK
 - RETURNS nothing
 
main(ser, infile, debug) function:
 - PURPOSE: send each frame of firmware to bootloader
 - opens infile and unpacks release message length
 - sends the metadata to the Stellaris using send_metadata function
 - uses while(True) loop to send firmware packets to bootloader
    - sends 50 byte frames: 2 bytes for length, 16 bytes for firmware, 32 bytes for HMAC
 - sends a zero length payload to tell the bootlader to finish writing it's page

-----bootloader.c-----
int aes_decrypt(char* key, char* iv, char* ct, int len) function:
 - decrypts a cipher with a key and iv. 
 - overwrites variable passed into the char* ct parameter
 - RETURNS 1 if successfully decrypts
 
int sha_hmac(char* key, int key_len, char* data, int len, char* out) function:
 - PURPOSE: uses a key to HMAC given data and assigns it to the output variable
 - verifies if the frame sent is not corrupted using SHA256
 - RETURNS 32 which is the length of the HMAC

static size_t hextobin(unsigned char *dst, const char *src) function:
 - PURPOSE: turn the hex given from the parameter src into a binary put into the parameter dst
 - RETURNS length of the binary

static void test_HKDF_inner(const br_hash_class *dig, const char *ikmhex, const char *salthex, const char *infohex, const char *okmhex,unsigned char *key) function:
 - PURPOSE: uses HKDF to generate random keys 
 - given the same password and salt as the python side, so it generate the same key, keeping symmetric encryption
 - RETURNS nothing

void load_firmware(void) function:
 - PURPOSE: takes a metadata package from UART1 and checks the authenticity of it with HMAC. It then takes frames of     
   firmware where it checks its authencitiy and decrypt
 - 
 - RETURNS nothing
