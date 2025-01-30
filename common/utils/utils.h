#ifndef _UTILS_H
#define _UTILS_H


#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <common/utils/oai_allocator.h>
#include <sys/types.h>
#include <common/utils/assertions.h>

#define sizeofArray(a) (sizeof(a)/sizeof(*(a)))
#define CHECK_INDEX(ARRAY, INDEX) assert((INDEX) < sizeofArray(ARRAY))

// Prevent double evaluation in max macro
#define cmax(a,b) ({ __typeof__ (a) _a = (a); \
                     __typeof__ (b) _b = (b); \
                     _a > _b ? _a : _b; })


#define cmax3(a,b,c) ( cmax(cmax(a,b), c) )  

// Prevent double evaluation in min macro
#define cmin(a,b) ({ __typeof__ (a) _a = (a); \
                     __typeof__ (b) _b = (b); \
                     _a < _b ? _a : _b; })

#ifdef __cplusplus
#ifdef min
#undef min
#undef max
#endif
#else
#define max(a,b) cmax(a,b)
#define min(a,b) cmin(a,b)
#endif
#if !defined(msg)
# define msg(aRGS...) LOG_D(PHY, ##aRGS)
#endif
const char *hexdump(const void *data, size_t data_len, char *out, size_t out_len);

// Converts an hexadecimal ASCII coded digit into its value. **
int hex_char_to_hex_value (char c);
// Converts an hexadecimal ASCII coded string into its value.**
int hex_string_to_hex_value (uint8_t *hex_value, const char *hex_string, int size);

char *itoa(int i);

#define STRINGIFY(S) #S
#define TO_STRING(S) STRINGIFY(S)
int read_version(const char *version, uint8_t *major, uint8_t *minor, uint8_t *patch);

#define findInList(keY, result, list, element_type) {\
    int i;\
    for (i=0; i<sizeof(list)/sizeof(element_type) ; i++)\
      if (list[i].key==keY) {\
        result=list[i].val;\
        break;\
      }\
    AssertFatal(i < sizeof(list)/sizeof(element_type), "List %s doesn't contain %s\n",#list, #keY); \
  }
#ifdef __cplusplus
}
#endif

#endif
