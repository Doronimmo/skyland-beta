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


#include "projectile.hpp"


#include "incineratorBolt.hpp"
#include "number/fixnum.hpp"
#include "skyland/entity/drones/drone.hpp"
#include "skyland/entity/explosion/exploSpawner.hpp"
#include "skyland/entity/explosion/explosion.hpp"
#include "skyland/entity/misc/smokePuff.hpp"
#include "skyland/room.hpp"
#include "skyland/room_metatable.hpp"
#include "skyland/rooms/cannon.hpp"
#include "skyland/rooms/forcefield.hpp"
#include "skyland/sharedVariable.hpp"
#include "skyland/sound.hpp"
#include "skyland/timeStreamEvent.hpp"



namespace skyland
{



// SHARED_VARIABLE(cannonball_damage);
extern Sound cannon_sound;



IncineratorBolt::IncineratorBolt(const Vec2<Fixnum>& position,
                                 const Vec2<Fixnum>& target,
                                 Island* source,
                                 const RoomCoord& origin_tile)
    : Projectile({{10, 10}, {8, 8}}), source_(source), origin_tile_(origin_tile)
{
    sprite_.set_position(position);
    sprite_.set_size(Sprite::Size::w16_h16);
    sprite_.set_texture_index(78 * 2);

    sprite_.set_origin({8, 8});

    static const Float speed = 0.00030f;

    auto step = direction(fvec(position), fvec(target)) * speed;
    step_vector_ = Vec2<Fixnum>{Fixnum(step.x), Fixnum(step.y)};
}



void IncineratorBolt::update(Platform& pfrm, App& app, Microseconds delta)
{
    auto pos = sprite_.get_position();
    pos = pos + app.delta_fp() * step_vector_;
    sprite_.set_position(pos);

    timer_ += delta;


    Island* target;
    if (source_ == &app.player_island()) {
        target = app.opponent_island();
    } else {
        target = &app.player_island();
    }

    if (target) {
        destroy_out_of_bounds(pfrm, app, target);
    }

    if (timer_ > seconds(2)) {
        kill();
    }
}



void IncineratorBolt::rewind(Platform& pfrm, App& app, Microseconds delta)
{
    auto pos = sprite_.get_position();
    pos = pos - app.delta_fp() * step_vector_;
    sprite_.set_position(pos);

    timer_ -= delta;

    if (timer_ < 0) {
        if (auto room = source_->get_room(origin_tile_)) {
            room->___rewind___ability_used(pfrm, app);
        } else if (auto drone = source_->get_drone(origin_tile_)) {
            (*drone)->___rewind___ability_used(pfrm, app);
        }
        kill();
    }
}



extern Sound sound_impact;



void IncineratorBolt::on_collision(Platform& pfrm, App& app, Room& room)
{
    if (source_ == room.parent()) {
        if (room.position().x + (room.size().x - 1) == origin_tile_.x) {
            // Because we do not want to include collisions with the originating
            // cannon, or with any blocks directly above or below the cannon.
            return;
        }
        if (auto origin = source_->get_room(origin_tile_)) {
            if (origin == &room) {
                return;
            }
        }
    }

    if (source_ == room.parent() and is_forcefield(room.metaclass())) {
        return;
    }

    auto coord = room.position();

    room.apply_damage(pfrm, app, 8, source_);
    room.parent()->fire_create(pfrm, app, coord);

    auto damage = [&](int xo, int yo) {
        Vec2<u8> c{u8(coord.x + xo), u8(coord.y + yo)};
        if (auto r = room.parent()->get_room(c)) {
            r->apply_damage(pfrm, app, 2, source_);
            room.parent()->fire_create(pfrm, app, c);
        }
    };

    damage(-2, 0);
    damage(-2, 0);
    damage(-1, 0);
    damage(1, 0);
    damage(2, 0);
    damage(3, 0);
    damage(0, -1);
    damage(0, -2);
    damage(0, -3);
    damage(0, 1);
    damage(0, 2);
    damage(0, 3);
    damage(-1, -1);
    damage(-1, 1);
    damage(1, -1);
    damage(1, 1);

    auto flak_smoke = [](Platform& pfrm, App& app, const Vec2<Fixnum>& pos) {
        auto e = app.alloc_entity<SmokePuff>(
            pfrm, rng::sample<32>(pos, rng::utility_state), 61);

        if (e) {
            app.effects().push(std::move(e));
        }
    };

    flak_smoke(pfrm, app, sprite_.get_position());
    flak_smoke(pfrm, app, sprite_.get_position());


    if (str_eq(room.name(), "mirror-hull")) {
        room.set_ai_aware(pfrm, app, true);
        record_destroyed(pfrm, app);
        step_vector_.x *= Fixnum::from_integer(-1);
        step_vector_.y *= Fixnum::from_integer(-1);
        source_ = room.parent();
        origin_tile_ = room.position();
        timer_ = 0;
        pfrm.speaker().play_sound("cling", 2);
    } else {
        this->destroy(pfrm, app, true);
        if (room.health()) {
            sound_impact.play(pfrm, 1);
        }
    }
}



void IncineratorBolt::record_destroyed(Platform& pfrm, App& app)
{
    auto timestream_record =
        [&](time_stream::event::BasicProjectileDestroyed& c) {
            c.x_origin_ = origin_tile_.x;
            c.y_origin_ = origin_tile_.y;
            c.timer_.set(timer_);
            c.x_pos_.set(sprite_.get_position().x.as_integer());
            c.y_pos_.set(sprite_.get_position().y.as_integer());
            c.x_speed__data_.set(step_vector_.x.data());
            c.y_speed__data_.set(step_vector_.y.data());
        };


    if (source_ == &app.player_island()) {
        time_stream::event::PlayerIncineratorboltDestroyed c;
        timestream_record(c);
        app.time_stream().push(app.level_timer(), c);
    } else {
        time_stream::event::OpponentIncineratorboltDestroyed c;
        timestream_record(c);
        app.time_stream().push(app.level_timer(), c);
    }
}



void IncineratorBolt::destroy(Platform& pfrm, App& app, bool explosion)
{
    record_destroyed(pfrm, app);

    kill();
    app.camera()->shake(14);

    if (explosion) {
        big_explosion(pfrm, app, sprite_.get_position());
        ExploSpawner::create(pfrm, app, sprite_.get_position());
    }
}



void IncineratorBolt::on_collision(Platform& pfrm, App& app, Entity& entity)
{
    if (auto drone = entity.cast_drone()) {
        if (drone->position() == origin_tile_ and drone->parent() == source_) {
            // Do not shoot ourself.
            return;
        }
    }


    this->destroy(pfrm, app, true);

    entity.apply_damage(pfrm, app, 3);
}



} // namespace skyland