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


#include "macrocosmSector.hpp"
#include "macrocosmEngine.hpp"



namespace skyland::macro
{



terrain::Sector::Sector(Vec2<s8> position, Shape shape)
{
    p_.shape_ = shape;

    // FIXME: derived class should set the size, of course.
    switch (p_.shape_) {
    case Shape::cube:
        size_ = {8, 8, 9};
        break;

    case Shape::pancake:
        size_ = {12, 12, 4};
        break;

    case Shape::pillar:
        size_ = {6, 6, 16};
    }

    set_name("");

    p_.x_ = position.x;
    p_.y_ = position.y;

    set_population(4);
}



fiscal::Ledger terrain::Sector::budget() const
{
    fiscal::Ledger result;

    auto st = stats();

    int productive_population = population();
    int unproductive_population = 0;

    if (st.housing_ < population()) {
        // Homeless people are less economically productive? Sounds cynical, but
        // probably true.
        productive_population = st.housing_;
        unproductive_population = population() - st.housing_;
    }

    int employed_population = productive_population;
    int unemployed_population = 0;
    if (st.employment_ < employed_population) {
        unemployed_population = productive_population - st.employment_;
        employed_population = productive_population - unemployed_population;
    }

    auto& pfrm = Platform::instance();

    if (employed_population) {
        result.add_entry(SYSTR(macro_fiscal_employed)->c_str(),
                         employed_population * 0.3f);
    }

    if (unemployed_population) {
        result.add_entry(SYSTR(macro_fiscal_unemployed)->c_str(),
                         unemployed_population * 0.1f);
    }

    if (unproductive_population) {
        result.add_entry(SYSTR(macro_fiscal_homelessness)->c_str(),
                         -unproductive_population * 0.4f);
    }

    if (st.food_ < population() / food_consumption_factor) {
        result.add_entry(
            SYSTR(macro_fiscal_starvation)->c_str(),
            -0.4f * (population() - st.food_ * food_consumption_factor));
    }


    for (auto& c : st.commodities_) {

        if (c.type_ == Commodity::Type::food) {
            Platform::fatal("special case food commodity, unsupported in "
                            "commodity list");
        }

        int count = 0;
        Float accum = 0;
        Float value = c.value(c.type_);
        for (int i = 0; i < c.supply_; ++i) {
            accum += value;
            value *= 0.85f; // Diminishing returns
            ++count;
        }

        fiscal::LineItem::Label l;
        l += loadstr(pfrm, c.name())->c_str();
        l += " (";
        l += stringify(count);
        l += ")";

        result.add_entry(l, accum);
    }

    return result;
}



u16 terrain::Sector::quantity_non_exported(Commodity::Type t)
{
    auto s = base_stats();
    int total = 0;

    if (t == Commodity::Type::food) {
        total = s.food_;
    } else {
        for (auto& c : s.commodities_) {
            if (c.type_ == t) {
                total = c.supply_;
                break;
            }
        }
    }


    for (auto& e : exports()) {
        if (e.c == t) {
            total -= e.export_supply_.get();
        }
    }

    return std::max(0, total);
}



void terrain::Sector::repaint()
{
    set_repaint(true);


    raster::globalstate::_changed = true;
    raster::globalstate::_shrunk = true;
}



Coins terrain::Sector::coin_yield() const
{
    Coins result = 0;

    auto values = budget();
    auto current = values.entries();

    while (current) {
        result += current->contribution_;
        current = current->next_;
    }

    if (result <= 0) {
        result = 1;
    }

    return result;
}



const terrain::Sector::Exports& terrain::Sector::exports() const
{
    return exports_;
}



void terrain::Sector::set_export(const ExportInfo& e)
{
    remove_export(e.source_coord_);

    auto& block = get_block(e.source_coord_);
    if (block.type() not_eq Type::port) {
        return;
    }

    if (not exports_.full()) {
        exports_.emplace_back();
        memcpy(&exports_.back(), &e, sizeof e);
    }
}



void terrain::Sector::remove_export(Vec3<u8> source_coord)
{
    for (auto it = exports_.begin(); it not_eq exports_.end();) {
        if (it->source_coord_ == source_coord) {
            it = exports_.erase(it);
            return;
        } else {
            ++it;
        }
    }
}



void terrain::Sector::set_name(const StringBuffer<name_len - 1>& name)
{
    memset(p_.name_, '\0', name_len);

    auto out = p_.name_;
    for (char c : name) {
        *(out++) = c;
    }
}



StringBuffer<terrain::Sector::name_len - 1> terrain::Sector::name()
{
    auto np = p_.name_;

    StringBuffer<terrain::Sector::name_len - 1> result;

    while (*np not_eq '\0') {
        result.push_back(*(np++));
    }

    return result;
}



terrain::Sector::Population terrain::Sector::population() const
{
    terrain::Sector::Population p;
    memcpy(&p, p_.population_packed_, sizeof p);
    return p;
}



void terrain::Sector::set_population(Population p)
{
    memcpy(p_.population_packed_, &p, sizeof p);
}



Vec2<s8> terrain::Sector::coordinate() const
{
    return {p_.x_, p_.y_};
}



void terrain::Sector::advance(int years)
{
    set_population(population() + population_growth_rate() * years);
}



void add_supply(terrain::Stats& s, terrain::Commodity::Type t, int supply)
{
    if (t == terrain::Commodity::Type::food) {
        s.food_ += supply;
        return;
    }

    for (auto& c : s.commodities_) {
        if (c.type_ == t) {
            c.supply_ += supply;
            return;
        }
    }

    s.commodities_.push_back({t, (u16)supply, false});
}



int remove_supply(terrain::Stats& s, terrain::Commodity::Type t, int supply)
{
    if (t == terrain::Commodity::Type::food) {
        auto remove_amount = std::min((int)s.food_, supply);
        s.food_ -= remove_amount;
        s.food_exports_ += remove_amount;
        return remove_amount;
    }

    for (auto& c : s.commodities_) {
        if (c.type_ == t) {
            auto remove_amount = std::min((int)c.supply_, supply);
            c.supply_ -= remove_amount;
            if (c.supply_ == 0) {
                s.commodities_.erase(&c);
            }
            return remove_amount;
        }
    }

    return 0;
}



static void intersector_exchange_commodities(const Vec2<s8> source_sector,
                                             terrain::Stats& stat)
{
    Buffer<std::pair<const Vec2<s8>, terrain::Stats>, State::max_sectors - 1>
        stats;

    State& state = *_bound_state;

    if (state.data_->origin_sector_.coordinate() == source_sector) {
        stats.push_back({source_sector, stat});
    } else {
        stats.push_back({state.data_->origin_sector_.coordinate(),
                         state.data_->origin_sector_.base_stats()});
    }

    for (auto& sector : state.data_->other_sectors_) {
        if (sector->coordinate() == source_sector) {
            stats.push_back({source_sector, stat});
        } else {
            stats.push_back({sector->coordinate(), sector->base_stats()});
        }
    }


    for (auto& st : stats) {
        if (auto src_sector = state.load_sector(st.first)) {
            for (auto& exp : src_sector->exports()) {

                auto supply = exp.export_supply_.get();

                auto exported = remove_supply(st.second, exp.c, supply);

                for (auto& target : stats) {
                    if (target.first == exp.destination_) {
                        add_supply(target.second, exp.c, exported);
                        break;
                    }
                }
            }
        }
    }


    for (auto& data : stats) {
        if (data.first == source_sector) {
            stat = data.second;
            return;
        }
    }
}



terrain::Stats terrain::Sector::stats() const
{
    auto result = base_stats();

    intersector_exchange_commodities(coordinate(), result);

    return result;
}


terrain::Stats terrain::Sector::base_stats() const
{
    if (base_stats_cache_) {
        return *base_stats_cache_;
    }

    terrain::Stats result;

    for (int z = 0; z < size_.z - 1; ++z) {
        for (int x = 0; x < 8; ++x) {
            for (int y = 0; y < 8; ++y) {
                if (get_block({(u8)x, (u8)y, (u8)z}).type() == Type::air) {
                    continue;
                }

                auto block_stats = get_block({(u8)x, (u8)y, (u8)z}).stats();

                // NOTE: if a block is covered, then it's not possible to
                // harvest the supplied food, so a stacked block should yield
                // zero food.
                if (get_block({(u8)x, (u8)y, (u8)(z + 1)}).type() ==
                        Type::air or
                    get_block({(u8)x, (u8)y, (u8)(z + 1)}).type() ==
                        Type::selector) {
                    result.food_ += block_stats.food_;
                }

                result.employment_ += block_stats.employment_;

                result.housing_ += block_stats.housing_;

                for (auto& c : block_stats.commodities_) {
                    bool existing = false;
                    for (auto& e : result.commodities_) {
                        if (e.type_ == c.type_) {
                            e.supply_ += c.supply_;
                            existing = true;
                            break;
                        }
                    }

                    if (not existing) {
                        result.commodities_.push_back(c);
                    }
                }
            }
        }
    }

    base_stats_cache_ = result;

    return result;
}



Float terrain::Sector::population_growth_rate() const
{
    auto s = stats();

    auto required_food = population() / food_consumption_factor;

    Float result = 0.f;


    if (s.food_ >= required_food) {
        result = 0.1f * (s.food_ - required_food);
    } else {
        result = -0.5f * (required_food - s.food_);
    }

    if (population() > s.housing_) {
        result -= 0.025f * (population() - s.housing_);
    } else {
        result += 0.025f * (s.housing_ - population());
    }


    return result;
}



u16 terrain::Sector::cursor_raster_pos() const
{
    int min = 9999;
    for (auto p : raster::globalstate::_cursor_raster_tiles) {
        if (p < min) {
            min = p;
        }
    }

    return min;
}



void terrain::Sector::set_block(const Vec3<u8>& coord, Type type)
{
    auto& selected = ref_block({(u8)coord.x, (u8)coord.y, (u8)coord.z});

    const auto prev_type = selected.type();

    if (prev_type == type) {
        return;
    }

    if (type == Type::selector) {
        selected.data_ = 16;
    } else {
        selected.data_ = 0;
    }

    selected.type_ = (u8)type;
    selected.repaint_ = true;

    if ((prev_type not_eq Type::air and prev_type not_eq Type::selector and
         type == Type::air) or
        (type not_eq Type::selector and type not_eq Type::air)) {
        for (int z = coord.z - 1; z > -1; --z) {
            auto& selected = ref_block({coord.x, coord.y, (u8)z});
            if (selected.type_ not_eq 0 and not selected.shadowed_) {
                selected.repaint_ = true;
            }
        }
    }

    if (type not_eq Type::air and type not_eq Type::selector) {
        raster::globalstate::_grew = true;
    }

    if (type not_eq Type::selector) {
        clear_cache();
    }


    raster::globalstate::_recast_shadows = true;


    if (prev_type == Type::light_source or type == Type::light_source or
        (type == Type::air and prev_type not_eq Type::selector)) {

        if (type == Type::air) {
            raster::globalstate::_shrunk = true;
        }


        // Lighting pattern changed, or block removed (assigned as air), just
        // redraw everything.

        set_repaint(true);
    }

    raster::globalstate::_changed = true;
}



void terrain::Sector::set_cursor(const Vec3<u8>& pos, bool lock_to_floor)
{
    if (pos.z >= size().z or pos.x >= size().x or pos.y >= size().y) {
        Platform::fatal(format("set cursor to out of bounds position"
                               " % % %, % % %",
                               pos.x,
                               pos.y,
                               pos.z,
                               size().x,
                               size().y,
                               size().z)
                            .c_str());
    }

    auto old_cursor = p_.cursor_;
    auto& block = ref_block(old_cursor);
    if (block.type_ == (u8)terrain::Type::selector) {
        set_block(old_cursor, macro::terrain::Type::air);
    }

    p_.cursor_ = pos;

    if (lock_to_floor) {
        while (ref_block(p_.cursor_).type() not_eq terrain::Type::air) {
            ++p_.cursor_.z;
        }

        raster::globalstate::_cursor_moved = true;

        while (p_.cursor_.z > 0 and
               ref_block({p_.cursor_.x, p_.cursor_.y, (u8)(p_.cursor_.z - 1)})
                       .type() == terrain::Type::air) {
            --p_.cursor_.z;
        }
    }

    if (not(p_.cursor_ == old_cursor)) {
        raster::globalstate::_cursor_moved = true;
    }

    if (p_.cursor_.z >= z_view_) {
        set_z_view(p_.cursor_.z + 1);
        raster::globalstate::_cursor_moved = false;
    }

    set_block(p_.cursor_, terrain::Type::selector);
}



bool terrain::Sector::set_z_view(u8 z_view)
{
    // auto c = cursor();
    // if (z_view <= c.z) {
    //     return false;
    // }

    if (z_view > size_.z) {
        z_view_ = size_.z;
        return false;
    } else {
        z_view_ = z_view;
    }

    raster::globalstate::_changed = true;
    raster::globalstate::_shrunk = true;


    set_repaint(true);


    return true;
}



void terrain::Sector::clear_cache()
{
    base_stats_cache_.reset();
}



void terrain::Sector::render(Platform& pfrm)
{
    using namespace raster;

    // #define RASTER_DEBUG_ENABLE

#ifdef RASTER_DEBUG_ENABLE
#define RASTER_DEBUG()                                                         \
    do {                                                                       \
        pfrm.sleep(20);                                                        \
    } while (false)
#else
#define RASTER_DEBUG()                                                         \
    do {                                                                       \
    } while (false)
#endif

    auto flush_stack_t0 = [&pfrm](auto& stack, int i) {
        // The first tile can be drawn much faster, as we don't care what's
        // currently onscreen.
        bool overwrite = true;

        while (not stack.empty()) {
            int tile = stack.back();
            RASTER_DEBUG();
            pfrm.blit_t0_tile_to_texture(tile + 480, i, overwrite);
            stack.pop_back();
            overwrite = false;
        }
    };


    auto flush_stack_t1 = [&pfrm](auto& stack, int i) {
        bool overwrite = true;

        while (not stack.empty()) {
            int tile = stack.back();
            RASTER_DEBUG();
            pfrm.blit_t1_tile_to_texture(tile + 480, i, overwrite);
            stack.pop_back();
            overwrite = false;
        }
    };


    if (not globalstate::_changed_cursor_flicker_only and
        not globalstate::_changed) {
        return;
    }

    if (globalstate::_changed_cursor_flicker_only and
        not globalstate::_changed) {

        // Optimized rendering for the flickering cursor. No need to perform
        // depth testing and all that stuff just to repaint the cursor tiles!
        // Use a cached copy.

        for (u32 i = 0; i < globalstate::_cursor_raster_tiles.size(); ++i) {
            auto t = globalstate::_cursor_raster_tiles[i];
            auto& stk = globalstate::_cursor_raster_stack[i];
            u32 j = 0;
            for (; j < stk.size(); ++j) {
                // Flickering implementation, manually offset the tiles between
                // light and dark if part of the cursor tile range.
                if (stk[j] > 59 and stk[j] < 66) {
                    stk[j] += 6;
                    break;
                } else if (stk[j] > 65 and stk[j] < 72) {
                    stk[j] -= 6;
                    break;
                }
            }
            if (j == stk.size()) {
                // The rendering stack does not contain the cursor tile in the
                // first place. Skip.
                continue;
            } else if (j == 0 and not stk.empty()) {
                // If the cursor tile sits at the bottom of the rendering stack,
                // it will be drawn last. Therefore, we don't need to worry
                // about overlapping tiles on top of the cursor, and can just
                // draw overtop of what's already in the frame buffer. NOTE:
                // this only works because the two cursor frames occupy the same
                // pixels.
                RASTER_DEBUG();
                if (t >= 480) {
                    pfrm.blit_t1_tile_to_texture(stk[0] + 480, t - 480, false);
                } else {
                    pfrm.blit_t0_tile_to_texture(stk[0] + 480, t, false);
                }
            } else {
                auto stk_cpy = globalstate::_cursor_raster_stack[i];
                if (t >= 480) {
                    flush_stack_t1(stk_cpy, t - 480);
                } else {
                    flush_stack_t0(stk_cpy, t);
                }
            }
        }

        globalstate::_changed_cursor_flicker_only = false;
        return;
    }

    const bool cursor_moved = globalstate::_cursor_moved;
    const bool shrunk = globalstate::_shrunk;
    const bool grew = globalstate::_grew;


    render_setup(pfrm);

    [[maybe_unused]] auto start = pfrm.delta_clock().sample();

    for (int i = 0; i < 480; ++i) {

        if (auto head = (*_db)->depth_1_->visible_[i]) {

            const bool redraw =
                grew or
                (not cursor_moved or
                 (cursor_moved and (*_db)->depth_1_cursor_redraw.get(i)));

            if (redraw) {
                Buffer<int, 6> stack;
                while (head) {
                    stack.push_back(head->tile_);
                    head = head->next_;
                }

                flush_stack_t0(stack, i);
            }

        } else if ((cursor_moved or shrunk) and
                   not(*_db)->depth_1_skip_clear.get(i)) {
            pfrm.blit_t0_erase(i);
        }

        if (auto head = (*_db)->depth_2_->visible_[i]) {

            const bool redraw =
                grew or
                (not cursor_moved or
                 (cursor_moved and (*_db)->depth_2_cursor_redraw.get(i)));

            if (redraw) {
                Buffer<int, 6> stack;
                while (head) {
                    stack.push_back(head->tile_);
                    head = head->next_;
                }

                flush_stack_t1(stack, i);
            }


        } else if ((cursor_moved or shrunk) and
                   not(*_db)->depth_2_skip_clear.get(i)) {
            pfrm.blit_t1_erase(i);
        }
    }

    [[maybe_unused]] auto stop = pfrm.delta_clock().sample();
    // pfrm.fatal(stringify(stop - start).c_str());


    if (globalstate::_cursor_moved) {
        // Handle these out of line, as not to slow down the main rendering
        // block.
        for (int i = 0; i < 480; ++i) {
            if (cursor_moved and (*_db)->depth_1_empty.get(i)) {
                pfrm.blit_t0_erase(i);
            }
            if (cursor_moved and (*_db)->depth_2_empty.get(i)) {
                pfrm.blit_t1_erase(i);
            }
        }
    }


    set_repaint(false);


    globalstate::_changed = false;
    globalstate::_shrunk = false;
    globalstate::_grew = false;
    globalstate::_cursor_moved = false;
    globalstate::_changed_cursor_flicker_only = false;

    _db.reset();
}



} // namespace skyland::macro