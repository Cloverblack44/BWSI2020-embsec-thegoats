// Hardware Imports
#include "inc/hw_memmap.h" // Peripheral Base Addresses
#include "inc/lm3s6965.h" // Peripheral Bit Masks and Registers
#include "inc/hw_types.h" // Boolean type
#include "inc/hw_ints.h" // Interrupt numbers
#include <stdlib.h>
// Driver API Imports
#include "driverlib/flash.h" // FLASH API
#include "driverlib/sysctl.h" // System control API (clock/reset)
#include "driverlib/interrupt.h" // Interrupt API
 
// Application Imports
#include "uart.h"
#include "bearssl.h"
#include <math.h>
// Forward Declarations
void load_initial_firmware(void);
void load_firmware(void);
void boot_firmware(void);
long program_flash(uint32_t, unsigned char*, unsigned int);

static void
test_HKDF_inner(const br_hash_class *dig, const char *ikmhex,
	const char *salthex, const char *infohex, const char *okmhex,unsigned char *key);
 
int
aes_decrypt(char* key, char* iv, char* ct, int len);
 
int
sha_hmac(char* key, int key_len, char* data, int len, char* out);
 
static size_t
hextobin(unsigned char *dst, const char *src);
// Firmware Constants
#define METADATA_BASE 0xF800  // base address of version and firmware size in Flash
#define FW_BASE 0x10000  // base address of firmware in Flash
 
 
// FLASH Constants
#define FLASH_PAGESIZE 1024
#define FLASH_WRITESIZE 4
 
 
// Protocol Constants
#define OK    ((unsigned char)0x00)
#define ERROR ((unsigned char)0x01)
#define UPDATE ((unsigned char)'U')
#define BOOT ((unsigned char)'B')
 
//BeaverSSL stuff
#define KEY_LEN 16  // Length of AES key (16 = AES-128)
#define IV_LEN 16   // Length of IV (16 is secure)
 
// Firmware v2 is embedded in bootloader
extern int _binary_firmware_bin_start;
extern int _binary_firmware_bin_size;
 
 
// Device metadata
uint16_t *fw_version_address = (uint16_t *) METADATA_BASE;
uint16_t *fw_size_address = (uint16_t *) (METADATA_BASE + 2);
uint8_t *fw_release_message_address;
 
// Firmware Buffer
unsigned char data[FLASH_PAGESIZE];
 
 
int main(void) {
 
  // Initialize UART channels
  // 0: Reset
  // 1: Host Connection
  // 2: Debug
  uart_init(UART0);
  uart_init(UART1);
  uart_init(UART2);
 
  // Enable UART0 interrupt
  IntEnable(INT_UART0);
  IntMasterEnable();
 
  load_initial_firmware();
 
  uart_write_str(UART2, "Welcome to the BWSI Vehicle Update Service!\n");
  uart_write_str(UART2, "Send \"U\" to update, and \"B\" to run the firmware.\n");
  uart_write_str(UART2, "Writing 0x20 to UART0 will reset the device.\n");
 
  // Wait for boot or update instruction
  int resp;
  while (1){
    uint32_t instruction = uart_read(UART1, BLOCKING, &resp);
    if (instruction == UPDATE){
      uart_write_str(UART1, "U");
      load_firmware();
    } else if (instruction == BOOT){
      uart_write_str(UART1, "B");
      boot_firmware();
    }
  }
}
 
/*
 * Load initial firmware into flash
 */
void load_initial_firmware(void) {

  if (*((uint32_t*)(METADATA_BASE+512)) != 0){
    /*
     * Default Flash startup state in QEMU is all zeros since it is
     * secretly a RAM region for emulation purposes. Only load initial
     * firmware when metadata page is all zeros. Do this by checking
     * 4 bytes at the half-way point, since the metadata page is filled
     * with 0xFF after an erase in this function (program_flash()).
     */
    return;
  }

  int size = (int)&_binary_firmware_bin_size;
  int *data = (int *)&_binary_firmware_bin_start;
    
  uint16_t version = 2;
  uint32_t metadata = (((uint16_t) size & 0xFFFF) << 16) | (version & 0xFFFF);
  program_flash(METADATA_BASE, (uint8_t*)(&metadata), 4);
  fw_release_message_address = (uint8_t *) "This is the initial release message.";
    
  int i = 0;
  for (; i < size / FLASH_PAGESIZE; i++){
       program_flash(FW_BASE + (i * FLASH_PAGESIZE), ((unsigned char *) data) + (i * FLASH_PAGESIZE), FLASH_PAGESIZE);
  }
  program_flash(FW_BASE + (i * FLASH_PAGESIZE), ((unsigned char *) data) + (i * FLASH_PAGESIZE), size % FLASH_PAGESIZE);
}

