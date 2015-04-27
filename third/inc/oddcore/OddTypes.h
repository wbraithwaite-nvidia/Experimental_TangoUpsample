
#ifndef ODD_TYPES_H_INCLUDED
#define ODD_TYPES_H_INCLUDED

#define ODD_USE_STL 1

#include "oddcore/OddPlatform.h"

#include <sys/stat.h> // for file access
#if ODD_USE_STL
#	include <vector>		// for Array
#	include <algorithm>		// for Pair
#	include <map>			// for Map
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

namespace odd {
/** \addtogroup Core
*  @{
*/
/** \addtogroup Util
*  @{
*/
//---------------------------------------------------------------------------------------------------------------------
// standard types:
typedef unsigned int uint32;
typedef unsigned short uint16;
typedef unsigned char uint8;
typedef int int32;
typedef short int16;
typedef char int8;

//#if ODD_ARCHITECTURE == ODD_ARCHITECTURE_64
#   if ODD_COMPILER == ODD_COMPILER_MSVC
typedef unsigned __int64 uint64;
typedef __int64 int64;
#       define ODD_MAKE_I64(x) x##I64
#   else
typedef unsigned long long uint64;
typedef long long int64;
#       define ODD_MAKE_I64(x) x
#   endif
#   define ODD_1I64 ODD_MAKE_I64(1)
//#endif

//----------------------------------------------------------------------------------------------------------
#if defined(__cplusplus) || defined(__CUDACC__)

// Indicate the compiler that the parameter is not used to suppress compier warnings.
// @hideinitializer
#define ODD_UNUSED(a) ((a)=(a))

// Disable copy constructor and assignment operator.
// @hideinitializer
#define ODD_FORBID_COPY(C) \
    private: \
    C( const C & ); \
    C &operator=( const C & );


// Disable dynamic allocation on the heap.
// See Prohibiting Heap-Based Objects in More Effective C++.
// @hideinitializer
#define ODD_FORBID_HEAPALLOC() \
	private: \
	static void *operator new(size_t size); \
	static void *operator new[](size_t size);

//----------------------------------------------------------------------------------------------------------
// Swap two values.
template<typename T> ODD_CUDA_EXPORT inline void ODD_SWAP(T& a, T& b) { T c = a; a = b; b = c; }
// Return the minimum of two values.
template<typename T> ODD_CUDA_EXPORT inline const T& ODD_MIN(const T &a, const T &b) { return ((a > b) ? b : a); }
// Return the maximum of two values.
template<typename T> ODD_CUDA_EXPORT inline const T& ODD_MAX(const T &a, const T &b) { return ((a < b) ? b : a); }
// Return true if between the inclusive range.
template<typename T> ODD_CUDA_EXPORT inline bool ODD_BETWEEN(const T &v, const T &lo, const T &hi) { return(v >= lo && v <= hi); }
// Swap byte order of @a v.
// @warning only works for standard types upto 64bit.
template<class T> ODD_CUDA_EXPORT void ODD_BYTE_REVERSE(T& v)
{
	if (sizeof(T) == 2)			{ uint16 *p = (uint16*)&v; *p = (*p >> 8) | (*p << 8); }
	else if (sizeof(T) == 4)	{ uint32 *p = (uint32*)&v; *p = (*p >> 24) | ((*p & 0x00ff0000) >> 8) | ((*p & 0x0000ff00) << 8) | (*p << 24); }
	else if (sizeof(T) == 8)	{
		uint64 *p = (uint64*)&v; *p = (*p >> 56) | ((*p << 40) & 0x00FF000000000000) | ((*p << 24) & 0x0000FF0000000000) | ((*p << 8) & 0x000000FF00000000) |
			((*p >> 8) & 0x00000000FF000000) | ((*p >> 24) & 0x0000000000FF0000) | ((*p >> 40) & 0x000000000000FF00) | (*p << 56);
	}
}
//----------------------------------------------------------------------------------------------------------
#if ODD_ENDIAN == ODD_ENDIAN_LITTLE
template<class T> bool ODD_FORCE_BIG_ENDIAN(T &v) { ODD_BYTE_REVERSE(v); return true; }
template<class T> bool ODD_FORCE_LITTLE_ENDIAN(T &v) { return false; }
#else
template<class T> bool ODD_FORCE_BIG_ENDIAN(T &v) { return false; }
template<class T> bool ODD_FORCE_LITTLE_ENDIAN(T &v) { ODD_BYTE_REVERSE(v); return true; }
#endif

#if ODD_COMPILER == ODD_COMPILER_MSVC
#	define ODD_ISNAN(x) ::_isnan(x)
#else
#	define ODD_ISNAN(x) isnan(x)
#endif

#endif // __cplusplus

ODD_CUDA_EXPORT inline uint32 ODD_NEXT_POW2(uint32 x) { x--; x |= x >> 1; x |= x >> 2; x |= x >> 4; x |= x >> 8; x |= x >> 16; x++; return x; }
ODD_CUDA_EXPORT inline uint32 ODD_LOG2(uint32 x) {
	x |= (x >> 1); x |= (x >> 2); x |= (x >> 4); x |= (x >> 8); x |= (x >> 16); x >>= 1;
	x -= (x >> 1) & 0x55555555; x = ((x >> 2) & 0x33333333) + (x & 0x33333333); x = ((x >> 4) + x) & 0x0f0f0f0f; x += (x >> 8); x += (x >> 16); return x & 63;
}

//---------------------------------------------------------------------------------------------------------------------
inline uint32 ODD_SDBM_HASH(const uint8* buffer, int32 n, uint32 h = 5381)
{
	for (int i = 0; i < n; ++i) { h = (h << 16) + (h << 6) - h + (uint32)buffer[i++]; }
	return h;
}

template <typename T> struct Hash { uint32 operator()(const T& k) { return ODD_SDBM_HASH(&k, sizeof(T)); } };
template <> struct Hash < int32 > { uint32 operator()(int32 x) const { return x; } };
template <> struct Hash < uint32 > { uint32 operator()(uint32 x) const { return x; } };

//---------------------------------------------------------------------------------------------------------------------
#ifndef ODD_MUTEX
#	define ODD_SCOPEDMUTEX(lock_)
#	define ODD_MUTEX int
#endif

//---------------------------------------------------------------------------------------------------------------------
// a scoped pointer.
template<class T> class AutoPtr
{
public:
	AutoPtr(const AutoPtr &s) : _owner(s._owner), _pointer(s.release()) { }
	AutoPtr() : _owner(false), _pointer(0) { }
	AutoPtr(T *a) : _owner(true), _pointer(a) { }
	~AutoPtr() { if (_owner) { delete _pointer; } }

	AutoPtr& operator=(T *a) { set(a); return(*this); }
	AutoPtr &operator=(const AutoPtr &);

	void set(T *a) { if (_owner) { delete _pointer; } _pointer = a; _owner = (a != 0); }

	T* pointer() const { return _pointer; }
	operator T*() const { return _pointer; }
	T& operator*() const { ODD_DASSERT(_pointer); return(*_pointer); }
	T* operator->() const { ODD_DASSERT(_pointer); return(_pointer); }

	// set pointer without ownership
	void setPointer(T *a) { setNull(); _pointer = a; }
	void setNull() { if (_owner) { delete _pointer; } _pointer = 0; _owner = false; }
	bool owner() const { return(_owner); }
	T* release() const { _owner = false; return _pointer; }

private:
	mutable bool _owner;
	T* _pointer;
};

//---------------------------------------------------------------------------------------------------------------------
template<class T> AutoPtr<T>& AutoPtr<T>::operator=(const AutoPtr<T> &s)
{
	if (&s != this)
	{
		if (!(_pointer == s._pointer))
			setNull();
		_owner = s._owner;
		_pointer = s.release();
	}
	return *this;
}

//---------------------------------------------------------------------------------------------------------------------
template<class T> class SharedPtr
{
protected:
	T* pointer_;
	unsigned int* count_;

public:
	mutable ODD_MUTEX* lock_;

	SharedPtr();
	SharedPtr(const SharedPtr& r);
	template<class OtherT> SharedPtr(OtherT* ptr);
	template<class OtherT> SharedPtr(const SharedPtr<OtherT>& r);

	SharedPtr& operator=(const SharedPtr& r);
	template<class OtherT> SharedPtr& operator=(const SharedPtr<OtherT>& r);

	virtual ~SharedPtr();

	inline operator T*() const;
	inline T& operator*() const;
	inline T* operator->() const;
	inline T* pointer() const;

	void assign(T* rep);
	inline bool isUnique() const;
	inline unsigned int count() const;
	inline unsigned int* countPointer() const;
	inline bool valid() const;
	inline void setNull(void);

protected:

	inline void release(void);
	virtual void destroy(void);

public:
	virtual void swap(SharedPtr<T> &other);
};

template<class T>
SharedPtr<T>::SharedPtr() : pointer_(0), count_(0), lock_(0) {}

template<class T>
SharedPtr<T>::SharedPtr(const SharedPtr& r) : pointer_(0), count_(0), lock_(0)
{
	if (r.lock_)
	{
		ODD_SCOPEDMUTEX(*r.lock_);
		ODD_ASSERT(!lock_);
		lock_ = r.lock_;
		pointer_ = r.pointer_;
		count_ = r.count_;
		if (count_)
			++(*count_);
	}
}

template<class T>
template< class OtherT> SharedPtr<T>::SharedPtr(OtherT* ptr) : pointer_(ptr), count_(ptr ? new unsigned int : 0), lock_(0)
{
	if (ptr)
	{
		*count_ = 1;
		lock_ = new ODD_MUTEX;
	}
}

template<class T>
template<class OtherT> SharedPtr<T>::SharedPtr(const SharedPtr<OtherT>& r) : pointer_(0), count_(0), lock_(0)
{
	if (r.lock_)
	{
		ODD_SCOPEDMUTEX(*r.lock_);
		ODD_ASSERT(!lock_);
		lock_ = r.lock_;
		pointer_ = static_cast<T*>(r.pointer());
		count_ = r.countPointer();
		if (count_)
			++(*count_);
	}
}

template<class T>
SharedPtr<T>& SharedPtr<T>::operator=(const SharedPtr& r)
{
	if (pointer_ == r.pointer_)
		return *this;
	SharedPtr<T> tmp(r);
	swap(tmp);
	return *this;
}

template<class T>
template<class OtherT> SharedPtr<T>& SharedPtr<T>::operator=(const SharedPtr<OtherT>& r)
{
	if (pointer_ == r.pointer())
		return *this;
	// swap current data into a local copy
	// this ensures we deal with rhs and this being dependent
	SharedPtr<T> tmp(r);
	swap(tmp);
	return *this;
}

template<class T>
SharedPtr<T>::~SharedPtr()
{
	release();
}

template<class T>
SharedPtr<T>::operator T*() const
{
	return pointer_;
}

template<class T>
T& SharedPtr<T>::operator*() const
{
	ODD_DASSERT(pointer_);
	return *pointer_;
}

template<class T>
T* SharedPtr<T>::operator->() const
{
	return pointer_;
}

template<class T>
T* SharedPtr<T>::pointer() const
{
	return pointer_;
}

template<class T>
void SharedPtr<T>::assign(T* rep)
{
	ODD_DASSERT(!pointer_ && !count_);
	lock_ = new ODD_MUTEX;
	ODD_SCOPEDMUTEX(*lock_);
	count_ = new unsigned int;
	pointer_ = rep;
}

template<class T>
bool SharedPtr<T>::isUnique() const
{
	ODD_SCOPEDMUTEX(*lock_);
	ODD_DASSERT(count_);
	bool rc = (*count_ == 1);
	return rc;
}

template<class T>
unsigned int SharedPtr<T>::count() const
{
	ODD_SCOPEDMUTEX(*lock_);
	ODD_DASSERT(count_);
	unsigned int rc = (*count_);
	return rc;
}

template<class T>
unsigned int* SharedPtr<T>::countPointer() const
{
	return count_;
}

template<class T>
bool SharedPtr<T>::valid() const
{
	return (pointer_ != 0);
}

template<class T>
void SharedPtr<T>::setNull(void)
{
	if (pointer_)
	{
		release();
		pointer_ = 0;
		count_ = 0;
	}
}

template<class T>
void SharedPtr<T>::release(void)
{
	bool destroyThis = false;

	if (lock_)
	{
		ODD_SCOPEDMUTEX(*lock_);
		if (count_)
		{
			if (--(*count_) == 0)
				destroyThis = true;
		}
	}
	if (destroyThis)
		destroy();

	lock_ = 0;
}

template<class T>
void SharedPtr<T>::destroy(void)
{
	delete pointer_;
	delete count_;
	delete lock_;
}

template<class T>
void SharedPtr<T>::swap(SharedPtr<T> &other)
{
	ODD_SWAP(pointer_, other.pointer_);
	ODD_SWAP(count_, other.count_);
	ODD_SWAP(lock_, other.lock_);
}

template<class T, class U>
inline bool operator==(SharedPtr<T> const& a, SharedPtr<U> const& b)
{
	return a.pointer() == b.pointer();
}

template<class T, class U>
inline bool operator!=(SharedPtr<T> const& a, SharedPtr<U> const& b)
{
	return a.pointer() != b.pointer();
}

template<class T, class U>
inline bool operator<(SharedPtr<T> const& a, SharedPtr<U> const& b)
{
	return std::less<const void*>()(a.pointer(), b.pointer());
}

//---------------------------------------------------------------------------------------------------------------------
// A tuple of the same type.
template <class Real, int N> class Tuple
{
public:
	typedef Tuple<Real, N> Type;

	// constructor: uninitialized for performance
	ODD_CUDA_EXPORT ODD_FORCE_INLINE Tuple() {}
	// copy constructor.
	template<class OtherReal, int OtherN> ODD_CUDA_EXPORT ODD_FORCE_INLINE Tuple(const Tuple<OtherReal, OtherN>& other) { assign(other); }
	// construct from another vector.
	template<class OtherReal, int OtherN> ODD_CUDA_EXPORT ODD_FORCE_INLINE Tuple(const Tuple<OtherReal, OtherN>& other, Real missingValue) { assign(other, missingValue); }
	// construct with same value on all components.
	ODD_CUDA_EXPORT ODD_FORCE_INLINE explicit Tuple(Real x) { assign(x); }
	// construct with upto four component values.
	ODD_CUDA_EXPORT ODD_FORCE_INLINE explicit Tuple(Real x, Real y, Real z = 0, Real w = 0) { assign(x, y, z, w); }
	// construct from array of known size, which must access valid memory.
	ODD_CUDA_EXPORT ODD_FORCE_INLINE static Tuple fromArray(Real *otherArray, int otherSize = N, Real missingValue = 0);

	// return the size of the tuple.
	ODD_CUDA_EXPORT ODD_FORCE_INLINE static int size() { return N; }

	// coordinate access
	ODD_CUDA_EXPORT ODD_FORCE_INLINE Real operator[] (int i) const { return v[i]; }
	ODD_CUDA_EXPORT ODD_FORCE_INLINE Real& operator[] (int i) { return v[i]; }

	// conversion
	//ODD_CUDA_EXPORT ODD_FORCE_INLINE operator const Real* () const { return v; }
	//ODD_CUDA_EXPORT ODD_FORCE_INLINE operator Real* ()  { return v; }
	ODD_CUDA_EXPORT ODD_FORCE_INLINE const Real* asPtr() const  { return v; }
	ODD_CUDA_EXPORT ODD_FORCE_INLINE Real* asPtr()  { return v; }

	// assignment
	ODD_CUDA_EXPORT ODD_FORCE_INLINE void assign(Real x);
	ODD_CUDA_EXPORT ODD_FORCE_INLINE void assign(Real x, Real y, Real z = 0, Real w = 0);
	template <class OtherReal, int OtherN> ODD_CUDA_EXPORT ODD_FORCE_INLINE void assign(const Tuple<OtherReal, OtherN>& other, Real missingValue = 0);
	ODD_CUDA_EXPORT void assignFromArray(Real* otherArray, int otherSize = N, Real missingValue = 0);

	ODD_CUDA_EXPORT ODD_FORCE_INLINE Tuple& operator= (Real otherValue) { assign(otherValue); return *this; }
	ODD_CUDA_EXPORT ODD_FORCE_INLINE Tuple& operator= (const Tuple& other) { assign(other); return *this; }

	// comparison
	ODD_CUDA_EXPORT int compare(const Tuple& other) const;
	ODD_CUDA_EXPORT ODD_FORCE_INLINE bool operator== (const Tuple& other) const;
	ODD_CUDA_EXPORT ODD_FORCE_INLINE bool operator!= (const Tuple& other) const;
	ODD_CUDA_EXPORT ODD_FORCE_INLINE bool operator<  (const Tuple& other) const { return compare(other) < 0; }
	ODD_CUDA_EXPORT ODD_FORCE_INLINE bool operator<= (const Tuple& other) const { return compare(other) <= 0; }
	ODD_CUDA_EXPORT ODD_FORCE_INLINE bool operator>  (const Tuple& other) const { return compare(other) > 0; }
	ODD_CUDA_EXPORT ODD_FORCE_INLINE bool operator>= (const Tuple& other) const { return compare(other) >= 0; }

	Real v[N];
};

//---------------------------------------------------------------------------------------------------------------------
template <class Real, int N>
ODD_CUDA_EXPORT ODD_FORCE_INLINE Tuple<Real, N> Tuple<Real, N>::fromArray(Real *otherArray, int otherSize, Real missingValue)
{
	Tuple r;
	r.assignFromArray(otherArray, otherSize, missingValue);
	return r;
}

//---------------------------------------------------------------------------------------------------------------------
template <class Real, int N>
ODD_CUDA_EXPORT ODD_FORCE_INLINE void Tuple<Real, N>::assign(Real x)
{
	for (int i = 0; i < N; i++)
		v[i] = x;
}

//---------------------------------------------------------------------------------------------------------------------
template <class Real, int N>
ODD_CUDA_EXPORT ODD_FORCE_INLINE void Tuple<Real, N>::assign(Real x, Real y, Real z, Real w)
{
	// we do it this way to prevent compiler's buffer-overrun warning!
	Real* ptr = &v[0];
	*ptr++ = x;
	if (N > 1)
		*ptr++ = y;
	if (N > 2)
		*ptr++ = z;
	if (N > 3)
		*ptr++ = w;
}

//---------------------------------------------------------------------------------------------------------------------
template <class Real, int N>
ODD_CUDA_EXPORT ODD_FORCE_INLINE void Tuple<Real, N>::assignFromArray(Real* otherArray, int otherSize, Real missingValue)
{
	if (N <= otherSize)
	{
		for (int i = 0; i < N; ++i)
			v[i] = otherArray[i];
	}
	else
	{
		int i = 0;
		for (; i < otherSize; i++)
			v[i] = otherArray[i];
		for (; i < N; i++)
			v[i] = missingValue;
	}
}

//---------------------------------------------------------------------------------------------------------------------
template <class Real, int N>
template <class OtherReal, int OtherN>
ODD_CUDA_EXPORT ODD_FORCE_INLINE void Tuple<Real, N>::assign(const Tuple<OtherReal, OtherN>& other, Real missingValue)
{
	if (N <= OtherN)
	{
		for (int i = 0; i < N; ++i)
			v[i] = other[i];
	}
	else
	{
		int i = 0;
		for (; i < OtherN; i++)
			v[i] = other[i];
		for (; i < N; i++)
			v[i] = missingValue;
	}
}

//---------------------------------------------------------------------------------------------------------------------
template <class Real, int N>
ODD_CUDA_EXPORT ODD_FORCE_INLINE bool Tuple<Real, N>::operator==(const Tuple& other) const
{
	for (int i = 0; i < N; ++i)
		if (v[i] != other[i])
			return false;
	return true;
}

//---------------------------------------------------------------------------------------------------------------------
template <class Real, int N>
ODD_CUDA_EXPORT ODD_FORCE_INLINE bool Tuple<Real, N>::operator!=(const Tuple& other) const
{
	for (int i = 0; i < N; ++i)
		if (v[i] != other[i])
			return true;
	return false;
}

//---------------------------------------------------------------------------------------------------------------------
template <class Real, int N> ODD_CUDA_EXPORT ODD_FORCE_INLINE int Tuple<Real, N>::compare(const Tuple& other) const
{
	int count = sizeof(Real)*N;
	char* buf1 = (char*)&v[0];
	char* buf2 = (char*)&other.v[0];
	while (--count && *buf1 == *buf2) { buf1++; buf2++; }
	return (*(buf1)-*(buf2));
}

//---------------------------------------------------------------------------------------------------------------------
// A class representing a memory buffer.
class Memory
{
public:
	// construct an empty memory block.
	inline Memory() : offset_(0), pointer_(0), size_(0), capacity_(0), owner_(true) { }
	// construct a memory block of a size.
	inline Memory(uint32 size) : offset_(0), pointer_(0), size_(0), capacity_(0), owner_(true) { allocate(size, 0); }
	// construct a memory block for an existing pointer.
	inline Memory(void* ptr, uint32 size, bool owner = false) : offset_(0), pointer_((uint8*)ptr), size_(size), capacity_(size_), owner_(owner) {}
	// destruct.
	inline ~Memory() { reset(); }
	// assignment:
	inline Memory(const Memory& other) : pointer_(0), size_(0), capacity_(0) { assign(other); }
	inline Memory& operator=(const Memory& other) { assign(other); return *this; }
	inline void assign(const Memory &v);
	
