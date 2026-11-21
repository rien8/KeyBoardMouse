#pragma once

class ButtonState {
public:
    static constexpr int kMaxButtons = 3;

    bool press(int button);
    bool release(int button);
    int takeAll();
    int mask() const { return m_mask; }
    bool isDown(int button) const;

private:
    int m_mask = 0;
};

inline bool ButtonState::isDown(int button) const
{
    if (button < 0 || button >= kMaxButtons)
        return false;
    return (m_mask & (1 << button)) != 0;
}

inline bool ButtonState::press(int button)
{
    if (button < 0 || button >= kMaxButtons)
        return false;
    if (isDown(button))
        return false;
    m_mask |= (1 << button);
    return true;
}

inline bool ButtonState::release(int button)
{
    if (!isDown(button))
        return false;
    m_mask &= ~(1 << button);
    return true;
}

inline int ButtonState::takeAll()
{
    const int mask = m_mask;
    m_mask = 0;
    return mask;
}
