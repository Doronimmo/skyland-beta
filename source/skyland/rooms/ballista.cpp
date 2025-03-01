////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025  Evan Bowman. Some rights reserved.
//
// This program is source-available; the source code is provided for educational
// purposes. All copies of the software must be distributed along with this
// license document.
//
// 1. DEFINITION OF SOFTWARE: The term "Software" refers to SKYLAND,
// including any updates, modifications, or associated documentation provided by
// Licensor.
//
// 2. DERIVATIVE WORKS: Licensee is permitted to modify the source code.
//
// 3. COMMERCIAL USE: Commercial use is not allowed.
//
// 4. ATTRIBUTION: Licensee is required to provide attribution to Licensor.
//
// 5. INTELLECTUAL PROPERTY RIGHTS: All intellectual property rights in the
// Software shall remain the property of Licensor. The Licensee does not acquire
// any rights to the Software except for the limited use rights specified in
// this Agreement.
//
// 6. WARRANTY AND LIABILITY: The Software is provided "as is" without warranty
// of any kind. Licensor shall not be liable for any damages arising out of or
// related to the use or inability to use the Software.
//
// 7. TERMINATION: This Agreement shall terminate automatically if Licensee
// breaches any of its terms and conditions. Upon termination, Licensee must
// cease all use of the Software and destroy all copies.
//
////////////////////////////////////////////////////////////////////////////////


#include "ballista.hpp"
#include "platform/platform.hpp"
#include "skyland/alloc_entity.hpp"
#include "skyland/entity/misc/animatedEffect.hpp"
#include "skyland/scene_pool.hpp"
#include "skyland/skyland.hpp"
#include "skyland/sound.hpp"
#include "skyland/tile.hpp"
#include "skyland/sharedVariable.hpp"
#include "skyland/entity/projectile/ballistaBolt.hpp"


Fixnum abs(const Fixnum& val)
{
    return (val > 0.0_fixed) ? val : val * Fixnum::from_integer(-1);
}


namespace skyland
{



SHARED_VARIABLE(ballista_reload_ms);



extern Sound cannon_sound;



void Ballista::format_description(StringBuffer<512>& buffer)
{
    buffer += SYSTR(description_ballista)->c_str();
}



Ballista::Ballista(Island* parent, const RoomCoord& position)
    : Weapon(parent, name(), position, 1000 * ballista_reload_ms)
{
}



class CollisionTestEntity : public Projectile
{
public:
    Optional<Vec2<Fixnum>> coll_pos;


    CollisionTestEntity(const Vec2<Fixnum>& position) :
        Projectile({{10, 10}, {8, 8}})
    {
        sprite_.set_position(position);
        sprite_.set_origin({8, 8});
    }


    void update(Time delta) override
    {
    }


    void rewind(Time delta) override
    {
    }


    void on_collision(Room& r, Vec2<u8> origin)
    {
        coll_pos = r.center();
    }

};



void Ballista::fire()
{
    if (last_target_ and *get_target() not_eq *last_target_) {
        arc_height_.reset();
    }

    last_target_ = get_target();

    Vec2<Fixnum> target;

    target = target_xy({get_target()->x, get_target()->y});

    APP.camera()->shake(6);

    auto start = emit_xy();

    cannon_sound.play(3);

    if (not arc_height_) {
        arc_height_ = recalc_arc_height(start, target);
    }

    if (arc_height_) {
        auto arc_height = Fixnum::from_integer(*arc_height_);
        auto c = APP.alloc_entity<BallistaBolt>(start, target, arc_height, *parent());
        if (c) {
            parent()->projectiles().push(std::move(c));
        }

        auto e = alloc_entity<AnimatedEffect>(start, 47, 49, milliseconds(100));
        if (e) {
            APP.effects().push(std::move(e));
        }
    }
}



Vec2<Fixnum> Ballista::target_xy(RoomCoord c) const
{
    auto origin = other_island()->origin();
    origin.x += Fixnum::from_integer(c.x * 16 + 8);
    origin.y += Fixnum::from_integer(c.y * 16 + 8);

    return origin;
}



Vec2<Fixnum> Ballista::emit_xy() const
{
    auto start = center();

    // This just makes it a bit less likely for cannonballs to
    // run into the player's own buildings, especially around
    // corners.
    if (is_player_island(other_island())) {
        start.x -= 26.0_fixed;
    } else {
        start.x += 26.0_fixed;
    }

    return start;
}



Optional<u8> Ballista::recalc_arc_height(const Vec2<Fixnum>& start,
                                         const Vec2<Fixnum>& target) const
{
    Fixnum arc_height;

    Buffer<std::pair<Fixnum, Vec2<Fixnum>>, 16> arc_test_buffer;

    for (int i = 1; i < 16; i += 2) {

        arc_height = Fixnum::from_integer(i * 10);

        BallistaBolt::Path path;
        BallistaBolt::generate_path(path, start.x, start.y,
                                    target.x, target.y,
                                    arc_height);

        for (int i = 0; i < path.size(); ++i) {
            auto node = path[i];
            CollisionTestEntity ct(Vec2<Fixnum>{
                    Fixnum::from_integer(node.x),
                    Fixnum::from_integer(node.y)
                });
            other_island()->test_collision(ct);

            if (ct.coll_pos) {
                arc_test_buffer.push_back({arc_height, *ct.coll_pos});
                break;
            }

            parent()->test_collision(ct);
            if (ct.coll_pos) {
                arc_test_buffer.push_back({arc_height, *ct.coll_pos});
                break;
            }
        }
    }

    std::sort(arc_test_buffer.begin(), arc_test_buffer.end(),
              [&](auto& lhs, auto& rhs) {
                  return manhattan_length(target, lhs.second).as_integer() <
                         manhattan_length(target, rhs.second).as_integer();
              });

    if (not arc_test_buffer.empty()) {
        return arc_test_buffer[0].first.as_integer();
    }

    return nullopt();
}



Time Ballista::reload_impl() const
{
    return 1000 * ballista_reload_ms;
}



void Ballista::render_interior(App* app, TileId buffer[16][16])
{
    buffer[position().x][position().y] = Tile::ballista_1;
    buffer[position().x + 1][position().y] = Tile::ballista_2;
}



void Ballista::render_exterior(App* app, TileId buffer[16][16])
{
    buffer[position().x][position().y] = Tile::ballista_1;
    buffer[position().x + 1][position().y] = Tile::ballista_2;
}



} // namespace skyland
