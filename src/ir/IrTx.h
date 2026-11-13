#pragma once

// Minimal outbound IR for Watson: one NEC action, no receiver or learning.
// begin() must run before startNightMareESP() binds the ManagedAction.
namespace watson
{
    namespace ir
    {
        bool begin();
    }
}
