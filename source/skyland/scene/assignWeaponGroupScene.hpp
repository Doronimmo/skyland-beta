#pragma once


#include "skyland/room.hpp"
#include "worldScene.hpp"



namespace skyland {



class AssignWeaponGroupScene : public ActiveWorldScene {
public:
    ScenePtr<Scene>
    update(Platform& pfrm, App& app, Microseconds delta) override;


    void enter(Platform& pfrm, App& app, Scene& prev) override;
    void exit(Platform& pfrm, App& app, Scene& next) override;


    void display(Platform& pfrm, App& app) override;


private:
    enum class State {
        select_group,
        assign_rooms,
    } state_ = State::assign_rooms;

    Room::Group current_group_ = Room::Group::one;

    u8 group_cursor_;

    std::optional<Text> msg_;
};



} // namespace skyland