/*
 * Load the firmware into flash.
 */
void load_firmware(void)
{
  /* This function uses UART to load firmware onto the bootloader
  inputs: None
  Outputs: None
  This function takes a metadata package from UART1 and checks the authenticity of it with HMAC. It then takes frames of firmware where it checks its authencitiy and decrypt.
  */  
    
    
  // This is just a huge list of declared variables
    // used for debugging purposes
  char passW[16] = {48,48,48,48,48,48,48,48,48,48,48,48,48,48,48,48};
    // determines how much data is being passed through a frame
  int frame_length = 0;
  int read = 0;
    // temporary data array to prevent changing data array
  char temporary_data[16];
    // used to makea cumulaiton of all the metadata so we can HMAC
  char comboMetadata[1078];
    // these variables are assigned when metadata is read
  char HMAC[32];
  char IV[16];
  char salt[32];
  uint32_t rcv = 0;
    // this variable is used as the output for HMAC
  unsigned char output[32];
  uint32_t data_index = 0;
  uint32_t page_addr = FW_BASE;
  uint32_t version = 0;
  uint32_t size = 0;
    // keys provisioned from the server.
  char passwordKey[32] = mySalt;
  char metadataKey[16] = myMETADATA_HMAC;
  //create variable buffers to hold key
  unsigned char key[32];
  unsigned char hmac_key[16];
  unsigned char aes_key[16];
  uint32_t release_message_length = 0;
    
  // Get version.
  rcv = uart_read(UART1, BLOCKING, &read);
  comboMetadata[0] = rcv;
  version = (uint32_t)rcv;
  rcv = uart_read(UART1, BLOCKING, &read);
  comboMetadata[1] = rcv;
  version |= (uint32_t)rcv << 8;
 
  uart_write_str(UART2, "Received Firmware Version: ");
  uart_write_hex(UART2, version);
  nl(UART2);
 
  // Get size.
  rcv = uart_read(UART1, BLOCKING, &read);
  comboMetadata[2] = rcv;
  size = (uint32_t)rcv;
  rcv = uart_read(UART1, BLOCKING, &read);
  comboMetadata[3] = rcv;
  size |= (uint32_t)rcv << 8;
 
  uart_write_str(UART2, "Received Firmware Size: ");
  uart_write_hex(UART2, size);
  nl(UART2);
 
  // get release message size
  rcv = uart_read(UART1, BLOCKING, &read);
  comboMetadata[4] = rcv;
  release_message_length = (uint32_t)rcv;
  rcv = uart_read(UART1, BLOCKING, &read);
  comboMetadata[5] = rcv;
  release_message_length |= (uint32_t)rcv << 8;
    
  uart_write_str(UART2, "got message size");
  uart_write_hex(UART2, release_message_length);
  nl(UART2);
    
  // Get cipherIV.
  for (int i = 0; i < 16; i++) {
      IV[i] = uart_read(UART1, BLOCKING, &read);
      comboMetadata[6+i] = IV[i];
  }
  uart_write_str(UART2, "Received cipherIV");
  nl(UART2); 
 
  // get salt
  for (int i = 0; i < 32; i++) {
      salt[i] = uart_read(UART1, BLOCKING, &read);
      comboMetadata[22+i] = salt[i];
  }
  uart_write_str(UART2, "Received salt");
  nl(UART2); 
    
  char release_message[1025];
  // get message
  for (int i = 0; i < release_message_length; i++) {
      release_message[i] = uart_read(UART1, BLOCKING, &read);
      comboMetadata[54+i] = release_message[i];
  }
  uart_write_str(UART2, "Received message");
  nl(UART2); 
    
  // get HMAC
  for (int i = 0; i < 32; i++) {
      HMAC[i] = uart_read(UART1, BLOCKING, &read);
  }
  uart_write_str(UART2, "Received HMAC");
  nl(UART2);
    
  // HMACing
    sha_hmac(
        metadataKey,
        16, //size of key
        comboMetadata,
        54+release_message_length, //metadata size
        output);
    
  // if tampered return error and reset
  if(memcmp(HMAC,output, 32) != 0){
      uart_write_str(UART2, "OOP");
      uart_write(UART1, 3);
      return;
  } else {
  // Generate keys
    test_HKDF_inner(&br_sha512_vtable,
        passwordKey, //master key
        salt, //salt
        "",	//leave blank
    "0000000000000000000000000000000000000000000000000000000000000000", //length of key
    key); 
  }
    
    // assign the keys we generate to variables
    for (int i = 0; i <= 15; i++){
        aes_key[i] = key[i];
        hmac_key[i] = key[i + 16];
        }
    
  // Compare to old version and abort if older (note special case for version 0).
  uint16_t old_version = *fw_version_address;
  uart_write_hex(UART2, old_version);
  if (version != 0 && version < old_version) {
    uart_write(UART1, ERROR); // Reject the metadata.
    SysCtlReset(); // Reset device
    return;
  } else if (version == 0) {
    // If debug firmware, don't change version
    version = old_version;
  }
 
  // Write new firmware size and version to Flash
  // Create 32 bit word for flash programming, version is at lower address, size is at higher address
  // The version and size is kept 2Kb before the firmware
  uart_write_hex(UART2, version);
  uint32_t metadata = ((size & 0xFFFF) << 16) | (version & 0xFFFF);
  program_flash(METADATA_BASE, (uint8_t*)(&metadata), 4);
  // Writes the release message 1Kb before the start of the firmware
  fw_release_message_address = (uint8_t *) (FW_BASE - 1024);
  program_flash(FW_BASE - 1024, release_message, sizeof(release_message));
    
  uart_write(UART1, OK); // Acknowledge the metadata.
  uart_write_hex(UART2, *fw_version_address);
    
  /* Loop here until you can get all your characters and stuff */
  while (1) {
 
    // Get two bytes for the length.
    rcv = uart_read(UART1, BLOCKING, &read);
    frame_length = (int)rcv << 8;
    rcv = uart_read(UART1, BLOCKING, &read);
    frame_length += (int)rcv;
      
    // Write length debug message
    if (frame_length != 0) {
        // Get encrypted firmware
        for (int i = 0; i < 16; i++){
            temporary_data[i] = uart_read(UART1, BLOCKING, &read);
            data_index += 1;
        }
        // get HMAC of encrypted firmware
        for (int i = 0; i<32; i++){
            HMAC[i] = uart_read(UART1, BLOCKING, &read);
        } 

        // Verify the frame
        sha_hmac(
        hmac_key,
            16, //size of key
            temporary_data,
            16, //firmware size
            output);

        // if tampered return error and reset
        if(memcmp(HMAC,output, 32) != 0){
            uart_write_str(UART2, "failed verification\n");
            uart_write(UART1, 3);
            continue;
        }

        // Decrypt
        aes_decrypt(aes_key, IV, temporary_data, 16);

    // transfers temporary_data to data (1Kb array)
        for (int i = 0; i < frame_length; i++ ){
            data[data_index - 16 + i] = temporary_data[i];
        } 
    }
        // If we filed our page buffer, program it
        if (data_index == FLASH_PAGESIZE || frame_length == 0) {
          // Try to write flash and check for error
          if (program_flash(page_addr, data, data_index)){
            uart_write(UART1, ERROR); // Reject the firmware
            SysCtlReset(); // Reset device
            return;
          }
    #if 1
          // Write debugging messages to UART2.
          uart_write_str(UART2, "Page successfully programmed\nAddress: ");
          uart_write_hex(UART2, page_addr);
          uart_write_str(UART2, "\nBytes: ");
          uart_write_hex(UART2, data_index-16+frame_length);
          nl(UART2);
    #endif
          // Update to next page
          page_addr += FLASH_PAGESIZE;
          data_index = 0;
          memset(data, 0, sizeof(data));
          if (frame_length == 0) {
              uart_write(UART1, '\x00');
              uart_write_str(UART2, "I returned");
              return;
          }
          // If at end of firmware, go to main
        } // if
    uart_write(UART1, OK); // Acknowledge the frame.
  } // while(1)
}


