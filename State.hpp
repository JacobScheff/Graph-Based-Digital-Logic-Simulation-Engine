#pragma once

enum class State
{
    LOW      = 0,  // Logic 0
    HIGH     = 1,  // Logic 1
    FLOATING = 2,  // High-Z / undriven
    UNDEFINED = 3  // Bus contention / unknown
};