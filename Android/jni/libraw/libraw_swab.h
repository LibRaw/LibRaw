#ifndef __SWAB_H__
#define __SWAB_H__
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void swab(const void *p_src_, void *p_dst_, ssize_t n);
#ifdef __cplusplus
}
#endif
#endif
