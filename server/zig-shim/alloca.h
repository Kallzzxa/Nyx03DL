/* Shim for `zig cc` Windows cross-compiles only.
 *
 * amx.h includes <alloca.h> whenever __GNUC__ is defined, but the
 * MinGW headers bundled with zig do not provide alloca.h in the
 * include path. MinGW's malloc.h provides _alloca(), and amx.h maps
 * alloca() to _alloca() on Windows, so this shim is enough.
 *
 * Only used by the zig build (see README). MSVC and real MinGW
 * builds never see this file.
 */
#ifndef TDBRIDGE_ALLOCA_SHIM_H
#define TDBRIDGE_ALLOCA_SHIM_H
#include <malloc.h>
#endif
