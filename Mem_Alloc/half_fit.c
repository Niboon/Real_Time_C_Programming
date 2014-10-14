#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include "half_fit.h"


inline int getBucketIndex(uint16_t size) {
    return (int)ceil(((log10(size))/(log10(2)))-5);
}

inline uint16_t unMaskPreviousMemPtr(uint8_t *currentPtr) {
    return (((uint16_t) *currentPtr) << 2) + (((uint16_t) *(currentPtr + 1)) >> 6);
}

inline uint16_t unMaskNextMemPtr(uint8_t *currentPtr) {
    return (((uint16_t) (*(currentPtr + 1) & 63)) << 4) + (((uint16_t) *(currentPtr + 2)) >> 4);
}

inline uint16_t replaceZeroSizeWithMaxSize(uint16_t blockSize) {
    if (blockSize == 0) {
        blockSize = 32768;
    }
    return blockSize;
}

inline uint16_t unMaskSize(uint8_t *currentPtr) {
    uint16_t blockSize = ((((uint16_t) (*(currentPtr + 2) & 15)) << 6) + (((uint16_t) *(currentPtr + 3)) >> 2)) << 5;
    return replaceZeroSizeWithMaxSize(blockSize);
}


inline uint8_t *getAbsoluteAddress(uint16_t storedPtr) {
    return (uint8_t *)((storedPtr << 5) + constantAddress);
}

inline uint16_t maskPtrForStorage(uint8_t *ptrToMask) {
    return (uint16_t) ((((uint32_t) ptrToMask) & changingAddressBits) >> 5);
}

inline uint16_t maskSizeForStorage(uint16_t blockSize) {
    return (blockSize >> 5) & ((uint16_t)1023);                             // Make sure only 10 bits are used, not 11
}

inline uint8_t *getPrevBucketPtr(uint8_t *ptrInBucket) {
    return getAbsoluteAddress((uint16_t) ((((uint16_t) *(ptrInBucket + 4)) << 2) + ((*(ptrInBucket + 5) & 192) >> 6)));
}

inline uint8_t *getNextBucketPtr(uint8_t *ptrInBucket) {
    return getAbsoluteAddress(((((uint16_t) (*(ptrInBucket + 5) & 63)) << 4) + ((uint16_t) (*(ptrInBucket + 6) & 240) >> 4)));
}

inline uint8_t unMaskAllocated(uint8_t *ptr) {
    return (uint8_t) ((*(ptr + 3) & 2) >> 1);
}

inline void initHeader(uint16_t size, uint8_t * previousPtr, uint8_t * currentPtr, uint8_t * nextPtr, bool allocated) {
    // Divide size by 32 for storage
    uint16_t storedSize = maskSizeForStorage(size);
    uint16_t storedPreviousAddress = maskPtrForStorage(previousPtr);
    uint16_t storedNextAddress = maskPtrForStorage(nextPtr);
    *(currentPtr) = (uint8_t) (storedPreviousAddress >> 2);
    *(currentPtr + 1) = (uint8_t) ((storedPreviousAddress & 3) << 6) + (uint8_t) (storedNextAddress >> 4);
    *(currentPtr + 2) = (uint8_t) ((storedNextAddress & 15) << 4) + (uint8_t) (storedSize >> 6);
    *(currentPtr + 3) = (uint8_t) ((storedSize & 63) << 2) + (uint8_t) ((allocated & 1) << 1);
}

inline void replaceHeader(uint8_t *newHeaderPtr, uint8_t *ptr, uint16_t storedSize, bool allocated) {
    *(newHeaderPtr + 1) = (uint8_t) (*(newHeaderPtr + 1) & 192) + (uint8_t) (*(ptr + 1) & 63);
    *(newHeaderPtr + 2) = (uint8_t) (*(ptr + 2) & 240) + (uint8_t) (storedSize >> 6);
    *(newHeaderPtr + 3) = (uint8_t) ((storedSize & 63) << 2) + (uint8_t) ((allocated) << 1);
}

inline void updateHeaderPrevious(uint8_t *ptr, uint16_t newPrevious) {// update next mem block's previous to point to new merged block
    *(ptr) = (uint8_t) (newPrevious >> 2);
    *(ptr + 1) = (uint8_t) ((newPrevious & 3) << 6) + (uint8_t) (*(ptr + 1) & 63);
}

