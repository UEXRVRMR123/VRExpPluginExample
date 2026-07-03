#pragma once

#include "Modules/ModuleManager.h"

class FVRExpansionExtensionsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
