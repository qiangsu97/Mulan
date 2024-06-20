/* Copyright (c) 2024, Meili Authors*/

#ifndef _INCLUDE_SHA_UTILS_H
#define _INCLUDE_SHA_UTILS_H
#include <stdint.h>

#define SHA_HASH_SIZE 20 

typedef struct {
    uint32_t state[5];
    uint32_t count[2];
    unsigned char buffer[64];
} SHA1_CTX;

void SHA1Transform(
                   uint32_t state[5],
                   const unsigned char buffer[64]
                  );

void SHA1Init(
              SHA1_CTX * context
             );

void SHA1Update(
                SHA1_CTX * context,
                const unsigned char *data,
                uint32_t len
               );

void SHA1Final(
               unsigned char digest[20],
               SHA1_CTX * context
              );

void SHA1(
          char *hash_out,
          const char *str,
          int len
          );

#endif /* _INCLUDE_SHA_UTILS_H */