	// comparison:
	inline bool operator==(const Memory& s) const { return (size_ == s.size_ && pointer_ == s.pointer_); }
	// returns how many bytes are used.
	inline uint32 size() const { return size_; }
	// returns how many bytes are allocated.
	inline uint32 capacity() const { return capacity_; }
	// return true if valid.
	inline operator bool() const { return (pointer_ != 0); }

	// access:
	inline const uint8& operator[](int i) const { return (pointer_+offset_)[i]; }
	inline uint8& operator[](int i) { return (pointer_+offset_)[i]; }
	inline void* pointer() const { return (pointer_+offset_); }
	
	inline void release() const { owner_ = false; }
	int resize(uint32 newSize, uint32 alignment=0);
	bool allocate(uint32 size, uint32 alignment=0);
	void reset();
	bool copy(const Memory &source, uint32 d_offset = 0, uint32 s_offset = 0, uint32 count = (uint32)-1);
	void clear(int v = 0);
	void dump(const char* title = 0, int width = 32) const;
	void copyFromBuffer(const void *source, uint32 d_offset = 0, uint32 s_offset = 0, uint32 count = (uint32)-1);
	void copyToBuffer(void *dest, uint32 d_offset = 0, uint32 s_offset = 0, uint32 count = (uint32)-1);
	bool load(const char* filename, uint32 s_offset = 0, uint32 count = (uint32)-1);

