#ifndef HALF_FIT_H_
#define HALF_FIT_H_

typedef unsigned char   uint8_t;
typedef unsigned int    uint32_t;

static const uint32_t totalSize = 32768;

uint32_t * baseAddress;

uint32_t * buckets[11];

static const uint32_t PREV_PTR_HEADER_BITS = (uint32_t) 0b11111111110000000000000000000000;
static const uint32_t NEXT_PTR_HEADER_BITS = (uint32_t) 0b00000000001111111111000000000000;
static const uint32_t SIZE_HEADER_BITS = (uint32_t) 0b00000000000000000000111111111100;
static const uint32_t ALLOCATED_FLAG_HEADER_BITS = (uint32_t) 0b00000000000000000000000000000010;

uint32_t constantAddress;

uint32_t  changingAddressBits;

void  half_init( void );
void *half_alloc( uint32_t );
void  half_free( void * );

#endif
