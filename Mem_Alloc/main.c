//#include <stdio.h>
//#include <stdlib.h>
//#include "half_fit.h"
//char *charToBinary(char n);
//
//main()
//{
////	system_init();
//    int i;
//    extern uint8_t* buckets[11];
//    uint8_t* allocated1;
//    uint8_t* allocated2;
//    uint8_t* allocated3;
//    uint8_t* allocated4;
//    uint8_t* allocated5;
//    printf("Start Program Run\n");
//	half_init();
//    printf("Address of entire memory block starts at %p \n",(void*) baseAddress);
//    printf("1st Byte:  %s\n", charToBinary((*(baseAddress))));
//    printf("2nd Byte:  %s\n", charToBinary(*(baseAddress +1)));
//    printf("3rd Byte:  %s\n", charToBinary(*(baseAddress +2)));
//    printf("4th Byte:  %s\n", charToBinary(*(baseAddress +3)));
//    printf("5th Byte:  %s\n", charToBinary(*(baseAddress +4)));
//    allocated1 = half_alloc(32768/2);
////    allocated2 = half_alloc(32768/4);
////    allocated3 = half_alloc(32768/8);
////    allocated4 = half_alloc(32768/16);
////    allocated5 = half_alloc(32768/32);
//    for (i=0; i<11; i++) {
//        printf("Bucket %d : %p\n",i,(void *)buckets[i]);
//    }
//    half_free(allocated1);
//    printf("after FREE\n");
////    half_free(allocated2);
////    half_free(allocated3);
////    half_free(allocated4);
////    half_free(allocated5);
//    for (i=0; i<11; i++) {
//        printf("Bucket %d : %p\n",i,(void *)buckets[i]);
//    }
//    return 0;
//}
//
//
//char *charToBinary(char n)
//{
//    static char binary[9];
//    int x;
//    for(x=0;x<8;x++)
//    {
//        binary[x] = n & 0x80 ? (char) '1' : (char) '0';
//        n <<= 1;
//    }
//    binary[x] = '\0';
//    return(binary);
//}