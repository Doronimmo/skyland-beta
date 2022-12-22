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

#include "achievement.hpp"
#include "characterId.hpp"
#include "coins.hpp"
#include "number/random.hpp"
#include "timeStreamHeader.hpp"



// At first, you might question why we have both time_stream::event and
// network::event, when there might seem to be a lot of overlap between the
// two. But with rewind functionality, you care about the state before the
// current state, and with network sync, you care about the current state, so
// stuff can in some cases be stored more compactly in some cases when you only
// need to worry about storing the previous state. Furthermore, some events
// necessary for rewind are unnecessary for multiplayer.


// Note to anyone who might be reading this code:
// I know bitfields are not portable, but these datastructures are not meant to
// be permanently recorded or sent to other devices over a network. And I'm
// using the endian classes below simply because they store their data as arrays
// of char, so I don't need to worry about losing memory due to alignment, not
// because I care about portability for the data. In summary, bitfields are
// simply convenient, accesses don't need to be fast as event read/write is very
// infrequent, I decided in favor of the more convenient syntax.



namespace skyland::time_stream::event
{



enum Type : u8 {
    __reserved,

    initial,

    player_room_created,
    opponent_room_created,

    player_room_destroyed,
    opponent_room_destroyed,

    player_room_destroyed_with_group,

    player_room_transmuted,
    opponent_room_transmuted,

    player_cannonball_destroyed,
    opponent_cannonball_destroyed,

    player_arcbolt_destroyed,
    opponent_arcbolt_destroyed,

    player_firebolt_destroyed,
    opponent_firebolt_destroyed,

    player_flak_destroyed,
    opponent_flak_destroyed,

    player_ionburst_destroyed,
    opponent_ionburst_destroyed,

    player_missile_destroyed,
    opponent_missile_destroyed,

    player_rocketbomb_destroyed,
    opponent_rocketbomb_destroyed,

    player_decimator_burst_destroyed,
    opponent_decimator_burst_destroyed,

    player_nemesis_blast_destroyed,
    opponent_nemesis_blast_destroyed,

    player_antimatter_destroyed,
    opponent_antimatter_destroyed,

    player_plugin_projectile_destroyed,
    opponent_plugin_projectile_destroyed,

    player_room_damaged,
    opponent_room_damaged,

    player_room_damaged_small,
    opponent_room_damaged_small,

    player_room_repaired,
    opponent_room_repaired,

    coins_changed,

    character_moved,
    character_died,
    character_health_changed,
    character_transported,
    character_disembark,
    character_movement_path_assigned,


    player_room_salvaged,
    opponent_room_salvaged,

    replicant_created,

    weapon_set_target,

    player_room_reload_complete,
    opponent_room_reload_complete,

    opponent_island_drift_changed,

    island_terrain_changed,

    player_room_plundered,
    opponent_room_plundered,

    player_room_moved,
    opponent_room_moved,

    drone_deployed,
    drone_health_changed,
    drone_destroyed,

    drone_set_target,
    drone_reload_complete,

    rng_changed,

    achievement,

    sound_completed,

    // These three sounds happen a lot, and we can save memory by using special
    // events to represent them.
    cannon_sound_completed,
    missile_sound_completed,
    hit_sound_completed,

    bird_left_map,

    player_fire_created,
    opponent_fire_created,

    player_fire_extinguished,
    opponent_fire_extinguished,

    weapon_set_group,

    lightning,
    lightning_done,

    weather_changed,

    player_room_ai_awareness,
    opponent_room_ai_awareness,

    explosion,
    core_explosion,

    boarding_pod_landed,

    move_region_begin,
    move_region_end,
};



struct Initial
{
    Header header_;

    static constexpr const auto t = Type::initial;
};



struct PlayerRoomCreated
{
    Header header_;
    u8 x_ : 4;
    u8 y_ : 4;

    static constexpr const auto t = Type::player_room_created;
};



struct OpponentRoomCreated
{
    Header header_;
    u8 x_ : 4;
    u8 y_ : 4;

    static constexpr const auto t = Type::opponent_room_created;
};



struct PlayerRoomDestroyed
{
    Header header_;
    u8 x_ : 4;
    u8 y_ : 4;
    u8 type_;

    static constexpr const auto t = Type::player_room_destroyed;
};



struct PlayerRoomDestroyedWithGroup
{
    Header header_;
    u8 x_ : 4;
    u8 y_ : 4;
    u8 type_;
    u8 group_;

