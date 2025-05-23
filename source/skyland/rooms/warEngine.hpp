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

#include "skyland/coins.hpp"
#include "skyland/room.hpp"
#include "skyland/systemString.hpp"



namespace skyland
{



class WarEngine : public Room
{
public:
    WarEngine(Island* parent,
              const RoomCoord& position,
              const char* n = name());


    void update(Time delta) override;


    void render_interior(App* app, TileId buffer[16][16]) override;
    void render_exterior(App* app, TileId buffer[16][16]) override;


    void finalize() override;


    void plot_walkable_zones(bool matrix[16][16],
                             Character* for_character) override;


    static void format_description(StringBuffer<512>& buffer);


    static Category category()
    {
        return Category::power;
    }


    static ATP atp_value()
    {
        return 1210.0_atp;
    }


    static Vec2<u8> size()
    {
        return {3, 4};
    }


    static const char* name()
    {
        return "war-engine";
    }


    static SystemString ui_name()
    {
        return SystemString::block_war_engine;
    }


    static Icon icon()
    {
        return 2840;
    }


    static Icon unsel_icon()
    {
        return 2824;
    }


    static RoomProperties::Bitmask properties()
    {
        return RoomProperties::has_chimney |
               RoomProperties::only_constructible_in_sandbox |
               RoomProperties::habitable | RoomProperties::destroy_quietly;
    }
};



} // namespace skyland