/*
 * Program a stream of bytes to the flash.
 * This function takes the starting address of a 1KB page, a pointer to the
 * data to write, and the number of byets to write.
 *
 * This functions performs an erase of the specified flash page before writing
 * the data.
 */
long program_flash(uint32_t page_addr, unsigned char *data, unsigned int data_len)
{
  unsigned int padded_data_len;
 
  // Erase next FLASH page
  FlashErase(page_addr);
 
  // Clear potentially unused bytes in last word
  if (data_len % FLASH_WRITESIZE){
    // Get number unused
    int rem = data_len % FLASH_WRITESIZE;
    int i;
    // Set to 0
    for (i = 0; i < rem; i++){
      data[data_len-1-i] = 0x00;
    }
    // Pad to 4-byte word
    padded_data_len = data_len+(FLASH_WRITESIZE-rem);
  } else {
    padded_data_len = data_len;
  }
 
  // Write full buffer of 4-byte words
  return FlashProgram((unsigned long *)data, page_addr, padded_data_len);
}
 
 
void boot_firmware(void)
{
  uart_write_str(UART2, (char *) fw_release_message_address);
  // Boot the firmware
    __asm(
    "LDR R0,=0x10001\n\t"
    "BX R0\n\t"
  );
}
 
int
sha_hmac(char* key, int key_len, char* data, int len, char* out) {
/* this code HMACs code using SHA256
Inputs: key, key length, data, data length, and output
This funciton uses a key to HMAC given data and assigns it to the output variable
*/
    br_hmac_key_context kc;
    br_hmac_context ctx;
    br_hmac_key_init(&kc, &br_sha256_vtable, key, key_len);
    br_hmac_init(&ctx, &kc, 0);
    br_hmac_update(&ctx, data, len);
    br_hmac_out(&ctx, out);
 
    return 32;
}
 
