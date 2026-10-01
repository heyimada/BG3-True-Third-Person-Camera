#include "Hooks/Hooks.h"
#include "Hooks/MouseLookHook.h"

namespace Hooks
{
	void Install()
	{
		Offsets::Init();
		HookManager::Hook();
		MouseLook::Install();
	}
}
