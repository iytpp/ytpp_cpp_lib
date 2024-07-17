//ºÊ»› WideChar ∫Õ MultiByte
#ifdef _UNICODE

#ifndef _T
#define _T(x) L##x
#endif // _T

#ifndef _tstring
#define _tstring wstring
#endif // _tstring

#else

#ifndef _T
#define _T(x) x
#endif

#ifndef _tstring
#define _tstring string
#endif // _tstring

#endif // _UNICODE