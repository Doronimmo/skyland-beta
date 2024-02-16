////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2023  Evan Bowman. Some rights reserved.
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


#pragma once

#include "allocator.hpp"
#include "bulkTimer.hpp"
#include "entity.hpp"
#include "entity/character/basicCharacter.hpp"
#include "entity/drones/drone.hpp"
#include "memory/buffer.hpp"
#include "player/player.hpp"
#include "power.hpp"
#include "room.hpp"
#include "roomTable.hpp"
#include "room_alloc.hpp"



// NOTE:
//
// Throughout the code, you will see mentions of the player island, and the
// opponent island, or the near island, and the far island.
//
// The near, or player island, is the player controlled island at the left side
// of the screen.
//
// The far, or opponent island, is the opponennt controlled island on the right
// side of the screen.
//



namespace skyland
{



class Island
{
public:
    Island(Layer layer, u8 width, Player& owner);
    Island(const Island&) = delete;


    using Rooms = RoomTable;


    bool add_room(RoomPtr<Room> insert, bool do_repaint = true);


    template <typename T, typename... Args>
    bool add_room(const RoomCoord& position, bool do_repaint, Args&&... args)
    {
        if (rooms().full()) {
            return false;
        }
        if (auto room = room_pool::alloc<T>(
                this, position, std::forward<Args>(args)...)) {
            if (rooms_.insert_room({room.release(), room_pool::deleter})) {
                if (do_repaint) {
                    repaint();
                }
                recalculate_power_usage();
                on_layout_changed(position);
                return true;
            }
        }
        return false;
    }


    void move_room(const RoomCoord& from, const RoomCoord& to);


    void init_terrain(int width, bool render = true);


    bool add_character(EntityRef<BasicCharacter> character);


    void remove_character(const RoomCoord& location);


    Rooms& rooms();


    void clear_rooms();


    void clear();


    void update(Time delta);
    void update_simple(Time delta);


    void rewind(Time delta);


    void display();
    void display_fires();

    const Vec2<Fixnum>& get_position() const;


    void set_position(const Vec2<Fixnum>& position);


    Room* get_room(const RoomCoord& coord);


    Optional<SharedEntityRef<Drone>> get_drone(const RoomCoord& coord);


    void destroy_room(const RoomCoord& coord);


    s8 get_ambient_movement();


    Layer layer() const
    {
        return layer_;
    }


    void set_hidden(bool hidden);


    void render_interior();
    void render_interior_fast();


    void render_exterior();


    void render();


    void plot_rooms(u8 matrix[16][16]) const;


    void plot_construction_zones(bool matrix[16][16]) const;


    void plot_walkable_zones(bool matrix[16][16],
                             BasicCharacter* for_character) const;


    BasicCharacter* character_at_location(const RoomCoord& loc);


    Pair<BasicCharacter*, Room*> find_character_by_id(CharacterId id);


    // NOTE: generally, you should use render() intead of repaint().
    void repaint();


    bool interior_visible() const
    {
        return interior_visible_;
    }


    // The origin used for collision checking and important stuff.
    Vec2<Fixnum> origin() const;


    // The origin with some added ambient movement, looks nice, but not
    // sufficient for calculating collision checking or anything like that,
    // mostly due to multiplayer, continuously moving things can get out of
    // sync.
    Vec2<Fixnum> visual_origin() const;


    using Terrain = Buffer<u8, 16>;


    Terrain& terrain()
    {
        return terrain_;
    }


    void render_terrain();


    void set_float_timer(Time value);


    void show_flag(bool show)
    {
        show_flag_ = show;
    }


    void set_drift(Fixnum drift);


    Fixnum get_drift() const
    {
        return drift_;
    }


    u8 workshop_count() const;
    u8 manufactory_count() const;
    u8 core_count() const;


    EntityList<Entity>& projectiles();


    SharedEntityList<Drone>& drones();


    void test_collision(Entity& entity);


    Player& owner()
    {
        return *owner_;
    }


    bool is_destroyed();


    Optional<RoomCoord> chimney_loc() const;


    Power power_supply() const;


    Power power_drain() const;


    void set_owner(Player& player);


    bool has_radar() const;


    bool is_boarded() const;


    void on_layout_changed(const RoomCoord& room_added_removed_coord);


    HitBox hitbox() const;


    Optional<RoomCoord> flag_pos();


    const Bitmatrix<16, 16>& rooms_plot() const;


