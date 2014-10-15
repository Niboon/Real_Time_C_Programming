#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include "half_fit.h"


inline int getBucketIndex(uint32_t size) {
    return (int)ceil(((log10(size))/(log10(2)))-5);
}

inline uint32_t unMaskPreviousMemPtr(uint32_t *currentPtr) {
    return (*currentPtr & PREV_PTR_HEADER_BITS) >> 22;
}

inline uint32_t unMaskNextMemPtr(uint32_t *currentPtr) {
    return (*currentPtr & NEXT_PTR_HEADER_BITS) >> 12;
}

inline uint32_t replaceZeroSizeWithMaxSize(uint32_t blockSize) {
    if (blockSize == 0) {
        blockSize = (uint32_t) 32768;
    }
    return blockSize;
}

inline uint32_t unMaskSize(uint32_t *currentPtr) {
    uint32_t blockSize = (*currentPtr & SIZE_HEADER_BITS) << 3;
    return replaceZeroSizeWithMaxSize(blockSize);
}


inline uint32_t *getAbsoluteAddressPtr(uint32_t storedPtr) {
    return (uint32_t *)((storedPtr << 5) + constantAddress);
}

inline uint32_t maskPtrForStorage(uint32_t *ptrToMask) {
    return (((uint32_t) ptrToMask) & changingAddressBits) >> 5;
}

inline uint32_t maskSizeForStorage(uint32_t blockSize) {
    return (blockSize >> 5) & ((uint32_t)1023);                             // Make sure only 10 bits are used, not 11
}

inline uint32_t *getPrevBucketPtr(uint32_t *ptrInBucket) {
    return getAbsoluteAddressPtr((*(ptrInBucket + 1) & PREV_PTR_HEADER_BITS) >> 22);
}

inline uint32_t *getNextBucketPtr(uint32_t *ptrInBucket) {
    return getAbsoluteAddressPtr((*(ptrInBucket + 1) & NEXT_PTR_HEADER_BITS) >> 12);
}

inline uint32_t unMaskAllocated(uint32_t *currentPtr) {
    return (*currentPtr & (uint32_t) 0b10) >> 1;
}

inline void initHeader(uint32_t size, uint32_t * previousPtr, uint32_t * currentPtr, uint32_t * nextPtr, bool allocated) {
    // Divide size by 32 for storage
    uint32_t storedSize = maskSizeForStorage(size);
    uint32_t storedPreviousAddress = maskPtrForStorage(previousPtr);
    uint32_t storedNextAddress = maskPtrForStorage(nextPtr);
    *currentPtr =  (storedPreviousAddress << 22) + (storedNextAddress << 12) + (storedSize << 2) + (((uint32_t) allocated) << 1);
}

inline void replaceHeaderAfterMerge(uint32_t *newHeaderPtr, uint32_t *prevToNewNextPtr, uint32_t newSize, bool isAllocated) {
    *newHeaderPtr = (*(newHeaderPtr) & PREV_PTR_HEADER_BITS)
            + (*(prevToNewNextPtr) & NEXT_PTR_HEADER_BITS)
            + (((uint32_t) newSize) << 2 )
            + (((uint32_t) isAllocated) << 1);
}

inline void updateHeaderPtrs(uint32_t *ptr, uint32_t newPrevious, uint32_t newNext) {
    *ptr = (newPrevious << 22) + (newNext << 12) + (*ptr & (SIZE_HEADER_BITS + ALLOCATED_FLAG_HEADER_BITS));
}

inline void updateHeaderPrevious(uint32_t *ptr, uint32_t newPrevious) {
    *ptr = (newPrevious << 22) + (*ptr & ~PREV_PTR_HEADER_BITS);
}

inline void updateHeaderNext(uint32_t *ptr, uint32_t newNext) {
    *ptr = (newNext << 12) + (*ptr & ~NEXT_PTR_HEADER_BITS);
}

inline void updateHeaderSize(uint32_t *ptr, uint32_t newSize) {
    *ptr = (newSize << 2) + (*ptr & ~(SIZE_HEADER_BITS));
}

inline void updateHeaderAllocated(uint32_t *ptr, bool isAllocated) {
    *ptr = (((uint32_t) isAllocated) << 1) + (*ptr & ~(ALLOCATED_FLAG_HEADER_BITS));
}

inline void addToBucket(uint32_t *blockPtr, int bucketIndex) {
    uint32_t storedBlockAddress = maskPtrForStorage(blockPtr);
    uint32_t * oldBucketHead = buckets[bucketIndex];
    // if nothing in bucket, just make bucket point to the new address that we have deallocated
    if (oldBucketHead == NULL) {
        // change block to point to itself for "previous in bucket" and "next in bucket"
        updateHeaderPtrs(blockPtr + 1, storedBlockAddress, storedBlockAddress);
    } else {
        // change old bucket head's "previous pointer in bucket" to the new bucket head's address
        updateHeaderPrevious(oldBucketHead + 1, storedBlockAddress);
        // change new bucket head's "next pointer in bucket" to the old bucket head's address
        updateHeaderNext(blockPtr + 1, maskPtrForStorage(oldBucketHead));
    }
    updateHeaderAllocated(blockPtr, false);
    // move bucket head to point to new bucket head
    buckets[bucketIndex] = blockPtr;
}