inline void addToBucket(uint8_t *blockPtr, int bucketIndex) {
    uint16_t storedBlockAddress = maskPtrForStorage(blockPtr);
    uint8_t * oldBucketHead = buckets[bucketIndex];
    // if nothing in bucket, just make bucket point to the new address that we have deallocated
    if (oldBucketHead == NULL) {
        // change block to point to itself for "previous in bucket" and "next in bucket"
        *(blockPtr + 4) = (uint8_t) (storedBlockAddress >> 2);
        *(blockPtr + 5) = (uint8_t) ((storedBlockAddress & 3) << 6) + (uint8_t) (storedBlockAddress >> 4);
        *(blockPtr + 6) = (uint8_t) ((storedBlockAddress & 15) << 4);
    }
    else {
        // change old bucket head's "previous pointer in bucket" to the new bucket head's address
        *(oldBucketHead + 4) = (uint8_t) (storedBlockAddress >> 2);
        *(oldBucketHead + 5) = (uint8_t) ((storedBlockAddress & 3) << 6) + (uint8_t) (*(oldBucketHead + 5) & 63);
        // change new bucket head's "next pointer in bucket" to the old bucket head's address
        *(blockPtr + 4) = (uint8_t) (storedBlockAddress >> 2);
        *(blockPtr + 5) = (uint8_t) ((storedBlockAddress & 3) << 6) + (uint8_t) (maskPtrForStorage(oldBucketHead) >> 4);
        *(blockPtr + 6) = (uint8_t) (maskPtrForStorage(oldBucketHead) << 4);
    }
    *(blockPtr + 3) = (uint8_t) (*(blockPtr + 3) & 252) + ((uint8_t) 0 << 1);
    // move bucket head to point to new bucket head
    buckets[bucketIndex] = blockPtr;
    unAllocatedSize += unMaskSize(blockPtr);
}

inline void removeFromBucket(int bucketIndex, uint8_t *currentPtr) {
    uint8_t *ptrInBucket, *prevBucketPtr, *nextBucketPtr, *newBucketHead;
    ptrInBucket = buckets[bucketIndex];
    while(ptrInBucket != NULL && currentPtr != ptrInBucket) {
        nextBucketPtr = getNextBucketPtr(ptrInBucket);
        if ( ptrInBucket != nextBucketPtr) {
            ptrInBucket = nextBucketPtr;
        } else {
            // means that ptr is at tail and no ptr with matching address is found in this bucket
            ptrInBucket = NULL;
            break;
        }
    }
    if (ptrInBucket != NULL) {
        prevBucketPtr = getPrevBucketPtr(ptrInBucket);
        nextBucketPtr = getNextBucketPtr(ptrInBucket);
        // check the next and previous pointers of the matched ptr to find it's location in the bucket
        if (ptrInBucket == prevBucketPtr && ptrInBucket == nextBucketPtr) {
            // when matched ptr is the only one in the bucket
            // make the head pointer in the buckets array for this bucket size to become NULL
            buckets[bucketIndex] = NULL;
        } else if (ptrInBucket == prevBucketPtr) {
            // when matched ptr is just the head in the bucket list
            // move the head pointer in the buckets array
            newBucketHead = nextBucketPtr;
            buckets[bucketIndex] = newBucketHead;
            // denote the newBucketHead as the new head by pointing it's previous pointer at itself
            *(newBucketHead + 4) =  ((uint8_t) (maskPtrForStorage(newBucketHead)) >> 2);
            *(newBucketHead + 5) =  ((uint8_t) (maskPtrForStorage(newBucketHead) & 3) << 6) + (uint8_t)(*(newBucketHead + 5) & 63);
        } else if (ptrInBucket == nextBucketPtr){
            // when matched ptr is just the tail in the bucket list
            // Change previousBucket's next to point to itself to denote that it is the new tail
            *(prevBucketPtr + 5) = (uint8_t) (*(prevBucketPtr + 5) & 192) + (uint8_t) maskPtrForStorage(prevBucketPtr) >> 4;
            *(prevBucketPtr + 6) = (uint8_t) (maskPtrForStorage(prevBucketPtr) & 15) << 4;
        } else {
            // when matched ptr is not the head or the tail
            // Change previousBucket's next to point to nextBucket
            *(prevBucketPtr + 5) = (uint8_t) (*(prevBucketPtr + 5) & 192) + (uint8_t) maskPtrForStorage(nextBucketPtr) >> 4;
            *(prevBucketPtr + 6) = (uint8_t) (maskPtrForStorage(nextBucketPtr) & 15) << 4;
            // Change nextBucket's previous to point to previousBucket
            *(nextBucketPtr + 4) = (uint8_t) maskPtrForStorage(prevBucketPtr) >> 2;
            *(nextBucketPtr + 5) = (uint8_t) (maskPtrForStorage(prevBucketPtr) & 3) << 6  + (uint8_t)(*(nextBucketPtr + 5) & 63);
        }
    }
    unAllocatedSize -= unMaskSize(currentPtr);
}


