////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2023  Evan Bowman. Some rights reserved.
//
// This program is source-available; the source code is provided for educational
// purposes. All copies of the software must be distributed along with this
// license document.
//
// 1. DEFINITION OF SOFTWARE: The term "Software" refers to SKYLAND,
// including any updates, modifications, or associated documentation provided by
// Licensor.
//
// 2. DERIVATIVE WORKS: Licensee is permitted to modify the source code.
//
// 3. COMMERCIAL USE: Commercial use is not allowed.
//
// 4. ATTRIBUTION: Licensee is required to provide attribution to Licensor.
//
// 5. INTELLECTUAL PROPERTY RIGHTS: All intellectual property rights in the
// Software shall remain the property of Licensor. The Licensee does not acquire
// any rights to the Software except for the limited use rights specified in
// this Agreement.
//
// 6. WARRANTY AND LIABILITY: The Software is provided "as is" without warranty
// of any kind. Licensor shall not be liable for any damages arising out of or
// related to the use or inability to use the Software.
//
// 7. TERMINATION: This Agreement shall terminate automatically if Licensee
// breaches any of its terms and conditions. Upon termination, Licensee must
// cease all use of the Software and destroy all copies.
//
////////////////////////////////////////////////////////////////////////////////


#pragma once

#include "number/int.h"
#include "scene.hpp"
#include "systemString.hpp"



class Platform;



namespace skyland
{



class App;



}



namespace skyland::achievements
{



enum Achievement : u8 {
    // clang-format off
    none            = 0,
    builder         = 1,
    architect       = 2,
    architect_2     = 3,
    explorer        = 4,
    strategist      = 5,
    stronghold      = 6,
    dynamite        = 7,
    maestro_1       = 8,
    maestro_2       = 9,
    triage          = 10,
    banana_man      = 11,
    edge_of_world   = 12,
    ship_of_theseus = 13,
    lemons          = 14,
    new_colossus    = 15,
    meltdown        = 16,
    completionist   = 17,
    mycelium        = 18,
    primitive       = 19,
    hero            = 20,
    // pacifist        = 23,
    count
    // clang-format on
};



// Inspects save data, and re-awards any achievements from a previous session.
void init();



// If the library matches a new achievement, return the achievement.
Achievement update();



// Raise an alert, forcibly unlocking an achievement.
void raise(Achievement achievement);



// Re-lock the achievement. Really just needed for implementing rewind.
void lock(Achievement achievement);



// update() or unlock() return an achievement, or true (respectively), call
// award to award the achievement item to the player.
void award(Achievement achievement);



bool is_unlocked(Achievement achievement);



SystemString description(Achievement achievement);



SystemString name(Achievement achievement);



const char* reward(Achievement achievement);



} // namespace skyland::achievements
