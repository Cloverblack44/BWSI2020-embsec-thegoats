#include "uart.h"
#include "bearssl.h"
#include <stdlib.h>
#include <string.h>

int * generate_keys(char[] *mySalt){
    size_t size = strlen(mySalt)
    
    //get key from file
    ptr = fopen("secret_build_output.txt", 'r');
    
    //Read the data from the file
    key = fscanf(ptr)
    keyLength = strlen(key)
        
    //needed variables
    //const br_hash_class *digest_class = &br_sha512_vtable;
    unsigned char tmp[32]; //temporary storage 
    unsigned char info[100]; //I still don't know what info is... just gonna leave this here
    br_hkdf_context hc;
    
    br_hkdf_init(&hc, &br_sha512_vtable, &mySalt, size);
    br_hkdf_inject(&hc, key, keyLength);
    br_hkdf_flip(&hc);
    myKey = br_hkdf_produce(&hc, info, strlen(info), tmp, 32)
    
    key1 = myKey >> 256; //first sixteen bytes is key1
    tempKey = myKey << 256;
    key2 = tempKey >> 256; //second sixteen bytes is key2
    
    static int  answer[2];
    answer = {key1, key2};
    return answer;
    
    /*
    test_HKDF_inner(const br_hash_class *dig, const char *ikmhex,
	const char *salthex, const char *infohex, const char *okmhex)

    unsigned char ikm[100], saltbuf[100], info[100], okm[100], tmp[107];
	const unsigned char *salt;
	size_t ikm_len, salt_len, info_len, okm_len;
	br_hkdf_context hc;
	size_t u;
    
    br_hkdf_init(&hc, dig, salt, salt_len);
	br_hkdf_inject(&hc, ikm, ikm_len);
	br_hkdf_flip(&hc);
	br_hkdf_produce(&hc, info, info_len, tmp, okm_len);
	check_equals("KAT HKDF 1", tmp, okm, okm_len); */
}