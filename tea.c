#include "tea.h"

uint64_t tea_encode(uint64_t v, uint64_t k01, uint64_t k23)
{
uint32_t	y=v, z=v>>32, sum=0, delta=0x9e3779b9, n=32;
uint32_t        k0 = k01, k1 = k01 >> 32, k2 = k23, k3 = k23 >> 32;

    while(n-->0) {
	sum += delta;
	y += ((z<<4)+k0) ^ (z+sum) ^ ((z>>5)+k1);
	z += ((y<<4)+k2) ^ (y+sum) ^ ((y>>5)+k3); }
    return ((uint64_t)z << 32) + y;
}

uint64_t tea_decode(uint64_t v, uint64_t k01, uint64_t k23)
{
uint32_t	y=v, z=v>>32, sum, delta=0x9e3779b9, n=32;
uint32_t        k0 = k01, k1 = k01 >> 32, k2 = k23, k3 = k23 >> 32;

    sum = n*delta;
    while(n-->0) {
	z -= ((y<<4)+k2) ^ (y+sum) ^ ((y>>5)+k3);
	y -= ((z<<4)+k0) ^ (z+sum) ^ ((z>>5)+k1);
	sum -= delta; }
    return ((uint64_t)z << 32) + y;
}
