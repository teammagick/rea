#include "omega_rules.hpp"
#include <stdexcept>
#define CHECK(c) do { if (!(c)) throw std::runtime_error("CHECK failed: " #c); } while (0)
#include <cmath>
#include <iostream>
#include <numeric>
#include <set>

using namespace omega;
static void near(double a, double b) { CHECK(std::abs(a-b)<1e-8); }

int main() {
    // The 0.1.0 increase overrides the October 12 proposal, including thresholds.
    for (auto [level, expected] : std::vector<std::pair<int,double>>{
        {0,0},{1,.10},{3,.10},{4,.20},{7,.20},{8,.40},{10,.40},{11,.80},{50,.80}}) {
        near(war_frenzy_lifesteal_rate(level), expected);
    }
    near(war_frenzy_heal(25,100,100,11,true).health,100); // healing capped by missing health
    near(war_frenzy_heal(25,100,10,4,true).healed,2);
    near(war_frenzy_heal(25,100,10,11,false).healed,0);
    near(war_frenzy_heal(0,100,10,11,true).healed,0); // no resurrection
    near(guardian_cyclone_reflect(100,4,true,true),10);
    near(guardian_cyclone_reflect(100,8,true,true),20);
    near(guardian_cyclone_reflect(100,11,true,true),40);
    near(guardian_cyclone_reflect(100,11,false,true),0);
    near(guardian_cyclone_reflect(100,11,true,false),0);

    BleedTracker bleeding;
    bleeding.hit(1,10,100,11); bleeding.hit(1,10,100,11);
    CHECK(bleeding.active_count()==1);
    near(bleeding.advance(1),100.0/3);
    bleeding.hit(1,10,100,11); // refresh rather than stack
    CHECK(bleeding.active_count()==1);
    near(bleeding.advance(3),100);
    CHECK(bleeding.active_count()==0);
    bleeding.hit(1,10,100,4);bleeding.hit(2,10,100,8);
    CHECK(bleeding.active_count()==2);
    near(bleeding.advance(3),75);
    bleeding.hit(1,10,100,4); bleeding.clear_on_death(10);
    CHECK(bleeding.active_count()==0);

    int counts[4]={};
    for (int i=0;i<100;++i) counts[(int)generated_equipment_reward(i)]++;
    CHECK(counts[0]==40 && counts[1]==40 && counts[2]==10 && counts[3]==10);
    near(equipped_enemy_droprate_after_reduction(.8),.2);

    std::vector<Stack> stacks={{"wood",10},{"stone",5},{"wood",6},{"wood",6},{"stone",10}};
    auto merged=merge_inventory_stacks(stacks,12);
    std::uint64_t wood=0,stone=0;
    for (const auto& s: merged) {
        CHECK(s.count<=12 && s.count>0);
        if(s.item_key=="wood") wood+=s.count;
        if(s.item_key=="stone") stone+=s.count;
    }
    CHECK(wood==22 && stone==15 && merged.size()==4);
    try {
        (void)merge_inventory_stacks({{"a",100001}},1);
        CHECK(false);
    } catch (const std::length_error&) {}
    auto too_many=merge_inventory_stacks({{"a",std::numeric_limits<std::uint64_t>::max()}},
                                         std::numeric_limits<std::uint64_t>::max());
    CHECK(too_many.size()==1 && too_many[0].count==std::numeric_limits<std::uint64_t>::max());

    RecentVillages history;
    CHECK(!history.selected());
    for (int i=0;i<12;++i) history.visit({std::to_string(i),"Town",i,0});
    CHECK(history.items().size()==10 && history.items().front().id=="2");
    history.visit({"4","Renamed Village",44,9});
    CHECK(history.items().size()==10 && history.items().back().name=="Renamed Village");
    history.previous(); CHECK(history.selected()->id=="11");
    history.next(); CHECK(history.selected()->id=="4");
    RecentVillages restored;
    restored.restore(history.snapshot());
    CHECK(restored.items().size()==10 && restored.selected()->id=="4");

    BulkSale sale;
    SaleItem sword{1,1,50,true},potions{2,3,10,false},gem{3,2,25,false};
    try { sale.toggle(sword,true); CHECK(false); } catch (const std::invalid_argument&) {}
    sale.toggle(potions,true);sale.toggle(gem,true);
    try { (void)sale.quote({potions,potions}); CHECK(false); }
    catch (const std::invalid_argument&) {}
    auto quoted=sale.quote({sword,potions,gem});
    CHECK(quoted.gold==80 && quoted.stacks==2);
    std::vector<SaleItem> inventory={sword,potions,gem};
    std::uint64_t balance=20;
    auto transaction=sale.commit(inventory,balance);
    CHECK(transaction.gold==80 && balance==100);
    CHECK(inventory.size()==1 && inventory[0].instance_id==1);
    CHECK(sale.quote(inventory).gold==0);
    std::vector<Buff> buffs={{"poison",BuffCategory::Temporary},{"bleed",BuffCategory::Temporary},{"power",BuffCategory::Elixir}};
    clear_effects_on_death(buffs);
    CHECK(buffs.size()==1 && buffs[0].id=="power");

    VoxelEditor weapon({{0,0,0}},EquipmentKind::Weapon);
    weapon.record_material_cubes(5);
    CHECK(!weapon.remove({0,0,0})); // can't delete final voxel
    CHECK(weapon.add({4,0,0})); CHECK(!weapon.add({5,0,0})); // expanded bounds
    CHECK(!weapon.reset_after_hold(1999));
    CHECK(weapon.reset_after_hold(2000));
    CHECK(weapon.model().size()==1 && weapon.retained_material_cubes()==5);
    CHECK(weapon.undo() && weapon.model().size()==2);
    CHECK(weapon.redo() && weapon.model().size()==1);
    VoxelEditor armor({{0,0,0}},EquipmentKind::Armor);
    CHECK(armor.add({2,2,2}) && !armor.add({3,0,0}));
    int edits = 0;
    for (int i=0;i<100;++i) { // high-volume editing with 64 step bound
        if (weapon.add({i%9-4,(i/9)%9-4,(i/81)%9-4})) ++edits;
    }
    CHECK(edits>=95);
    CHECK(weapon.undo_count()<=64);
    std::cout << "Omega-inspired core rules: all checks passed\n";
}