inline void removeFromBucket(int bucketIndex, uint32_t *currentPtr) {
    uint32_t *ptrInBucket, *prevBucketPtr, *nextBucketPtr, *newBucketHead;
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
            updateHeaderPrevious(newBucketHead + 1, maskPtrForStorage(newBucketHead));
        } else if (ptrInBucket == nextBucketPtr){
            // when matched ptr is just the tail in the bucket list
            // Change previousBucket's next to point to itself to denote that it is the new tail
            updateHeaderNext(prevBucketPtr + 1, maskPtrForStorage(prevBucketPtr));
        } else {
            // when matched ptr is not the head or the tail
            // Change previousBucket's next to point to nextBucket
            updateHeaderNext(prevBucketPtr + 1, maskPtrForStorage(nextBucketPtr));
            // Change nextBucket's previous to point to previousBucket
            updateHeaderPrevious(nextBucketPtr + 1, maskPtrForStorage(prevBucketPtr));
        }
    }
}


void  half_init() {
    int bucketIndex;
    changingAddressBits = 32736;                                            // value of ten binary ones bit shifted left by 5,
                                                                            // Used to indicate the 10 bits that would change in the address
    baseAddress = (uint32_t *) malloc(totalSize);

    constantAddress = ((uint32_t)baseAddress) & ~(changingAddressBits);     // All the bits of the base address except the 10 bits that can change
                                                                            // the complement of the bits that would change,
                                                                            // Used to "&" with the base address to get
                                                                            // the bits before and after the 10 bits that would change

    initHeader(totalSize, baseAddress, baseAddress, baseAddress, false);

    // Add to bucket
    bucketIndex = getBucketIndex(totalSize);
    addToBucket(baseAddress, bucketIndex);
}

void *half_alloc(uint32_t requestedSize) {
    int bucketIndex;
    uint32_t *block, *excessBlock;
    uint32_t blockSize, excessSize;
    // roundup requested  size to multiple of 32
    requestedSize = (uint32_t) ((ceil(requestedSize / 32.0)) * 32);
    // find bucket + 1
    bucketIndex = getBucketIndex(requestedSize);
    block = NULL;
    // Handle maximum allocation size into largest bucket by not incrementing (Ignoring Half Fit)
    if (bucketIndex == 10) {
        block = buckets[10];
    } else if (bucketIndex > 10){
        printf("INVALID SIZE, TOO BIG\n");
        return NULL;
    } else {
        // while loop through buckets array if not block found in the calculated half fit bucket
        while (block == NULL) {
            bucketIndex++;                                                  // Plus one first because of Half Fit
            if (bucketIndex > 10) {
                printf("FAILED TO FIND APPROPRIATE BUCKET\n");
                return NULL;
            }
            block = buckets[bucketIndex];
        }
    }
    blockSize = unMaskSize(block);
    excessSize = blockSize - requestedSize;
    // round down excess size to multiple of 32
    excessSize = (uint32_t) ((floor(excessSize / 32.0)) * 32);
    if (requestedSize > blockSize) {
        printf("FAILED TO FIND APPROPRIATE BUCKET\n");
        return NULL;
    }

    // remove from bucket
    removeFromBucket(bucketIndex, block);
    // If difference between requestedSize and block size is greater than 32
    if (excessSize < 32) {
        // make change to allocated in header
        updateHeaderAllocated(block, true);
        return block;
    } else {
        blockSize = requestedSize;
        excessBlock = block + (blockSize >> 2);
        // cut up and put back into bucket
        // TODO: Make the excess the first part of the block and return what is left for the requested allocation to save bucket manipulation since we don't remove (and rearrange), just edit
        uint32_t *nextMemPtr = getAbsoluteAddressPtr(unMaskNextMemPtr(block));
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
        updateHeaderNext(block, maskPtrForStorage(block + blockSize));
        updateHeaderSize(block, maskSizeForStorage(blockSize));
        // make change to allocated in header
        updateHeaderAllocated(block, true);
    }
	return block;
}

void half_free( void * ptrToDelete ) {
    int bucketIndex;
    uint32_t *previousMemPtr, *nextMemPtr, *currentPtr, *nextNextMemPtr, previousIsAllocated, nextIsAllocated;
    uint32_t blockSize, previousSize, nextSize, storedPreviousMemPtr, storedCurrentPtr, storedNextMemPtr, storedBlockSize;
    currentPtr = ptrToDelete;
    blockSize = unMaskSize(currentPtr);
    storedPreviousMemPtr = unMaskPreviousMemPtr(currentPtr);
    storedCurrentPtr = maskPtrForStorage(currentPtr);
    storedNextMemPtr = unMaskNextMemPtr(currentPtr);
	previousMemPtr = getAbsoluteAddressPtr(storedPreviousMemPtr);
	nextMemPtr = getAbsoluteAddressPtr(storedNextMemPtr);
    nextNextMemPtr = getAbsoluteAddressPtr(unMaskNextMemPtr(nextMemPtr));
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
        replaceHeaderAfterMerge(previousMemPtr, nextMemPtr, storedBlockSize, false);
        // update next mem block's previous to point to new merged block
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
            replaceHeaderAfterMerge(previousMemPtr, getAbsoluteAddressPtr(unMaskPreviousMemPtr(previousMemPtr)), storedBlockSize, false);
        } else {
            replaceHeaderAfterMerge(previousMemPtr, currentPtr, storedBlockSize, false);
        }
        // update next mem block's previous to point to new merged block
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
            replaceHeaderAfterMerge(currentPtr, previousMemPtr, storedBlockSize, false);
        } else {
            replaceHeaderAfterMerge(currentPtr, nextMemPtr, storedBlockSize, false);
        }
        // update next mem block's previous to point to new merged block
        updateHeaderPrevious(nextNextMemPtr, storedCurrentPtr);
        // add to appropriate bucket
        bucketIndex = getBucketIndex(blockSize);
        addToBucket(currentPtr, bucketIndex);
    } else {
        // add to appropriate bucket
        bucketIndex = getBucketIndex(blockSize);
        addToBucket(currentPtr, bucketIndex);
    }
}