    static constexpr const auto t = Type::player_room_destroyed_with_group;
};



struct OpponentRoomDestroyed
{
    Header header_;
    u8 x_ : 4;
    u8 y_ : 4;
    u8 type_;

    static constexpr const auto t = Type::opponent_room_destroyed;
};



struct PlayerRoomTransmuted
{
    Header header_;
    u8 x_ : 4;
    u8 y_ : 4;
    u8 prev_type_;

    static constexpr const auto t = Type::player_room_transmuted;
};



struct OpponentRoomTransmuted
{
    Header header_;
    u8 x_ : 4;
    u8 y_ : 4;
    u8 prev_type_;

    static constexpr const auto t = Type::opponent_room_transmuted;
};



struct PlayerRoomMoved
{
    Header header_;
    u8 x_ : 4;
    u8 y_ : 4;
    u8 prev_x_ : 4;
    u8 prev_y_ : 4;

    static constexpr const auto t = Type::player_room_moved;
};



struct OpponentRoomMoved
{
    Header header_;
    u8 x_ : 4;
    u8 y_ : 4;
    u8 prev_x_ : 4;
    u8 prev_y_ : 4;

    static constexpr const auto t = Type::opponent_room_moved;
};



struct RoomPlundered
{
    Header header_;
    u8 x_ : 4;
    u8 y_ : 4;
    u8 type_;
};



struct PlayerRoomPlundered : RoomPlundered
{
    static constexpr const auto t = Type::player_room_plundered;
};



struct OpponentRoomPlundered : RoomPlundered
{
    static constexpr const auto t = Type::opponent_room_plundered;
};



struct RoomSalvaged
{
    Header header_;
    u8 x_ : 4;
    u8 y_ : 4;
    u8 type_;
};



struct PlayerRoomSalvaged : RoomSalvaged
{
    static constexpr const auto t = Type::player_room_salvaged;
};



struct OpponentRoomSalvaged : RoomSalvaged
{
    static constexpr const auto t = Type::opponent_room_salvaged;
};



struct BasicProjectileDestroyed
{
    Header header_;
    u8 x_origin_ : 4;
    u8 y_origin_ : 4;
    HostInteger<Microseconds> timer_;
    host_s16 x_pos_;
    host_s16 y_pos_;
    host_s64 x_speed__data_;
    host_s64 y_speed__data_;
};



struct PlayerCannonballDestroyed : BasicProjectileDestroyed
{
    static constexpr const auto t = Type::player_cannonball_destroyed;
};



struct OpponentCannonballDestroyed : BasicProjectileDestroyed
{
    static constexpr const auto t = Type::player_cannonball_destroyed;
};



struct PlayerArcboltDestroyed : BasicProjectileDestroyed
{
    static constexpr const auto t = Type::player_arcbolt_destroyed;
};



struct OpponentArcboltDestroyed : BasicProjectileDestroyed
{
    static constexpr const auto t = Type::player_arcbolt_destroyed;
};



struct PlayerFireboltDestroyed : BasicProjectileDestroyed
{
    static constexpr const auto t = Type::player_firebolt_destroyed;
};



struct OpponentFireboltDestroyed : BasicProjectileDestroyed
{
    static constexpr const auto t = Type::player_firebolt_destroyed;
};



struct PlayerFlakDestroyed : BasicProjectileDestroyed
{
    static constexpr const auto t = Type::player_flak_destroyed;
};



struct OpponentFlakDestroyed : BasicProjectileDestroyed
{
    static constexpr const auto t = Type::opponent_flak_destroyed;
};



struct PlayerIonBurstDestroyed : BasicProjectileDestroyed
{
    static constexpr const auto t = Type::player_ionburst_destroyed;
};



struct OpponentIonBurstDestroyed : BasicProjectileDestroyed
{
    static constexpr const auto t = Type::opponent_ionburst_destroyed;
};



struct PlayerDecimatorBurstDestroyed : BasicProjectileDestroyed
{
    static constexpr const auto t = Type::player_decimator_burst_destroyed;
};



struct OpponentDecimatorBurstDestroyed : BasicProjectileDestroyed
{
    static constexpr const auto t = Type::opponent_decimator_burst_destroyed;
};



struct PlayerAntimatterDestroyed : BasicProjectileDestroyed
{
    static constexpr const auto t = Type::player_antimatter_destroyed;
};



struct OpponentAntimatterDestroyed : BasicProjectileDestroyed
{
    static constexpr const auto t = Type::opponent_antimatter_destroyed;
};



struct NemesisBlastDestroyed
{
    Header header_;
    u8 x_origin_ : 4;
    u8 y_origin_ : 4;
    HostInteger<Microseconds> timer_;
    host_s16 x_pos_;
    host_s16 y_pos_;
    host_s64 x_speed__data_;
    host_s64 y_speed__data_;
    u8 variant_;
};



struct PlayerNemesisBlastDestroyed : NemesisBlastDestroyed
{
    static constexpr const auto t = Type::player_nemesis_blast_destroyed;
};



struct OpponentNemesisBlastDestroyed : NemesisBlastDestroyed
{
    static constexpr const auto t = Type::opponent_nemesis_blast_destroyed;
};



struct PluginProjectileDestroyed
{
    Header header_;
    u8 x_origin_ : 4;
    u8 y_origin_ : 4;
    HostInteger<Microseconds> timer_;
    host_s16 x_pos_;
    host_s16 y_pos_;
    host_s64 x_speed__data_;
    host_s64 y_speed__data_;
    u8 variant_;
    host_u16 tile_;
    HostInteger<s16> damage_;
    bool hflip_;
};



struct PlayerPluginProjectileDestroyed : PluginProjectileDestroyed
{
    static constexpr const auto t = Type::player_plugin_projectile_destroyed;
};



struct OpponentPluginProjectileDestroyed : PluginProjectileDestroyed
{
    static constexpr const auto t = Type::opponent_plugin_projectile_destroyed;
};



struct MissileDestroyed
{
    Header header_;

