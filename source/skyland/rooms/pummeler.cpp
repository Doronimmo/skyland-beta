#include "platform/platform.hpp"
#include "pummeler.hpp"
#include "skyland/alloc_entity.hpp"
#include "skyland/entity/projectile/curveshot.hpp"
#include "skyland/scene_pool.hpp"
#include "skyland/skyland.hpp"
#include "skyland/sound.hpp"
#include "skyland/tile.hpp"



namespace skyland {



Pummeler::Pummeler(Island* parent, const Vec2<u8>& position)
    : Weapon(parent, name(), position, 1000 * milliseconds(3))
{
}



void Pummeler::fire(Platform& pfrm, App& app)
{
    auto island = other_island(app);

    Vec2<Float> target;

    auto origin = island->origin();
    origin.x += target_->x * 16 + 8;
    origin.y += target_->y * 16 + 8;
    target = origin;

    app.camera()->shake(4);

    auto start = center();

    if (island == &app.player_island()) {
        start.x -= 6;
    } else {
        start.x += 6;
    }

    if (not pfrm.network_peer().is_connected() and
        app.game_mode() not_eq App::GameMode::tutorial) {
        target = rng::sample<6>(target, rng::critical_state);
    }

    auto c =
        app.alloc_entity<Curveshot>(pfrm,
                                    start,
                                    target,
                                    parent(),
                                    other_island(app),
                                    position(),
                                    *target_);
    if (c) {
        parent()->projectiles().push(std::move(c));
    }
}



Microseconds Pummeler::reload() const
{
    return 1000 * milliseconds(3);
}



void Pummeler::render_interior(App& app, u8 buffer[16][16])
{
    buffer[position().x][position().y] = Tile::cannon_1;
}



void Pummeler::render_exterior(App& app, u8 buffer[16][16])
{
    buffer[position().x][position().y] = Tile::cannon_1;
}



} // namespace skyland