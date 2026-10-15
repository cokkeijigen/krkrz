/****************************************************************************/
/*! @file
@brief NV12 -> BGRA 変換の実装 (AVX2 / SSE2 / スカラ)

	解説はヘッダ参照。
-----------------------------------------------------------------------------
*****************************************************************************/
#include "tjsCommHead.h"

#include <emmintrin.h>
#include <immintrin.h>
#include "tvpgl_ia32_intf.h"
#include "DetectCPU.h"
#include "MFVideoColor.h"

// {3231564E-0000-0010-8000-00AA00389B71}
const GUID kMFVideoFormat_NV12 =
	{ 0x3231564e, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };

bool TVPMFColorHasSSE2()
{
	return ( TVPGetCPUType() & TVP_CPU_HAS_SSE2 ) != 0;
}

bool TVPMFColorHasAVX2()
{
	return ( TVPGetCPUType() & TVP_CPU_HAS_AVX2 ) != 0;
}

// 色差は Q6 (>>6)。Q7 にすると B の 2.112 が -128*270 となり int16 を溢れる。
// 601: R=+1.596V  G=-0.391U-0.813V  B=+2.017U
// 709: R=+1.793V  G=-0.213U-0.533V  B=+2.112U
static void GetChromaCoef( bool bt709, int *crv, int *cgu, int *cgv, int *cbu )
{
	if( bt709 ) {
		*crv = 115; *cgu = 14; *cgv = 34; *cbu = 135;
	} else {
		*crv = 102; *cgu = 25; *cgv = 52; *cbu = 129;
	}
}

// Y の 1.164384 は <<8 した 298 を 16bit で掛けると 235*298=70030 で溢れるので
// 1 + 42/256 に分け、整数部は足すだけ、小数部だけ 16bit で掛けて >>8 する
static const int Y_FRAC = 42;
static const int Y_ROUND = 128;

//! SIMD の反復幅に乗らない末尾。全バージョン同じ結果を出すため処理をここに寄せてある
static void ConvertTail( const BYTE *py, const BYTE *puv, BYTE *pd,
	long x0, long width, bool bt709 )
{
	int crv, cgu, cgv, cbu;
	GetChromaCoef( bt709, &crv, &cgu, &cgv, &cbu );
	for( long x = x0; x < width; x++ ) {
		int yy = py[x] - 16;
		int uu = puv[x & ~1] - 128;
		int vv = puv[x | 1] - 128;
		int yc = yy + ( ( yy * Y_FRAC + Y_ROUND ) >> 8 );
		int b = yc + ( ( uu * cbu ) >> 6 );
		int g = yc - ( ( uu * cgu + vv * cgv ) >> 6 );
		int r = yc + ( ( vv * crv ) >> 6 );
		pd[x*4+0] = (BYTE)( b < 0 ? 0 : b > 255 ? 255 : b );
		pd[x*4+1] = (BYTE)( g < 0 ? 0 : g > 255 ? 255 : g );
		pd[x*4+2] = (BYTE)( r < 0 ? 0 : r > 255 ? 255 : r );
		pd[x*4+3] = 255;
	}
}

void TVPConvertNV12ToBGRAFlip_Scalar( const BYTE *srcY, const BYTE *srcUV, long yStride,
	BYTE *dst, long dstPitch, long width, long height, bool srcBottomUp, bool bt709 )
{
	const size_t absStride = (size_t)( yStride < 0 ? -yStride : yStride );
	for( long y = 0; y < height; y++ ) {
		const size_t srcRow = (size_t)( srcBottomUp ? ( height - 1 - y ) : y );
		const BYTE *py  = srcY  + srcRow * absStride;
		const BYTE *puv = srcUV + ( srcRow >> 1 ) * absStride;
		BYTE *pd = dst - (size_t)y * dstPitch;
		ConvertTail( py, puv, pd, 0, width, bt709 );
	}
}

//! Y_FRAC の定義理由参照
static __forceinline __m128i YContrib128( __m128i y16, const __m128i cyFrac, const __m128i cRound )
{
	return _mm_add_epi16( y16, _mm_srai_epi16(
		_mm_add_epi16( _mm_mullo_epi16( y16, cyFrac ), cRound ), 8 ) );
}

