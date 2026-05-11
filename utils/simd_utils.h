#pragma once


#if defined(_MSC_VER) && !defined(__clang__)
#define simd_u8(v, i) ((v).m128i_u8[(i)])
#define simd_extract_u32(v, i) ((v).m128i_u32[(i)])
#define simd_extract_u8(v, i) ((v).m128i_u8[(i)])
#define simd_extract_i32(v, i) ((v).m128i_i32[(i)])

#elif defined(__SSE4_1__) || (defined(_MSC_VER) && _MSC_VER >= 1500)
#include <smmintrin.h>
#define simd_extract_i32(v, i) _mm_extract_epi32((v), (i))
#define simd_extract_u32(v, i) ((unsigned int)_mm_extract_epi32((v), (i)))
#define simd_extract_u8(v, i) ((unsigned char)_mm_extract_epi8((v), (i)))

#endif

#ifndef simd_extract_i32
#define simd_extract_i32(v, i) (((const int*)&(v))[(i)])
#endif

#ifndef simd_extract_u32
#define simd_extract_u32(v, i) (*(((unsigned int*)&(v)) + (i)))
#endif

#ifndef simd_u8
#define simd_u8(v, i) (((unsigned char*)&(v))[(i)])
#endif
	
#ifndef simd_extract_u8
#define simd_extract_u8(v, i) (((const unsigned char*)&(v))[(i)])
#endif