    HostInteger<Microseconds> timer_;

    host_s16 x_pos_;
    host_s16 y_pos_;

    host_s16 target_x_;

    u8 source_x_ : 4;
    u8 source_y_ : 4;
    u8 state_ : 4;
};



struct PlayerMissileDestroyed : MissileDestroyed
{
    static constexpr const auto t = Type::player_missile_destroyed;
};



struct OpponentMissileDestroyed : MissileDestroyed
{
    static constexpr const auto t = Type::opponent_missile_destroyed;
};



struct PlayerRocketBombDestroyed : MissileDestroyed
{
    static constexpr const auto t = Type::player_rocketbomb_destroyed;
};



struct OpponentRocketBombDestroyed : MissileDestroyed
{
    static constexpr const auto t = Type::opponent_rocketbomb_destroyed;
};



// To save space in RAM, when a room takes less than 255 damage (pretty much all
// cases), use a single byte to represent the amount of damage applied.
struct RoomDamagedSmall
{
    Header header_;
    u8 x_ : 4;
    u8 y_ : 4;
    u8 diff_;
};



struct PlayerRoomDamagedSmall : RoomDamagedSmall
{
    static constexpr const auto t = Type::player_room_damaged_small;
};



struct OpponentRoomDamagedSmall : RoomDamagedSmall
{
    static constexpr const auto t = Type::opponent_room_damaged_small;
};



struct RoomHealthChanged
{
    Header header_;
    u8 x_ : 4;
    u8 y_ : 4;
    // The whole point of this timestream stuff is for implementing rewind, so
    // we only care about what the health was before it changed, to execute a
    // revert.
    host_u16 previous_health_;
};



struct PlayerRoomDamaged : RoomHealthChanged
{
    static constexpr const auto t = Type::player_room_damaged;
};



struct OpponentRoomDamaged : RoomHealthChanged
{
    static constexpr const auto t = Type::opponent_room_damaged;
};



struct PlayerRoomRepaired : RoomHealthChanged
{
    static constexpr const auto t = Type::player_room_repaired;
};



struct OpponentRoomRepaired : RoomHealthChanged
{
    static constexpr const auto t = Type::opponent_room_repaired;
};



struct CoinsChanged
{
    Header header_;
    HostInteger<u32> previous_value_;

    static constexpr const auto t = Type::coins_changed;
};



struct ReplicantCreated
{
    Header header_;
    u8 x_ : 4;
    u8 y_ : 4;
    u8 owned_by_player_ : 1;
    u8 near_ : 1;
    u8 unused_ : 6;

    static constexpr const auto t = Type::replicant_created;
};



struct CharacterMoved
{
    Header header_;
    HostInteger<CharacterId> id_;
    u8 previous_x_ : 4;
    u8 previous_y_ : 4;
    u8 near_ : 1;
    u8 unused_ : 7;

