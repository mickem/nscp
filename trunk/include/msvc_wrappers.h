#pragma once

#if (_MSC_VER <= 1300)  // 1300 == VC++ 7.0
#define strncpy_s(dst, dsz, src, ssz) strncpy(dst, src, ssz)
#endif
