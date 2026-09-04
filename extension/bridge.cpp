#include "voidfront_sim.hpp"
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <algorithm>
#include <cstdio>
#include <limits>

using namespace godot;

class VoidfrontBridge : public RefCounted {
    GDCLASS(VoidfrontBridge, RefCounted)
    vf::Sim simulation{1};
    uint32_t human_sequence = 0;
    uint32_t ai_sequence = 0;
    bool ai_enabled = true;
protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("reset", "seed", "ai"), &VoidfrontBridge::reset);
        ClassDB::bind_method(D_METHOD("advance"), &VoidfrontBridge::advance);
        ClassDB::bind_method(D_METHOD("snapshot"), &VoidfrontBridge::snapshot);
        ClassDB::bind_method(D_METHOD("issue", "order", "ids", "x", "z"), &VoidfrontBridge::issue);
        ClassDB::bind_method(D_METHOD("is_blocked", "x", "z"), &VoidfrontBridge::is_blocked);
    }
public:
    void reset(int64_t seed, bool ai) {
        simulation = vf::Sim(static_cast<uint32_t>(seed));
        human_sequence = 0; ai_sequence = 0; ai_enabled = ai;
    }
    void advance() {
        if (ai_enabled && simulation.tick() >= 100 && simulation.tick() % 20 == 0) {
            for (auto &command : vf::make_ai_commands(simulation, 1, ai_sequence)) simulation.submit(command);
        }
        simulation.step();
    }
    bool issue(int64_t order, PackedInt32Array ids, int64_t x, int64_t z) {
        if (order < 0 || order > 3 || ids.size() == 0 || ids.size() > 256 ||
            x < 0 || z < 0 || x >= vf::kMapWidth * vf::kScale || z >= vf::kMapHeight * vf::kScale) return false;
        vf::Command command{};
        command.tick = simulation.tick(); command.sequence = ++human_sequence;
        command.player = 0; command.order = static_cast<vf::Order>(order);
        command.x = static_cast<int32_t>(x); command.z = static_cast<int32_t>(z);
        for (int64_t i = 0; i < ids.size(); ++i) {
            if (ids[i] <= 0) return false;
            command.units.push_back(static_cast<uint32_t>(ids[i]));
        }
        std::sort(command.units.begin(), command.units.end());
        command.units.erase(std::unique(command.units.begin(), command.units.end()), command.units.end());
        return simulation.submit(command);
    }
    bool is_blocked(int64_t x, int64_t z) const {
        if (x < 0 || z < 0 || x >= vf::kMapWidth || z >= vf::kMapHeight) return true;
        return simulation.blocked(static_cast<int>(x), static_cast<int>(z));
    }
    Dictionary snapshot() const {
        Dictionary result;
        result["tick"] = simulation.tick(); result["winner"] = simulation.winner();
        char hash_text[17]; std::snprintf(hash_text, sizeof(hash_text), "%016llx", static_cast<unsigned long long>(simulation.hash()));
        result["hash"] = String(hash_text);
        Array units;
        for (const auto &unit : simulation.units()) {
            Dictionary row;
            row["id"] = unit.id; row["player"] = unit.player;
            row["x"] = unit.x; row["z"] = unit.z; row["hp"] = unit.hp;
            row["moving"] = unit.moving; row["target"] = unit.target_id;
            row["cooldown"] = unit.cooldown; row["order"] = static_cast<int>(unit.order);
            units.push_back(row);
        }
        result["units"] = units; return result;
    }
};

static void initialize(ModuleInitializationLevel level) {
    if (level == MODULE_INITIALIZATION_LEVEL_SCENE) GDREGISTER_CLASS(VoidfrontBridge);
}
static void uninitialize(ModuleInitializationLevel) {}
extern "C" GDExtensionBool GDE_EXPORT voidfront_library_init(GDExtensionInterfaceGetProcAddress address,
        GDExtensionClassLibraryPtr library, GDExtensionInitialization *initialization) {
    GDExtensionBinding::InitObject init(address, library, initialization);
    init.register_initializer(initialize); init.register_terminator(uninitialize);
    init.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
    return init.init();
}
