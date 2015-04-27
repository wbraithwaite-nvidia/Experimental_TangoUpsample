
#ifndef ODD_PLATFORM_H_INCLUDED
#define ODD_PLATFORM_H_INCLUDED

#define ODD_PLATFORM_WIN32 1
#define ODD_PLATFORM_LINUX 2
#define ODD_PLATFORM_APPLE 3
#define ODD_PLATFORM_SYMBIAN 4
#define ODD_PLATFORM_IPHONE 5
#define ODD_PLATFORM_ANDROID 6

#define ODD_COMPILER_MSVC 1
#define ODD_COMPILER_GNUC 2
#define ODD_COMPILER_BORL 3
#define ODD_COMPILER_WINSCW 4
#define ODD_COMPILER_GCCE 5

#define ODD_ENDIAN_LITTLE 1
#define ODD_ENDIAN_BIG 2

#define ODD_ARCHITECTURE_32 1
#define ODD_ARCHITECTURE_64 2

//---------------------------------------------------------------------------------------------------------------------
// get compiler:
//---------------------------------------------------------------------------------------------------------------------

#if defined( __GCCE__ )
#   define ODD_COMPILER ODD_COMPILER_GCCE
#   define ODD_COMPILER_VER _MSC_VER
//#	include <staticlibinit_gcce.h> // This is a GCCE toolchain workaround needed when compiling with GCCE
#	define ODD_COMPILER_STRING "gcce"

#elif defined( __WINSCW__ )
#   define ODD_COMPILER ODD_COMPILER_WINSCW
#   define ODD_COMPILER_VER _MSC_VER
#	define ODD_COMPILER_STRING "winscw"

#elif defined( _MSC_VER )
#   define ODD_COMPILER ODD_COMPILER_MSVC
#   define ODD_COMPILER_VER _MSC_VER
#	define ODD_COMPILER_STRING "msvc"

#elif defined( __GNUC__ )
#   define ODD_COMPILER ODD_COMPILER_GNUC
#   define ODD_COMPILER_VER (((__GNUC__)*100) + \
        (__GNUC_MINOR__*10) + \
        __GNUC_PATCHLEVEL__)
#	define ODD_COMPILER_STRING "gcc"

#elif defined( __BORLANDC__ )
#   define ODD_COMPILER ODD_COMPILER_BORL
#   define ODD_COMPILER_VER __BCPLUSPLUS__
#   define __FUNCTION__ __FUNC__
#	define ODD_COMPILER_STRING "borlandc"

#else
#   pragma error "Unsupported compiler"

#endif

//---------------------------------------------------------------------------------------------------------------------
/// get platform:
///
#ifndef ODD_PLATFORM // just in case this isn't defined in the compiler args

#	if defined( __SYMBIAN32__ )
#   	define ODD_PLATFORM ODD_PLATFORM_SYMBIAN
#	elif defined( __WIN32__ ) || defined( _WIN32 )
#   	define ODD_PLATFORM ODD_PLATFORM_WIN32
#	elif defined( __ANDROID__ ) || defined( ANDROID )
#   	define ODD_PLATFORM ODD_PLATFORM_ANDROID
#       ifndef __ANDROID__
#           define __ANDROID__
#       endif
#       ifndef ANDROID
#           define ANDROID
#       endif
#	elif defined( __APPLE_CC__)
// Device                                                     Simulator
// Both requiring OS version 3.0 or greater
#   	if __ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__ >= 30000 || __IPHONE_OS_VERSION_MIN_REQUIRED >= 30000
#   	    define ODD_PLATFORM ODD_PLATFORM_IPHONE
#	   	else
#       	define ODD_PLATFORM ODD_PLATFORM_APPLE
#   	endif
#	else
#   	define ODD_PLATFORM ODD_PLATFORM_LINUX
#	endif
#endif

#if ODD_PLATFORM == ODD_PLATFORM_LINUX
#   define ODD_PLATFORM_STRING "linux"
#elif ODD_PLATFORM == ODD_PLATFORM_APPLE
#   define ODD_PLATFORM_STRING "apple"
#elif ODD_PLATFORM == ODD_PLATFORM_IPHONE
#   define ODD_PLATFORM_STRING "iphone"
#elif ODD_PLATFORM == ODD_PLATFORM_ANDROID
#   define ODD_PLATFORM_STRING "android"
#elif ODD_PLATFORM == ODD_PLATFORM_WIN32
#   define ODD_PLATFORM_STRING "win32"
#endif

//---------------------------------------------------------------------------------------------------------------------
// get arch:
//---------------------------------------------------------------------------------------------------------------------

