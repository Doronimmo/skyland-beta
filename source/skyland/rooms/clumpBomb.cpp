////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2022  Evan Bowman
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


#include "clumpBomb.hpp"
#include "platform/platform.hpp"
#include "skyland/alloc_entity.hpp"
#include "skyland/entity/misc/animatedEffect.hpp"
#include "skyland/entity/projectile/missile.hpp"
#include "skyland/scene/weaponSetTargetScene.hpp"
#include "skyland/scene_pool.hpp"
#include "skyland/skyland.hpp"
#include "skyland/sound.hpp"
#include "skyland/tile.hpp"



namespace skyland
{



extern SharedVariable missile_damage;



extern Sound missile_sound;



void ClumpBomb::format_description(Platform& pfrm, StringBuffer<512>& buffer)
{
    buffer += SYSTR(description_clump_missile)->c_str();
}



static auto reload_time()
{
    return seconds(8);
}



ClumpBomb::ClumpBomb(Island* parent, const RoomCoord& position)
    : Weapon(parent, name(), position, reload_time())
{
}



void ClumpBomb::fire(Platform& pfrm, App& app)
{
    auto island = other_island(app);

    Vec2<Fixnum> target;

    auto room = island->get_room(*target_);
    if (room and not pfrm.network_peer().is_connected()) {
        // Note: if we use the center of a room as a target, we
        // have issues with multiplayer games, where a missile
        // targets a 2x2 room covered by 1x1 hull blocks for
        // example. Because the multiplayer coordinate system is
        // sort of mirrored over the y-axis, a missile aimed at
        // the border between two 1x1 blocks might hit the left
        // block in one game and the right block in another. So
        // missiles really should be constrained to columns for
        // multiplayer games. Just trying to explain the
        // network_peer().is_connected() check above.
        target = room->center();
    } else {
        auto origin = island->origin();
        origin.x += Fixnum::from_integer(target_->x * 16 + 8);
        origin.y += Fixnum::from_integer(target_->y * 16 + 8);
        target = origin;
    }

    if (not pfrm.network_peer().is_connected() and
        app.game_mode() not_eq App::GameMode::tutorial) {
        target = rng::sample<2>(target, rng::critical_state);
    }

    auto start = center();
    start.y -= 24.0_fixed;

    app.camera()->shake(6);

    auto m = app.alloc_entity<ClumpMissile>(
        pfrm, start, target, position().x, position().y, parent());

    missile_sound.play(pfrm, 3, milliseconds(400));

    if (m) {
        parent()->projectiles().push(std::move(m));
    }

    auto e = alloc_entity<AnimatedEffect>(start, 47, 49, milliseconds(100));
    if (e) {
        app.effects().push(std::move(e));
    }
}



Microseconds ClumpBomb::reload() const
{
    return reload_time();
}



void ClumpBomb::render_interior(App* app, TileId buffer[16][16])
{
    buffer[position().x][position().y] = InteriorTile::clumpbomb_1;
    buffer[position().x + 1][position().y] = InteriorTile::clumpbomb_2;
    buffer[position().x][position().y + 1] = InteriorTile::clumpbomb_3;
    buffer[position().x + 1][position().y + 1] = InteriorTile::clumpbomb_4;
}



void ClumpBomb::render_exterior(App* app, TileId buffer[16][16])
{
    buffer[position().x][position().y] = InteriorTile::clumpbomb_1;
    buffer[position().x + 1][position().y] = InteriorTile::clumpbomb_2;
    buffer[position().x][position().y + 1] = InteriorTile::clumpbomb_3;
    buffer[position().x + 1][position().y + 1] = InteriorTile::clumpbomb_4;
}



} // namespace skyland