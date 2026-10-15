// Copyright (c) 2026 REA Cube World interoperability study contributors.
// Independent Omega-inspired rule prototypes, not extracted Cube World code.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace omega {
inline double valid_nonnegative(double value) {
    if (!std::isfinite(value) || value < 0) throw std::invalid_argument("expected finite nonnegative value");
    return value;
}

// v0.1.0 supersedes the October 12 War Frenzy percentages: 10/20/40/80%.
inline double war_frenzy_lifesteal_rate(int talent_level) {
    if (talent_level >= 11) return 0.80;
    if (talent_level >= 8) return 0.40;
    if (talent_level >= 4) return 0.20;
    return talent_level >= 1 ? 0.10 : 0.0;
}
inline double guardian_cyclone_reflect_rate(int talent_level) {
    if (talent_level >= 11) return 0.40;
    if (talent_level >= 8) return 0.20;
    return talent_level >= 4 ? 0.10 : 0.0;
}
inline double berserker_cyclone_bleed_rate(int talent_level) {
    if (talent_level >= 11) return 1.0;
    if (talent_level >= 8) return 0.50;
    return talent_level >= 4 ? 0.25 : 0.0;
}
struct HealResult { double healed; double health; };
inline HealResult war_frenzy_heal(double health, double max_health, double actual_damage,
                                   int talent_level, bool frenzy_active) {
    valid_nonnegative(health); valid_nonnegative(max_health); valid_nonnegative(actual_damage);
    if (health > max_health) throw std::invalid_argument("health exceeds max health");
    if (!frenzy_active || health == 0 || max_health == 0) return {0.0, health};
    const double healed = std::min(max_health - health,
                                   actual_damage * war_frenzy_lifesteal_rate(talent_level));
    return {healed, health + healed};
}
inline double guardian_cyclone_reflect(double blocked_damage, int talent_level,
                                      bool cyclone_active, bool block_succeeded) {
    valid_nonnegative(blocked_damage);
    if (!cyclone_active || !block_succeeded) return 0.0;
    return blocked_damage * guardian_cyclone_reflect_rate(talent_level);
}

// A per-attacker, per-target bleed refreshes its 3-second timer. Damage is
// distributed linearly for testability; Omega's actual tick cadence is unknown.
struct Bleed {
    std::uint64_t attacker;
    std::uint64_t target;
    double damage_budget;
    double elapsed = 0.0;
    double applied = 0.0;
};
class BleedTracker {
  public:
    void hit(std::uint64_t attacker, std::uint64_t target, double weapon_damage,
             int talent_level) {
        valid_nonnegative(weapon_damage);
        const double budget = weapon_damage * berserker_cyclone_bleed_rate(talent_level);
        if (budget == 0) return;
        auto it = std::find_if(effects_.begin(), effects_.end(), [&](const Bleed& b) {
            return b.attacker == attacker && b.target == target;
        });
        const Bleed next{attacker, target, budget, 0.0, 0.0};
        if (it == effects_.end()) effects_.push_back(next);
        else *it = next; // refresh, never add a second stack from this attacker
    }
    double advance(double seconds) {
        valid_nonnegative(seconds);
        double damage = 0;
        for (auto& effect : effects_) {
            effect.elapsed = std::min(3.0, effect.elapsed + seconds);
            const double cumulative = effect.damage_budget * (effect.elapsed / 3.0);
            damage += cumulative - effect.applied;
            effect.applied = cumulative;
        }
        effects_.erase(std::remove_if(effects_.begin(), effects_.end(), [](const Bleed& b) {
            return b.elapsed >= 3.0;
        }), effects_.end());
        return damage;
    }
    std::size_t active_count() const { return effects_.size(); }
    void clear_on_death(std::uint64_t target) {
        effects_.erase(std::remove_if(effects_.begin(), effects_.end(), [&](const Bleed& b) {
            return b.target == target;
        }), effects_.end());
    }
  private:
    std::vector<Bleed> effects_;
};

enum class RewardCategory { Weapon, Armor, Ring, Amulet };
// Omega 0.1.2: 40% weapon, 40% armor, 20% jewelry, evenly split.
inline RewardCategory generated_equipment_reward(int uniform_roll_0_to_99) {
    if (uniform_roll_0_to_99 < 0 || uniform_roll_0_to_99 > 99)
        throw std::invalid_argument("roll must be from 0 to 99");
    if (uniform_roll_0_to_99 < 40) return RewardCategory::Weapon;
    if (uniform_roll_0_to_99 < 80) return RewardCategory::Armor;
    if (uniform_roll_0_to_99 < 90) return RewardCategory::Ring;
    return RewardCategory::Amulet;
}
inline double equipped_enemy_droprate_after_reduction(double old_probability) {
    if (!std::isfinite(old_probability) || old_probability < 0 || old_probability > 1)
        throw std::invalid_argument("probability out of range");
    return old_probability * 0.25; // 75% reduction, not -75 percentage points
}

