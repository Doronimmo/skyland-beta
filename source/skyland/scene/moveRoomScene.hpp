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


#pragma once


#include "constructionScene.hpp"
#include "readyScene.hpp"
#include "skyland/island.hpp"
#include "skyland/player/player.hpp"
#include "skyland/scene_pool.hpp"
#include "skyland/skyland.hpp"
#include "skyland/systemString.hpp"
#include "worldScene.hpp"



namespace skyland
{



class MoveRoomScene : public ActiveWorldScene
{
public:
    void exit(Platform& pfrm, App& app, Scene& prev) override
    {
        ActiveWorldScene::exit(pfrm, app, prev);

        text_.reset();
        no_text_.reset();
        yes_text_.reset();

        pfrm.fill_overlay(0);
    }


    ScenePtr<Scene>
    update(Platform& pfrm, App& app, Microseconds delta) override
    {
        if (auto next = ActiveWorldScene::update(pfrm, app, delta)) {
            return next;
        }

        if (app.game_speed() not_eq GameSpeed::stopped) {
            set_gamespeed(pfrm, app, GameSpeed::stopped);
        }

        switch (state_) {
        case State::setup_prompt: {
            auto st = calc_screen_tiles(pfrm);
            StringBuffer<30> text;
            text += format(SYSTR(move_room_prompt)->c_str(), 800).c_str();

            text_.emplace(pfrm, text.c_str(), OverlayCoord{0, u8(st.y - 1)});

            const int count = st.x - text_->len();
            for (int i = 0; i < count; ++i) {
                pfrm.set_tile(Layer::overlay, i + text_->len(), st.y - 1, 426);
            }

            for (int i = 0; i < st.x; ++i) {
                pfrm.set_tile(Layer::overlay, i, st.y - 2, 425);
            }

            yes_text_.emplace(pfrm, OverlayCoord{u8(st.x - 7), u8(st.y - 3)});
            no_text_.emplace(pfrm, OverlayCoord{u8(st.x - 7), u8(st.y - 2)});

            yes_text_->assign(SYSTR(salvage_option_A)->c_str());
            no_text_->assign(SYSTR(salvage_option_B)->c_str());

            for (int i = 23; i < st.x; ++i) {
                pfrm.set_tile(Layer::overlay, i, st.y - 4, 425);
            }

            pfrm.set_tile(Layer::overlay, st.x - 8, st.y - 2, 419);
            pfrm.set_tile(Layer::overlay, st.x - 8, st.y - 3, 128);

            state_ = State::prompt;
            persist_ui();
            break;
        }

        case State::prompt:
            if (player(app).key_down(pfrm, Key::action_2)) {
                return scene_pool::alloc<ReadyScene>();
            }
            if (player(app).key_down(pfrm, Key::action_1)) {
                unpersist_ui();
                pfrm.speaker().play_sound("coin", 2);
                app.set_coins(pfrm, app.coins() - 800);
                state_ = State::move_stuff;
                yes_text_.reset();
                no_text_.reset();
                text_.reset();
                pfrm.fill_overlay(0);

                auto st = calc_screen_tiles(pfrm);

                text_.emplace(pfrm,
                              SYSTR(move_room_1)->c_str(),
                              OverlayCoord{0, u8(st.y - 1)});

                for (int i = 0; i < text_->len(); ++i) {
                    pfrm.set_tile(Layer::overlay, i, st.y - 2, 425);
                }
            }
            break;

        case State::move_stuff:
            if (player(app).key_down(pfrm, Key::action_2)) {
                return scene_pool::alloc<ReadyScene>();
            }
            if (player(app).key_down(pfrm, Key::action_1)) {
                auto cursor_loc = globals().near_cursor_loc_;
                if (auto r = app.player_island().get_room(cursor_loc)) {
                    if (str_eq(r->name(), "mycelium")) {
                        // Can't be moved!
                        pfrm.speaker().play_sound("beep_error", 3);
                        return null_scene();
                    }
                    state_ = State::move_block;
                    move_src_ = r->position();
                    move_diff_ =
                        (cursor_loc.cast<int>() - move_src_.cast<int>())
                            .cast<u8>();
                    text_.reset();
                    pfrm.fill_overlay(0);

                    mv_size_.x = r->size().x;
                    mv_size_.y = r->size().y;

                    auto st = calc_screen_tiles(pfrm);

                    text_.emplace(pfrm,
                                  SYSTR(move_room_2)->c_str(),
                                  OverlayCoord{0, u8(st.y - 1)});
                    for (int i = 0; i < text_->len(); ++i) {
                        pfrm.set_tile(Layer::overlay, i, st.y - 2, 425);
                    }
                }
            }
            break;

        case State::move_block:
            if (player(app).key_down(pfrm, Key::action_1) or
                player(app).key_down(pfrm, Key::action_2)) {

                auto cursor_loc = globals().near_cursor_loc_;
                cursor_loc = (cursor_loc.cast<int>() - move_diff_.cast<int>())
                                 .cast<u8>();

                if (player(app).key_down(pfrm, Key::action_1)) {
                    if (auto room = app.player_island().get_room(move_src_)) {
                        for (u32 x = 0; x < room->size().x; ++x) {
                            for (int y = 0; y < room->size().y; ++y) {
                                RoomCoord c;
                                c.x = cursor_loc.x + x;
                                c.y = cursor_loc.y + y;

                                auto err = [&]() {
                                    pfrm.speaker().play_sound("beep_error", 3);
                                    return null_scene();
                                };

                                if (auto d = app.player_island().get_drone(c)) {
                                    return err();
                                }
                                if (c.x >=
                                        app.player_island().terrain().size() or
                                    c.y >= 15 or
                                    c.y < construction_zone_min_y) {
                                    return err();
                                }
                                if (auto o = app.player_island().get_room(c)) {
                                    if (o not_eq room) {
                                        return err();
                                    }
                                }
                            }
                        }
                        pfrm.speaker().play_sound("build0", 4);
                        app.player_island().move_room(
                            pfrm, app, move_src_, cursor_loc);
                    }
                }

                text_.reset();
                pfrm.fill_overlay(0);

                auto st = calc_screen_tiles(pfrm);

                text_.emplace(pfrm,
                              SYSTR(move_room_1)->c_str(),
                              OverlayCoord{0, u8(st.y - 1)});

                for (int i = 0; i < text_->len(); ++i) {
                    pfrm.set_tile(Layer::overlay, i, st.y - 2, 425);
                }

                state_ = State::move_stuff;
            }
            break;
        }

        auto test_key = [&](Key k) {
            return app.player().test_key(
                pfrm, k, milliseconds(500), milliseconds(100));
        };

        auto& cursor_loc = globals().near_cursor_loc_;

        auto sync_cursor = [&] {
            app.player().network_sync_cursor(
                pfrm, cursor_loc, cursor_anim_frame_, true);
        };

        if ((int)state_ > (int)State::prompt) {
            if (test_key(Key::left)) {
                if (cursor_loc.x > 0) {
                    --cursor_loc.x;
                    sync_cursor();
                    pfrm.speaker().play_sound("cursor_tick", 0);
                }
            } else if (test_key(Key::right)) {
                if (cursor_loc.x < app.player_island().terrain().size()) {
                    ++cursor_loc.x;
                    sync_cursor();
                    pfrm.speaker().play_sound("cursor_tick", 0);
                }
            }

            if (test_key(Key::up)) {
                if (cursor_loc.y > construction_zone_min_y) {
                    --cursor_loc.y;
                    sync_cursor();
                    pfrm.speaker().play_sound("cursor_tick", 0);
                }
            } else if (test_key(Key::down)) {
                if (cursor_loc.y < 14) {
                    ++cursor_loc.y;
                    sync_cursor();
                    pfrm.speaker().play_sound("cursor_tick", 0);
                }
            }

            cursor_anim_timer_ += delta;
            if (cursor_anim_timer_ > milliseconds(200)) {
                cursor_anim_timer_ -= milliseconds(200);
                cursor_anim_frame_ = not cursor_anim_frame_;
                sync_cursor();
            }
        }
        return null_scene();
    }


