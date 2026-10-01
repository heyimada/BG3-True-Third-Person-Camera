#include "Input/MouseLookEvents.h"
#include <deque>
#include <iostream>
#include <stdexcept>

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition) {
            throw std::runtime_error(message);
        }
    }

    SDL_Event Button(Uint32 type, Uint8 button = SDL_BUTTON_MIDDLE)
    {
        SDL_Event event{};
        event.type = type;
        event.button.windowID = 7;
        event.button.which = 0;
        event.button.button = button;
        event.button.state = type == SDL_MOUSEBUTTONDOWN ? SDL_PRESSED : SDL_RELEASED;
        event.button.clicks = 2;
        event.button.x = 123;
        event.button.y = 456;
        return event;
    }

    SDL_Event Motion(Uint32 buttons)
    {
        SDL_Event event{};
        event.type = SDL_MOUSEMOTION;
        event.motion.windowID = 7;
        event.motion.state = buttons;
        event.motion.x = 321;
        event.motion.y = 654;
        event.motion.xrel = 10;
        event.motion.yrel = -8;
        return event;
    }

    struct Queue
    {
        std::deque<SDL_Event> input;
        int consumed = 0;
        int Poll(SDL_Event* event)
        {
            if (input.empty()) { return 0; }
            if (event) {
                *event = input.front();
                input.pop_front();
                ++consumed;
            }
            return 1;
        }
    };

    struct Session
    {
        Input::MouseLookEvents look;
        Queue queue;
        bool enabled = true;
        int Poll(SDL_Event* event)
        {
            return look.Poll(event, enabled, [this](SDL_Event* out) { return queue.Poll(out); });
        }
        SDL_Event Next()
        {
            SDL_Event event{};
            Check(Poll(&event) == 1, "expected an event");
            return event;
        }
        void Start()
        {
            queue.input.push_back(Button(SDL_MOUSEBUTTONDOWN));
            Check(Next().type == SDL_MOUSEBUTTONDOWN && look.Held(), "first click starts mouse look");
        }
    };

    void TwoClicksAndMotion()
    {
        Session s;
        s.Start();
        s.queue.input = {Button(SDL_MOUSEBUTTONUP), Motion(SDL_BUTTON_LMASK),
            Button(SDL_MOUSEBUTTONDOWN), Motion(SDL_BUTTON_MMASK | SDL_BUTTON_RMASK),
            Button(SDL_MOUSEBUTTONUP)};
        const auto motion = s.Next();
        Check(motion.type == SDL_MOUSEMOTION, "physical release must not reach game");
        Check(motion.motion.state == (SDL_BUTTON_LMASK | SDL_BUTTON_MMASK), "motion must report held middle and preserve left");
        Check(motion.motion.xrel == 10 && motion.motion.yrel == -8, "preserve movement deltas");
        Check(s.look.Buttons(SDL_BUTTON_RMASK, true) == (SDL_BUTTON_RMASK | SDL_BUTTON_MMASK), "polled mask agrees with motion");
        const auto release = s.Next();
        Check(release.type == SDL_MOUSEBUTTONUP && release.button.state == SDL_RELEASED && release.button.clicks == 1, "second click releases, not double clicks");
        Check(!s.look.Held(), "second click restores cursor");
        Check(s.Next().motion.state == SDL_BUTTON_RMASK, "physical hold after second click cannot reactivate look");
        SDL_Event event{};
        Check(s.Poll(&event) == 0, "second physical release is swallowed");
    }

    void CancelBeforeTrigger(SDL_Event trigger)
    {
        Session s;
        s.Start();
        s.queue.input.push_back(trigger);
        const auto release = s.Next();
        Check(release.type == SDL_MOUSEBUTTONUP && release.button.windowID == 7, "emit release for original window before cancel event");
        Check(release.button.x == 123 && release.button.y == 456, "preserve coordinates on synthetic release");
        Check(!s.look.Held(), "cancel releases latch");
        const auto consumed = s.queue.consumed;
        Check(s.Poll(nullptr) == 1 && s.Poll(nullptr) == 1, "pending event is visible to peek");
        Check(s.queue.consumed == consumed, "peeking pending event cannot consume input");
        Check(s.Next().type == trigger.type, "cancel trigger is preserved");
        SDL_Event event{};
        Check(s.Poll(&event) == 0 && !s.look.Held(), "no repeated release or auto resume");
        // Focus loss may discard MMB up. A fresh click must still engage.
        s.Start();
    }

    void DisableDuringClick()
    {
        Session s;
        s.Start();
        s.enabled = false;
        Check(s.Poll(nullptr) == 1 && s.look.Held(), "peek does not apply pending cancellation");
        Check(s.Next().type == SDL_MOUSEBUTTONUP, "disabling releases with an empty queue");
        Check(s.look.Buttons(SDL_BUTTON_MMASK | SDL_BUTTON_LMASK, false) == SDL_BUTTON_LMASK, "mask consumed physical click until release");
        s.queue.input.push_back(Button(SDL_MOUSEBUTTONUP));
        SDL_Event event{};
        Check(s.Poll(&event) == 0, "consume release from cancelled click");
        s.queue.input = {Button(SDL_MOUSEBUTTONDOWN), Button(SDL_MOUSEBUTTONUP)};
        Check(s.Next().type == SDL_MOUSEBUTTONDOWN, "disabled down passes");
        Check(s.look.Buttons(SDL_BUTTON_MMASK, false) == SDL_BUTTON_MMASK, "disabled polling passes through");
        Check(s.Next().type == SDL_MOUSEBUTTONUP, "disabled up passes");
    }

    void DuplicateEventsAndPeek()
    {
        Session s;
        s.queue.input.push_back(Button(SDL_MOUSEBUTTONDOWN));
        Check(s.Poll(nullptr) == 1 && !s.look.Held() && s.queue.consumed == 0, "native queue peek cannot toggle");
        s.Next();
        s.queue.input = {Button(SDL_MOUSEBUTTONDOWN), Button(SDL_MOUSEBUTTONUP), Button(SDL_MOUSEBUTTONUP)};
        SDL_Event event{};
        Check(s.Poll(&event) == 0 && s.look.Held(), "duplicate downs and ups cannot undo latch");
        s.queue.input.push_back(Button(SDL_MOUSEBUTTONDOWN));
        Check(s.Next().type == SDL_MOUSEBUTTONUP, "next actual click still releases");
    }

    void EnableDuringVanillaHold()
    {
        Session s;
        s.enabled = false;
        s.queue.input.push_back(Button(SDL_MOUSEBUTTONDOWN));
        Check(s.Next().type == SDL_MOUSEBUTTONDOWN && !s.look.Held(), "vanilla press passes before enable");
        s.enabled = true;
        s.queue.input = {Motion(SDL_BUTTON_MMASK), Button(SDL_MOUSEBUTTONUP)};
        Check(s.Next().motion.state == SDL_BUTTON_MMASK, "enabling must not release an existing vanilla hold");
        Check(!s.look.OverridesMiddleButton(true), "global polling must preserve existing vanilla hold");
        Check(s.Next().type == SDL_MOUSEBUTTONUP, "existing vanilla hold gets its matching release");
        s.Start();
    }

    void OtherInputPasses()
    {
        Session s;
        auto touch = Button(SDL_MOUSEBUTTONDOWN);
        touch.button.which = SDL_TOUCH_MOUSEID;
        s.queue.input = {Button(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT), touch};
        Check(s.Next().button.button == SDL_BUTTON_LEFT, "left click preserved");
        Check(s.Next().button.which == SDL_TOUCH_MOUSEID && !s.look.Held(), "touch is not a physical middle click");
        s.Start();
        SDL_Event smallAxis{};
        smallAxis.type = SDL_CONTROLLERAXISMOTION;
        smallAxis.caxis.value = 100;
        s.queue.input.push_back(smallAxis);
        Check(s.Next().type == SDL_CONTROLLERAXISMOTION && s.look.Held(), "controller drift cannot cancel");
        SDL_Event otherWindow{};
        otherWindow.type = SDL_WINDOWEVENT;
        otherWindow.window.windowID = 9;
        otherWindow.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
        s.queue.input.push_back(otherWindow);
        Check(s.Next().type == SDL_WINDOWEVENT && s.look.Held(), "other window focus changes cannot cancel");
    }

    void QuickClickInOneEventBatch()
    {
        Session s;
        SDL_Event event{};
        for (int click = 0; click < 6; ++click) {
            auto down = Button(SDL_MOUSEBUTTONDOWN);
            auto up = Button(SDL_MOUSEBUTTONUP);
            down.button.timestamp = 1000 + click * 100;
            up.button.timestamp = down.button.timestamp + 5;
            s.queue.input = {down, up};
            const bool shouldHold = click % 2 == 0;
            Check(s.Poll(&event) == 1, "quick click must produce a logical edge");
            Check(event.type == static_cast<Uint32>(shouldHold ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP), "quick click edge must alternate");
            Check(s.Poll(&event) == 0, "quick physical release must not reach the game");
            Check(s.look.Held() == shouldHold, "five-millisecond click must survive draining the queue");
            Check(s.look.Buttons(0, true) == (shouldHold ? Uint32(SDL_BUTTON_MMASK) : 0u), "polling must retain the quick click result");
        }
    }

    void FirstAndLoopPollsShareState(bool firstHookCallsImport)
    {
        Session s;
        int cursorUpdates = 0;
        auto wasdPoll = [&](SDL_Event* event) {
            ++cursorUpdates;
            // WASD normally saves SDL directly. Also support a chain through our IAT.
            return firstHookCallsImport ? s.Poll(event) : s.queue.Poll(event);
        };
        auto rawPoll = [&](SDL_Event* event) { return s.queue.Poll(event); };
        SDL_Event event{};
        for (int click = 0; click < 6; ++click) {
            s.queue.input = {Button(SDL_MOUSEBUTTONDOWN), Button(SDL_MOUSEBUTTONUP)};
            Check(s.look.Poll(&event, true, wasdPoll, rawPoll) == 1, "first poll must deliver the click");
            Check(s.Poll(&event) == 0, "loop poll must consume the matching physical release");
            Check(s.look.Held() == (click % 2 == 0), "first-poll clicks must toggle exactly once");
            Check(s.queue.consumed == (click + 1) * 2, "chained hooks cannot consume events twice");
        }
        Check(cursorUpdates == 6, "retain one WASD cursor update per first poll");
    }

    void SkippingReleaseDoesNotRepeatFirstPollHook()
    {
        Session s;
        s.Start();
        s.queue.input = {Button(SDL_MOUSEBUTTONUP), Motion(0)};
        int cursorUpdates = 0;
        SDL_Event event{};
        Check(s.look.Poll(&event, true, [&](SDL_Event* output) {
            ++cursorUpdates;
            return s.queue.Poll(output);
        }, [&](SDL_Event* output) { return s.queue.Poll(output); }) == 1, "motion remains available after consumed release");
        Check(event.type == SDL_MOUSEMOTION && s.look.Held(), "consumed release cannot unlatch look");
        Check(cursorUpdates == 1, "skipping release must not rerun WASD cursor setup");
    }
}