#if defined(__x86_64__) || defined(_M_X64) || defined(__powerpc64__) || defined(__alpha__) || defined(__ia64__) || defined(__s390__) || defined(__s390x__)
#   define ODD_ARCHITECTURE ODD_ARCHITECTURE_64
#   define ODD_ARCHITECTURE_STRING "64bit"
#else
#   define ODD_ARCHITECTURE ODD_ARCHITECTURE_32
#   define ODD_ARCHITECTURE_STRING "32bit"
#endif

//---------------------------------------------------------------------------------------------------------------------
// endianness:
//---------------------------------------------------------------------------------------------------------------------

#if ODD_PLATFORM == ODD_PLATFORM_LINUX || ODD_PLATFORM == ODD_PLATFORM_APPLE || ODD_PLATFORM == ODD_PLATFORM_WIN32 || ODD_PLATFORM == ODD_PLATFORM_ANDROID
#    define ODD_ENDIAN ODD_ENDIAN_LITTLE
#else
#    define ODD_ENDIAN ODD_ENDIAN_BIG
#endif


//---------------------------------------------------------------------------------------------------------------------
// debug:
//---------------------------------------------------------------------------------------------------------------------

#if 1 || ODD_PLATFORM == ODD_PLATFORM_WIN32
// Win32 compilers use _DEBUG for specifying debug builds.
// for MinGW, we set DEBUG
#   if defined(_DEBUG) || defined(DEBUG)
#       define ODD_DEBUG 1
#   endif
#endif

//---------------------------------------------------------------------------------------------------------------------
// unicode:
//---------------------------------------------------------------------------------------------------------------------
#if ODD_PLATFORM == ODD_PLATFORM_WIN32
// Disable unicode support on MingW for GCC 3, poorly supported in stdlibc++
// STLPORT fixes this though so allow if found
// MinGW C++ Toolkit supports unicode and sets the define __MINGW32_TOOLBOX_UNICODE__ in _mingw.h
// GCC 4 is also fine
#if defined(__MINGW32__)
# if ODD_COMPILER_VER < 400
#  if !defined(_STLPORT_VERSION)
#   include<_mingw.h>
#   if defined(__MINGW32_TOOLBOX_UNICODE__) || ODD_COMPILER_VER > 345
#    define ODD_UNICODE_SUPPORT 1
#   else
#    define ODD_UNICODE_SUPPORT 0
#   endif
#  else
#   define ODD_UNICODE_SUPPORT 1
#  endif
# else
#  define ODD_UNICODE_SUPPORT 1
# endif
#else
#  define ODD_UNICODE_SUPPORT 1
#endif

#else
#	define ODD_UNICODE_SUPPORT 1
#endif

/*
/// Null pointer.
#ifndef NULL
#define NULL 0
#endif
*/


#if ODD_COMPILER == ODD_COMPILER_GNUC

#   define ODD_NO_RETURN        __attribute__((noreturn))
#   define ODD_INLINE           inline
//#   define ODD_INLINE           __attribute__((always_inline))
#   define ODD_NO_INLINE        __attribute__((noinline))
#   define ODD_FORCE_INLINE     inline __attribute__((always_inline))
#   define ODD_ALIGN(n)         __attribute__((aligned(n)))
#   define ODD_ANNOTATE(a)      __attribute__((a))
#   define ODD_LOCATION(a)      ODD_ANNOTATE(a)

#elif ODD_COMPILER == ODD_COMPILER_MSVC

#   if ODD_COMPILER_VER >= 1200
#       define ODD_FORCE_INLINE __forceinline
#   else
#       define ODD_FORCE_INLINE __inline
#   endif
#   define ODD_NO_RETURN    __declspec(noreturn)
#   define ODD_INLINE       __inline
#   define ODD_NO_INLINE    __declspec(noinline)

#	if ODD_ARCHITECTURE == ODD_ARCHITECTURE_64
#		define ODD_ALIGN(n)     __declspec(align(n))
#	else
#		define ODD_ALIGN(n)     
#	endif
#   define ODD_ANNOTATE(a)  __declspec(a)
#   define ODD_LOCATION(a)  ODD_ANNOTATE(__##a##__)

#elif defined(__MINGW32__)

#   if !defined(ODD_FORCE_INLINE)
#       define ODD_FORCE_INLINE __inline
#   endif

#else

#   define ODD_FORCE_INLINE __inline

#endif
/*
#   ifndef __align__
#   define __align__(n)     ODD_ALIGN(n)
#   endif
*/
#if defined(__CUDACC__) || defined(__CUDABE__)

#   define ODD_CUDA_EXPORT      __host__ __device__
#   undef ODD_ALIGN
#   define ODD_ALIGN(n)         __align__(n)

#else

