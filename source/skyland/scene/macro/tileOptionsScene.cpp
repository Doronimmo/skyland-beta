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


#include "tileOptionsScene.hpp"
#include "createBlockScene.hpp"
#include "selectorScene.hpp"
#include "skyland/scene_pool.hpp"
#include "skyland/skyland.hpp"



namespace skyland::macro
{



void TileOptionsScene::enter(Platform& pfrm,
                             macro::EngineImpl& state,
                             Scene& prev)
{
    MacrocosmScene::enter(pfrm, state, prev);
    collect_options(pfrm, state);
    show_options(pfrm, state);
}



void TileOptionsScene::exit(Platform& pfrm,
                            macro::EngineImpl& state,
                            Scene& next)
{
    MacrocosmScene::exit(pfrm, state, next);
    text_.reset();

    const auto st = calc_screen_tiles(pfrm);
    for (int y = st.y - 8; y < st.y; ++y) {
        for (int x = 0; x < 32; ++x) {
            pfrm.set_tile(Layer::overlay, x, y, 0);
        }
    }
}



struct TileOptionsScene::OptionInfo
{
    SystemString name_;
    int sel_icon_;
    int unsel_icon_;
    void (*render_cost_)(Platform&, macro::EngineImpl&, terrain::Type, Text&);
    ScenePtr<Scene> (*next_)(MacrocosmScene&, macro::EngineImpl&);
};



void render_cost(Platform& pfrm,
                 macro::EngineImpl& state,
                 terrain::Type t,
                 Text& text,
                 bool harvest);



static const TileOptionsScene::OptionInfo options[] = {
    {SystemString::macro_create_block,
     2568,
     2584,
     [](Platform&, macro::EngineImpl&, terrain::Type, Text&)
     {
     },
     [](MacrocosmScene& s, macro::EngineImpl& state) -> ScenePtr<Scene> {
         return scene_pool::alloc<CreateBlockScene>();
     }},
    {SystemString::macro_build_improvement,
     2520,
     2536,
     [](Platform&, macro::EngineImpl&, terrain::Type, Text&)
     {
     },
     [](MacrocosmScene& s, macro::EngineImpl& state) -> ScenePtr<Scene> {
         return scene_pool::alloc<BuildImprovementScene>();
     }},
    {SystemString::macro_demolish,
     2600,
     2616,
     [](Platform& pfrm, macro::EngineImpl& state, terrain::Type t, Text& text)
     {
         render_cost(pfrm, state, t, text, true);
     },
     [](MacrocosmScene& s, macro::EngineImpl& state) -> ScenePtr<Scene> {
         auto c = state.sector().cursor();
         c.z--;
         auto t = state.sector().get_block(c).type();
         auto [cost, nt] = harvest(t);
         if (cost.productivity_ > state.sector().productivity()) {
             Platform::instance().speaker().play_sound("beep_error", 2);
             return scene_pool::alloc<TileOptionsScene>();
         }
         auto prod = state.sector().productivity();
         prod -= cost.productivity_;
         state.sector().set_productivity(prod);
         auto& p = state.data_->p();
         p.stone_.set(p.stone_.get() + cost.stone_);
         p.lumber_.set(p.lumber_.get() + cost.lumber_);
         p.marble_.set(p.marble_.get() + cost.marble_);
         p.crystal_.set(p.crystal_.get() + cost.crystal_);
         p.food_.set(p.food_.get() + cost.food_);
         p.water_.set(p.water_.get() + cost.water_);
         state.sector().set_block(c, nt);
         state.sector().remove_export(c);
         s.update_ui(state);
         return scene_pool::alloc<SelectorScene>();
     }},
    {SystemString::macro_export,
     776,
     760,
     [](Platform&, macro::EngineImpl&, terrain::Type, Text&)
     {
     },
     [](MacrocosmScene& s, macro::EngineImpl& state) -> ScenePtr<Scene> {
         return scene_pool::alloc<ConfigurePortScene>();
     }}};


ScenePtr<Scene> TileOptionsScene::update(Platform& pfrm,
                                         Player& player,
                                         macro::EngineImpl& state)
{
    if (auto scene = MacrocosmScene::update(pfrm, player, state)) {
        return scene;
    }

    if (state.sector().cursor().z == 0) {
        // If z == 0, no tiles beneath, so you can't improve or remove a
        // block. Go directly to the block creation menu.
        last_option_ = &options[0];
        return scene_pool::alloc<CreateBlockScene>();
    } else {
        auto c = state.sector().cursor();
        --c.z;
        auto& beneath = state.sector().get_block(c);
        if (beneath.type() == terrain::Type::air) {
            last_option_ = &options[0];
            return scene_pool::alloc<CreateBlockScene>();
        }
    }

    if (player.key_down(pfrm, Key::action_1)) {
        pfrm.speaker().play_sound("button_wooden", 3);
        auto next = options_[selector_]->next_(*this, state);
        last_option_ = options_[selector_];
        return next;
    }

    if (player.key_down(pfrm, Key::action_2)) {
        return scene_pool::alloc<SelectorScene>();
    }

    if (player.key_down(pfrm, Key::left)) {
        if (selector_ == 0) {
            selector_ = options_.size() - 1;
        } else {
            --selector_;
        }
        show_options(pfrm, state);
        pfrm.speaker().play_sound("click", 1);
    }

    if (player.key_down(pfrm, Key::right)) {
        ++selector_;
        selector_ %= options_.size();
        show_options(pfrm, state);
        pfrm.speaker().play_sound("click", 1);
    }

    return null_scene();
}



const TileOptionsScene::OptionInfo* TileOptionsScene::last_option_ =
    &options[1];



void TileOptionsScene::collect_options(Platform& pfrm, macro::EngineImpl& state)
{

    auto c = state.sector().cursor();
    if (c.z == 0) {
        return;
    }
    --c.z;
    auto& block = state.sector().get_block(c);
    auto improvements = terrain::improvements((terrain::Type)block.type_);
    if (not improvements.empty()) {
        options_.push_back(&options[1]);
    }

    options_.push_back(&options[0]);

    options_.push_back(&options[2]);

    for (u32 i = 0; i < options_.size(); ++i) {
        if (options_[i] == last_option_) {
            selector_ = i;
        }
    }
}



void TileOptionsScene::show_options(Platform& pfrm, macro::EngineImpl& state)
{
    if (options_.empty()) {
        return;
    }

    auto st = calc_screen_tiles(pfrm);

    StringBuffer<32> str = loadstr(pfrm, options_[selector_]->name_)->c_str();
    msg(pfrm, state, str.c_str());

    for (int y = st.y - 5; y < st.y - 2; ++y) {
        pfrm.set_tile(Layer::overlay, st.x - 22, y, 128);
        pfrm.set_tile(Layer::overlay, st.x - 9, y, 433);
    }

    pfrm.set_tile(Layer::overlay, st.x - 22, st.y - 2, 419);
    pfrm.set_tile(Layer::overlay, st.x - 9, st.y - 2, 418);

    pfrm.load_overlay_chunk(
        258, options_[(selector_ + 1) % options_.size()]->unsel_icon_, 16);

    int sel = selector_;
    if (sel - 1 < 0) {
        sel = options_.size() - 1;
    } else {
        sel -= 1;
    }
    pfrm.load_overlay_chunk(181, options_[sel]->unsel_icon_, 16);

    pfrm.load_overlay_chunk(
        197, options_[(selector_) % options_.size()]->sel_icon_, 16);

    for (int i = st.x - 21; i < st.x - 9; ++i) {
        pfrm.set_tile(Layer::overlay, i, st.y - 6, 425);
    }

    draw_image(pfrm, 181, st.x - 21, st.y - 5, 4, 4, Layer::overlay);
    draw_image(pfrm, 197, st.x - 17, st.y - 5, 4, 4, Layer::overlay);
    draw_image(pfrm, 258, st.x - 13, st.y - 5, 4, 4, Layer::overlay);
}



void TileOptionsScene::msg(Platform& pfrm,
                           macro::EngineImpl& state,
                           const char* text)
{
    auto st = calc_screen_tiles(pfrm);
    text_.emplace(pfrm, text, OverlayCoord{0, u8(st.y - 1)});

    if (state.sector().cursor().z > 0) {
        auto c = state.sector().cursor();
        --c.z;
        auto t = state.sector().get_block(c).type();
        options_[selector_]->render_cost_(pfrm, state, t, *text_);
    }


    const int count = st.x - text_->len();
    for (int i = 0; i < count; ++i) {
        pfrm.set_tile(Layer::overlay, i + text_->len(), st.y - 1, 426);
    }

    for (int i = 0; i < st.x; ++i) {
        pfrm.set_tile(Layer::overlay, i, st.y - 2, 425);
        pfrm.set_tile(Layer::overlay, i, st.y - 3, 0);
        pfrm.set_tile(Layer::overlay, i, st.y - 4, 0);
        pfrm.set_tile(Layer::overlay, i, st.y - 5, 0);

        pfrm.set_tile(Layer::overlay, i, st.y - 6, 0);
    }
}



} // namespace skyland::macro