    void dispatch_room(Room* room);
    void drawfirst(Room* room);


    void cancel_dispatch();


    // The number of ways that the enemy has of attacking the player (weapons,
    // transporters, etc.).
    u8 offensive_capabilities() const;


    u8 character_count() const;


    // The y value of the top-most tile on the island.
    u8 min_y() const;


    void schedule_repaint()
    {
        schedule_repaint_ = true;
    }


    void schedule_repaint_partial()
    {
        schedule_repaint_partial_ = true;
    }


    Optional<Platform::DynamicTexturePtr> fire_texture();


    bool fire_present(const RoomCoord& coord) const;
    void fire_extinguish(const RoomCoord& coord);
    void fire_create(const RoomCoord& coord);

    void fires_extinguish();


    const EntityList<BasicCharacter>& outdoor_characters();


    RoomCoord critical_core_loc() const
    {
        return {critical_core_x_, critical_core_y_};
    }


    using BlockChecksum = u16;


    BlockChecksum checksum() const
    {
        return checksum_;
    }


    void init_ai_awareness();


    BulkTimer& bulk_timer()
    {
        return bulk_timer_;
    }


    void set_custom_flag_graphics(u8 val)
    {
        custom_flag_graphics_ = val;
    }


    u8 custom_flag_graphics() const
    {
        return custom_flag_graphics_;
    }


    void recalculate_power_usage();


private:
    void repaint_partial();


    bool repaint_alloc_tiles(TileId buffer[16][16], bool retry);


    void resolve_cancelled_dispatch();


    void check_destroyed();


    BulkTimer bulk_timer_;

    EntityList<BasicCharacter> characters_;
    EntityList<Entity> projectiles_;
    SharedEntityList<Drone> drones_;

    Player* owner_;

    Rooms rooms_;
    Room* dispatch_list_ = nullptr;
    Room* drawfirst_ = nullptr;
    const Layer layer_;
    Terrain terrain_;
    Time chimney_spawn_timer_ = 0;
    Time flag_anim_timer_ = 0;
    Time timer_;
    Vec2<Fixnum> position_;
    Fixnum drift_ = 0.0_fixed;

    struct FireState
    {
        Bitmatrix<16, 16> positions_;
        Optional<Platform::DynamicTexturePtr> texture_;
        Time spread_timer_ = 0;
        Time damage_timer_ = 0;
        Time anim_timer_ = 0;
        s8 anim_index_ = 0;

        void update(Island& island, Time delta);

        void rewind(Island& island, Time delta);

        void display(Island& island);

    } fire_;

    Power power_supply_ = 0;
    Power power_drain_ = 0;

    BlockChecksum checksum_ = 0;

    u8 flag_anim_index_;
    u8 workshop_count_ = 0;
    u8 manufactory_count_ = 0;
    u8 core_count_ = 0;
    u8 min_y_ = 0;
    s8 ambient_movement_;

    // These parameters represent the location where a power core might possibly
    // be. Used during the death animation when placing the center of the
    // explosion effect, as the power core no longer exists.
    u8 critical_core_x_ : 4;
    u8 critical_core_y_ : 4;

    u8 character_count_ = 0;
    u8 offensive_capabilities_ = 0;
    u8 custom_flag_graphics_ = 0;

    bool destroyed_ = false;
    bool all_characters_awaiting_movement_ = false;

    bool interior_visible_ : 1;
    bool show_flag_ : 1;
    bool dispatch_cancelled_ : 1;
    bool schedule_repaint_ : 1;
    bool schedule_repaint_partial_ : 1;

    bool has_radar_ : 1;
    bool is_boarded_ : 1;
    bool hidden_ : 1;


    // During repaint(), the game caches the results of plot_rooms() in this
    // matrix of bitflags. We use the result to speed up collision checking.
    Bitmatrix<16, 16> rooms_plot_;

    Optional<RoomCoord> flag_pos_;

    Optional<RoomCoord> chimney_loc_;
};



void show_island_interior(Island* island);
void show_island_exterior(Island* island);



Island& player_island();
Island* opponent_island();



// (See documentation for 'near' and 'far' island above)
Island* get_island(bool island_is_near);



bool is_player_island(Island* isle);
bool is_near_island(Island* isle);



bool synth_notes_store(Island& island, const char* path);



bool synth_notes_load(Island& island, const char* path);



bool speaker_data_store(Island& island, const char* path);



bool speaker_data_load(Island& island, const char* path);



} // namespace skyland