void  half_init() {
    int bucketIndex;
    changingAddressBits = 32736;                                            // value of ten binary ones bit shifted left by 5,
                                                                            // Used to indicate the 10 bits that would change in the address
    baseAddress = (uint8_t*) malloc(totalSize);

    constantAddress = ((uint32_t)baseAddress) & ~(changingAddressBits);     // All the bits of the base address except the 10 bits that can change
                                                                            // the complement of the bits that would change,
                                                                            // Used to "&" with the base address to get
                                                                            // the bits before and after the 10 bits that would change

    initHeader(totalSize, baseAddress, baseAddress, baseAddress, false);

    // Add to bucket
    bucketIndex = getBucketIndex(totalSize);
    addToBucket(baseAddress, bucketIndex);
    unAllocatedSize = totalSize;
}

void *half_alloc(uint16_t requestedSize) {
    int bucketIndex;
    uint8_t *block, *excessBlock;
    uint16_t blockSize, excessSize;
    // find bucket + 1
    bucketIndex = getBucketIndex(requestedSize);
    block = NULL;
    // while loop through buckets array if not block found in the calculated half fit bucket
    while (block == NULL) {
        if (bucketIndex < 10) {
            bucketIndex++;                                                  // Plus one first because of Half Fit
        }
        // Else, Handle maximum allocation size of into largest bucket by not incrementing (Ignoring Half Fit)
        block = buckets[bucketIndex];
    }
    // remove from bucket
    removeFromBucket(bucketIndex, block);

    blockSize = unMaskSize(block);
    excessSize = blockSize - requestedSize;
    // If difference between requestedSize and block size is greater than 32
    if (excessSize < 32) {
        // make change to allocated in header
        *(block + 3) = (uint8_t) (*(block + 3) & 253) + (uint8_t) 2;
        printf("UNALLOCATED SIZE : %u\n", unAllocatedSize);
        return block;
    } else {
        // roundup requested block's size to multiple of 32
        blockSize = (uint16_t) ((ceil(requestedSize / 32.0)) * 32);
        // round down excess size to multiple of 32
        excessSize = (uint16_t) ((floor(excessSize / 32.0)) * 32);
        excessBlock = block + blockSize;
        // cut up and put back into bucket
        // TODO: Make the excess the first part of the block and return what is left for the requested allocation to save bucket manipulation since we don't remove (and rearrange), just edit
        uint8_t *nextMemPtr = getAbsoluteAddress(unMaskNextMemPtr(block));
        if (block == nextMemPtr) {
            // Means that block is last in total MemBlock
            // Denote that excessBlock is now the last in total MemBlock by pointing it's next to itself
            nextMemPtr = excessBlock;
        }
        initHeader(excessSize, block, excessBlock, nextMemPtr, false);
        // add to appropriate bucket
        bucketIndex = getBucketIndex(excessSize);
        addToBucket(excessBlock, bucketIndex);

        // declare new header for block that we are returning
        *(block + 1) = (uint8_t) (*(block + 1) & 192) + ((uint8_t) (maskPtrForStorage(block + blockSize) >> 4));
        *(block + 2) = (uint8_t) ((uint8_t) (maskPtrForStorage(block + blockSize) << 6)) + (uint8_t) (maskSizeForStorage(blockSize) >> 6);
//        *(block + 3) = (uint8_t) ((maskSizeForStorage(blockSize) & 63) << 2) + (uint8_t) 2;
        *(block + 3) = (uint8_t) ((maskSizeForStorage(blockSize) & 63) << 2) + (uint8_t) 2;
    }
//    printf("UNALLOCATED SIZE : %u\n", unAllocatedSize);
	return block;
}