    static constexpr const auto t = Type::character_moved;
};



struct CharacterMovementPathAssigned
{
    Header header_;
    HostInteger<CharacterId> id_;
    u8 near_ : 1;
    u8 unused_ : 7;

    static constexpr const auto t = Type::character_movement_path_assigned;
};



struct CharacterDied
{
    Header header_;
    HostInteger<CharacterId> id_;
    u8 x_ : 4;
    u8 y_ : 4;
    u8 owned_by_player_ : 1;
    u8 near_ : 1;
    u8 is_replicant_ : 1;
    u8 unused_ : 5;

    static constexpr const auto t = Type::character_died;
};



struct CharacterHealthChanged
{
    Header header_;
    HostInteger<CharacterId> id_;
    u8 owned_by_player_ : 1;
    u8 near_ : 1;
    u8 unused_ : 6;
    u8 previous_health_;

    static constexpr const auto t = Type::character_health_changed;
};



struct CharacterTransported
{
    Header header_;
    HostInteger<CharacterId> id_;
    u8 previous_x_ : 4;
    u8 previous_y_ : 4;
    u8 source_near_ : 1;
    u8 unused_ : 7;

    static constexpr const auto t = Type::character_transported;
};



struct CharacterDisembark
{
    Header header_;
    HostInteger<CharacterId> id_;
    u8 previous_x_ : 4;
    u8 previous_y_ : 4;
    u8 chr_near_ : 1;
    u8 unused_ : 7;

    static constexpr const auto t = Type::character_disembark;
};



struct WeaponSetGroup
{
    Header header_;
    u8 room_x_ : 4;
    u8 room_y_ : 4;
    u8 prev_group_;

    static constexpr const auto t = Type::weapon_set_group;
};



struct WeaponSetTarget
{
    Header header_;
    u8 room_x_ : 4;
    u8 room_y_ : 4;
    u8 previous_target_x_ : 4;
    u8 previous_target_y_ : 4;

    u8 has_previous_target_ : 1;
    u8 near_ : 1;
    u8 unused_ : 6;

    static constexpr const auto t = Type::weapon_set_target;
};



struct RoomReloadComplete
{
    Header header_;
    u8 room_x_ : 4;
    u8 room_y_ : 4;
};



struct PlayerRoomReloadComplete : RoomReloadComplete
{
    static constexpr const auto t = Type::player_room_reload_complete;
};



struct OpponentRoomReloadComplete : RoomReloadComplete
{
    static constexpr const auto t = Type::opponent_room_reload_complete;
};



struct OpponentIslandDriftChanged
{
    Header header_;
    host_s64 previous_speed__data_;

    static constexpr const auto t = Type::opponent_island_drift_changed;
};



struct IslandTerrainChanged
{
    Header header_;
    u8 previous_terrain_size_ : 4;
    u8 near_ : 1;
    u8 unused_ : 3;

    static constexpr const auto t = Type::island_terrain_changed;
};



// Event occurs after a drone finishes moving to its deployment point.  When we
// see this event, we know to set the drone to its launch state, and rewind.
struct DroneDeployed
{
    Header header_;
    u8 x_pos_ : 4;
    u8 y_pos_ : 4;
    u8 parent_near_ : 1;
    u8 destination_near_ : 1;
    HostInteger<Microseconds> duration_;

    static constexpr const auto t = Type::drone_deployed;
};



struct DroneHealthChanged
{
    Header header_;
    u8 x_pos_ : 4;
    u8 y_pos_ : 4;
    u8 destination_near_ : 1;
    host_u16 previous_health_;

    static constexpr const auto t = Type::drone_health_changed;
};



struct DroneDestroyed
{
    Header header_;
    u8 x_pos_ : 4;
    u8 y_pos_ : 4;
    u8 destination_near_ : 1;
    u8 parent_near_ : 1;
    u8 db_x_pos_ : 4;
    u8 db_y_pos_ : 4;
    u8 type_;
    u8 state_;
    HostInteger<Microseconds> timer_;
    HostInteger<Microseconds> duration_;

    static constexpr const auto t = Type::drone_destroyed;
};



struct DroneSetTarget
{
    Header header_;
    u8 x_pos_ : 4;
    u8 y_pos_ : 4;
    u8 previous_target_x_ : 4;
    u8 previous_target_y_ : 4;
    u8 previous_target_near_ : 1;
    u8 has_previous_target_ : 1;
    u8 destination_near_ : 1;
    u8 unused_ : 5;

