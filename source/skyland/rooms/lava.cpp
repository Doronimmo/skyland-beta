#include "lava.hpp"
#include "globals.hpp"
#include "skyland/room_metatable.hpp"
#include "skyland/skyland.hpp"
#include "skyland/tile.hpp"
#include "skyland/timeStreamEvent.hpp"
#include "water.hpp"



namespace skyland
{



Lava::Lava(Island* parent, const Vec2<u8>& position, const char* name)
    : Room(parent, name, position)
{
}



void Lava::check_flood_parent(Platform& pfrm, App& app, Microseconds delta)
{
    // NOTE: we want to destroy a lava block if its parent lava block no
    // longer exists.

    has_flood_parent_ = false;

    bool flood_source_is_lava = false;

    if (auto room = parent()->get_room(flood_parent_)) {
        if (auto w = dynamic_cast<Lava*>(room)) {
            flood_source_is_lava = true;
            if (w->has_flood_parent_) {
                has_flood_parent_ = true;
            } else if (dynamic_cast<LavaSource*>(w)) {
                has_flood_parent_ = true;
            }
        }
    }

    if (not flood_source_is_lava and not has_flood_parent_) {
        decay_ += delta;

        if (decay_ > milliseconds(1000)) {
            __set_health(0);
        }
    } else {
        decay_ = 0;
    }
}



void Lava::update(Platform& pfrm, App& app, Microseconds delta)
{
    Room::update(pfrm, app, delta);

    if (parent()->is_destroyed()) {
        return;
    }

    Room::ready();

    check_flood_parent(pfrm, app, delta);

    if (has_flood_parent_) {
        flood_timer_ += delta;
    }

    damage_timer_ += delta;
    if (damage_timer_ > milliseconds(400)) {
        damage_timer_ = 0;

        auto damage = [&](u8 x, u8 y) {
            if (auto room = parent()->get_room({x, y})) {
                if (not dynamic_cast<Lava*>(room) and
                    not str_eq(room->name(), "barrier") and
                    not str_eq(room->name(), "masonry")) {
                    room->apply_damage(pfrm, app, 10);
                }
            }
        };

        const auto x = position().x;
        const auto y = position().y;

        if (x > 0) {
            damage(x - 1, y);
        }

        damage(x + 1, y);
        damage(x, y + 1);
        damage(x, y - 1);
    }

    if (flood_timer_ >= milliseconds(1000)) {
        flood_timer_ -= milliseconds(1000);

        auto flood = [&](u8 x, u8 y) {
            (*load_metaclass("lava"))
                ->create(pfrm, app, parent(), {x, y}, false);

            parent()->schedule_repaint();

            if (auto room = parent()->get_room({x, y})) {
                if (auto w = dynamic_cast<Lava*>(room)) {
                    w->set_flood_parent(position());
                }
            }

            if (parent() == &app.player_island()) {
                time_stream::event::PlayerRoomCreated p;
                p.x_ = x;
                p.y_ = y;
                app.time_stream().push(pfrm, app.level_timer(), p);
            } else {
                time_stream::event::OpponentRoomCreated p;
                p.x_ = x;
                p.y_ = y;
                app.time_stream().push(pfrm, app.level_timer(), p);
            }
        };

        auto floor = parent()->get_room({position().x, (u8)(position().y + 1)});
        if (not floor and position().y < 14) {
            flood(position().x, position().y + 1);
        } else if (not floor or
                   (floor and not((*floor->metaclass())->properties() &
                                  RoomProperties::fluid))) {
            // Floor is not a fluid, expand laterally.

            if (position().x < 15 and
                position().x < parent()->terrain().size() - 1 and
                not parent()->get_room(
                    {(u8)(position().x + 1), position().y})) {
                flood(position().x + 1, position().y);
            }

            if (position().x > 0 and
                not parent()->get_room(
                    {(u8)(position().x - 1), position().y})) {
                flood(position().x - 1, position().y);
            }
        }
    }
}



void Lava::render_interior(App& app, u8 buffer[16][16])
{
    auto above = parent()->get_room({position().x, (u8)(position().y - 1)});
    if (above and (*above->metaclass())->properties() & RoomProperties::fluid) {
        buffer[position().x][position().y] = InteriorTile::lava_column;
    } else {
        u8 x = position().x;
        auto left = parent()->get_room({(u8)(x - 1), position().y});
        auto right = parent()->get_room({(u8)(x + 1), position().y});

        if (left and not right) {
            buffer[position().x][position().y] = InteriorTile::lava_right;
        } else if (right and not left) {
            buffer[position().x][position().y] = InteriorTile::lava_left;
        } else {
            buffer[position().x][position().y] = InteriorTile::lava_top;
        }
    }
}



void Lava::render_exterior(App& app, u8 buffer[16][16])
{
    auto above = parent()->get_room({position().x, (u8)(position().y - 1)});
    if (above and (*above->metaclass())->properties() & RoomProperties::fluid) {
        buffer[position().x][position().y] = Tile::lava_column;
    } else {
        u8 x = position().x;
        auto left = parent()->get_room({(u8)(x - 1), position().y});
        auto right = parent()->get_room({(u8)(x + 1), position().y});

        if (left and not right) {
            buffer[position().x][position().y] = InteriorTile::lava_right;
        } else if (right and not left) {
            buffer[position().x][position().y] = InteriorTile::lava_left;
        } else {
            buffer[position().x][position().y] = InteriorTile::lava_top;
        }
    }
}



LavaSource::LavaSource(Island* parent, const Vec2<u8>& position)
    : Lava(parent, position, name())
{
}



void LavaSource::update(Platform& pfrm, App& app, Microseconds delta)
{
    flood_timer_ += delta;

    Lava::update(pfrm, app, delta);
}



void LavaSource::check_flood_parent(Platform& pfrm,
                                    App& app,
                                    Microseconds delta)
{
    decay_ = 0;
    has_flood_parent_ = false;
}



} // namespace skyland