void half_free( void * ptrToDelete ) {
    int bucketIndex;
	uint8_t *previousMemPtr, *nextMemPtr, *currentPtr, *nextNextMemPtr, previousIsAllocated, nextIsAllocated;
    uint16_t blockSize, previousSize, nextSize, storedPreviousMemPtr, storedCurrentPtr, storedNextMemPtr, storedBlockSize;
    currentPtr = ptrToDelete;
    blockSize = unMaskSize(currentPtr);
    storedPreviousMemPtr = unMaskPreviousMemPtr(currentPtr);
    storedCurrentPtr = maskPtrForStorage(currentPtr);
    storedNextMemPtr = unMaskNextMemPtr(currentPtr);
	previousMemPtr = getAbsoluteAddress(storedPreviousMemPtr);
	nextMemPtr = getAbsoluteAddress(storedNextMemPtr);
    nextNextMemPtr = getAbsoluteAddress(unMaskNextMemPtr(nextMemPtr));
    previousSize = unMaskSize(previousMemPtr);
    nextSize = unMaskSize(nextMemPtr);
    previousIsAllocated = unMaskAllocated(previousMemPtr);
    nextIsAllocated = unMaskAllocated(nextMemPtr);
    // check if prev or next in mem block are both free too
    if ((previousIsAllocated == 0) && (nextIsAllocated == 0)) {
        // if so, merge with free neighbours
        // remove previous and next from old buckets
        bucketIndex = getBucketIndex(previousSize);
        removeFromBucket(bucketIndex, previousMemPtr);
        bucketIndex = getBucketIndex(nextSize);
        removeFromBucket(bucketIndex, nextMemPtr);
        // add all 3 sizes together
        blockSize += previousSize;
        blockSize += nextSize;
        storedBlockSize = maskSizeForStorage(blockSize);
        // declare new header
        replaceHeader(previousMemPtr, nextMemPtr, storedBlockSize, false);
        updateHeaderPrevious(nextNextMemPtr, storedPreviousMemPtr);
        // add to appropriate bucket
        bucketIndex = getBucketIndex(blockSize);
        addToBucket(previousMemPtr, bucketIndex);
    } else if (previousIsAllocated == 0) {
        // if only prev free, merge with prev
        // remove previous from old buckets
        bucketIndex = getBucketIndex(previousSize);
        removeFromBucket(bucketIndex, previousMemPtr);
        // add sizes together
        blockSize += previousSize;
        storedBlockSize = maskSizeForStorage(blockSize);
        // declare new header

        if (currentPtr == nextMemPtr) {
            replaceHeader(previousMemPtr, getAbsoluteAddress(unMaskPreviousMemPtr(previousMemPtr)), storedBlockSize, false);
        } else {
            replaceHeader(previousMemPtr, currentPtr, storedBlockSize, false);
        }
        updateHeaderPrevious(nextMemPtr, storedPreviousMemPtr);
        // add to appropriate bucket
        bucketIndex = getBucketIndex(blockSize);
        addToBucket(previousMemPtr, bucketIndex);
    } else if (nextIsAllocated == 0) {
        // if only next free, merge with next
        // remove next from old bucket
        bucketIndex = getBucketIndex(nextSize);
        removeFromBucket(bucketIndex, nextMemPtr);
        // add sizes together
        blockSize += nextSize;
        storedBlockSize = maskSizeForStorage(blockSize);
        // declare new header
        if ( nextMemPtr == nextNextMemPtr) {
            replaceHeader(currentPtr, previousMemPtr, storedBlockSize, false);
        } else {
            replaceHeader(currentPtr, nextMemPtr, storedBlockSize, false);
        }
        updateHeaderPrevious(nextNextMemPtr, storedCurrentPtr);
        // add to appropriate bucket
        bucketIndex = getBucketIndex(blockSize);
        addToBucket(currentPtr, bucketIndex);
    } else {
        // add to appropriate bucket
        bucketIndex = getBucketIndex(blockSize);
        addToBucket(currentPtr, bucketIndex);
    }
//    printf("UNALLOCATED SIZE : %u\n", unAllocatedSize);
}