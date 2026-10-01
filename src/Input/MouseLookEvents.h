#pragma once

#include "Input/MouseLookToggleState.h"
#include <SDL_events.h>
#include <SDL_mouse.h>
#include <optional>

namespace Input
{
    // SDL queue adapter. Only the game's event-pump thread may call Poll.
    class MouseLookEvents
    {
    public:
        template<class PollOriginal>
        int Poll(SDL_Event* output, bool enabled, PollOriginal original)
        {
            return Poll(output, enabled, original, original);
        }

        template<class PollOriginal, class PollNext>
        int Poll(SDL_Event* output, bool enabled, PollOriginal original, PollNext next)
        {
            // An earlier mod may chain back through our import hook. Transform once.
            if (polling_) {
                return next(output);
            }
            struct PollGuard {
                bool& active;
                explicit PollGuard(bool& value) : active(value) { active = true; }
                ~PollGuard() { active = false; }
            } guard(polling_);
            bool first = true;
            auto poll = [&](SDL_Event* event) {
                if (first) {
                    first = false;
                    return original(event);
                }
                // WASD's first-poll hook updates its cursor once per frame.
                // Do not repeat that work when skipping consumed button releases.
                return next(event);
            };
            if (!output) {
                return pending_ || (state_.Held() && !enabled) ? 1 : poll(nullptr);
            }
            if (!enabled && state_.Held()) {
                Release(*output);
                return 1;
            }
            for (;;) {
                SDL_Event event{};
                if (pending_) {
                    event = *pending_;
                    pending_.reset();
                } else if (!poll(&event)) {
                    return 0;
                }

                if (Cancels(event)) {
                    const bool held = state_.Held();
                    if (held) {
                        pending_ = event;
                        Release(*output);
                        return 1;
                    }
                    state_.Cancel();
                }

                if ((event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) &&
                    event.button.button == SDL_BUTTON_MIDDLE && event.button.which != SDL_TOUCH_MOUSEID) {
                    const auto action = event.type == SDL_MOUSEBUTTONDOWN ? state_.Down(enabled) : state_.Up();
                    if (action == ButtonAction::Suppress) {
                        continue;
                    }
                    if (action == ButtonAction::Press || action == ButtonAction::Release) {
                        event.type = action == ButtonAction::Press ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
                        event.button.state = action == ButtonAction::Press ? SDL_PRESSED : SDL_RELEASED;
                        event.button.clicks = 1;
                        lastButton_ = event.button;
                    }
                }
                if (event.type == SDL_MOUSEMOTION && event.motion.which != SDL_TOUCH_MOUSEID) {
                    event.motion.state = Buttons(event.motion.state, enabled);
                    if (state_.Held() && event.motion.windowID == lastButton_.windowID) {
                        lastButton_.x = event.motion.x;
                        lastButton_.y = event.motion.y;
                        lastButton_.timestamp = event.motion.timestamp;
                    }
                }
                *output = event;
                return 1;
            }
        }

        Uint32 Buttons(Uint32 physical, bool enabled) const
        {
            if (!OverridesMiddleButton(enabled)) {
                return physical;
            }
            return (physical & ~SDL_BUTTON_MMASK) | (state_.Held() ? SDL_BUTTON_MMASK : 0);
        }

        bool Held() const { return state_.Held(); }
        bool OwnsClick() const { return state_.OwnsClick(); }
        bool OverridesMiddleButton(bool enabled) const
        {
            return state_.OwnsClick() || state_.Held() || (enabled && !state_.PassThroughHold());
        }

    private:
        bool Cancels(const SDL_Event& event) const
        {
            if (event.type == SDL_QUIT || event.type == SDL_APP_WILLENTERBACKGROUND) {
                return true;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                return true;
            }
            if (event.type == SDL_CONTROLLERBUTTONDOWN ||
                (event.type == SDL_CONTROLLERAXISMOTION &&
                 (event.caxis.value > 16000 || event.caxis.value < -16000))) {
                return true;
            }
            return event.type == SDL_WINDOWEVENT &&
                (lastButton_.windowID == 0 || event.window.windowID == lastButton_.windowID) &&
                (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST ||
                 event.window.event == SDL_WINDOWEVENT_MINIMIZED ||
                 event.window.event == SDL_WINDOWEVENT_CLOSE);
        }

        void Release(SDL_Event& output)
        {
            state_.Cancel();
            output = {};
            output.button = lastButton_;
            output.type = SDL_MOUSEBUTTONUP;
            output.button.state = SDL_RELEASED;
            output.button.clicks = 1;
        }

        MouseLookToggleState state_;
        SDL_MouseButtonEvent lastButton_{};
        std::optional<SDL_Event> pending_;
        bool polling_ = false;
    };
}
