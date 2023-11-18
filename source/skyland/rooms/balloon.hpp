////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2023  Evan Bowman
//
// This program is free software; you can redistribute it and/or modify it under
// the terms of version 2 of the GNU General Public License as published by the
// Free Software Foundation.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc., 51
// Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
// GPL2 ONLY. No later versions permitted.
//
////////////////////////////////////////////////////////////////////////////////


#pragma once

#include "skyland/room.hpp"
#include "skyland/systemString.hpp"
#include "skyland/tile.hpp"



namespace skyland
{



class Balloon : public Room
{
public:
    Balloon(Island* parent, const RoomCoord& position)
        : Room(parent, name(), position)
    {
    }


    void plot_walkable_zones(App& app,
                             bool matrix[16][16],
                             BasicCharacter* for_character) override
    {
        matrix[position().x + 1][position().y + 4] = true;
    }


    void render_scaffolding(App& app, TileId buffer[16][16]) override
    {
    }


    void render_interior(App* app, TileId buffer[16][16]) override
    {
        auto pos = position();
        buffer[pos.x][pos.y] = Tile::balloon_1;
        buffer[pos.x + 1][pos.y] = Tile::balloon_2;
        buffer[pos.x + 2][pos.y] = Tile::balloon_3;
        buffer[pos.x][pos.y + 1] = Tile::balloon_4;
        buffer[pos.x + 1][pos.y + 1] = Tile::balloon_5;
        buffer[pos.x + 2][pos.y + 1] = Tile::balloon_6;
        buffer[pos.x][pos.y + 2] = Tile::balloon_7;
        buffer[pos.x + 1][pos.y + 2] = Tile::balloon_8;
        buffer[pos.x + 2][pos.y + 2] = Tile::balloon_9;
        buffer[pos.x + 1][pos.y + 3] = Tile::basket_1;
        buffer[pos.x][pos.y + 4] = Tile::basket_2;
        buffer[pos.x + 1][pos.y + 4] = Tile::basket_3;
        buffer[pos.x + 2][pos.y + 4] = Tile::basket_4;
    }


    void render_exterior(App* app, TileId buffer[16][16]) override
    {
        render_interior(app, buffer);
    }


    static RoomProperties::Bitmask properties()
    {
        return RoomProperties::disallow_chimney | RoomProperties::roof_hidden |
               RoomProperties::fragile |
               RoomProperties::multiplayer_unsupported |
               RoomProperties::habitable | RoomProperties::not_constructible;
    }


    bool description_visible() override
    {
        return true;
    }


    static Float atp_value()
    {
        return 1.f;
    }


    static const char* name()
    {
        return "balloon";
    }


    static SystemString ui_name()
    {
        return SystemString::block_balloon;
    }


    static Vec2<u8> size()
    {
        return {3, 5};
    }


    static Icon icon()
    {
        return 3960;
    }


    static Icon unsel_icon()
    {
        return 3944;
    }


    static Category category()
    {
        return Category::power;
    }


    static void format_description(StringBuffer<512>& buffer)
    {
        buffer += SYSTR(description_balloon)->c_str();
    }


    void display(Platform::Screen& screen, App& app) override
    {
        for (auto& c : characters()) {
            auto pos = c->sprite().get_position();
            if (pos.y < 700.0_fixed) {
                auto spr = c->prepare_sprite();
                pos.y -= 3.0_fixed;
                spr.set_position(pos);
                spr.set_priority(3);
                screen.draw(spr);
            }
        }
    }
};



} // namespace skyland
