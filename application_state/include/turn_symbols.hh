#pragma once

#include <cstdint>

enum class TurnSymbol : uint8_t
{
    kNone,
    kDestination,        // e0c8
    kStraight,           // eb95
    kTurnRight,          // ebab
    kTurnLeft,           // eba6
    kTurnSharpRight,     // ebaa
    kTurnSharpLeft,      // eba7
    kTurnSlightRight,    // eb9a
    kTurnSlightLeft,     // eba4
    kUturnRight,         // eba2
    kUturnLeft,          // eba1
    kRampRight,          // eb96
    kRampLeft,           // eb9c
    kForkRight,          // ebac
    kForkLeft,           // eba0
    kRoundaboutStraight, // eb95 (same as straight)
    kRoundaboutRight,    // eba3
    kRoundaboutLeft,     // eb99

    kValueCount,
};
