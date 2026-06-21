#pragma once

// Workaround: Arch/Manjaro Qt6 packages ship qcolor.h and qlayoutitem.h that
// reference USHRT_MAX / INT_MAX without pulling in <climits> themselves.
// Including it here ensures every TU that includes limits.h is covered.
#include <limits.h>   // defines USHRT_MAX/INT_MAX (workaround for broken Qt6 pkg)
#include "qt_compat.h"

// ── snipQ field length limits ─────────────────────────────────────────────
// These constants are the single source of truth for name/tag length limits.
// They are enforced in:
//   • Database schema (CHECK constraint)
//   • Database insert/update helpers
//   • UI widgets (QLineEdit::setMaxLength)
//   • Import validation / truncation logic

static constexpr int NAME_MAX_LEN = 256;   // folder names, snippet names
static constexpr int TAG_MAX_LEN  = 256;   // individual tag strings
