#pragma once

#include "number/random.hpp"
#include "readyScene.hpp"
#include "skyland/network.hpp"
#include "worldScene.hpp"



// This scene performs a handshake to coordinate rng state, to ensure that an
// rng sequence is synchronized between two game sessions.
//
// Step 1) The primary (host) device waits, while the secondary sends out sync
// requests.
//
// Step 2) The primary receives a sync request, and sends out a sync message.
//
// Step 3) The secondary receives the sync message, and sends a sync ack.
//
// Step 4) The primary receives the sync ack, and, if it matches the original
// sync message, sends its own sync ack. The primary now returns to the game.
//
// Step 5) The secondary receives the primary's sync ack. The secondary now
// returns to the game.



namespace skyland {



class CoopRngSyncScene : public WorldScene, public network::Listener {
public:
    ScenePtr<Scene> update(Platform& pfrm, App& app, Microseconds delta)
    {
        network::poll_messages(pfrm, app, *this);

        if (syncd_) {
            return scene_pool::alloc<ReadyScene>();
        }

        if (not pfrm.network_peer().is_host()) {
            timer_ += delta;
            if (timer_ > milliseconds(100)) {
                timer_ = 0;
                network::packet::CoopRngSyncRequest p;
                network::transmit(pfrm, p);
            }
        }

        return null_scene();
    }


    void receive(Platform& pfrm,
                 App& app,
                 const network::packet::CoopRngSyncRequest&)
    {
        if (pfrm.network_peer().is_host()) {
            network::packet::CoopRngSync p;
            p.rng_state_.set(rng::critical_state);
            network::transmit(pfrm, p);
        }
    }


    void receive(Platform& pfrm,
                 App& app,
                 const network::packet::CoopRngSyncAck& ack)
    {
        if (pfrm.network_peer().is_host()) {
            if (ack.rng_state_.get() == rng::critical_state) {
                syncd_ = true;

                network::packet::CoopRngSyncAck p;
                p.rng_state_.set(rng::critical_state);
                network::transmit(pfrm, p);
            }
        } else {
            syncd_ = true;
        }
    }


private:
    Microseconds timer_ = 0;
    bool syncd_ = false;
};



} // namespace skyland