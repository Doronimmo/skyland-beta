#pragma once

#include "decoration.hpp"
#include "skyland/systemString.hpp"
#include "skyland/tile.hpp"
#include "synth.hpp"



// A Speaker! When selected, plays notes from attached synth blocks!



namespace skyland {



class Speaker : public Decoration
{
public:
    Speaker(Island* parent, const Vec2<u8>& position);


    void update(Platform& pfrm, App& app, Microseconds delta) override;


    static void format_description(Platform& pfrm, StringBuffer<512>& buffer);


    static u32 properties()
    {
        return Decoration::properties();
    }


    static const char* name()
    {
        return "speaker";
    }


    static SystemString ui_name()
    {
        return SystemString::block_speaker;
    }


    static Vec2<u8> size()
    {
        return {1, 1};
    }


    static Icon icon()
    {
        return 1800;
    }


    static Icon unsel_icon()
    {
        return 1816;
    }


    void render_interior(App& app, u8 buffer[16][16]) override;


    void render_exterior(App& app, u8 buffer[16][16]) override;


    ScenePtr<Scene>
    select(Platform& pfrm, App& app, const Vec2<u8>& cursor) override;


    // If signal, the speaker will select the next block below it when finished.
    void play(Platform& pfrm, bool signal = true);


    void reset(Platform& pfrm, bool resume_music = true);


    void finalize(Platform& pfrm, App& app) override;


    struct Settings
    {
        Platform::Speaker::ChannelSettings square_1_;
        Platform::Speaker::ChannelSettings square_2_;
        Platform::Speaker::ChannelSettings noise_;
        u16 wave_;
    } settings_;


    using EffectVector = Bitvector<2 * 16 * 4>;


    struct EffectFlags
    {
        EffectVector vector_;


        int index(int channel, int i) const
        {
            auto channel_offset = 16 * channel;
            auto index_offset = i * 2;

            return channel_offset + index_offset;
        }


        void store(int channel, int location, Platform::Speaker::Effect effect)
        {
            vector_.set(index(channel, location), (u8)effect & 1);
            vector_.set(index(channel, location) + 1, (u8)effect & 2);
        }


        Platform::Speaker::Effect load(int channel, int location)
        {
            u8 val = 0;

            if (vector_[index(channel, location)]) {
                val |= 1;
            }

            if (vector_[index(channel, location) + 1]) {
                val |= 2;
            }

            return (Platform::Speaker::Effect)val;
        }
    };


    EffectFlags& effect_flags()
    {
        return effect_flags_;
    }


    Synth* square_1() const;
    Synth* square_2() const;
    Synth* wave() const;
    Synth* noise() const;


private:
    void process_effects();


    Platform::Speaker::Effect load_effect(int channel);


    EffectFlags effect_flags_;

    u8 playing_ : 1;
    u8 end_music_ : 1;
    u8 signal_ : 1;
    u8 reserved_ : 5;

    u8 unused_ : 8;

    // Index into the list of notes, 0->16
    s8 index_ = -1;

    Microseconds timer_ = 0;
};



} // namespace skyland