int
aes_decrypt(char* key, char* iv, char* ct, int len) {
// This function decrypts a cipher with a key and iv. It overwrites whatever variable is passed in as ct
    br_block_cbcdec_class* vd = &br_aes_big_cbcdec_vtable;
    br_aes_gen_cbcdec_keys v_dc;
    const br_block_cbcdec_class **dc;
 
    dc = &v_dc.vtable;
    vd->init(dc, key, KEY_LEN);
    vd->run(dc, iv, ct, len);
 
    return 1;
}
 
static void
test_HKDF_inner(const br_hash_class *dig, const char *ikm,
    const char *salthex, const char *infohex, const char *okmhex,unsigned char *key)
{
    unsigned char saltbuf[100], info[100], okm[100], tmp[107], res[107];
    const unsigned char *salt;
    size_t ikm_len, salt_len, info_len, okm_len;
    br_hkdf_context hc;
    size_t u;
    
    ikm_len = 32;
    if (salthex == NULL) {
        salt = BR_HKDF_NO_SALT;
        salt_len = 0;
    } else {
        salt = salthex;
        salt_len = 32;
    }
    info_len = hextobin(info, infohex);
    okm_len = hextobin(okm, okmhex);

    br_hkdf_init(&hc, dig, salt, salt_len);
    br_hkdf_inject(&hc, ikm, ikm_len);
    br_hkdf_flip(&hc);
    br_hkdf_produce(&hc, info, info_len, key, okm_len);
    //check_equals("KAT HKDF 1", tmp, okm, okm_len);
}

static size_t
hextobin(unsigned char *dst, const char *src)
{
	size_t num;
	unsigned acc;
	int z;
 
	num = 0;
	z = 0;
	acc = 0;
	while (*src != 0) {
		int c = *src ++;
		if (c >= '0' && c <= '9') {
			c -= '0';
		} else if (c >= 'A' && c <= 'F') {
			c -= ('A' - 10);
		} else if (c >= 'a' && c <= 'f') {
			c -= ('a' - 10);
		} else {
			continue;
		}
		if (z) {
			*dst ++ = (acc << 4) + c;
			num ++;
		} else {
			acc = c;
		}
		z = !z;
	}
	return num;
}