void TVPConvertNV12ToBGRAFlip_SSE2( const BYTE *srcY, const BYTE *srcUV, long yStride,
	BYTE *dst, long dstPitch, long width, long height, bool srcBottomUp, bool bt709 )
{
	const size_t absStride = (size_t)( yStride < 0 ? -yStride : yStride );

	int iCrv, iCgu, iCgv, iCbu;
	GetChromaCoef( bt709, &iCrv, &iCgu, &iCgv, &iCbu );
	const __m128i zero	= _mm_setzero_si128();
	const __m128i c16	= _mm_set1_epi16( 16 );
	const __m128i c128	= _mm_set1_epi16( 128 );
	const __m128i cyFrac= _mm_set1_epi16( Y_FRAC );
	const __m128i cRound= _mm_set1_epi16( Y_ROUND );
	const __m128i crv	= _mm_set1_epi16( iCrv );
	const __m128i cgu	= _mm_set1_epi16( iCgu );
	const __m128i cgv	= _mm_set1_epi16( iCgv );
	const __m128i cbu	= _mm_set1_epi16( iCbu );
	const __m128i aB	= _mm_set1_epi8( (char)0xFF );	// A チャンネル (8bit)
	const __m128i maskU	= _mm_set1_epi16( 0x00FF );

	for( long y = 0; y < height; y++ ) {
		const size_t srcRow = (size_t)( srcBottomUp ? ( height - 1 - y ) : y );
		const BYTE *py  = srcY  + srcRow * absStride;
		const BYTE *puv = srcUV + ( srcRow >> 1 ) * absStride;
		// dst は最終行を指すので y 行目は引き算で遡る
		BYTE *pd = dst - (size_t)y * dstPitch;

		long x = 0;
		for( ; x + 16 <= width; x += 16 ) {
			__m128i yv = _mm_loadu_si128( (const __m128i*)( py + x ) );
			// UV は 2 画素で 1 組 = 2 バイト。画素 x (16 の倍数) の組は puv + x にある
			__m128i uv = _mm_loadu_si128( (const __m128i*)( puv + x ) );

			// レーンの下位バイトが U・上位バイトが V
			__m128i u8 = _mm_and_si128( uv, maskU );
			__m128i v8 = _mm_srli_epi16( uv, 8 );
			// 自分との unpack で 1 組を隣接する 2 画素へ引き伸ばす
			__m128i u0 = _mm_unpacklo_epi16( u8, u8 );
			__m128i v0 = _mm_unpacklo_epi16( v8, v8 );
			__m128i u1 = _mm_unpackhi_epi16( u8, u8 );
			__m128i v1 = _mm_unpackhi_epi16( v8, v8 );

			__m128i ylo = _mm_sub_epi16( _mm_unpacklo_epi8( yv, zero ), c16 );
			__m128i yhi = _mm_sub_epi16( _mm_unpackhi_epi8( yv, zero ), c16 );

			__m128i b0, g0, r0;
			{
				__m128i yc = YContrib128( ylo, cyFrac, cRound );
				__m128i uu = _mm_sub_epi16( u0, c128 );
				__m128i vv = _mm_sub_epi16( v0, c128 );
				b0 = _mm_add_epi16( yc, _mm_srai_epi16( _mm_mullo_epi16( uu, cbu ), 6 ) );
				r0 = _mm_add_epi16( yc, _mm_srai_epi16( _mm_mullo_epi16( vv, crv ), 6 ) );
				g0 = _mm_sub_epi16( yc, _mm_srai_epi16( _mm_add_epi16(
					_mm_mullo_epi16( uu, cgu ), _mm_mullo_epi16( vv, cgv ) ), 6 ) );
			}
			__m128i b1, g1, r1;
			{
				__m128i yc = YContrib128( yhi, cyFrac, cRound );
				__m128i uu = _mm_sub_epi16( u1, c128 );
				__m128i vv = _mm_sub_epi16( v1, c128 );
				b1 = _mm_add_epi16( yc, _mm_srai_epi16( _mm_mullo_epi16( uu, cbu ), 6 ) );
				r1 = _mm_add_epi16( yc, _mm_srai_epi16( _mm_mullo_epi16( vv, crv ), 6 ) );
				g1 = _mm_sub_epi16( yc, _mm_srai_epi16( _mm_add_epi16(
					_mm_mullo_epi16( uu, cgu ), _mm_mullo_epi16( vv, cgv ) ), 6 ) );
			}

			__m128i bl = _mm_packus_epi16( b0, b1 );
			__m128i gl = _mm_packus_epi16( g0, g1 );
			__m128i rl = _mm_packus_epi16( r0, r1 );

			// B,G のバイト対を作ってから unpacklo_epi16 で R,A を挟む
			__m128i bg0 = _mm_unpacklo_epi8( bl, gl );
			__m128i bg1 = _mm_unpackhi_epi8( bl, gl );
			__m128i ra0 = _mm_unpacklo_epi8( rl, aB );	// R,A 対
			__m128i ra1 = _mm_unpackhi_epi8( rl, aB );
			_mm_storeu_si128( (__m128i*)( pd + x * 4 ),			_mm_unpacklo_epi16( bg0, ra0 ) );
			_mm_storeu_si128( (__m128i*)( pd + x * 4 + 16 ),	_mm_unpackhi_epi16( bg0, ra0 ) );
			_mm_storeu_si128( (__m128i*)( pd + (x+8) * 4 ),		_mm_unpacklo_epi16( bg1, ra1 ) );
			_mm_storeu_si128( (__m128i*)( pd + (x+8) * 4 + 16 ), _mm_unpackhi_epi16( bg1, ra1 ) );
		}

		ConvertTail( py, puv, pd, x, width, bt709 );
	}
}