struct Stack { std::string item_key; std::uint64_t count; };
// Caller chooses exact key (item type + level + rarity + bindings etc.) and
// the inventory-wide cap, since the patch notes do not give a numeric cap.
inline std::vector<Stack> merge_inventory_stacks(const std::vector<Stack>& input,
                                                 std::uint64_t cap) {
    if (cap == 0) throw std::invalid_argument("stack cap must be positive");
    std::vector<Stack> output;
    for (const auto& stack : input) {
        if (stack.count == 0) continue;
        // Avoid overflowing summed ownership by moving items in bounded pieces.
        std::uint64_t remaining = stack.count;
        for (auto& dest : output) {
            if (dest.item_key != stack.item_key || dest.count >= cap) continue;
            const auto taken = std::min(cap - dest.count, remaining);
            dest.count += taken;
            remaining -= taken;
            if (!remaining) break;
        }
        while (remaining != 0) {
            if (output.size() >= 100000)
                throw std::length_error("inventory would exceed analysis stack budget");
            const auto taken = std::min(cap, remaining);
            output.push_back({stack.item_key, taken});
            remaining -= taken;
        }
    }
    return output;
}

struct Village {
    std::string id; // stable village identity; NOT the display name
    std::string name;
    std::int64_t world_x;
    std::int64_t world_y;
};
class RecentVillages {
  public:
    void visit(Village value) {
        if (value.id.empty()) throw std::invalid_argument("village ID required");
        items_.erase(std::remove_if(items_.begin(), items_.end(), [&](const Village& v) {
            return v.id == value.id;
        }), items_.end());
        items_.push_back(std::move(value));
        if (items_.size() > 10) items_.erase(items_.begin());
        cursor_ = items_.empty() ? 0 : items_.size() - 1;
    }
    const std::vector<Village>& items() const { return items_; }
    std::optional<Village> selected() const {
        if (items_.empty()) return std::nullopt;
        return items_[cursor_];
    }
    std::optional<Village> previous() {
        if (cursor_ > 0) --cursor_;
        return selected();
    }
    std::optional<Village> next() {
        if (!items_.empty() && cursor_ + 1 < items_.size()) ++cursor_;
        return selected();
    }
    void clear() { items_.clear(); cursor_ = 0; }
    // Snapshots can be stored by a future per-character loader adapter.
    std::vector<Village> snapshot() const { return items_; }
    void restore(const std::vector<Village>& saved) {
        clear();
        for (const auto& entry : saved) visit(entry);
    }
  private:
    std::vector<Village> items_; // oldest -> newest
    std::size_t cursor_ = 0;
};

struct SaleItem {
    std::uint64_t instance_id; // not item type: distinguishes identical instances
    std::uint64_t count;
    std::uint64_t unit_price;
    bool equipped;
};
class BulkSale {
  public:
    void toggle(const SaleItem& item, bool selected) {
        if (item.equipped && selected) throw std::invalid_argument("cannot sell equipped item");
        if (selected) selected_.insert(item.instance_id);
        else selected_.erase(item.instance_id);
    }
    void deselect() { selected_.clear(); }
    struct Result { std::uint64_t gold; std::size_t stacks; };
    Result quote(const std::vector<SaleItem>& inventory) const {
        std::uint64_t gold = 0;
        std::size_t stacks = 0;
        std::set<std::uint64_t> counted;
        for (const auto& item : inventory) {
            if (!counted.insert(item.instance_id).second)
                throw std::invalid_argument("duplicate inventory instance ID");
            if (!selected_.count(item.instance_id) || item.equipped)
                continue;
            if (item.unit_price > 0 && item.count > std::numeric_limits<std::uint64_t>::max()/item.unit_price)
                throw std::overflow_error("gold multiplication overflow");
            auto amount = item.count*item.unit_price;
            if (amount > std::numeric_limits<std::uint64_t>::max()-gold)
                throw std::overflow_error("gold sum overflow");
            gold += amount;
            ++stacks;
        }
        return {gold, stacks};
    }
    // Atomically computes the sale and removes whole selected, unequipped stacks.
    // The host adapter is responsible for reconciling this result to game state.
    Result commit(std::vector<SaleItem>& inventory, std::uint64_t& gold_balance) {
        const Result amount = quote(inventory); // may throw, leaving state unchanged
        if (amount.gold > std::numeric_limits<std::uint64_t>::max() - gold_balance)
            throw std::overflow_error("gold balance overflow");
        auto updated = inventory;
        updated.erase(std::remove_if(updated.begin(), updated.end(), [&](const SaleItem& item) {
            return !item.equipped && selected_.count(item.instance_id) != 0;
        }), updated.end());
        inventory.swap(updated);
        gold_balance += amount.gold;
        selected_.clear();
        return amount;
    }
  private:
    std::set<std::uint64_t> selected_;
};

