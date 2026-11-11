#include <stddef.h>
void *memset(void *destination,int value,size_t size) {
    unsigned char *p=destination;
    while (size--) *p++=(unsigned char)value;
    return destination;
}
void *memcpy(void *destination,const void *source,size_t size) {
    unsigned char *d=destination;
    const unsigned char *s=source;
    while (size--) *d++=*s++;
    return destination;
}