static __forceinline __m256i YContrib256( __m256i y16, const __m256i cyFrac, const __m256i cRound )
{
	return _mm256_add_epi16( y16, _mm256_srai_epi16(
		_mm256_add_epi16( _mm256_mullo_epi16( y16, cyFrac ), cRound ), 8 ) );
}

void TVPConvertNV12ToBGRAFlip_AVX2( const BYTE *srcY, const BYTE *srcUV, long yStride,
	BYTE *dst, long dstPitch, long width, long height, bool srcBottomUp, bool bt709 )
{
	const size_t absStride = (size_t)( yStride < 0 ? -yStride : yStride );

	int iCrv, iCgu, iCgv, iCbu;
	GetChromaCoef( bt709, &iCrv, &iCgu, &iCgv, &iCbu );
	const __m256i zero	= _mm256_setzero_si256();
	const __m256i c16	= _mm256_set1_epi16( 16 );
	const __m256i c128	= _mm256_set1_epi16( 128 );
	const __m256i cyFrac= _mm256_set1_epi16( Y_FRAC );
	const __m256i cRound= _mm256_set1_epi16( Y_ROUND );
	const __m256i crv	= _mm256_set1_epi16( iCrv );
	const __m256i cgu	= _mm256_set1_epi16( iCgu );
	const __m256i cgv	= _mm256_set1_epi16( iCgv );
	const __m256i cbu	= _mm256_set1_epi16( iCbu );
	const __m256i aB	= _mm256_set1_epi8( (char)0xFF );	// A チャンネル (8bit)
	const __m256i maskU	= _mm256_set1_epi16( 0x00FF );

	for( long y = 0; y < height; y++ ) {
		const size_t srcRow = (size_t)( srcBottomUp ? ( height - 1 - y ) : y );
		const BYTE *py  = srcY  + srcRow * absStride;
		const BYTE *puv = srcUV + ( srcRow >> 1 ) * absStride;
		BYTE *pd = dst - (size_t)y * dstPitch;

		long x = 0;
		for( ; x + 32 <= width; x += 32 ) {
			__m256i yv = _mm256_loadu_si256( (const __m256i*)( py + x ) );
			// 32 画素ぶんの UV は 16 組 = 32 バイト (SSE2 版と同じく先頭は puv + x)
			__m256i uv = _mm256_loadu_si256( (const __m256i*)( puv + x ) );

			__m256i u8 = _mm256_and_si256( uv, maskU );
			__m256i v8 = _mm256_srli_epi16( uv, 8 );
			// unpack 系は 128bit レーン内でしか動かないが、Y 側も同じ並びなので
			// レーンをまたぐ並べ替えなしで画素順が揃う
			__m256i u0 = _mm256_unpacklo_epi16( u8, u8 );
			__m256i v0 = _mm256_unpacklo_epi16( v8, v8 );
			__m256i u1 = _mm256_unpackhi_epi16( u8, u8 );
			__m256i v1 = _mm256_unpackhi_epi16( v8, v8 );

			__m256i ylo = _mm256_sub_epi16( _mm256_unpacklo_epi8( yv, zero ), c16 );
			__m256i yhi = _mm256_sub_epi16( _mm256_unpackhi_epi8( yv, zero ), c16 );

			__m256i b0, g0, r0;
			{
				__m256i yc = YContrib256( ylo, cyFrac, cRound );
				__m256i uu = _mm256_sub_epi16( u0, c128 );
				__m256i vv = _mm256_sub_epi16( v0, c128 );
				b0 = _mm256_add_epi16( yc, _mm256_srai_epi16( _mm256_mullo_epi16( uu, cbu ), 6 ) );
				r0 = _mm256_add_epi16( yc, _mm256_srai_epi16( _mm256_mullo_epi16( vv, crv ), 6 ) );
				g0 = _mm256_sub_epi16( yc, _mm256_srai_epi16( _mm256_add_epi16(
					_mm256_mullo_epi16( uu, cgu ), _mm256_mullo_epi16( vv, cgv ) ), 6 ) );
			}
			__m256i b1, g1, r1;
			{
				__m256i yc = YContrib256( yhi, cyFrac, cRound );
				__m256i uu = _mm256_sub_epi16( u1, c128 );
				__m256i vv = _mm256_sub_epi16( v1, c128 );
				b1 = _mm256_add_epi16( yc, _mm256_srai_epi16( _mm256_mullo_epi16( uu, cbu ), 6 ) );
				r1 = _mm256_add_epi16( yc, _mm256_srai_epi16( _mm256_mullo_epi16( vv, crv ), 6 ) );
				g1 = _mm256_sub_epi16( yc, _mm256_srai_epi16( _mm256_add_epi16(
					_mm256_mullo_epi16( uu, cgu ), _mm256_mullo_epi16( vv, cgv ) ), 6 ) );
			}

			// packus もレーン内なので B0..B31 が素直に出る
			__m256i bl = _mm256_packus_epi16( b0, b1 );
			__m256i gl = _mm256_packus_epi16( g0, g1 );
			__m256i rl = _mm256_packus_epi16( r0, r1 );

			__m256i bg0 = _mm256_unpacklo_epi8( bl, gl );
			__m256i bg1 = _mm256_unpackhi_epi8( bl, gl );
			__m256i ra0 = _mm256_unpacklo_epi8( rl, aB );
			__m256i ra1 = _mm256_unpackhi_epi8( rl, aB );
			// unpack はレーン内なので下位半分が前半・上位半分が後半の画素になる。
			// permute2x128 で連続した 32 画素に組み直してから store する
			__m256i t0 = _mm256_unpacklo_epi16( bg0, ra0 );	// [P0..P3][P16..P19]
			__m256i t1 = _mm256_unpackhi_epi16( bg0, ra0 );	// [P4..P7][P20..P23]
			__m256i t2 = _mm256_unpacklo_epi16( bg1, ra1 );	// [P8..P11][P24..P27]
			__m256i t3 = _mm256_unpackhi_epi16( bg1, ra1 );	// [P12..P15][P28..P31]
			BYTE *p = pd + x * 4;
			_mm256_storeu_si256( (__m256i*)( p      ), _mm256_permute2x128_si256( t0, t1, 0x20 ) );
			_mm256_storeu_si256( (__m256i*)( p + 32 ), _mm256_permute2x128_si256( t2, t3, 0x20 ) );
			_mm256_storeu_si256( (__m256i*)( p + 64 ), _mm256_permute2x128_si256( t0, t1, 0x31 ) );
			_mm256_storeu_si256( (__m256i*)( p + 96 ), _mm256_permute2x128_si256( t2, t3, 0x31 ) );
		}

		ConvertTail( py, puv, pd, x, width, bt709 );
	}
}

void TVPConvertNV12ToBGRAFlip( const BYTE *srcY, const BYTE *srcUV, long yStride,
	BYTE *dst, long dstPitch, long width, long height, bool srcBottomUp, bool bt709 )
{
	if( TVPMFColorHasAVX2() ) {
		TVPConvertNV12ToBGRAFlip_AVX2( srcY, srcUV, yStride, dst, dstPitch,
			width, height, srcBottomUp, bt709 );
		return;
	}
	if( TVPMFColorHasSSE2() ) {
		TVPConvertNV12ToBGRAFlip_SSE2( srcY, srcUV, yStride, dst, dstPitch,
			width, height, srcBottomUp, bt709 );
		return;
	}
	TVPConvertNV12ToBGRAFlip_Scalar( srcY, srcUV, yStride, dst, dstPitch,
		width, height, srcBottomUp, bt709 );
}