	//template<class T, int N> inline bool dumpAs(Stream& s, const char* title = 0, uint32 count = uint32(-1), uint32 step = 1) const;
	//template<class T> inline void assign(const T &v) { MemoryStream(*this) << v; }

protected:
	mutable bool owner_;	// are we the owner of the pointer.
	uint8* pointer_;		// pointer to memory.
	uint32 size_;			// usable size of memory.
	uint32 capacity_;		// total allocation.
	uint32 offset_;
};
/*
//---------------------------------------------------------------------------------------------------------------------
template<class T, int N> bool Memory::dumpAs(Stream& s, const char* title, uint32 count, uint32 step) const
{
	if (title)
		s << title << NL;

	if (count == uint32(-1))
		count = size_ / (sizeof(T)*N);
	else if (count > size_ / (sizeof(T)*N))
		count = size_ / (sizeof(T)*N);
	if (step == 0)
		step = 1;
	for (int i = 0; i < count; i += step)
		s << i << ". " << odd::Tuple<T, N>(&((T*)pointer_)[i]) << NL;
	return true;
}
*/

//---------------------------------------------------------------------------------------------------------------------
inline void Memory::assign(const Memory &other)
{
    if (other == *this)
        return;
    
    reset();

    if (other.owner_ == false)
    {
        // if this Memory block is referencing other memory then we must reference it too.
        size_ = other.size_;
		capacity_ = other.capacity_;
        pointer_ = other.pointer_;
		offset_ = other.offset_;
		owner_ = false;
    }
    else if (other.size_ > 0)
    {
        // just clone the memory into a new block.
        allocate(other.size_, other.offset_);
        copyFromBuffer(other.pointer());
    }
}

//---------------------------------------------------------------------------------------------------------------------
inline void Memory::clear(int v)
{
    if (size_ > 0)
		::memset(pointer(), v, size_);
}

//---------------------------------------------------------------------------------------------------------------------
inline void Memory::reset()
{
    if (owner_ == true && pointer_)
        ::free(pointer_);
	pointer_ = 0;
	size_ = capacity_ = offset_ = 0;
	owner_ = true;
}

//---------------------------------------------------------------------------------------------------------------------
inline bool Memory::allocate(uint32 requiredSize, uint32 alignment)
{
    reset();
    if (requiredSize == 0)
		return false;

	uint32 allocationSize = ((requiredSize % 256 != 0) ? (requiredSize / 256 + 1) : (requiredSize / 256)) * 256;
    void* ptr = ::malloc(allocationSize);
    if (!ptr)
		return false;

	pointer_ = (uint8*)ptr;
	capacity_ = allocationSize;
    size_ = requiredSize;
	offset_ = 0;
    owner_ = true;
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
// resize functions as free if newSize is 0,
// or as allocate if current size is 0.
// Otherwise it uses realloc.
inline int Memory::resize(uint32 requiredSize, uint32 alignment)
{
    if (requiredSize == 0)
    {
        reset();
		return 0;
    }

    if (!owner_)
    {
        //System::fatalError(Stringf("Unable to grow Memory we don't own: %p", pointer_));
        return capacity_;
    }

	int allocationSize = ((requiredSize % 256 != 0) ? (requiredSize / 256 + 1) : (requiredSize / 256)) * 256;
    if (capacity_ < allocationSize)
    {
        if (pointer_)
        {
            //System::displayInfo(Stringf("Resizing Memory: %p[%d]", pointer_, allocationSize));

            pointer_ = (uint8*)::realloc(pointer_, allocationSize);
			capacity_ = allocationSize;
        }
        else
        {
            allocate(allocationSize, alignment);
        }        
    }

    if (pointer_ == 0)
	{
        //ODD_LOG_ERROR("mem") << "Unable to realloc Memory (allocation: " << allocationSize << ")" << NL;
	    return capacity_;
	}
	size_ = requiredSize;
    return capacity_;
}

//---------------------------------------------------------------------------------------------------------------------
inline bool Memory::copy(const Memory &source, uint32 d_offset, uint32 s_offset, uint32 count)
{
    if (count == uint32(-1))
        count = ODD_MIN(size_, source.size_);

    if ((d_offset+count) > size_)
    {
        //ODD_LOG_WARN("mem") << "Outside of memory range (size: "<<size_<<", n: "<<count<<", d_offset: "<<d_offset<<")" << NL;
        count = ODD_MIN(count, count-d_offset);
    }

    if ((s_offset+count) > source.size_)
    {
        //ODD_LOG_WARN("mem") << "Outside of memory range (size: "<<source.size_<<", n: "<<count<<", s_offset: "<<s_offset<<")" << NL;
        count = ODD_MIN(count, count-s_offset);
    }

    memcpy((char *)pointer_+d_offset, (char *)source.pointer_+s_offset, count);
    return true;
}
//---------------------------------------------------------------------------------------------------------------------
inline void Memory::copyFromBuffer(const void *source, uint32 d_offset, uint32 s_offset, uint32 count)
{
    if (count == uint32(-1))
        count = size_;

    if (count == 0)
        return; // nothing to do.

    if ((d_offset+count) > size_)
    {
        //ODD_LOG_WARN("mem") << "Trying to copy into outside of Memory (size: "<<size_<<", n: "<<count<<", d_offset: "<<d_offset<<")" << NL;
        count = ODD_MIN(count, count-d_offset);
    }
    memcpy((char *)pointer_+d_offset, (char *)source+s_offset, count);
}
//---------------------------------------------------------------------------------------------------------------------
inline void Memory::copyToBuffer(void *dest, uint32 d_offset, uint32 s_offset, uint32 count)
{
    if (count == uint32(-1))
        count = size_;

    if (count == 0)
        return; // nothing to do.

    if ((s_offset+count) > size_)
    {
        //ODD_LOG_WARN("mem") << "Trying to copy from outside of Memory (size: "<<size_<<", n: "<<count<<", s_offset: "<<s_offset<<")"<< NL;
        count = ODD_MIN(count, count-s_offset);
    }

    memcpy((char *)dest+d_offset, (char *)pointer_+s_offset, count);
}
//---------------------------------------------------------------------------------------------------------------------
inline void Memory::dump(const char* title, int width) const
{
    if (title)
        fprintf(stderr,"%s\n",title);

    fprintf(stderr,"%p [%d / %d] %s\n", pointer_, size_, capacity_, (owner_)?"owner":"");

    int cursorX = 0;
    int cursorY = 0;
    uint32 i;
    int lastLineCount;
    for (i=0; i<size_; ++i)
    {
        if (i!=0 && (i%width) == 0)
        {
            // print in ascii mode
            lastLineCount = cursorX;
            for (; cursorX < width; ++cursorX)
                fprintf(stderr,"   ");
            fprintf(stderr, "| ");
            for (cursorX = 0; cursorX < lastLineCount; ++cursorX)
            {
                char c = ((char *)pointer_)[cursorY*width+cursorX];//(i-1)-lastLineCount+cursorX]
                if (c < 48)
                    c = '.';
                fprintf(stderr,"%c",c);
            }

            fprintf(stderr,"\n");
            cursorX = 0;
            cursorY++;
        }
        fprintf(stderr,"%02x ",((unsigned char *)pointer_)[i]);
        cursorX++;
    }
    lastLineCount = cursorX;
    //CT(lastLineCount);

    for (; cursorX < width; ++cursorX)
        fprintf(stderr,"   ");
    fprintf(stderr, "| ");
    for (cursorX = 0; cursorX < lastLineCount; ++cursorX)
    {
        char c = ((char *)pointer_)[cursorY*width+cursorX];//(i-1)-lastLineCount+cursorX]
        if (c < 48)
            c = '.';
        fprintf(stderr,"%c",c);
    }
    fprintf(stderr,"\n");

    //Md5 md5((uint8 *)data,size);
    //fprintf(stderr,"Md5: %s\n",(const char *)String(md5));
    //fprintf(stderr,"\n");
}
//---------------------------------------------------------------------------------------------------------------------

/*
inline char* hexInt8ToAscii(uint8_t n)
{
	char temp[2 + 1];
	temp[2] = '\0';
	
	char* t = temp+2;
	for (int i = 0; i < 2; ++i)
	{
		int nibble = n & 0xf;
		*t-- = (nibble > 9) ? (nibble + 'a' - 10) : (nibble + '0');
		n >>= 4;
		if (n == 0)
			break;
	}

	int l = (temp + 2) - t;
	for (; l < 2; ++l)
		*t-- = '0';
	return t+1;
}
*/

//---------------------------------------------------------------------------------------------------------------------
inline bool Memory::load(const char* filename, uint32 s_offset, uint32 count)
{
    clear();

    FILE *f = fopen(filename, "rb");

    if (f==0)
    {
        //ODD_LOG_ERROR("mem") << "Could not open file: " << filename << NL;
        return false;
    }

    uint32 len = count;

    if (len == (uint32)-1)
	{
		struct stat st;
		if (stat(filename,&st) < 0)
		{
			//ODD_LOG_ERROR("mem") << "Could not stat file: " << filename << NL;
			return false;
		}
		len = (st.st_size);
	}

    if (len == 0)
        return false;

    if (!allocate(len))
    {
        //ODD_LOG_ERROR("mem") << "Could not allocate memory for file: path=" << filename << ", length=" << len << NL;
        return false;
    }

    // Read in 4096 blocks
    unsigned char buffer[4096];
    int dstOffset = 0;

    while (!feof(f))
    {
        uint32 nRead = fread(buffer, 1, 4096, f);
        if (nRead > 0)
        {
            if (count < dstOffset+nRead)
            {
                // truncate the end...
                nRead = count - dstOffset;
                copyFromBuffer(buffer, dstOffset, 0, nRead);
                break;
            }
            else
            {
                copyFromBuffer(buffer, dstOffset, 0, nRead);
                dstOffset += nRead;
            }
        }
        else
        {
            break;
        }
    }

    fclose(f);

    return true;
}

//---------------------------------------------------------------------------------------------------------------------
#if (ODD_USE_STL)

//---------------------------------------------------------------------------------------------------------------------
// a pair of two different types.
template<class T1, class T2> class Pair
{
public:
	std::pair<T1, T2> data;
	ODD_FORCE_INLINE Pair() : data() {}
	ODD_FORCE_INLINE Pair(T1 a, T2 b) : data(a, b) {}
	ODD_FORCE_INLINE T1& first() { return data.first; }
	ODD_FORCE_INLINE T2& second() { return data.second; }
	ODD_FORCE_INLINE const T1& first() const { return data.first; }
	ODD_FORCE_INLINE const T2& second() const { return data.second; }
	ODD_FORCE_INLINE friend bool operator==(const Pair &a, const Pair &b) { return (a.data == b.data); }
	ODD_FORCE_INLINE friend bool operator<(const Pair &a, const Pair &b) { return (a.data < b.data); }
	ODD_FORCE_INLINE friend bool operator!=(const Pair &a, const Pair &b) { return (a.data != b.data); }
};

//---------------------------------------------------------------------------------------------------------------------
// An array.
template<class T> class Array
{
public:
	typedef std::vector<T> StdType;
	StdType data;

	class ConstIterator
	{
		const Array<T>* data;
	public:
		typename StdType::const_iterator stditer;

		ConstIterator(const Array<T> &m)
		{
			data = &m;
			reset();
		}

		ConstIterator& operator++()
		{
			++stditer;
			return(*this);
		}

		T value() const
		{
			return(*stditer);
		}

		operator T* () const
		{
			return(*stditer);
		}

		T operator->() const
		{
			return(*stditer);
		}

		const T& operator*() const
		{
			return (*stditer);
		}

		bool valid()
		{
			return(stditer != data->data.end());
		}

		operator bool()
		{
			return(stditer != data->data.end());
		}

		void reset()
		{
			stditer = data->data.begin();
		}
	};

	class Iterator
	{
		Array<T>* data;
	public:
		typename StdType::iterator stditer;

		Iterator(Array<T> &m)
		{
			data = &m;
			reset();
		}

		Iterator& operator++()
		{
			++stditer;
			return(*this);
		}

		T value() const
		{
			return(*stditer);
		}

		T& value()
		{
			return(*stditer);
		}

		T operator->()
		{
			return (*stditer);
		}

		const T& operator*() const
		{
			return (*stditer);
		}

		bool valid()
		{
			return(stditer != data->data.end());
		}

		void reset()
		{
			stditer = data->data.begin();
		}
	};

public:
	Array() {}
	Array(int n, const T* array);
	Array(const Array<T> &);
	virtual ~Array() {}

	Array<T> &operator=(Array<T> &);

	void clear()
	{
		data.clear();
	}

	bool empty() const
	{
		return data.empty();
	}

	void zero()
	{
		memset((uint8*)pointer(), 0, size()*sizeof(T));
	}

	int add(T t)
	{
		data.push_back(t);
		return(size() - 1);
	}

	void removeLast()
	{
		if (!data.empty())
			data.pop_back();
	}

	void insert(int i, T t)
	{
		if (i == -1)
			return;
		data.insert(data.begin() + i, t);
	}

	void remove(int i)
	{
		if (i == -1)
			return;
		data.erase(data.begin() + i);
	}

	void remove(Iterator &i)
	{
		data.erase(i.stditer);
	}

	int size() const
	{
		return data.size();
	}

	/// create n default items in the array
	void resize(int n)
	{
		data.resize(n);
	}

	T* pointer()
	{
		if (size() == 0)
			return 0;
		return(&(data)[0]);
	}

	const T* pointer() const
	{
		if (size() == 0)
			return 0;
		return(&(data)[0]);
	}

	const T &operator[](int i) const
	{
		return item(i);
	}

	T &operator[](int i)
	{
		return item(i);
	}

	const T& item(int i) const
	{
#if ODD_PLATFORM == ODD_PLATFORM_ANDROID
		return data.at(i);
#else
		try {
			return data.at(i);
		} catch (const std::exception&) {
			ODD_ASSERT(false && "Array out of bounds");
			return data.at(0);
		}
#endif
	}

	T& item(int i)
	{
#if ODD_PLATFORM == ODD_PLATFORM_ANDROID
		return data.at(i);
#else
		try {
			return data.at(i);
		} catch (const std::exception&) {
			ODD_ASSERT(false && "Array out of bounds");
			return data.at(0);
		}
#endif
	}

	T &uncheckedItem(int i)
	{
		return data.at(i);
	}

	const T& uncheckedItem(int i) const
	{
		return data.at(i);
	}

	T& firstItem()
	{
		return data.front();
	}

	const T& firstItem() const
	{
		return data.front();
	}

	T& lastItem()
	{
		return data.back();
	}

	const T& lastItem() const
	{
		return data.back();
	}

	bool contains(const T &s) const;
	int index(const T &s) const;

	static int defaultSorter(T a, T b);
	void sort(int(*sorter)(T a, T b));
	void shuffle();
};
//---------------------------------------------------------------------------------------------------------------------
template<class T> inline Array<T>::Array(int n, const T* array_)
{
	data.reserve(n);
	for (int i = 0; i < n; ++i)
		data.push_back(array_[i]);
}
//---------------------------------------------------------------------------------------------------------------------
template<class T> inline Array<T>::Array(const Array<T> &copier)
{
	data = copier.data;
}
//---------------------------------------------------------------------------------------------------------------------
template<class T> inline Array<T>& Array<T>::operator=(Array<T> &copier)
{
	data = copier.data;
	return (*this);
}
//---------------------------------------------------------------------------------------------------------------------
template<class T> int Array<T>::defaultSorter(T a, T b)
{
	return a < b;
}
//---------------------------------------------------------------------------------------------------------------------
template<class T> inline void Array<T>::sort(int(*sorter)(T a, T b))
{
	if (size() == 0)
		return;
	//if (sorter == 0)
	//    sorter = defaultSorter;
	std::sort(data.begin(), data.end(), sorter);
}
//---------------------------------------------------------------------------------------------------------------------
template<class T> bool Array<T>::contains(const T &s) const
{
	int n = size();
	for (int i = 0; i < n; ++i)
		if ((*this)[i] == s)
			return(true);
	return(false);
}
//---------------------------------------------------------------------------------------------------------------------
template<class T> int Array<T>::index(const T &s) const
{
	int n = size();
	for (int i = 0; i < n; ++i)
		if ((*this)[i] == s)
			return(i);
	return(-1);
}

//---------------------------------------------------------------------------------------------------------------------
/// Taken from (http://benpfaff.org/writings/clc/shuffle.html)
///
/// Arrange the N elements of ARRAY in random order.
/// Only effective if N is much smaller than RAND_MAX;
/// if this may not be the case, use a better random
/// number generator.
///
template<class T> void Array<T>::shuffle()
{
	int n = size();
	if (n > 1)
	{
		size_t i;
		for (i = 0; i < n - 1; i++)
		{
			size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
			T t = uncheckedItem(j);
			uncheckedItem(j) = uncheckedItem(i);
			uncheckedItem(i) = t;
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------
// Array of SharedPtr that automatically dereferences the pointers for comparisons, sorting, etc.
template<class T> class SharedArray : public Array < SharedPtr<T> >
{
	typedef Array< SharedPtr<T> > inherited;

public:
	/*
	// create n default items in the array
	void resize(int n)
	{
	if (n == 0)
	inherited::clear();
	else if (n > inherited::size())
	inherited::resize(n);
	}

	static int defaultSorter(T* a, T* b);
	void sort(int(*sorter)(T *a, T *b) = 0)
	*/
};

	/*
	//---------------------------------------------------------------------------------------------------------------------
	template<class T>
	int SharedArray<T>::defaultSorter(T* a, T* b)
	{
	return (*a) < (*b);
	}

	//---------------------------------------------------------------------------------------------------------------------
	template<class T>
	inline void SharedArray<T>::sort(int(*sorter)(T* a, T* b))
	{
	if (inherited::size() == 0)
	return;
	if (sorter == 0)
	sorter = defaultSorter;
	std::sort(inherited::data.begin(), inherited::data.end(), sorter);
	}
	*/

//---------------------------------------------------------------------------------------------------------------------
template<class K,class T>
class Map
{
public:
    typedef std::map<K,T> StdType;
    //using std::map<K,T>::contains;
    StdType data;
    //T Null;

    class ConstIterator
    {
        const Map<K,T>* map;
    public:
        typename StdType::const_iterator stditer;

        ConstIterator()
            : map(0)
        {
        }

        ConstIterator(const Map<K,T> &m)
        {
            map = &m;
            reset();
        }

        ConstIterator& operator=(const Map<K,T> &m)
        {
            map = &m;
            reset();
            return *this;
        }

        ConstIterator& operator++()
        {
            ++stditer;
            return(*this);
        }

        K key() const
        {
            return(stditer->first);
        }

        T value() const
        {
            return(stditer->second);
        }

        T operator->() const
        {
            return(stditer->second);
        }

        const T& operator*() const
        {
            return (stditer->second);
        }

        bool valid()
        {
            return(stditer != map->data.end());
        }

		operator bool()
		{
			return(stditer != map->data.end());
		}

        void reset()
        {
            stditer = map->data.begin();
        }
    };

    class Iterator
    {
        Map<K,T>* map;
    public:
        typename StdType::iterator stditer;
        
        Iterator()
            : map(0)
        {
        }
        

        Iterator(Map<K,T> &m)
        {
            map = &m;
            reset();
        }

        Iterator& operator=(Map<K,T> &m)
        {
            map = &m;
            reset();
            return *this;
        }

        Iterator& operator++()
        {
            ODD_DASSERT(map);
            ++stditer;
            return(*this);
        }

        K key() const
        {
            return(stditer->first);
        }

        T value() const
        {
            return(stditer->second);
        }

        T& value()
        {
            return(stditer->second);
        }

        /*operator T* () const
        {
            ODD_DASSERT(map);
            return(stditer->second);
        }*/

        T operator->() const
        {
            ODD_DASSERT(map);
            return(stditer->second);
        }

        T& operator*() const
        {
            ODD_DASSERT(map);
            return (stditer->second);
        }

        bool valid()
        {
            if (!map)
                return false;
            return(stditer != map->data.end());
        }

		operator bool()
		{
            if (!map)
                return false;
			return(stditer != map->data.end());
		}

        void reset()
        {
            ODD_DASSERT(map);
            stditer = map->data.begin();
        }
    };


public:
    Map()
    {
    }

    ~Map()
    {
    }

    void clear()
    {
        data.clear();
    }

    bool empty() const
    {
        return data.empty();
    }

    inline size_t size() const
    {
        return data.size();
    }

    void remove(Iterator &i)
    {
        data.erase(i.stditer);
    }

    void remove(const K& k)
    {
		typename StdType::iterator it = data.find(k);
		if (it != data.end())
			data.erase(it);
    }

    void put(const K &k, const T &t)
    {
        data[k] = t;
    }

    T get(const K &k) const
    {
        return getNoRef(k);
    }

    T getNoRef(const K &k) const
    {
        typename StdType::const_iterator it = data.find(k);
        if (it == data.end())
        {
            // this is problematic if we returned a reference
            T x;
            return x;
        }
        return it->second;
    }

    T& get(const K &k)
    {
        typename StdType::iterator it = data.find(k);
        ODD_ASSERT (it != data.end());
        return it->second;
    }
/*
    const T &get(const K &k) const
    {
        typename StdType::const_iterator it = data.find(k);
        return it->second;
    }*/

    bool containsKey(const K &k) const
    {
        if (data.find(k) == data.end())
            return false;
        return true;
    }

    friend inline int operator==(const Map& a, const Map& b)
    {
        return (a.data == b.data);
    }
};

template<class K, class T>
class SharedMap : public Map< K, SharedPtr<T> >
{
    typedef Map< K, SharedPtr<T> > inherited;
public:

    void put(const K& k, SharedPtr<T> t)
    {
        inherited::put(k, t);
    }

	/// return the item for key.
	/// return an invalid ptr if it doesn't exist.
    SharedPtr<T> get(const K& k)
    {
        return inherited::getNoRef(k);
    }

    T* getPointer(const K& k) const
    {
        return inherited::getNoRef(k).pointer();
    }
    
    friend inline int operator==(const SharedMap& a, const SharedMap& b)
    {
        /*typename std::map<K, SharedPtr<T> >::const_iterator it, it2;
        for (it=a.data.begin(); it!=a.data.end(); ++it)
        {
			it2 = b.data.find(it->first);
            if (!it2->second || !(*it2->second == *it->second))
                return 0;
        }
        return 1;
        */
        ODD_EXCEPT(Exception::ERR_NOT_IMPLEMENTED, "operator==(SharedMap,SharedMap)", "OddTypes.h");
        return 0;
    }
    /*
    void dump() const
    {
        typename StdType::const_iterator it;
        for (it=data.begin(); it!=data.end(); it++)
        {
            CT2(it->first, it->second);
        }
    }*/
};


//---------------------------------------------------------------------------------------------------------------------
// an associative object which cannot be modified during all is lifetime.
template<class K, class V>
class StaticMap
{
public:
    class Map
    {
    public:
        K key;
        V value;
    };

private:
    std::map<K, V> map;
public:
    V get(K &key)
    {
        return map.find(key)->second;
    }

    size_t size() const
    {
        return map.size();
    }

    StaticMap(const Map *a, size_t n)
    {
        for (int i=0;i<n;++i)
            map.insert(std::make_pair(a[i].key,a[i].value));
    }
};

#endif // #if (ODD_USE_STL)

	//---------------------------------------------------------------------------------------------------------------------
	/** @} */
	/** @} */
}

#endif // ODD_TYPES_H_INCLUDED
