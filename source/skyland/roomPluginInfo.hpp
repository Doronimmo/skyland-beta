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



#include "room_metatable.hpp"



namespace skyland
{



// A metatable entry backed by a lisp datastructure, allowing users to
// define their own rooms via scripts.
struct RoomPluginInfo : public RoomMeta::Info
{
    RoomMeta* mt_;
    mutable std::optional<lisp::Protected> info_;

    s16 health_ = 10;
    s16 cost_ = 10;
    s16 power_ = 10;


    RoomPluginInfo(RoomMeta* mt) : mt_(mt)
    {
    }


    void
    construct(void* address, Island* parent, const RoomCoord& position) override
    {
        static_assert(sizeof(PluginRoom) <= room_pool::max_room_size);
        static_assert(alignof(PluginRoom) <= room_pool::alignment);

        new (address) PluginRoom(parent, position, mt_);
    }


    void create(Island* parent,
                const RoomCoord& position,
                bool do_repaint) const override
    {
        parent->add_room<PluginRoom>(position, do_repaint, mt_);
    }


    struct FieldTag
    {
        enum Tag {
            name,
            size,
            graphics_list,
            update_frequency,
            update,
        };
    };


    Room::WeaponOrientation weapon_orientation() const override
    {
        return Room::WeaponOrientation::none;
    }


    template <FieldTag::Tag info, typename T> T& fetch_info() const
    {
        if (info_) {
            return lisp::get_list(*info_, info)->expect<T>();
        }

        Platform::fatal("plugin room info unassigned");
    }


    const char* name() const override
    {
        return fetch_info<FieldTag::name, lisp::Symbol>().name();
    }


    SystemStringBuffer ui_name() const override
    {
        auto ret = allocate_dynamic<StringBuffer<1900>>("locale-string");
        *ret += name();
        return ret;
    }


    Vec2<u8> size() const override
    {
        auto& pair = fetch_info<FieldTag::size, lisp::Cons>();
        return {(u8)pair.car()->expect<lisp::Integer>().value_,
                (u8)pair.cdr()->expect<lisp::Integer>().value_};
    }


    Coins cost() const override
    {
        return cost_;
    }


    Float atp_value() const override
    {
        // FIXME!
        return 2;
    }


    Power consumes_power() const override
    {
        return power_;
    }


    RoomProperties::Bitmask properties() const override
    {
        return RoomProperties::plugin | RoomProperties::disallow_chimney |
               RoomProperties::roof_hidden | RoomProperties::locked_by_default;
    }


    Room::Category category() const override
    {
        return Room::Category::misc; // TODO...
    }


    void format_description(StringBuffer<512>&) const override
    {
        Platform::fatal("attempt to fetch desciption for a plugin room.");
    }


    Room::Icon icon() const override
    {
        return PluginRoom::icon();
    }


    Room::Icon unsel_icon() const override
    {
        return PluginRoom::unsel_icon();
    }


    Health full_health() const override
    {
        return health_;
    }
};



} // namespace skyland
