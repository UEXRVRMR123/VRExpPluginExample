// Copyright Zhang Shunlin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FCompositeFont;
struct FSlateFontInfo;

/** Controls how plugin ranges are merged with an existing composite font. */
enum class ECJKFontFallbackMergePolicy : uint8
{
    /** Existing culture-independent sub-font ranges keep precedence. */
    PreserveExistingRanges,

    /** Plugin ranges replace overlapping culture-independent sub-font ranges. */
    OverrideTargetRanges
};

/** Public C++ entry points for applying the CJK fallback chain manually. */
class CJKFONTFALLBACK_API FCJKFontFallback final
{
public:
    /**
     * Adds the plugin fallback ranges to an existing composite font.
     * Must be called on the game thread.
     *
     * @return true when the composite font was changed.
     */
    static bool
    ApplyToCompositeFont(FCompositeFont &CompositeFont,
                         ECJKFontFallbackMergePolicy MergePolicy = ECJKFontFallbackMergePolicy::PreserveExistingRanges);

    /**
     * Adds fallback support to a Slate font info. UFont objects are patched in memory;
     * standalone font data is copied into an owned standalone composite font.
     * Must be called on the game thread.
     *
     * @return true when the font info or its composite font was changed.
     */
    static bool
    ApplyToFontInfo(FSlateFontInfo &FontInfo,
                    ECJKFontFallbackMergePolicy MergePolicy = ECJKFontFallbackMergePolicy::PreserveExistingRanges);

    /** Rescans loaded runtime UFont objects and refreshes Slate's font cache if needed. */
    static void RefreshLoadedFonts();
};
