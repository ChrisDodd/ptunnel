#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
uint64_t tea_encode(uint64_t v, uint64_t k0, uint64_t k1);
uint64_t tea_decode(uint64_t v, uint64_t k0, uint64_t k1);
#ifdef __cplusplus
}
#endif