#   define ODD_CUDA_EXPORT

#endif

#if ODD_PLATFORM == ODD_PLATFORM_WIN32
#   ifndef _WIN32_WINNT
#       define _WIN32_WINNT 0x0501
#   endif
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN 1
#   endif
#endif

// 4018: signed/unsigned mismatch is common (and ok for signed_i < unsigned_i)
// 4244: otherwise we get problems when substracting two size_t's to an int
// 4288: VC++7 gets confused when a var is defined in a loop and then after it
// 4267: too many false positives for "conversion gives possible data loss"
// 4290: it's ok windows ignores the "throw" directive
// 4996: Yes, we're ok using "unsafe" functions like vsnprintf and getenv()
#ifdef _MSC_VER
#pragma warning(disable:4018 4244 4288 4267 4290 4996)
#endif

#ifdef _MSC_VER

#ifndef S_ISREG
#   ifdef S_IFREG
#       define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#   else
#       define S_ISREG(m) 0
#   endif
# endif
#ifndef S_ISDIR
#   ifdef S_IFDIR
#       define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#   else
#       define S_ISDIR(m) 0
#   endif
# endif
#ifndef S_ISCHR
#   ifdef S_IFCHR
#       define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
#   else
#       define S_ISCHR(m) 0
#   endif
# endif
#ifndef S_ISBLK
#   ifdef S_IFBLK
#       define S_ISBLK(m) (((m) & S_IFMT) == S_IFBLK)
#   else
#       define S_ISBLK(m) 0
#   endif
# endif
#ifndef S_ISFIFO
#   ifdef S_IFIFO
#       define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#   else
#       define S_ISFIFO(m) 0
#   endif
# endif

#endif

//---------------------------------------------------------------------------------------------------------------------
// Assertion macros...

#define ODD_USE_ASSERT 1

#ifdef ODD_USE_ASSERT
#	include <cassert>
#	define ODD_ASSERT( arg ) assert( (arg) )
#else
#	define ODD_ASSERT( arg ) ((void)0)
#endif

#ifdef ODD_DEBUG
#define ODD_USE_DEBUG_ASSERT 1
#endif

#if ODD_USE_DEBUG_ASSERT
#include <cassert>
#define ODD_DASSERT( arg ) assert( (arg) )
#define ODD_DASSERT_PTR( arg1 ) ODD_DASSERT(0!=(arg1))
#define ODD_DASSERT_PTR4( arg1 ) ODD_DASSERT((0!=(arg1)) && (0==(((unsigned int)(arg1))&3)))
#define ODD_DASSERT_PTR16( arg1 ) ODD_DASSERT((0!=(arg1)) && (0==(((unsigned int)(arg1))&15)))
#define ODD_DASSERT_FNEG( arg1 ) ODD_DASSERT(0.0f<=(arg1));
#define ODD_DASSERT_ZERO( arg1 ) ODD_DASSERT(0.0f!=(arg1));
#else
#define ODD_DASSERT( arg1 )
#define ODD_DASSERT_PTR( arg1 )
#define ODD_DASSERT_PTR4( arg1 )
#define ODD_DASSERT_PTR16( arg1 )
#define ODD_DASSERT_FNEG( arg1 )
#define ODD_DASSERT_ZERO( arg1 )
#endif

#define ODD_STATIC_CHECK(x) typedef char __static_assert__ ## __LINE__[(x)]

#define ODD_EXCEPT(num, desc, src) ODD_ASSERT(false && src) 

//---------------------------------------------------------------------------------------------------------------------
// DLL exports.

#if ODD_PLATFORM == ODD_PLATFORM_WIN32

// If we're not including this from a client build, specify that the stuff
// should get exported. Otherwise, import it.
#   if defined( ODD_STATIC_BUILD )
#       define _OddExport
#       define _OddPrivate
#   else
#   	if defined( ODD_NONCLIENT_BUILD )
#       	define _OddExport __declspec( dllexport )
#   	else
#           if defined( __MINGW32__ )
#               define _OddExport
#           else
#       	    define _OddExport __declspec( dllimport )
#           endif
#   	endif
#       define _OddPrivate
#   endif
#   define _OddApi __stdcall

#elif ODD_COMPILER == ODD_COMPILER_GNUC
// Enable GCC symbol visibility
#   if defined( ODD_BASE_GCC_VISIBILITY )
#       define _OddExport  __attribute__ ((visibility("default")))
#       define _OddPrivate __attribute__ ((visibility("hidden")))
#   else
#       define _OddExport
#       define _OddPrivate
#   endif
#   define _OddApi
#endif

//----------------------------------------------------------------------------------------------------------
#endif // ODD_PLATFORM_H_INCLUDED


