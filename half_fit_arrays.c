#include <stdio.h>

char memBlock[];

void initHeader(int requestedSize, int address, char allocated);

void* buckets[11];

void  half_init() {
    int size = 32768;
    memBlock = (char[]) malloc(size);
    initHeader(size,0,(char)0);

}

void *half_alloc( int requestedSize) {
    int address;

    address = findAddress(requestedSize);
    initHeader(requestedSize, address);
}

void initHeader(int size, int address, char allocated) {
    // Divide size by 32 for storage
    int storedSize = size >> 5;
    // Divide size by 32 for storage
    int storedAddress = address >> 5;
    // Divide size by 32 for storage
    int storedNextAddress = (address + size) >> 5;
    memBlock[address] = (char) (storedAddress >> 2);
    memBlock[address + 1] = (char) ((storedAddress & 3) << 6) + (char) (storedNextAddress >> 4);
    memBlock[address + 2] = (char) ((storedNextAddress & 15) << 4) + (char) (storedSize >> 6);
    memBlock[address + 3] = (char) ((storedSize & 63) << 2) + (allocated << 1);
}

// or void *half_alloc( unsigned int );

void  half_free( void * ptrToDelete ) {
	int i;
	int address = ptrToDelete;
	int previousMem;
	int nextMem;
	int size;
	int storedPreviousMem = ((int) (memBlock[address] << 2)) + ((int) (memBlock[address + 1] >> 6));
	int storedNextMem = ((int) ((memBlock[address + 1] & 63) << 4)) + ((int) (memBlock[address + 2] >> 4));
	int storedSize = ((int) ((memBlock[address + 2] & 15) << 6)) + ((int) memBlock[address + 3] >> 2);
	previousMem = storedPreviousMem << 5;
	nextMem = storedNextMem << 5;
	size = storedSize << 5;
    // check if prev or next in mem block are both free too
	if ((((memBlock[previousMem + 3] & 2) >> 1) == 0) && (((memBlock[nextMem + 3] & 2) >> 1) == 0)) {
    // if so, merge with free neighbour
        // remove both from old buckets
        // get new header values  -- new size, new next pointer
			// add sizes together
			size += (((int) ((memBlock[previousMem + 2] & 15) << 6)) + ((int) memBlock[previousMem + 3] >> 2)) << 5;
			size += (((int) ((memBlock[nextMem + 2] & 15) << 6)) + ((int) memBlock[nextMem + 3] >> 2)) << 5;
			storedSize = size >> 5;
			// init new header
			memBlock[previousMem + 1] = (char) (memBlock[previousMem + 1] & 192) + (char) (memBlock[nextMem + 1] & 63);
			memBlock[previousMem + 2] = (char) (memBlock[nextMem + 2] & 240) + (char) (storedSize >> 6);
			memBlock[previousMem + 3] = (char) ((storedSize & 63) << 2);
			// removed ptrToDelete's header
			for ( i = 0; i < 7; i++) {
				memBlock[address + i] = 0;
				memBlock[nextMem + i] = 0;
			}
	} else if (((memBlock[previousMem + 3] & 2) >> 1) == 0) {
    // if only prev free, merge with prev
        // remove both from old buckets
        // get new header values  -- new size, new next pointer
			// add sizes together
			size += (((int) ((memBlock[previousMem + 2] & 15) << 6)) + ((int) memBlock[previousMem + 3] >> 2)) << 5;
			storedSize = size >> 5;
			// init new header
			memBlock[previousMem + 1] = (char) (memBlock[previousMem + 1] & 192) + (char) (storedNextMem >> 4);
			memBlock[previousMem + 2] = (char) ((storedNextMem & 15) << 4) + (char) (storedSize >> 6);
			memBlock[previousMem + 3] = (char) ((storedSize & 63) << 2);
			// removed ptrToDelete's header
			for ( i = 0; i < 7; i++) {
				memBlock[address + i] = 0;
			}
	} else if (((memBlock[nextMem + 3] & 2) >> 1) == 0) {{
    // if only next free, merge with next
        // remove both from old buckets
			// add sizes together
			size += (((int) ((memBlock[nextMem + 2] & 15) << 6)) + ((int) memBlock[nextMem + 3] >> 2)) << 5;
			storedSize = size >> 5;
			// init new header
			memBlock[address + 1] = (char) ((storedPreviousMem & 3) << 6) + (char) (memBlock[nextMem + 1] & 63);
			memBlock[address + 2] = (char) (memBlock[nextMem + 2] & 240) + (char) (storedSize >> 6);
			memBlock[address + 3] = (char) ((storedSize & 63) << 2);
			// removed next in memeroy block's header
			for ( i = 0; i < 7; i++) {
				memBlock[nextMem + i] = 0;
			}
	}
    // add to appropriate bucket
    //buckets[(((log10(size))/(log10(2)))-5)] = ptrToDelete;

}
