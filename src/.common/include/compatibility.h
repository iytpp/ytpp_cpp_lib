//ºÊ»› WideChar ∫Õ MultiByte
#ifdef _UNICODE

#ifndef _T
#define _T(x) L##x
#endif // _T

#ifndef _tstring
#define _tstring wstring
#endif // _tstring

#ifndef _TCHAR
#define _TCHAR wchar_t
#endif // _TCHAR

#else

#ifndef _T
#define _T(x) x
#endif

#ifndef _tstring
#define _tstring string
#endif // _tstring

#ifndef _TCHAR
#define _TCHAR char
#endif // _TCHAR

#endif // _UNICODE