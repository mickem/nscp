#pragma once
#include <unicode_char.hpp>
#include <string>
#include <strEx.h>
#include <vector>

#ifdef WIN32
#include <error_impl_w32.hpp>
#else
#include <error_impl_unix.hpp>
#endif