int main()
{
    try {
        TwoClicksAndMotion();
        DisableDuringClick();
        DuplicateEventsAndPeek();
        EnableDuringVanillaHold();
        OtherInputPasses();
        QuickClickInOneEventBatch();
        FirstAndLoopPollsShareState(false);
        FirstAndLoopPollsShareState(true);
        SkippingReleaseDoesNotRepeatFirstPollHook();
        SDL_Event escape{};
        escape.type = SDL_KEYDOWN;
        escape.key.keysym.sym = SDLK_ESCAPE;
        CancelBeforeTrigger(escape);
        for (const auto reason : {SDL_WINDOWEVENT_FOCUS_LOST, SDL_WINDOWEVENT_MINIMIZED, SDL_WINDOWEVENT_CLOSE}) {
            SDL_Event focus{};
            focus.type = SDL_WINDOWEVENT;
            focus.window.windowID = 7;
            focus.window.event = static_cast<Uint8>(reason);
            CancelBeforeTrigger(focus);
        }
        SDL_Event controller{};
        controller.type = SDL_CONTROLLERBUTTONDOWN;
        CancelBeforeTrigger(controller);
        controller.type = SDL_CONTROLLERAXISMOTION;
        controller.caxis.value = -20000;
        CancelBeforeTrigger(controller);
        SDL_Event quit{};
        quit.type = SDL_QUIT;
        CancelBeforeTrigger(quit);
        std::cout << "Mouse look event tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