enum class BuffCategory { Elixir, Temporary };
struct Buff { std::string id; BuffCategory category; };
inline void clear_effects_on_death(std::vector<Buff>& buffs) {
    buffs.erase(std::remove_if(buffs.begin(), buffs.end(), [](const Buff& value) {
        return value.category != BuffCategory::Elixir;
    }), buffs.end());
}

struct Voxel {
    int x, y, z;
    bool operator<(const Voxel& b) const {
        if (x!=b.x) return x<b.x;
        if (y!=b.y) return y<b.y;
        return z<b.z;
    }
    bool operator==(const Voxel& b) const {
        return x==b.x && y==b.y && z==b.z;
    }
};
enum class EquipmentKind { Weapon, Armor };
class VoxelEditor {
  public:
    explicit VoxelEditor(std::vector<Voxel> original, EquipmentKind type)
      : original_(std::move(original)), current_(original_), type_(type) {
        canonicalize(original_);
        current_ = original_;
        if (current_.empty()) throw std::invalid_argument("equipment requires at least one voxel");
        min_ = max_ = original_.front();
        for (const auto& v : original_) {
            min_.x=std::min(min_.x,v.x); min_.y=std::min(min_.y,v.y); min_.z=std::min(min_.z,v.z);
            max_.x=std::max(max_.x,v.x); max_.y=std::max(max_.y,v.y); max_.z=std::max(max_.z,v.z);
        }
        states_.push_back(current_);
    }
    bool add(Voxel voxel) {
        if (!within_bounds(voxel) || std::binary_search(current_.begin(),current_.end(),voxel)) return false;
        auto next=current_; next.push_back(voxel); canonicalize(next); return mutate(std::move(next));
    }
    bool remove(Voxel voxel) {
        if (current_.size() <= 1) return false; // v0.1.2: never remove last voxel
        auto next=current_;
        auto it=std::lower_bound(next.begin(),next.end(),voxel);
        if (it==next.end() || !(*it==voxel)) return false;
        next.erase(it); return mutate(std::move(next));
    }
    bool reset_after_hold(std::uint64_t milliseconds) {
        if (milliseconds < 2000) return false;
        return mutate(original_); // reset is itself undoable
    }
    bool undo() {
        if (cursor_==0) return false;
        current_=states_[--cursor_]; return true;
    }
    bool redo() {
        if (cursor_+1>=states_.size()) return false;
        current_=states_[++cursor_]; return true;
    }
    void record_material_cubes(std::uint64_t added) {
        if (added > std::numeric_limits<std::uint64_t>::max()-material_cubes_)
            throw std::overflow_error("material tracking overflow");
        material_cubes_+=added;
    }
    std::uint64_t retained_material_cubes() const { return material_cubes_; }
    const std::vector<Voxel>& model() const { return current_; }
    std::size_t undo_count() const { return cursor_; }
  private:
    static void canonicalize(std::vector<Voxel>& v) {
        std::sort(v.begin(),v.end());v.erase(std::unique(v.begin(),v.end()),v.end());
    }
    bool within_bounds(const Voxel& v) const {
        const int room = type_==EquipmentKind::Weapon ? 4 : 2;
        return v.x>=min_.x-room && v.x<=max_.x+room &&
               v.y>=min_.y-room && v.y<=max_.y+room &&
               v.z>=min_.z-room && v.z<=max_.z+room;
    }
    bool mutate(std::vector<Voxel> next) {
        if (next==current_) return false;
        states_.resize(cursor_+1); // abandon redo branch on new edit
        states_.push_back(std::move(next));
        if (states_.size()>65) { states_.erase(states_.begin()); } // 64 undo steps
        cursor_=states_.size()-1;
        current_=states_[cursor_];
        return true;
    }
    std::vector<Voxel> original_, current_;
    EquipmentKind type_;
    Voxel min_{},max_{};
    std::vector<std::vector<Voxel>> states_;
    std::size_t cursor_=0;
    std::uint64_t material_cubes_=0;
};
} // namespace omega
