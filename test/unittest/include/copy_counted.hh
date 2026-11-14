#pragma once

// An application state value which counts its copies, to test that values aren't copied needlessly
struct CopyCounted
{
    CopyCounted() = default;

    explicit CopyCounted(int a_value)
        : value(a_value)
    {
    }

    CopyCounted(const CopyCounted& other)
        : value(other.value)
    {
        copies++;
    }

    CopyCounted& operator=(const CopyCounted& other)
    {
        value = other.value;
        copies++;

        return *this;
    }

    CopyCounted(CopyCounted&&) = default;
    CopyCounted& operator=(CopyCounted&&) = default;

    bool operator==(const CopyCounted& other) const
    {
        return value == other.value;
    }

    int value {0};

    // The number of copies made of any CopyCounted
    static inline unsigned copies {0};
};
