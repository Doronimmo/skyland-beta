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


#include "keyComboScene.hpp"
#include "selectorScene.hpp"
#include "skyland/keyCallbackProcessor.hpp"
#include "skyland/skyland.hpp"
#include "skyland/systemString.hpp"



namespace skyland::macro
{



void KeyComboScene::enter(Scene& prev)
{
    text_data_ = SYSTR(key_combo_prompt)->c_str();

    text_.emplace(text_data_.c_str(), OverlayCoord{0, 19});
    for (int i = text_->len(); i < 30; ++i) {
        text_->append(" ");
    }

    for (int i = 0; i < 30; ++i) {
        PLATFORM.set_tile(Layer::overlay, i, 18, 425);
    }

    APP.key_callback_processor().reset();
}



void KeyComboScene::exit(Scene& next)
{
    PLATFORM.fill_overlay(0);
    text_.reset();
}



ScenePtr<Scene> KeyComboScene::update(Microseconds delta)
{
    if (APP.player().key_down(Key::start) or
        APP.key_callback_processor().seek_state() ==
            KeyCallbackProcessor::seq_max - 1 or
        (APP.key_callback_processor().possibilities() == 1 and
         APP.key_callback_processor().match())) {

        if (auto binding = APP.key_callback_processor().match()) {
            binding->callback_();
        }
        APP.key_callback_processor().reset();


        return scene_pool::alloc<SelectorScene>();
    }

    APP.key_callback_processor().update();

    Key found = Key::count;
    for (int i = 0; i < (int)Key::count; ++i) {
        auto k = (Key)i;
        if (PLATFORM.keyboard().down_transition(k)) {
            found = k;
            break;
        }
    }

    if (found not_eq Key::count and found not_eq Key::alt_1) {

        text_data_ += [found] {
            switch (found) {
            case Key::left:
                return "l-";

            case Key::right:
                return "r-";

            case Key::up:
                return "u-";

            case Key::down:
                return "d-";

            case Key::action_1:
                return "a-";

            case Key::action_2:
                return "b-";

            default:
                return "err";
            }
        }();

        text_->assign(text_data_.c_str());

        for (int i = text_->len(); i < 30; ++i) {
            text_->append(" ");
        }
    }

    return null_scene();
}



} // namespace skyland::macro
