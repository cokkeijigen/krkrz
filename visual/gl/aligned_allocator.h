

#ifndef __ALIGNED_ALLOCATOR_H__
#define __ALIGNED_ALLOCATOR_H__

#include <intrin.h>
#include <malloc.h>		// _aligned_malloc and _aligned_free
#include <memory>		// std::allocator

#if defined( _M_IX86 ) || defined( _M_X64 )
// STL allocator
template< class T, int TAlign=16 >
struct aligned_allocator : public std::allocator<T>
{
	using value_type = T;
	using pointer = T*;
	using const_pointer = const T*;
	using reference = T&;
	using const_reference = const T&;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;

	static const int ALIGN_SIZE = TAlign;
	template <class U> struct rebind    { typedef aligned_allocator<U,TAlign> other; };
	aligned_allocator() throw() {}
	aligned_allocator(const aligned_allocator&) throw () {}
	template <class U> aligned_allocator(const aligned_allocator<U, TAlign>&) throw() {}
	template <class U> aligned_allocator& operator=(const aligned_allocator<U, TAlign>&) throw()  {}
	// allocate
	pointer allocate(size_type c, const void* hint = 0) {
		return static_cast<pointer>( _mm_malloc( sizeof(T)*c, TAlign ) );
	}
	// deallocate
	void deallocate(pointer p, size_type n) {
		_mm_free( p );
	}
};
#endif

#endif // __ALIGNED_ALLOCATOR_H__


