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


#include "island.hpp"
#include "alloc_entity.hpp"
#include "entity/explosion/explosion.hpp"
#include "entity/misc/smokePuff.hpp"
#include "entity/projectile/projectile.hpp"
#include "globals.hpp"
#include "latency.hpp"
#include "network.hpp"
#include "number/random.hpp"
#include "platform/flash_filesystem.hpp"
#include "roomPool.hpp"
#include "room_metatable.hpp"
#include "rooms/core.hpp"
#include "skyland.hpp"
#include "skyland/entity/ghost.hpp"
#include "skyland/rooms/speaker.hpp"
#include "skyland/rooms/synth.hpp"
#include "skyland/timeStreamEvent.hpp"
#include "tile.hpp"



namespace skyland
{



void Island::init_terrain(Platform& pfrm, int width, bool render)
{
    if (width < 3) {
        return;
    }

    terrain_.clear();

    terrain_.push_back(13), --width;

    for (int i = 0; i < width - 1; ++i) {
        terrain_.push_back(12);
    }

    terrain_.push_back(14);

    if (render) {
        render_terrain(pfrm);
    }
}



Island::Island(Platform& pfrm, Layer layer, u8 width, Player& owner)
    : owner_(&owner), layer_(layer), timer_(0),
      flag_anim_index_(Tile::flag_start), interior_visible_(false),
      show_flag_(false), dispatch_cancelled_(false), schedule_repaint_(false),
      schedule_repaint_partial_(false), has_radar_(false), is_boarded_(false),
      hidden_(false)
{
    init_terrain(pfrm, width, false);
}



Island::Rooms& Island::rooms()
{
    return rooms_;
}


void Island::remove_character(const RoomCoord& location)
{
    if (auto room = get_room(location)) {
        for (auto it = room->characters().begin();
             it not_eq room->characters().end();) {
            if ((*it)->grid_position() == location) {
                room->characters().erase(it);
                return;
            } else {
                ++it;
            }
        }
    }

    for (auto it = characters_.begin(); it not_eq characters_.end();) {
        if ((*it)->grid_position() == location) {
            characters_.erase(it);
            return;
        } else {
            ++it;
        }
    }
}


BasicCharacter* Island::character_at_location(const RoomCoord& loc)
{
    if (auto room = get_room(loc)) {
        for (auto& chr : room->characters()) {
            if (chr->grid_position() == loc) {
                return chr.get();
            }
        }
    }
    for (auto& chr : characters_) {
        if (chr->grid_position() == loc) {
            return chr.get();
        }
    }

    return nullptr;
}



std::pair<BasicCharacter*, Room*> Island::find_character_by_id(CharacterId id)
{
    for (auto& room : rooms_) {
        for (auto& character : room->characters()) {
            if (character->id() == id) {
                return {character.get(), room.get()};
            }
        }
    }
    return {nullptr, nullptr};
}



static auto fire_alloc_texture(Platform& pfrm, App& app, Island& island)
{
    std::optional<Platform::DynamicTexturePtr> result;

    // Check to see if the other island already has a texture allocated
    // for the fire effect. If so, share the texture.
    if (&island == &app.player_island() and app.opponent_island()) {
        result = app.opponent_island()->fire_texture();
    } else if (&island == app.opponent_island()) {
        result = app.player_island().fire_texture();
    }

    if (not result) {
        result = pfrm.make_dynamic_texture();
    }

    if (result) {
        (*result)->remap(154);
    }

    return result;
}



static const auto fire_spread_time = seconds(9);



void Island::FireState::rewind(Platform& pfrm,
                               App& app,
                               Island& island,
                               Microseconds delta)
{
    if (spread_timer_ > 0) {
        spread_timer_ -= delta;
    } else {
        spread_timer_ += fire_spread_time - delta;
    }

    if (damage_timer_ > 0) {
        damage_timer_ -= delta;
    } else {

        bool present = false;
        for (u8 x = 0; x < 16; ++x) {
            for (u8 y = 0; y < 16; ++y) {
                if (island.fire_present({x, y})) {
                    present = true;
                }
            }
        }

        if (present and not texture_) {

            texture_ = fire_alloc_texture(pfrm, app, island);

        } else if (not present) {
            texture_.reset();
        }

        damage_timer_ += seconds(2) - delta;
    }

    anim_timer_ -= delta;
    if (anim_timer_ < 0) {
        anim_timer_ += milliseconds(100);

        --anim_index_;
        if (anim_index_ == -1) {
            anim_index_ = 5;
        }

        if (texture_ and
            // We only want to maintain a single reference to the fire texture,
            // to save vram. Both islands share a texture for the fire effect,
            // and only one island updates the tile glyph.
            (texture_->strong_count() == 1 or
             &island == &app.player_island())) {
            (*texture_)->remap(154 + anim_index_);
        }
    }
}



void Island::rewind(Platform& pfrm, App& app, Microseconds delta)
{
    timer_ -= delta;

    if (show_flag_ and flag_pos_) {
        flag_anim_timer_ -= delta;
        if (flag_anim_timer_ < 0) {
            flag_anim_timer_ = milliseconds(80);
            auto current = flag_anim_index_;
            if (current > Tile::flag_start) {
                --flag_anim_index_;
            } else {
                flag_anim_index_ = Tile::flag_end;
            }
        }
    }

    if (chimney_loc_) {
        chimney_spawn_timer_ -= delta;

        if (chimney_spawn_timer_ < 0) {
            chimney_spawn_timer_ = milliseconds(600);

            auto o = origin();

            o.x += Fixnum::from_integer(chimney_loc_->x * 16 + 8);
            o.y += Fixnum::from_integer(chimney_loc_->y * 16 - 4);

            if (drift_ == 0.0_fixed) {
                // FIXME: faking the rewound smoke effects doesn't work if the
                // island is moving. So just don't spawn them. We'll need to
                // think of another way.
                if (auto e = app.alloc_entity<SmokePuff>(pfrm, o)) {
                    e->jump_to_end();
                    app.effects().push(std::move(e));
                }
            }
        }
    }

    auto& projectiles = this->projectiles();
    for (auto it = projectiles.begin(); it not_eq projectiles.end();) {
        if ((*it)->health() == 0) {
            it = projectiles.erase(it);
        } else {
            (*it)->rewind(pfrm, app, delta);
            ++it;
        }
    }

    bulk_timer_.rewind(pfrm, app, delta);

    for (auto& room : rooms_) {
        room->rewind(pfrm, app, delta);
        for (auto& chr : room->characters()) {
            chr->rewind(pfrm, app, delta);
        }
    }

    if (drift_ not_eq 0.0_fixed) {
        position_.x -= drift_ * Fixnum::from_integer(delta);
    }


    for (auto it = drones_.begin(); it not_eq drones_.end();) {
        if (not(*it)->alive()) {
            it = drones_.erase(it);
        } else {
            (*it)->rewind(pfrm, app, delta);
            ++it;
        }
    }


    if (schedule_repaint_) {
        schedule_repaint_ = false;
        schedule_repaint_partial_ = false;
        repaint(pfrm, app);
    } else if (schedule_repaint_partial_) {
        schedule_repaint_partial_ = false;
        repaint_partial(pfrm, app);
    }


    u8 ambient_offset = 4 * float(sine(4 * 3.14f * 0.0005f * timer_ + 180)) /
                        std::numeric_limits<s16>::max();

    ambient_movement_ = ambient_offset;


    pfrm.set_scroll(layer(),
                    -get_position().x.as_integer(),
                    -get_position().y.as_integer() - ambient_offset);

    fire_.rewind(pfrm, app, *this, delta);
}



void Island::check_destroyed()
{
    destroyed_ = false;
    bool has_core = false;
    for (auto& room : rooms_) {
        if ((*room->metaclass())->category() == Room::Category::power) {
            has_core = true;
            break;
        }
    }
    if (not has_core) {
        destroyed_ = true;
    }
}



void Island::set_hidden(Platform& pfrm, App& app, bool hidden)
{
    bool was_hidden = hidden_;

    hidden_ = hidden;

    if (not hidden_ and was_hidden) {
        repaint(pfrm, app);
    } else if (hidden_ and not was_hidden) {
        for (int x = 0; x < 16; ++x) {
            for (int y = 0; y < 16; ++y) {
                pfrm.set_tile(layer(), x, y, 0);
            }
        }
    }
}



std::optional<Platform::DynamicTexturePtr> Island::fire_texture()
{
    return fire_.texture_;
}



bool Island::fire_present(const RoomCoord& coord) const
{
    if (not fire_.texture_) {
        // If the fire texture doesn't exist, then we know the island isn't
        // burning, and don't need to do a lookup.
        return false;
    }
    return fire_.positions_.get(coord.x, coord.y);
}



void Island::fires_extinguish(Platform& pfrm, App& app)
{
    for (int x = 0; x < 16; ++x) {
        for (int y = 0; y < 16; ++y) {
            pfrm.set_tile(layer_, x, y, 0);
            if (fire_present({(u8)x, (u8)y})) {
                fire_extinguish(pfrm, app, {(u8)x, (u8)y});
            }
        }
    }
}



void Island::fire_extinguish(Platform& pfrm, App& app, const RoomCoord& coord)
{
    if (not fire_present(coord)) {
        return;
    }

    if (this == &player_island(app)) {
        time_stream::event::PlayerFireExtinguished e;
        e.x_ = coord.x;
        e.y_ = coord.y;
        app.time_stream().push(app.level_timer(), e);
    } else {
        time_stream::event::OpponentFireExtinguished e;
        e.x_ = coord.x;
        e.y_ = coord.y;
        app.time_stream().push(app.level_timer(), e);
    }

    fire_.positions_.set(coord.x, coord.y, false);
}



void Island::fire_create(Platform& pfrm, App& app, const RoomCoord& coord)
{
    if (fire_present(coord)) {
        return;
    }

    if (this == &player_island(app)) {
        time_stream::event::PlayerFireCreated e;
        e.x_ = coord.x;
        e.y_ = coord.y;
        app.time_stream().push(app.level_timer(), e);
    } else {
        time_stream::event::OpponentFireCreated e;
        e.x_ = coord.x;
        e.y_ = coord.y;
        app.time_stream().push(app.level_timer(), e);
    }

    fire_.positions_.set(coord.x, coord.y, true);
}



void Island::FireState::update(Platform& pfrm,
                               App& app,
                               Island& island,
                               Microseconds delta)
{
    damage_timer_ += delta;
    spread_timer_ += delta;

    if (spread_timer_ > fire_spread_time) {
        spread_timer_ -= fire_spread_time;

        auto mat = allocate_dynamic<bool[16][16]>("fire-spread-paths");
        bool plotted = false;

        Bitmatrix<16, 16> old_positions = positions_;

        for (u8 x = 0; x < 16; ++x) {
            for (u8 y = 0; y < 16; ++y) {
                if (old_positions.get(x, y)) {

                    if (not plotted) {
                        island.plot_walkable_zones(app, *mat);
                        plotted = true;
                    }

                    const auto props = [&] {
                        if (auto room = island.get_room({x, y})) {
                            return (*room->metaclass())->properties();
                        }
                        return u32(0);
                    }();

                    if (not(*mat)[x][y] and
                        not(props & RoomProperties::highly_flammable)) {
                        island.fire_extinguish(pfrm, app, {x, y});
                    }

                    auto try_spread = [&](u8 x, u8 y) {
                        if (auto room = island.get_room({x, y})) {
                            auto props = (*room->metaclass())->properties();
                            if (props & RoomProperties::fireproof or
                                (not(*mat)[x][y] and
                                 not(props &
                                     RoomProperties::highly_flammable))) {
                                return;
                            }
                        } else if (not(*mat)[x][y]) {
                            island.fire_extinguish(pfrm, app, {x, y});
                            return;
                        }
                        if (not old_positions.get(x, y)) {
                            island.fire_create(pfrm, app, {x, y});
                        }
                    };

                    if (x > 0) {
                        try_spread(x - 1, y);
                    }

                    if (x < 15) {
                        try_spread(x + 1, y);
                    }

                    if (y > 0) {
                        try_spread(x, y - 1);

                        if (x > 0) {
                            try_spread(x - 1, y - 1);
                        }
                        if (x < 15) {
                            try_spread(x + 1, y - 1);
                        }
                    }

                    if (y < 15) {
                        try_spread(x, y + 1);

                        if (x > 0) {
                            try_spread(x - 1, y + 1);
                        }
                        if (x < 15) {
                            try_spread(x + 1, y + 1);
                        }
                    }
                }
            }
        }
    }

    if (damage_timer_ > seconds(2)) {

        damage_timer_ -= seconds(2);

        bool fire_present = false;

        Buffer<RoomCoord, 32> spread_queue;

        for (u8 x = 0; x < 16; ++x) {
            for (u8 y = 0; y < 16; ++y) {
                if (positions_.get(x, y)) {
                    fire_present = true;
                    if (auto room = island.get_room({x, y})) {
                        if ((*room->metaclass())->properties() &
                            RoomProperties::fireproof) {
                            island.fire_extinguish(pfrm, app, {x, y});
                        } else {
                            room->burn_damage(pfrm, app, 2);

                            auto try_spread = [&](u8 x, u8 y) {
                                if (island.fire_present({x, y})) {
                                    return;
                                }
                                auto room = island.get_room({x, y});
                                if (room) {
                                    auto props =
                                        (*room->metaclass())->properties();
                                    if (props &
                                        RoomProperties::highly_flammable) {
                                        spread_queue.push_back({x, y});
                                    }
                                }
                            };

                            // If fire just destroyed a room, and there are
                            // un-lit highly-flammable rooms nearby, try to
                            // spread.
                            if (room->health() == 0) {
                                if (x > 0) {
                                    try_spread(x - 1, y);
                                }

                                if (x < 15) {
                                    try_spread(x + 1, y);
                                }

                                if (y > 0) {
                                    try_spread(x, y - 1);
                                }

                                if (y < 15) {
                                    try_spread(x, y + 1);
                                }
                            }
                        }
                    } else {
                        island.fire_extinguish(pfrm, app, {x, y});
                    }
                }
            }
        }

        for (auto& c : spread_queue) {
            island.fire_create(pfrm, app, c);
        }

        if (not fire_present) {
            spread_timer_ = 0;
        }

        if (fire_present and not texture_) {

            texture_ = fire_alloc_texture(pfrm, app, island);

        } else if (not fire_present) {
            texture_.reset();
        }
    }

    anim_timer_ += delta;
    if (anim_timer_ > milliseconds(100)) {
        anim_timer_ -= milliseconds(100);

        ++anim_index_;
        if (anim_index_ == 6) {
            anim_index_ = 0;
        }

        if (texture_ and
            // We only want to maintain a single reference to the fire texture,
            // to save vram. Both islands share a texture for the fire effect,
            // and only one island updates the tile glyph.
            (texture_->strong_count() == 1 or
             &island == &app.player_island())) {
            (*texture_)->remap(154 + anim_index_);
        }
    }
}



void Island::FireState::display(Platform& pfrm, Island& island)
{
    if (not texture_) {
        return;
    }

    auto o = ivec(island.visual_origin());

    auto batch = allocate_dynamic<Buffer<Vec2<s32>, 64>>("fire-spr-buffer");

    for (int x = 0; x < 16; ++x) {
        for (int y = 0; y < 16; ++y) {
            if (positions_.get(x, y)) {
                batch->push_back({o.x + x * 16, o.y + y * 16 - 16});
            }
        }
    }

    pfrm.screen().draw_batch((*texture_)->mapping_index() * 2, *batch);
}



void Island::update(Platform& pfrm, App& app, Microseconds dt)
{
    timer_ += dt;

    TIMEPOINT(t1);

    if (not hidden_ and show_flag_ and flag_pos_) {
        flag_anim_timer_ += dt;
        if (flag_anim_timer_ > milliseconds(80)) {
            flag_anim_timer_ = 0;
            auto current = flag_anim_index_;
            if (current < Tile::flag_end) {
                ++flag_anim_index_;
            } else {
                flag_anim_index_ = Tile::flag_start;
            }
        }
    }

    if (not hidden_ and chimney_loc_) {
        chimney_spawn_timer_ += dt;

        if (chimney_spawn_timer_ > milliseconds(600)) {
            chimney_spawn_timer_ = 0;

            auto o = origin();

            o.x += Fixnum::from_integer(chimney_loc_->x * 16 + 8);
            o.y += Fixnum::from_integer(chimney_loc_->y * 16 - 4);

            if (auto e = app.alloc_entity<SmokePuff>(pfrm, o)) {
                app.effects().push(std::move(e));
            }
        }
    }

    TIMEPOINT(t2);

    u8 ambient_offset = 4 * float(sine(4 * 3.14f * 0.0005f * timer_ + 180)) /
                        std::numeric_limits<s16>::max();

    ambient_movement_ = ambient_offset;

    const bool movement_ready = all_characters_awaiting_movement_;
    all_characters_awaiting_movement_ = true;

    is_boarded_ = false;


    auto on_character_died = [&](BasicCharacter& c) {
        if (not pfrm.network_peer().is_connected()) {
            auto fn = lisp::get_var("on-crew-died");
            if (fn->type() == lisp::Value::Type::function) {
                // NOTE: fn is in a global var, as we accessed it through
                // get_var. So there's no need to protect fn from the gc, as
                // it's already attached to a gc root.
                lisp::push_op(lisp::make_userdata(this));
                lisp::push_op(lisp::make_integer(c.id()));
                lisp::safecall(fn, 2);
                lisp::pop_op(); // result
            }
        }
        c.finalize(app);
        time_stream::event::CharacterDied e;
        e.x_ = c.grid_position().x;
        e.y_ = c.grid_position().y;
        e.id_.set(c.id());
        e.owned_by_player_ = c.owner() == &app.player();
        e.near_ = this == &app.player_island();
        e.is_replicant_ = c.is_replicant();
        app.time_stream().push(app.level_timer(), e);
    };


    character_count_ = 0;

    auto update_characters = [&](Room* room, auto& chr_list, bool exterior) {
        for (auto it = chr_list.begin(); it not_eq chr_list.end();) {
            if (not(*it)->alive()) {

                network::packet::ChrDiedV2 packet;
                packet.chr_id_.set((*it)->id());
                packet.near_island_ = this not_eq &app.player_island();
                network::transmit(pfrm, packet);

                on_character_died(**it);

                const auto pos = (*it)->sprite().get_position();
                if (auto e = alloc_entity<Ghost>(pos)) {
                    app.effects().push(std::move(e));
                }

                it = chr_list.erase(it);
            } else {


                if ((*it)->is_awaiting_movement()) {
                    if (movement_ready) {
                        (*it)->set_can_move();
                    }
                } else {
                    all_characters_awaiting_movement_ = false;
                }
                if ((*it)->owner() not_eq &owner()) {
                    if (not exterior) {
                        is_boarded_ = true;
                    }
                } else {
                    ++character_count_;
                }

                (*it)->update(pfrm, app, dt, room);
                ++it;
            }
        }
    };


    static constexpr const auto sync_delay = milliseconds(100);


    for (auto it = drones_.begin(); it not_eq drones_.end();) {
        if (not(*it)->alive()) {

            auto sync = [x = (*it)->position().x,
                         y = (*it)->position().y,
                         near = (*it)->destination() == &app.player_island()](
                            Platform& pfrm, App& app) {
                network::packet::DroneDestroyed destroyed;
                destroyed.drone_x_ = x;
                destroyed.drone_y_ = y;
                destroyed.destination_near_ = near;

                network::transmit(pfrm, destroyed);
            };

            if (pfrm.network_peer().is_connected()) {
                if (not app.on_timeout(pfrm, sync_delay, sync)) {
                    sync(pfrm, app);
                }
            }

            medium_explosion(pfrm, app, (*it)->sprite().get_position());

            it = drones_.erase(it);
        } else {
            (*it)->update(pfrm, app, dt);
            ++it;
        }
    }



    // Before I added the ArcGun weapon, which chains to multiple rooms, there
    // was no reason to do any optimizations on certain operations, because only
    // one room would be destroyed per frame, except in truely rare occasions.
    int destroyed_count = 0;
    bool core_destroyed = false;


    bool do_repaint = false;
    if (schedule_repaint_) {
        schedule_repaint_ = false;
        do_repaint = true;
    }


    bulk_timer_.update(pfrm, app, dt);


    resolve_cancelled_dispatch();


    Room* room = dispatch_list_;
    dispatch_list_ = nullptr;


    TIMEPOINT(t3);

    int dispatch_count = 0;
    int dispatch_max_lat = 0;
    int dispatch_min_lat = std::numeric_limits<int>::max();
    [[maybe_unused]] RoomMeta* max_mt = nullptr;


    while (room) {

        TIMEPOINT(before);

        ++dispatch_count;
        const auto next = room->dispatch_next();

        if (room->health() == 0) {

            const bool quiet = (*room->metaclass())->properties() &
                               RoomProperties::destroy_quietly;

            if (++destroyed_count < 5 and not quiet) {
                // Five rooms destroyed on the same island in the same frame! If
                // we create tons of huge explosions all at once, we'll lag the
                // game and use lots of entities.
                big_explosion(pfrm, app, room->center());
            }

            if (str_eq(room->name(), "power-core") or
                str_eq(room->name(), "reactor")) {
                core_destroyed = true;
            }

            if (destroyed_count < 2 and
                app.game_mode() not_eq App::GameMode::multiplayer and
                app.game_mode() not_eq App::GameMode::co_op) {
                pfrm.sleep(2);
            }

            const auto pos = room->position();

            auto mt = room->metaclass_index();

            auto sync = [x = pos.x,
                         y = pos.y,
                         near = &owner() not_eq &app.player(),
                         mt](Platform& pfrm, App& app) {
                network::packet::RoomDestroyed packet;
                packet.room_x_ = x;
                packet.room_y_ = y;
                packet.near_island_ = near;
                packet.metaclass_index_.set(mt);
                network::transmit(pfrm, packet);
            };

            for (auto& chr : room->characters()) {
                // The room was destroyed, along with any inhabitants.
                on_character_died(*chr);
            }


            // Running a callback for each room destroyed and then resetting the
            // delta clock would ruin multiplayer sync. We have no need to
            // register an on-room-destroyed callback in multiplayer anyway...
            if (app.game_mode() not_eq App::GameMode::multiplayer and
                app.game_mode() not_eq App::GameMode::co_op) {

                static auto destroyed_str = lisp::intern("on-room-destroyed");

                // Creates some garbage, puts a bit of pressure on the gc, but
                // not too much. The system supports thousands of lisp values at
                // once, so we're unlikely to trigger the gc. Furthermore, we
                // intentionally run the gc manually after running a level setup
                // script, so we should have plenty of values available.
                auto sym = lisp::make_symbol(
                    destroyed_str, lisp::Symbol::ModeBits::stable_pointer);

                // This is quite expensive! But it's convenient to be able to be
                // able to register a callback when a room's destroyed.
                auto fn = lisp::get_var(sym);
                if (fn->type() == lisp::Value::Type::function) {
                    // NOTE: fn is in a global var, as we accessed it through
                    // get_var. So there's no need to protect fn from the gc, as
                    // it's already attached to a gc root.
                    lisp::push_op(lisp::make_userdata(this));
                    lisp::push_op(lisp::make_symbol(room->name()));
                    lisp::push_op(lisp::make_integer(room->position().x));
                    lisp::push_op(lisp::make_integer(room->position().y));
                    lisp::safecall(fn, 4);
                    lisp::pop_op(); // result

                    // We cannot know how much latency that the custom script
                    // added. i.e. reset the clock in case some scripted code
                    // spent too long executing custom lisp code. Otherwise
                    // collision checking and other stuff could get messed up!
                    pfrm.delta_clock().reset();
                } else if (destroyed_count == 0) {
                    // No pauses during multiplayer, of course.
                    if (not pfrm.network_peer().is_connected()) {
                        // Sleep a few frames when a block is destroyed.
                        if (this == &app.player_island()) {
                            pfrm.sleep(4);
                        } else {
                            pfrm.sleep(2);
                        }
                    }
                }
            }

            const auto group = room->group();

            room->finalize(pfrm, app);

            if (&owner() == &app.player()) {
                if (group not_eq Room::Group::none) {
                    time_stream::event::PlayerRoomDestroyedWithGroup p;
                    p.x_ = pos.x;
                    p.y_ = pos.y;
                    p.type_ = mt;
                    p.group_ = (u8)group;
                    app.time_stream().push(app.level_timer(), p);
                } else {
                    time_stream::event::PlayerRoomDestroyed p;
                    p.x_ = pos.x;
                    p.y_ = pos.y;
                    p.type_ = mt;
                    app.time_stream().push(app.level_timer(), p);
                }
            } else {
                time_stream::event::OpponentRoomDestroyed p;
                p.x_ = pos.x;
                p.y_ = pos.y;
                p.type_ = mt;
                app.time_stream().push(app.level_timer(), p);
            }


            if (destroyed_count < 2 and not quiet and
                // Core and Reactor play a loud sound on their own, playing an
                // explosion simultaneously causes clipping on some consoles.
                not core_destroyed) {
                pfrm.speaker().play_sound("explosion1", 2);
            }

            if (pfrm.network_peer().is_connected()) {
                if (not app.on_timeout(pfrm, sync_delay, sync)) {
                    // Explanation: we don't want to transmit the room destroyed
                    // packet right away. We only transmit room destroyed
                    // packets in the first place to keep games in sync. If one
                    // game happens to be a frame or two ahead of another game,
                    // we might end up destroying a room before it is hit by a
                    // projectile that it was already hit by in the other game,
                    // in which case, the projectile might keep going and
                    // destroy an additional room, appearing to the other player
                    // as though the projectile did twice as much damage as it
                    // should have. So, instead, we attempt to enqueue a
                    // deferred callback, which transmits the RoomDestroyed
                    // packet at some point a frame or two in the future, at
                    // which point, both games should already be synchronized,
                    // and there is in fact nothing to destroy. If we fail to
                    // enqueue the deferred callback for some reason, invoke
                    // immediately. I want to stress: we only transmit the
                    // room-destroyed packet as a last resort, in case the games
                    // somehow got out of sync. This packet should in almost all
                    // cases be meaningless to the other game.
                    sync(pfrm, app);
                }
            }


            app.player().on_room_destroyed(pfrm, app, *room);

            [&] {
                for (auto it = rooms_.begin(); it not_eq rooms_.end(); ++it) {
                    if (it->get() == room) {
                        rooms_.erase(it);
                        return;
                    }
                }
                Platform::fatal("dispatch list erase room not in roomsTable");
            }();

            owner().rooms_lost_++;

            on_layout_changed(app, pos);

            check_destroyed();

            if (is_destroyed() and not pfrm.network_peer().is_connected()) {
                pfrm.sleep(3);
            }

            recalculate_power_usage(app);

            do_repaint = true;

        } else {
            if (dt not_eq 0) {
                // Do not update a room if the game is stopped.
                room->update(pfrm, app, dt);
            } else {
                // If a room was ready, and we didn't update it, then it's still
                // ready.
                dispatch_room(room);
            }

            update_characters(room, room->characters(), false);
        }

        TIMEPOINT(after);
        const auto lat = after - before;
        if (lat < dispatch_min_lat) {
            dispatch_min_lat = lat;
        }
        if (lat > dispatch_max_lat) {
            dispatch_max_lat = lat;
            max_mt = room->metaclass();
        }

        room = next;
    }

    TIMEPOINT(t4);

    if (do_repaint) {
        repaint(pfrm, app);
        schedule_repaint_partial_ = false;
    } else if (schedule_repaint_partial_) {
        schedule_repaint_partial_ = false;
        repaint_partial(pfrm, app);
    }


    TIMEPOINT(t5);


    update_characters(nullptr, characters_, true);


    TIMEPOINT(t6);


    update_entities(pfrm, app, dt, projectiles_);


    TIMEPOINT(t7);


    if (drift_ not_eq 0.0_fixed) {
        position_.x += drift_ * Fixnum::from_integer(dt);
    }

    TIMEPOINT(t8);

    pfrm.set_scroll(layer(),
                    -get_position().x.as_integer(),
                    -get_position().y.as_integer() - ambient_offset);

    fire_.update(pfrm, app, *this, dt);


    TIMEPOINT(t9);

// #define ISLAND_PROFILE_LATENCY
#ifdef ISLAND_PROFILE_LATENCY
    if (pfrm.keyboard().pressed<Key::select>()) {
        Platform::fatal(format("% % % % % % % % cnt:% max:% min:% max-mt:%",
                               t2 - t1,
                               t3 - t2,
                               t4 - t3,
                               t5 - t4,
                               t6 - t5,
                               t7 - t6,
                               t8 - t7,
                               t9 - t8,
                               dispatch_count,
                               dispatch_max_lat,
                               dispatch_min_lat,
                               (*max_mt)->name())
                            .c_str());
    }
#endif
}



void Island::on_layout_changed(App& app,
                               const RoomCoord& room_added_removed_coord)
{
    check_destroyed();

    bool buffer[16][16];
    plot_walkable_zones(app, buffer);

    checksum_ = 0;

    for (auto& room : rooms()) {
        for (auto& chr : room->characters()) {
            if (auto path = chr->get_movement_path()) {
                int pos = 0;
                for (auto& node : *path) {
                    if (get_room(node) == nullptr or
                        not buffer[node.x][node.y]) {
                        // Ok, a character is moving, but the room in the
                        // character's movement path no longer exists. We want
                        // to crop the path after the nonexistent point.
                        ++pos;
                        while (pos) {
                            --pos;
                            path->erase(path->begin());
                        }
                        break; // NOTE: we just deallocated the path, so we
                               // cannot run the enclosing loop over the path
                               // nodes anymore.
                    } else {
                        ++pos;
                    }
                }
            }
        }

        // NOTE: the checksum must incorporate room type as well as position,
        // otherwise, in some scenarios, the checksum may be the same after a
        // block is removed. Checksums can still collide, of course.
        checksum_ +=
            room->metaclass_index() + room->position().y + room->position().x;
    }
}



static constexpr const int screen_limit_y = 700;



void Island::display(Platform& pfrm)
{
    if (hidden_) {
        return;
    }

    if (flag_pos_ and show_flag_) {

        std::optional<u16> palette;

        if (layer_ == Layer::map_0_ext) {
            // NOTE: the player can design his/her own flag, so we reserve a
            // specific palette bank just for the flag image. Untimately, doing
            // so simplifies things.
            palette = 12;
        }
        pfrm.set_tile(
            layer_, flag_pos_->x, flag_pos_->y, flag_anim_index_, palette);
    }

    for (auto& c : characters_) {
        // The interior floor is two pixels thick. But our character is now
        // standing outside, where there's no floor, so we need to shift the
        // character down by two pixels.
        Sprite cpy = c->sprite();
        auto pos = cpy.get_position();
        if (pos.y.as_integer() < screen_limit_y) {
            pos.y += 2.0_fixed;
            cpy.set_position(pos);
            pfrm.screen().draw(cpy);
        }
    }

    for (auto& p : projectiles_) {
        pfrm.screen().draw(p->sprite());
    }


    resolve_cancelled_dispatch();

    Room* room = dispatch_list_;
    while (room) {
        room->display(pfrm.screen());
        room = room->dispatch_next();
    }
}



void Island::display_fires(Platform& pfrm)
{
    fire_.display(pfrm, *this);
}



HitBox Island::hitbox() const
{
    Vec2<Fixnum> hitbox_pos = this->origin();
    HitBox island_hitbox;
    island_hitbox.position_ = &hitbox_pos;
    island_hitbox.dimension_.size_.x = terrain_.size() * 16;
    island_hitbox.dimension_.size_.y = 16 * 16;
    return island_hitbox;
}



void Island::test_collision(Platform& pfrm, App& app, Entity& entity)
{
    // Calculate the position of the entity in terms of the island's grid
    // coordinates.
    auto entity_pos = ivec((entity.sprite().get_position() - this->origin()));
    entity_pos.x /= 16;
    entity_pos.y /= 16;

    for (int x = entity_pos.x - 1; x < entity_pos.x + 2; ++x) {
        for (int y = entity_pos.y - 1; y < entity_pos.y + 2; ++y) {
            if (x < 0 or y < 0 or x > 15 or y > 15) {
                continue;
            }

            if (rooms_plot_.get(x, y)) {
                if (auto room = get_room({(u8)x, (u8)y})) {
                    static constexpr const int tile_size = 16;

                    auto hitbox_pos = this->origin();
                    hitbox_pos.x +=
                        Fixnum::from_integer(room->position().x * tile_size);

                    hitbox_pos.y +=
                        Fixnum::from_integer(room->position().y * tile_size);

                    HitBox room_hitbox;
                    room_hitbox.position_ = &hitbox_pos;
                    room_hitbox.dimension_.size_.x = room->size().x * tile_size;
                    room_hitbox.dimension_.size_.y = room->size().y * tile_size;

                    if (room_hitbox.overlapping(entity.hitbox())) {
                        entity.on_collision(pfrm, app, *room);
                        return;
                    }
                }
            }
        }
    }

    Vec2<Fixnum> hitbox_pos = this->origin();
    HitBox island_hitbox;
    island_hitbox.position_ = &hitbox_pos;
    island_hitbox.dimension_.size_.x = terrain_.size() * 16;
    island_hitbox.dimension_.size_.y = 16 * 16;

    if (island_hitbox.overlapping(entity.hitbox())) {
        for (auto& drone_sp : drones_) {
            if (entity.hitbox().overlapping((drone_sp)->hitbox())) {
                entity.on_collision(pfrm, app, *drone_sp);
                (drone_sp)->on_collision(pfrm, app, entity);
            }
        }
    }
}



void Island::render_terrain(Platform& pfrm)
{
    for (u32 i = 0; i < terrain_.size(); ++i) {
        auto tile_handle = layer_ == Layer::map_0_ext
                               ? pfrm.map_tile0_chunk(terrain_[i])
                               : pfrm.map_tile1_chunk(terrain_[i]);

        pfrm.set_tile(layer_, i, 15, tile_handle);
    }

    for (int i = terrain_.size(); i < 16; ++i) {
        pfrm.set_tile(layer_, i, 15, 0);
    }
}



const Vec2<Fixnum>& Island::get_position() const
{
    return position_;
}



void Island::set_position(const Vec2<Fixnum>& position)
{
    position_ = position;
}



void Island::render(Platform& pfrm, App& app)
{
    if (interior_visible_) {
        render_interior(pfrm, app);
    } else {
        render_exterior(pfrm, app);
    }
}



void Island::render_interior_fast(Platform& pfrm, App& app)
{
    if (interior_visible_) {
        repaint(pfrm, app);
        return;
    }

    render_interior(pfrm, app);
}



void Island::render_interior(Platform& pfrm, App& app)
{
    interior_visible_ = true;

    if (layer_ == Layer::map_0_ext) {
        auto t = app.environment().player_island_interior_texture();
        pfrm.load_tile0_texture(t);
    } else {
        auto t = app.environment().opponent_island_interior_texture();
        pfrm.load_tile1_texture(t);
    }

    // When rendering the interior/exterior of a castle, we've swapped the
    // tileset texture, so all tiles mapped into memory need to be discarded,
    // allowing the repaint() code to insert new mappings into vram.
    layer_ == Layer::map_0_ext ? pfrm.clear_tile0_mappings()
                               : pfrm.clear_tile1_mappings();

    repaint(pfrm, app);
}



void Island::render_exterior(Platform& pfrm, App& app)
{
    interior_visible_ = false;

    if (layer_ == Layer::map_0_ext) {
        pfrm.load_tile0_texture(app.environment().player_island_texture());
    } else {
        pfrm.load_tile1_texture(app.environment().opponent_island_texture());
    }

    layer_ == Layer::map_0_ext ? pfrm.clear_tile0_mappings()
                               : pfrm.clear_tile1_mappings();

    repaint(pfrm, app);
}



void Island::plot_rooms(u8 matrix[16][16]) const
{
    for (int x = 0; x < 16; ++x) {
        for (int y = 0; y < 16; ++y) {
            matrix[x][y] = 0;
        }
    }

    for (auto& room : rooms_) {
        auto pos = room->position();
        auto sz = room->size();

        int val = 2;
        if (not((*room->metaclass())->properties() &
                RoomProperties::roof_hidden)) {
            val = 1;
        } else if ((*room->metaclass())->properties() &
                   RoomProperties::disallow_chimney) {
            val = 3;
        }

        for (int x = 0; x < sz.x; ++x) {
            for (int y = 0; y < sz.y; ++y) {
                matrix[x + pos.x][y + pos.y] = val;
            }
        }
    }
}


void Island::recalculate_power_usage(App& app)
{
    power_supply_ = 0;
    power_drain_ = 0;

    for (auto& room : rooms_) {
        auto power = room->power_usage(app);

        if (power < 0) {
            power_supply_ += -power;
        } else {
            power_drain_ += power;
        }
    }
}



bool Island::add_character(EntityRef<BasicCharacter> character)
{
    if (auto room = get_room(character->grid_position())) {
        return room->add_occupant(std::move(character));
    } else {
        characters_.push(std::move(character));
        return true;
    }
}



void Island::move_room(Platform& pfrm,
                       App& app,
                       const RoomCoord& from,
                       const RoomCoord& to)
{
    for (auto it = rooms_.begin(); it not_eq rooms_.end(); ++it) {
        if (not(*it)->hidden() and (*it)->position() == from) {
            auto room = std::move(*it);
            it = rooms_.erase(it);

            room->__set_position(to);

            const int x_off = to.x - from.x;
            const int y_off = to.y - from.y;

            for (auto& chr : room->characters()) {
                auto p = chr->grid_position();
                p.x = p.x + x_off;
                p.y = p.y + y_off;

                chr->set_grid_position(p);
                chr->drop_movement_path();
            }

            if (app.game_speed() == GameSpeed::stopped and
                not room->ai_aware()) {
                // The room is cloaked, and we're moving it. We want to update
                // visibility in case the room was moved out of the range of a
                // cloaking field.
                room->init_ai_awareness_upon_unpause();
            }

            rooms_.insert_room(std::move(room));

            recalculate_power_usage(app);
            on_layout_changed(app, from);

            schedule_repaint_ = true;

            if (this == &app.player_island()) {
                time_stream::event::PlayerRoomMoved e;
                e.x_ = to.x;
                e.y_ = to.y;
                e.prev_x_ = from.x;
                e.prev_y_ = from.y;
                app.time_stream().push(app.level_timer(), e);
            } else {
                time_stream::event::OpponentRoomMoved e;
                e.x_ = to.x;
                e.y_ = to.y;
                e.prev_x_ = from.x;
                e.prev_y_ = from.y;
                app.time_stream().push(app.level_timer(), e);
            }

            if (fire_present(from)) {
                fire_extinguish(pfrm, app, from);
                fire_create(pfrm, app, to);
            }

            return;
        }
    }
}



void Island::plot_walkable_zones(App& app, bool matrix[16][16]) const
{
    for (int x = 0; x < 16; ++x) {
        for (int y = 0; y < 16; ++y) {
            matrix[x][y] = 0;
        }
    }

    // TODO: label outdoor grass areas as walkable.

    for (auto& room : rooms_) {
        auto props = (*room->metaclass())->properties();

        if (props & RoomProperties::habitable) {
            room->plot_walkable_zones(app, matrix);
        }
    }
}



void Island::plot_construction_zones(bool matrix[16][16]) const
{
    u8 matrix_temp[16][16];

    plot_rooms(matrix_temp);

    for (int x = 0; x < 16; ++x) {
        for (int y = 0; y < 15; ++y) {
            matrix[x][y] =
                matrix_temp[x][y] == 0 and y < 15 and matrix_temp[x][y + 1];
        }
    }

    for (int x = 0; x < 16; ++x) {
        matrix[x][15] = false;
    }

    for (u32 x = 0; x < terrain_.size(); ++x) {
        if (matrix_temp[x][14] == 0) {
            matrix[x][14] = true;
        }
    }

    for (auto& sp : drones_) {
        auto pos = sp->position();
        matrix[pos.x][pos.y] = false;
        matrix[pos.x][pos.y + 1] = false;
    }
}



bool Island::repaint_alloc_tiles(Platform& pfrm,
                                 App& app,
                                 TileId buffer[16][16],
                                 bool retry)
{
    for (int x = 0; x < 16; ++x) {
        // NOTE: only handle 15 rows because render_terrain() takes care of the
        // last row.
        for (int y = 0; y < 15; ++y) {

            if (buffer[x][y] >= Tile::dlc_tiles_begin and
                buffer[x][y] < Tile::dlc_tiles_begin + 15) {
                pfrm.set_tile(layer_, x, y, buffer[x][y]);
                pfrm.set_palette(layer_, x, y, 12);
                continue;
            }

            auto tile_handle = layer_ == Layer::map_0_ext
                                   ? pfrm.map_tile0_chunk(buffer[x][y])
                                   : pfrm.map_tile1_chunk(buffer[x][y]);

            if (min_y_ == 0 and tile_handle) {
                min_y_ = y;
            }

            if (tile_handle == 112 and not retry) {

                // We ran out of vram for storing tiles! Clear out all mapped
                // tiles, and attempt to reconstruct. A bit of a lazy
                // brute-force solution. Alternatively, we could attempt to
                // automatically clean up tile mappings when a room's destroyed,
                // but then, all tiles would need to be reference-counted, and
                // running out of tiles is a rare edge-case, so it's not worth
                // optimizing at the moment.

                layer_ == Layer::map_0_ext ? pfrm.clear_tile0_mappings()
                                           : pfrm.clear_tile1_mappings();

                return false;
            }

            pfrm.set_tile(layer_, x, y, tile_handle);
        }
    }
    return true;
}



void Island::repaint_partial(Platform& pfrm, App& app)
{
    TileId buffer[16][16];
    for (u32 x = 0; x < terrain_.size(); ++x) {
        for (int y = 0; y < 16; ++y) {
            buffer[x][y] = 0;
        }
    }

    for (auto& r : rooms_) {
        if (r->poll_repaint()) {
            if (interior_visible_) {
                r->render_interior(&app, buffer);
            } else {
                r->render_exterior(&app, buffer);
            }
        }
    }

    for (auto& r : rooms_) {
        r->render_cloak(app, buffer);
    }

    for (u32 x = 0; x < terrain_.size(); ++x) {
        for (u32 y = 0; y < 15; ++y) {
            if (buffer[x][y] not_eq 0) {
                auto tile_handle = layer_ == Layer::map_0_ext
                                       ? pfrm.map_tile0_chunk(buffer[x][y])
                                       : pfrm.map_tile1_chunk(buffer[x][y]);

                if (min_y_ == 0 and tile_handle) {
                    min_y_ = y;
                }

                pfrm.set_tile(layer_, x, y, tile_handle);
            }
        }
    }
}



void Island::repaint(Platform& pfrm, App& app)
{
    if (hidden_) {
        return;
    }

    // The engine only knows how to draw an island wholistically, because some
    // tiles need to be joined etc., so whenever the island changes, the whole
    // things needs to be repainted. I could try to only re-render the changed
    // parts, doing so would be quite complex, as the positions of the flag,
    // chimney, roof tiles, etc. all sort of depend on the shape of the island
    // as a whole. Anyway, this function performs all of the tile rendering.

    u8 matrix[16][16];
    TileId buffer[16][16]; // TODO: move this off of the stack!?

    for (int x = 0; x < 16; ++x) {
        for (int y = 0; y < 16; ++y) {
            buffer[x][y] = 0;
        }
    }

    min_y_ = 0;

    flag_pos_.reset();

    if (interior_visible_) {
        for (auto& room : rooms()) {
            room->render_interior(&app, buffer);
        }
    } else {
        for (auto& room : rooms()) {
            room->render_exterior(&app, buffer);
        }
    }

    for (auto& room : rooms()) {
        room->render_scaffolding(app, buffer);
    }

    plot_rooms(matrix);

    Buffer<RoomCoord, terrain_.capacity()> chimney_locs;

    has_radar_ = false;
    manufactory_count_ = 0;
    workshop_count_ = 0;
    core_count_ = 0;
    offensive_capabilities_ = 0;
    for (auto& room : rooms_) {
        if ((*room->metaclass())->properties() & RoomProperties::has_chimney) {
            chimney_locs.push_back(room->position());
        }
        auto metac = room->metaclass();
        if (str_cmp((*metac)->name(), "radar") == 0) {
            has_radar_ = true;
        } else if (str_cmp((*metac)->name(), "workshop") == 0) {
            ++workshop_count_;
        } else if (str_cmp((*metac)->name(), "manufactory") == 0) {
            ++manufactory_count_;
        } else if ((*room->metaclass())->category() == Room::Category::power) {
            critical_core_x_ = room->position().x;
            critical_core_y_ = room->position().y;
            ++core_count_;
        } else if ((*metac)->category() == Room::Category::weapon or
                   (character_count_ and
                    str_eq((*metac)->name(), "transporter")) or
                   str_eq((*metac)->name(), "drone-bay")) {
            ++offensive_capabilities_;
        }
    }

    std::sort(chimney_locs.begin(),
              chimney_locs.end(),
              [&](const auto& lhs, const auto& rhs) { return lhs.y < rhs.y; });

    chimney_loc_.reset();

    bool placed_flag = false;
    bool placed_chimney = false;

    std::optional<RoomCoord> flag_loc;

    rooms_plot_.clear();

    for (u8 x = 0; x < 16; ++x) {
        for (int y = 15; y > -1; --y) {

            rooms_plot_.set(x, y, matrix[x][y]);

            if (matrix[x][y] == 0 and y < 15 and matrix[x][y + 1] == 1) {
                bool block_chimney = false;
                if (buffer[x][y] == Tile::strut) {
                    block_chimney = true;
                    buffer[x][y] = Tile::roof_strut;
                } else if (buffer[x][y] == Tile::strut_top) {
                    block_chimney = true;
                    buffer[x][y] = Tile::roof_strut_joined;
                } else if (buffer[x][y] == Tile::scaffolding_angled_l or
                           buffer[x][y] == Tile::scaffolding_angled_r) {
                    block_chimney = true;
                    buffer[x][y] = Tile::roof_strut_joined;
                } else {
                    buffer[x][y] = Tile::roof_plain;
                }

                bool placed_chimney_this_tile = false;
                if (not block_chimney and not placed_chimney and y > 5) {
                    for (auto& loc : chimney_locs) {
                        if (loc.x == x and loc.y >= y) {
                            buffer[x][y] = Tile::roof_chimney;
                            chimney_loc_ = RoomCoord{u8(x), u8(y)};
                            placed_chimney = true;
                            placed_chimney_this_tile = true;
                        }
                    }
                }
                // NOTE: when placing a flag, we need to make sure that the slot
                // above the current tile is empty, because the flag is two
                // tiles tall.
                if (y > 0 and matrix[x][y - 1] == 0 and buffer[x][y - 2] == 0) {
                    if (not placed_chimney_this_tile and show_flag_ and
                        not placed_flag and y > 5) {
                        placed_flag = true;
                        buffer[x][y] = Tile::roof_flag;
                        buffer[x][y - 1] = Tile::flag_start;
                        flag_pos_ = {x, u8(y - 1)};
                    }
                }
            } else if (y == 14 and buffer[x][y] == 0 and
                       x < (int)terrain_.size()) {
                buffer[x][y] = Tile::grass;
            } else if (matrix[x][y] == 0 and matrix[x][y + 1] == 2) {
                bool placed_chimney_this_tile = false;
                if (not placed_chimney and y > 5) {
                    for (auto& loc : chimney_locs) {
                        if (loc.x == x and loc.y >= y) {
                            buffer[x][y] = Tile::tin_chimney;
                            placed_chimney = true;
                            chimney_loc_ = RoomCoord{u8(x), u8(y)};
                            placed_chimney_this_tile = true;
                        }
                    }
                }
                if (y > 0 and matrix[x][y - 1] == 0) {
                    if (not placed_chimney_this_tile and show_flag_ and
                        not placed_flag and y > 1 and matrix[x][y - 1] == 0) {
                        if (auto room = get_room({x, (u8)(y + 1)})) {
                            if ((*room->metaclass())->properties() &
                                    RoomProperties::flag_mount and
                                y > 5) {
                                placed_flag = true;
                                buffer[x][y] = Tile::flag_mount;
                                buffer[x][y - 1] = Tile::flag_start;
                                flag_pos_ = {x, u8(y - 1)};
                            }
                        }
                    }
                }
            }
        }
    }

    if (not interior_visible_) {
        // Clean up connections between stacked rooms
        for (int x = 0; x < 16; ++x) {
            for (int y = 0; y < 15; ++y) {
                auto t1 = buffer[x][y];
                auto t2 = buffer[x][y + 1];

                if (t1 == Tile::wall_window_2) {
                    if (t2 == Tile::wall_window_1) {
                        buffer[x][y] = Tile::wall_window_middle_2;
                        buffer[x][y + 1] = Tile::wall_window_middle_1;
                    } else if (t2 == Tile::wall_plain_1) {
                        buffer[x][y] = Tile::wall_window_middle_2;
                        buffer[x][y + 1] = Tile::wall_plain_middle;
                    }
                } else if (t1 == Tile::wall_plain_2) {
                    if (t2 == Tile::wall_window_1) {
                        buffer[x][y] = Tile::wall_plain_middle;
                        buffer[x][y + 1] = Tile::wall_window_middle_1;
                    } else if (t2 == Tile::wall_plain_1) {
                        buffer[x][y] = Tile::wall_plain_middle;
                        buffer[x][y + 1] = Tile::wall_plain_middle;
                    }
                }
            }
        }
    }

    for (auto& r : rooms_) {
        r->render_cloak(app, buffer);
    }

    if (flag_pos_) {
        auto t = buffer[flag_pos_->x][flag_pos_->y];
        if (t == Tile::cloaked or t == Tile::null) {
            flag_pos_.reset();
        }
        t = buffer[flag_pos_->x][flag_pos_->y + 1];
        if (t == Tile::null) {
            flag_pos_.reset();
        }
    }

    if (chimney_loc_) {
        auto t = buffer[chimney_loc_->x][chimney_loc_->y];
        if (t == Tile::cloaked or t == Tile::null) {
            chimney_loc_.reset();
        }
    }

    if (not repaint_alloc_tiles(pfrm, app, buffer, false)) {
        repaint_alloc_tiles(pfrm, app, buffer, true);
    }

    if (layer_ == Layer::map_0_ext and flag_pos_) {
        pfrm.set_palette(layer_, flag_pos_->x, flag_pos_->y, 12);
    }

    render_terrain(pfrm);


    for (auto& room : rooms_) {
        if ((*room->metaclass())->category() == Room::Category::weapon) {
            auto pos = room->position();
            pos.y += room->size().y - 1;

            if (room->group() not_eq Room::Group::none) {
                // NOTE: 8x8px tile, so start index = 4 * 16ptile + 1.
                // +1 to skip the none enumeration.
                const auto tile = (6 * 4 - 1) + (int)room->group();
                if (layer_ == Layer::map_0_ext) {
                    pfrm.set_raw_tile(
                        Layer::map_0, pos.x * 2, pos.y * 2 + 1, tile);
                }
            }
        }
    }
}



void Island::set_drift(Platform& pfrm, App& app, Fixnum drift)
{
    if (app.opponent_island() and this == app.opponent_island()) {

        time_stream::event::OpponentIslandDriftChanged e;
        e.previous_speed__data_.set(drift_.data());

        app.time_stream().push(app.level_timer(), e);
    }

    if (this == &app.player_island()) {
        Platform::fatal("player island not intended to change position");
    }

    drift_ = drift;
}



Vec2<Fixnum> Island::origin() const
{
    return {position_.x, position_.y};
}



Vec2<Fixnum> Island::visual_origin() const
{
    return {position_.x, position_.y + Fixnum::from_integer(ambient_movement_)};
}



std::optional<SharedEntityRef<Drone>> Island::get_drone(const RoomCoord& coord)
{
    for (auto& drone_sp : drones()) {
        if (drone_sp->position() == coord) {
            return drone_sp;
        }
    }

    return {};
}



Room* Island::get_room(const RoomCoord& coord)
{
    return rooms_.get_room(coord);
}



void Island::destroy_room(Platform& pfrm, App& app, const RoomCoord& coord)
{
    for (auto& room : rooms_) {
        if (coord.x >= room->position().x and coord.y >= room->position().y and
            coord.x < room->position().x + room->size().x and
            coord.y < room->position().y + room->size().y) {

            room->finalize(pfrm, app);
            rooms_.erase(&room);
            owner().rooms_lost_++;

            on_layout_changed(app, coord);

            repaint(pfrm, app);
            recalculate_power_usage(app);
            return;
        }
    }
}



void Island::init_ai_awareness(Platform& pfrm, App& app)
{
    for (auto& r : rooms()) {
        r->init_ai_awareness(pfrm, app);
    }
}



void Island::dispatch_room(Room* room)
{
    room->dispatch_update(dispatch_list_);
    dispatch_list_ = room;
}



void Island::cancel_dispatch()
{
    dispatch_cancelled_ = true;
}



void Island::resolve_cancelled_dispatch()
{
    // If a room was destroyed, we could try to fix dangling pointer issues
    // around the dispatch list, but it's simpler just to destroy the list and
    // recreate it.
    if (UNLIKELY(dispatch_cancelled_)) {
        dispatch_list_ = nullptr;
        for (auto& room : rooms_) {
            dispatch_room(room.get());
        }
        dispatch_cancelled_ = false;
    }
}



void Island::clear(Platform& pfrm, App& app)
{
    clear_rooms(pfrm, app);

    characters_.clear();
    projectiles_.clear();
    drones_.clear();
}



void Island::clear_rooms(Platform& pfrm, App& app)
{
    for (auto& room : rooms_) {
        room->finalize(pfrm, app);
    }

    cancel_dispatch();

    rooms_.clear();
}



void Island::set_float_timer(Microseconds value)
{
    timer_ = value;
}



u8 Island::offensive_capabilities() const
{
    return offensive_capabilities_;
}



u8 Island::character_count() const
{
    return character_count_;
}



void show_island_interior(Platform& pfrm, App& app, Island* island)
{
    if (island) {
        island->render_interior(pfrm, app);
    }

    write_custom_graphics(pfrm, app);
}



void show_island_exterior(Platform& pfrm, App& app, Island* island)
{
    if (island) {
        island->render_exterior(pfrm, app);
    }

    write_custom_graphics(pfrm, app);
}



u8 Island::min_y() const
{
    return min_y_;
}



Island& player_island(App& app)
{
    return app.player_island();
}



Island* opponent_island(App& app)
{
    return app.opponent_island();
}



bool speaker_data_store(Platform& pfrm, Island& island, const char* path)
{
    Vector<char> data;

    for (auto& room : island.rooms()) {
        if (auto speaker = room->cast<Speaker>()) {
            u8 coord = 0;
            coord |= (speaker->position().x << 4);
            coord |= 0x0f & (speaker->position().y);
            data.push_back(coord);

            auto ef = speaker->effect_flags().vector_.data();

            // Store coordinate, followed by effect flags bitvector.

            for (u32 i = 0; i < sizeof *ef; ++i) {
                data.push_back(((u8*)ef)[i]);
            }

            for (u32 i = 0; i < sizeof speaker->settings_; ++i) {
                data.push_back(((u8*)&speaker->settings_)[i]);
            }
        }
    }

    if (not data.size() == 0) {
        return flash_filesystem::store_file_data_binary(pfrm, path, data);
    } else {
        flash_filesystem::unlink_file(pfrm, path);
    }

    return true;
}



bool speaker_data_load(Platform& pfrm, Island& island, const char* path)
{
    Vector<char> data;

    auto bytes = flash_filesystem::read_file_data_binary(pfrm, path, data);

    if (bytes) {
        auto current = data.begin();
        while (current not_eq data.end()) {
            u8 coord = *current;
            const u8 x = coord >> 4;
            const u8 y = coord & 0x0f;
            ++current;

            // flash_filesystem library automatically appends a null byte to the
            // end of whatever you read from it.
            if (coord == 0) {
                break;
            }

            if (auto room = island.get_room({x, y})) {
                if (auto speaker = room->cast<Speaker>()) {

                    Speaker::EffectVector v;

                    for (u32 i = 0; i < sizeof v; ++i) {
                        if (current == data.end()) {
                            info(pfrm, "effect flags truncated!");
                            return false;
                        }

                        ((u8*)&v)[i] = *current;

                        ++current;
                    }

                    memcpy(
                        speaker->effect_flags().vector_.data(), &v, sizeof v);


                    Speaker::Settings settings;

                    for (u32 i = 0; i < sizeof settings; ++i) {
                        if (current == data.end()) {
                            info(pfrm, "effect flags truncated!");
                            return false;
                        }

                        ((u8*)&settings)[i] = *current;

                        ++current;
                    }

                    speaker->settings_ = settings;

                } else {
                    info(pfrm, "target not a speaker!");
                    return false;
                }
            } else {
                info(pfrm, "room dne while loading speaker!");
                return false;
            }
        }
    }
    return true;
}



bool synth_notes_store(Platform& pfrm, Island& island, const char* path)
{
    Vector<char> data;

    for (auto& room : island.rooms()) {
        if (auto synth = room->cast<Synth>()) {
            u8 coord = 0;
            coord |= (synth->position().x << 4);
            coord |= 0x0f & (synth->position().y);
            data.push_back(coord);

            for (int i = 0; i < 16; ++i) {
                static_assert(sizeof(*synth->notes()) == sizeof(u8));
                data.push_back(((u8*)synth->notes())[i]);
            }

            for (int i = 0; i < 16; ++i) {
                data.push_back(synth->effect_parameters()[i].value_);
            }
        }
    }

    if (not data.size() == 0) {
        return flash_filesystem::store_file_data_binary(pfrm, path, data);
    } else {
        flash_filesystem::unlink_file(pfrm, path);
    }

    return true;
}



bool synth_notes_load(Platform& pfrm, Island& island, const char* path)
{
    Vector<char> data;

    auto bytes = flash_filesystem::read_file_data_binary(pfrm, path, data);

    if (bytes) {
        auto current = data.begin();
        while (current not_eq data.end()) {
            u8 coord = *current;
            const u8 x = coord >> 4;
            const u8 y = coord & 0x0f;
            ++current;

            if (coord == 0) {
                break;
            }

            if (auto room = island.get_room({x, y})) {
                if (auto synth = room->cast<Synth>()) {
                    for (int i = 0; i < 16; ++i) {
                        if (current == data.end()) {
                            info(pfrm, "synth notes truncated");
                            return false;
                        }

                        static_assert(sizeof(*synth->notes()) == sizeof(u8));

                        u8 val = *current;

                        ((u8*)synth->notes())[i] = val;

                        ++current;
                    }
                    for (int i = 0; i < 16; ++i) {
                        if (current == data.end()) {
                            info(pfrm, "synth effects truncated");
                            return false;
                        }

                        u8 val = *current;
                        synth->effect_parameters()[i].value_ = val;

                        ++current;
                    }

                } else {
                    info(pfrm, "target not a synth!");
                    return false;
                }
            } else {
                info(pfrm, "room dne while loading synth");
                return false;
            }
        }
    }

    return true;
}



} // namespace skyland
