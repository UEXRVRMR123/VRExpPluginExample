// Copyright Zhang Shunlin. All Rights Reserved.

#pragma once

struct FCompositeFont;

namespace CJKFontFallback::UE55Bridge
{
    /** Returns the mutable UE 5.5 CoreStyle default composite font. */
    FCompositeFont *GetMutableCoreStyleDefaultFont();
} // namespace CJKFontFallback::UE55Bridge
