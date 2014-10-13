#ifndef HALF_FIT_H_
#define HALF_FIT_H_

typedef unsigned char   uint8_t;
typedef unsigned short  uint16_t;
typedef unsigned int    uint32_t;

static const uint16_t totalSize = 32768;

uint8_t* baseAddress;

uint8_t* buckets[11];

uint32_t constantAddress;

uint32_t  changingAddressBits;

uint16_t unAllocatedSize;

void  half_init( void );
void *half_alloc( uint16_t );
void  half_free( void * );

#endif
