// Copyright Zhang Shunlin. All Rights Reserved.

#include "CJKFontFallbackUE55Bridge.h"

#include "Fonts/CompositeFont.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Styling/CoreStyle.h"

#if ENGINE_MAJOR_VERSION != 5 || ENGINE_MINOR_VERSION != 5
#error CJKFontFallback's CoreStyle bridge is verified only for Unreal Engine 5.5. Review the bridge before enabling it on another engine version.
#endif

FCompositeFont *CJKFontFallback::UE55Bridge::GetMutableCoreStyleDefaultFont()
{
    const TSharedRef<const FCompositeFont> DefaultFont = FCoreStyle::GetDefaultFont();
    return const_cast<FCompositeFont *>(&DefaultFont.Get());
}
