//
// Created by leoul on 2026-10-26.
//

#pragma once

#ifndef CUSTOM_MULTIPLAYER_SYNCHRONIZER_H
#define CUSTOM_MULTIPLAYER_SYNCHRONIZER_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

    class CustomMultiplayerSynchronizer : public Node {
        GDCLASS(CustomMultiplayerSynchronizer, Node)

    private:
        TypedArray<String> sync_properties;
        double sync_interval = 0.033; // ~30 ticks par seconde
        double _time_since_last_sync = 0.0;

        Node *_parent = nullptr;
        int authority_id = 1;

        void _sync_data();

    protected:
        static void _bind_methods();

    public:
        CustomMultiplayerSynchronizer();
        ~CustomMultiplayerSynchronizer();

        void set_sync_properties(const TypedArray<String> &props);
        TypedArray<String> get_sync_properties() const;

        void set_sync_interval(double interval);
        double get_sync_interval() const;

        void _ready() override;
        void _physics_process(double delta) override;
    };

} // namespace godot

#endif // CUSTOM_MULTIPLAYER_SYNCHRONIZER_H
