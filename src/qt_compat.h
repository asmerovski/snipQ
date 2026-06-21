#pragma once
// ── Qt6 broken-package workaround ────────────────────────────────────────────
// Some Qt6 distro packages (notably Arch/Manjaro) ship qcolor.h and
// qlayoutitem.h that reference USHRT_MAX / INT_MAX without including the
// headers that define them.  We define them ourselves here as a fallback,
// exactly as the C standard mandates, so any TU that includes this header
// before any Qt header is immune to the bug.
//
// This header must be included BEFORE any Qt header in every TU.

#include <climits>   // standard path — no-op if already included

// If the compiler/stdlib still hasn't provided these macros (broken packaging),
// define them using GCC built-ins which are always available.
#ifndef USHRT_MAX
#  define USHRT_MAX  __UINT16_MAX__
#endif
#ifndef SHRT_MAX
#  define SHRT_MAX   __INT16_MAX__
#endif
#ifndef INT_MAX
#  define INT_MAX    __INT_MAX__
#endif
#ifndef INT_MIN
#  define INT_MIN    (-__INT_MAX__ - 1)
#endif
#ifndef UINT_MAX
#  define UINT_MAX   __UINT32_MAX__
#endif
