#pragma once

namespace Input
{
    enum class ButtonAction { Pass, Suppress, Press, Release };

    // Physical clicks and the button held by the game have separate lifetimes.
    class MouseLookToggleState
    {
    public:
        ButtonAction Down(bool enabled)
        {
            if (physicalDown_) {
                return ownsClick_ ? ButtonAction::Suppress : ButtonAction::Pass;
            }
            physicalDown_ = true;
            ownsClick_ = enabled;
            if (!enabled) {
                return ButtonAction::Pass;
            }
            held_ = !held_;
            return held_ ? ButtonAction::Press : ButtonAction::Release;
        }

        ButtonAction Up()
        {
            physicalDown_ = false;
            const bool suppress = ownsClick_ || held_;
            ownsClick_ = false;
            return suppress ? ButtonAction::Suppress : ButtonAction::Pass;
        }

        bool Cancel()
        {
            const bool wasHeld = held_;
            held_ = false;
            // Focus loss can discard the physical release. The next click must work.
            physicalDown_ = false;
            return wasHeld;
        }

        bool Held() const { return held_; }
        bool OwnsClick() const { return ownsClick_; }
        bool PassThroughHold() const { return physicalDown_ && !ownsClick_; }

    private:
        bool physicalDown_ = false;
        bool ownsClick_ = false;
        bool held_ = false;
    };
}
