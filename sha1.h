/* Public API for Steve Reid's public domain SHA-1 implementation */
/* This file is in the public domain */

#ifndef __SHA1_H
#define __SHA1_H

typedef struct {
    uint32_t state[5];
    uint32_t count[2];
    uint8_t  buffer[64];
} SHA_CTX;

#define SHA_DIGEST_LENGTH 20

#ifndef ALLINONE
void SHA1_Init(SHA_CTX *context);
void SHA1_Update(SHA_CTX *context, const uint8_t *data, unsigned long len);
void SHA1_Final(uint8_t *digest, SHA_CTX *context);
#endif

#endif /* __SHA1_H */
