#pragma once

#if (_MSC_VER <= 1300)  // 1300 == VC++ 7.0
#ifdef _UNICODE
#define strncpy_s(dst, dsz, src, ssz) wcsncpy(dst, src, ssz)
#else
#define strncpy_s(dst, dsz, src, ssz) strncpy(dst, src, ssz)
#endif
#define wcsncpy_s(dst, dsz, src, ssz) wcsncpy(dst, src, ssz)
#endif
