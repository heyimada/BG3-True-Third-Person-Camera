#include "Hooks/MouseLookHook.h"

#include "Hooks/Hooks.h"
#include "Input/MouseLookEvents.h"
#include "Settings/Settings.h"

namespace Hooks::MouseLook
{
    namespace
    {
        using PollEvent = int (SDLCALL*)(SDL_Event*);
        using GetGlobalMouseState = Uint32 (SDLCALL*)(int*, int*);

        PollEvent originalPoll = nullptr;
        PollEvent originalFirstPoll = nullptr;
        GetGlobalMouseState originalMouseState = nullptr;
        std::unique_ptr<dku::Hook::IATHookHandle> pollHook;
        std::unique_ptr<dku::Hook::IATHookHandle> mouseStateHook;
        std::unique_ptr<dku::Hook::IATHookHandle> firstPollHook;
        Input::MouseLookEvents events;

        // Mouse-state queries need not run on the event-pump thread.
        constexpr unsigned kOverride = 1;
        constexpr unsigned kHeld = 2;
        std::atomic<unsigned> publishedState{0};

        bool Enabled()
        {
            bool enabled;
            {
                auto* settings = Settings::Main::GetSingleton();
                ReadLocker locker(settings->Lock);
                enabled = settings->MouseLookToggle;
            }
            DWORD foregroundProcess = 0;
            GetWindowThreadProcessId(GetForegroundWindow(), &foregroundProcess);
            return enabled && foregroundProcess == GetCurrentProcessId() &&
                Offsets::isInControllerMode && !*Offsets::isInControllerMode;
        }

        int ProcessPoll(SDL_Event* event, PollEvent source)
        {
            const bool enabled = Enabled();
            const int result = events.Poll(event, enabled, source, originalPoll);
            publishedState.store((events.OverridesMiddleButton(enabled) ? kOverride : 0) |
                (events.Held() ? kHeld : 0), std::memory_order_relaxed);
            return result;
        }

        int SDLCALL HookPollEvent(SDL_Event* event)
        {
            return ProcessPoll(event, originalPoll);
        }

        int SDLCALL HookFirstPollEvent(SDL_Event* event)
        {
            return ProcessPoll(event, originalFirstPoll);
        }

        Uint32 SDLCALL HookGetGlobalMouseState(int* x, int* y)
        {
            const auto physical = originalMouseState(x, y);
            const auto state = publishedState.load(std::memory_order_relaxed);
            return state & kOverride ? (physical & ~SDL_BUTTON_MMASK) |
                (state & kHeld ? SDL_BUTTON_MMASK : 0) : physical;
        }
    }

    bool Install()
    {
        if (pollHook) {
            return true;
        }
        const auto module = dku::Hook::GetProcessName();
        const auto pollImport = dku::Hook::GetImportAddress(module, "SDL2.dll", "SDL_PollEvent");
        if (!pollImport ||
            !dku::Hook::GetImportAddress(module, "SDL2.dll", "SDL_GetGlobalMouseState")) {
            WARN("Mouse look toggle unavailable: required SDL imports were not found.");
            return false;
        }
        // WASD redirects this FF 15 call to its own pointer slot, bypassing the IAT.
        // Match surrounding instructions, allowing the redirected displacement.
        auto* firstSite = static_cast<uint8_t*>(dku::Hook::Assembly::search_pattern<
            "48 8D 4D ?? FF 15 ?? ?? ?? ?? 85 C0 0F 84 ?? ?? ?? ?? 49 8D 47 60">());
        if (!firstSite) {
            WARN("Mouse look toggle unavailable: first SDL poll call was not found.");
            return false;
        }
        int32_t displacement;
        std::memcpy(&displacement, firstSite + 6, sizeof(displacement));
        auto* firstSlot = firstSite + 10 + displacement;
        if (firstSlot != pollImport) {
            firstPollHook = std::make_unique<dku::Hook::IATHookHandle>(
                reinterpret_cast<uintptr_t>(firstSlot), reinterpret_cast<uintptr_t>(&HookFirstPollEvent),
                "SDL_PollEvent first call", "HookFirstPollEvent");
            originalFirstPoll = reinterpret_cast<PollEvent>(firstPollHook->OldAddress);
        }
        pollHook = dku::Hook::AddIATHook(module, "SDL2.dll", "SDL_PollEvent", FUNC_INFO(HookPollEvent));
        mouseStateHook = dku::Hook::AddIATHook(module, "SDL2.dll", "SDL_GetGlobalMouseState", FUNC_INFO(HookGetGlobalMouseState));
        originalPoll = reinterpret_cast<PollEvent>(pollHook->OldAddress);
        originalMouseState = reinterpret_cast<GetGlobalMouseState>(mouseStateHook->OldAddress);
        mouseStateHook->Enable();
        pollHook->Enable();
        if (firstPollHook) {
            firstPollHook->Enable();
            INFO("Mouse look: chained existing first-poll hook at {:X} (WASD-compatible).",
                reinterpret_cast<uintptr_t>(firstSite + 4) - dku::Hook::Module::get().base());
        }
        INFO("Mouse look toggle installed: first and loop polls covered.");
        return true;
    }
}
