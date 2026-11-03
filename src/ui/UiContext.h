#pragma once

#include "ui/IconProvider.h"
#include "ui/Theme.h"

namespace cb {

/// Shared, read-mostly UI services passed to every ClassBoard widget (no global singletons).
struct UiContext
{
    Theme theme;
    IconProvider icons;
};

} // namespace cb
