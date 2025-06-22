////////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2023 Evan Bowman
//
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at http://mozilla.org/MPL/2.0/. */
//
////////////////////////////////////////////////////////////////////////////////

#include "bridge.hpp"
#include "skyland/scene/worldScene.hpp"
#include "skyland/scene/readyScene.hpp"
#include "skyland/scene/inspectP2Scene.hpp"
#include "script/lisp.hpp"
#include "script/listBuilder.hpp"
#include "skyland/timeStreamEvent.hpp"



namespace skyland
{



class ResizeBridgeScene : public ActiveWorldScene
{
public:
    ResizeBridgeScene(bool near, const RoomCoord& coord)
        : bridge_loc_(coord),
          near_(near)
    {
    }


    void enter(Scene& prev) override
    {
        ActiveWorldScene::enter(prev);
        if (not near_) {
            far_camera();
        }

        Text::print(SYS_CSTR(resize_bridge), {0, 19});
    }


    void exit(Scene& prev) override
    {
        ActiveWorldScene::exit(prev);

        PLATFORM.fill_overlay(0);
    }


    ScenePtr update(Time delta) override
    {
        if (auto next = ActiveWorldScene::update(delta)) {
            return next;
        }

        auto parent = near_ ? &player_island() : opponent_island();
        if (not parent) {
            return make_scene<ReadyScene>();
        }

        if (APP.player().key_down(Key::action_2)) {
            if (near_) {
                return make_scene<ReadyScene>();
            } else {
                return make_scene<InspectP2Scene>();
            }
        }

        auto test_key = [&](Key k) {
            return APP.player().test_key(
                k, milliseconds(500), milliseconds(150));
        };


        if (test_key(Key::left)) {
            if (auto room = parent->get_room(bridge_loc_)) {
                if (auto bridge = room->cast<Bridge>()) {
                    if (bridge->resize(-1)) {
                        PLATFORM.speaker().play_sound("build0", 4);
                    } else {
                        PLATFORM.speaker().play_sound("beep_error", 4);
                    }
                }
            }
        } else if (test_key(Key::right)) {
            if (auto room = parent->get_room(bridge_loc_)) {
                if (auto bridge = room->cast<Bridge>()) {
                    if (bridge->resize(1)) {
                        PLATFORM.speaker().play_sound("build0", 4);
                    } else {
                        PLATFORM.speaker().play_sound("beep_error", 4);
                    }
                }
            }
        }

        return null_scene();
    }


    void display() override
    {
        Sprite cursor;

        auto parent = near_ ? &player_island() : opponent_island();
        if (not parent) {
            return;
        }

        auto draw_cursor = [&](auto cursor_loc, int x_off, int y_off) {
            auto origin = parent->visual_origin();

            origin.x += Fixnum::from_integer(cursor_loc.x * 16 + x_off);
            origin.y += Fixnum::from_integer(cursor_loc.y * 16 + y_off);

            cursor.set_position(origin);

            PLATFORM.screen().draw(cursor);
        };

        int cursor_anim_frame_ = 0;

        cursor.set_size(Sprite::Size::w16_h16);
        cursor.set_texture_index((52 * 2) + cursor_anim_frame_ * 4);
        draw_cursor(bridge_loc_, -4, -4);
        cursor.set_texture_index((52 * 2) + 1 + cursor_anim_frame_ * 4);
        draw_cursor(bridge_loc_, -4, 4);
        cursor.set_texture_index((52 * 2) + 2 + cursor_anim_frame_ * 4);
        auto extent = bridge_loc_;
        if (auto room = parent->get_room(bridge_loc_)) {
            extent.x += room->size().x - 1;
        }
        draw_cursor(extent, 4, -4);
        cursor.set_texture_index((52 * 2) + 3 + cursor_anim_frame_ * 4);
        draw_cursor(extent, 4, 4);

        ActiveWorldScene::display();
    }


private:
    RoomCoord bridge_loc_;
    bool near_;
};



ScenePtr Bridge::select_impl(const RoomCoord& cursor)
{
    // Unlike most rooms, the bridge shows inhabitants while viewing a
    // castle's exterior. If selecting a character, we want to show the
    // interior representation of the castle.
    if (not characters().empty() and not parent()->interior_visible()) {
        show_island_interior(parent());
    }

    auto ret = Room::do_select();
    if (ret) {
        return ret;
    }

    return make_scene<ResizeBridgeScene>(parent() == &player_island(),
                                         position());
}



lisp::Value* Bridge::serialize()
{
    lisp::ListBuilder builder;

    builder.push_back(L_SYM(name()));
    builder.push_back(L_INT(position().x));
    builder.push_back(L_INT(position().y));

    builder.push_back(L_INT(health()));
    builder.push_back(L_INT(Room::size().x));

    return builder.result();
}



void Bridge::deserialize(lisp::Value* list)
{
    if (lisp::length(list) >= 4) {
        __set_health(lisp::get_list(list, 3)->integer().value_);
    }

    if (lisp::length(list) >= 5) {
        adjust_width(lisp::get_list(list, 4)->integer().value_ - size().x);
    } else {
        // NOTE: bridges used to be fixed at width 2. For backwards
        // compatibility...
        adjust_width(2 - size().x);
    }
}



void Bridge::finalize()
{
    Room::finalize();

    time_stream::event::RoomWidthAdjusted e;
    e.room_x_ = position().x;
    e.room_y_ = position().y;
    e.prev_width_ = ((Room*)this)->size().x;
    e.near_ = parent() == &APP.player_island();
    APP.time_stream().push(APP.level_timer(), e);
}



}