    void display(Platform& pfrm, App& app) override
    {
        if ((int)state_ > (int)State::prompt) {
            Sprite cursor;
            cursor.set_size(Sprite::Size::w16_h32);
            if (state_ == State::move_block) {
                cursor.set_texture_index(110);
            } else {
                cursor.set_texture_index(15 + cursor_anim_frame_);
            }

            auto origin = app.player_island().visual_origin();

            auto& cursor_loc = globals().near_cursor_loc_;

            origin.x += cursor_loc.x * 16;
            origin.y += cursor_loc.y * 16;

            cursor.set_position(origin);


            pfrm.screen().draw(cursor);
        }

        if (state_ == State::move_block) {

            auto origin = player_island(app).visual_origin();
            auto loc =
                (move_src_.cast<int>() + move_diff_.cast<int>()).cast<u8>();
            origin.x += loc.x * 16;
            origin.y += loc.y * 16;

            Sprite sprite;
            sprite.set_position(origin);
            sprite.set_texture_index(15 + cursor_anim_frame_);
            sprite.set_size(Sprite::Size::w16_h32);
            pfrm.screen().draw(sprite);

            origin = player_island(app).visual_origin();
            auto cursor_loc = globals().near_cursor_loc_;
            cursor_loc =
                (cursor_loc.cast<int>() - move_diff_.cast<int>()).cast<u8>();
            origin.x += cursor_loc.x * 16;
            origin.y += cursor_loc.y * 16;
            auto sz = mv_size_;
            if (sz.x == 1 and sz.y == 1) {
                Sprite sprite;
                sprite.set_texture_index(14);
                sprite.set_size(Sprite::Size::w16_h32);
                sprite.set_position({origin.x, origin.y - 16});
                pfrm.screen().draw(sprite);
            } else if (sz.x == 2 and sz.y == 1) {
                Sprite sprite;
                sprite.set_texture_index(14);
                sprite.set_size(Sprite::Size::w16_h32);
                sprite.set_position({origin.x, origin.y - 16});
                pfrm.screen().draw(sprite);
                sprite.set_position({origin.x + 16, origin.y - 16});
                pfrm.screen().draw(sprite);
            } else {
                Sprite sprite;
                sprite.set_texture_index(13);
                sprite.set_size(Sprite::Size::w16_h32);

                for (int x = 0; x < sz.x; ++x) {
                    for (int y = 0; y < sz.y / 2; ++y) {
                        sprite.set_position(
                            {origin.x + x * 16, origin.y + y * 32});
                        pfrm.screen().draw(sprite);
                    }
                }

                if (sz.y % 2 not_eq 0) {
                    // Odd sized room in y direction. Draw bottom row:
                    sprite.set_texture_index(14);
                    for (int x = 0; x < sz.x; ++x) {
                        sprite.set_position(
                            {origin.x + x * 16, origin.y + (sz.y - 2) * 16});
                        pfrm.screen().draw(sprite);
                    }
                }
            }
        }

        if ((int)state_ > (int)State::prompt) {
            auto& cursor_loc = globals().near_cursor_loc_;
            if (auto room = app.player_island().get_room(cursor_loc)) {
                room->display_on_hover(pfrm.screen(), app, cursor_loc);
            }
        }

        ActiveWorldScene::display(pfrm, app);
    }


private:
    enum class State {
        setup_prompt,
        prompt,
        move_stuff,
        move_block,
    } state_ = State::setup_prompt;

    Vec2<u8> move_diff_;
    u8 cursor_anim_frame_;
    Microseconds cursor_anim_timer_;

    std::optional<Text> text_;
    std::optional<Text> yes_text_;
    std::optional<Text> no_text_;

    RoomCoord move_src_;
    Vec2<u8> mv_size_;
};



} // namespace skyland