    static constexpr const auto t = Type::drone_set_target;
};



struct DroneReloadComplete
{
    Header header_;
    u8 x_pos_ : 4;
    u8 y_pos_ : 4;
    u8 destination_near_ : 1;

    static constexpr const auto t = Type::drone_reload_complete;
};



struct RngChanged
{
    Header header_;
    HostInteger<rng::LinearGenerator> previous_state_;

    static constexpr const auto t = Type::rng_changed;
};



struct Achievement
{
    Header header_;
    achievements::Achievement which_;
    static_assert(sizeof(which_) == 1);

    static constexpr const auto t = Type::achievement;
};



struct SoundCompleted
{
    Header header_;
#ifdef __GBA__
    host_u32 sound_name_ptr_;
#else
    host_u64 sound_name_ptr_;
#endif

    static constexpr const auto t = Type::sound_completed;
};



struct CannonSoundCompleted
{
    Header header_;

    static constexpr const auto t = Type::cannon_sound_completed;
};



struct MissileSoundCompleted
{
    Header header_;

    static constexpr const auto t = Type::missile_sound_completed;
};



struct HitSoundCompleted
{
    Header header_;

    static constexpr const auto t = Type::hit_sound_completed;
};



struct BirdLeftMap
{
    Header header_;
    u8 x_coord_ : 4;
    u8 y_coord_ : 4;
    host_s16 x_pos_;
    host_s16 y_pos_;
    u8 speed_[sizeof(Float)];
    HostInteger<Microseconds> flight_timer_;
    u8 color_ : 1;
    u8 flip_ : 1;
    u8 near_ : 1;

    static constexpr const auto t = Type::bird_left_map;
};



struct PlayerFireCreated
{
    Header header_;
    u8 x_ : 4;
    u8 y_ : 4;

    static constexpr const auto t = Type::player_fire_created;
};



struct OpponentFireCreated
{
    Header header_;
    u8 x_ : 4;
    u8 y_ : 4;

    static constexpr const auto t = Type::opponent_fire_created;
};



struct PlayerFireExtinguished
{
    Header header_;
    u8 x_ : 4;
    u8 y_ : 4;

    static constexpr const auto t = Type::player_fire_extinguished;
};



struct OpponentFireExtinguished
{
    Header header_;
    u8 x_ : 4;
    u8 y_ : 4;

    static constexpr const auto t = Type::opponent_fire_extinguished;
};



struct Lightning
{
    Header header_;
    static constexpr const auto t = Type::lightning;
};



struct LightningDone
{
    Header header_;
    static constexpr const auto t = Type::lightning_done;
};



struct WeatherChanged
{
    Header header_;
    u8 prev_weather_;
    static constexpr const auto t = Type::weather_changed;
};



struct PlayerRoomAiAwareness
{
    Header header_;
    u8 room_x_ : 4;
    u8 room_y_ : 4;
    bool prev_aware_;
    static constexpr const auto t = Type::player_room_ai_awareness;
};



struct OpponentRoomAiAwareness
{
    Header header_;
    u8 room_x_ : 4;
    u8 room_y_ : 4;
    bool prev_aware_;
    static constexpr const auto t = Type::opponent_room_ai_awareness;
};



struct Explosion
{
    Header header_;

    HostInteger<s16> x_;
    HostInteger<s16> y_;
    u8 half_angle_; // movement angle divided by 2 (to fit in a u8)
    u8 spd_;
    static constexpr const auto t = Type::explosion;
};



struct CoreExplosion
{
    Header header_;
    HostInteger<s16> x_;
    HostInteger<s16> y_;
    static constexpr const auto t = Type::core_explosion;
};



struct BoardingPodLanded
{
    Header header_;
    HostInteger<Microseconds> timer_;
    bool source_near_;
    u8 src_x_ : 4;
    u8 src_y_ : 4;
    u8 dst_x_ : 4;
    u8 dst_y_ : 4;
    u8 room_x_ : 4;
    u8 room_y_ : 4;
    HostInteger<u16> x_;
    HostInteger<u16> y_;

    static constexpr const auto t = Type::boarding_pod_landed;
};



struct MoveRegionBegin
{
    Header header_;
    static constexpr const auto t = Type::move_region_begin;
};



struct MoveRegionEnd
{
    Header header_;
    static constexpr const auto t = Type::move_region_end;
};



} // namespace skyland::time_stream::event
