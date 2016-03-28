#include <set>
#include <map>
#include <cmath>
#include <vector>
#include <exception>
#include <algorithm>
//#include "Pos.h"
#include "sdk.h"
#include "const.h"
#include "filter.h"
#include "console.h"

namespace rdai {

/********************************/
/*     Global Variables         */
/********************************/

const double ENEMY_GROUP_FACTOR = 1.5;

const double BUY_HERO_THRESHOLD = 0.4;

const double HP_DANGER_FACTOR = 1.0;
const double MP_DANGER_FACTOR = 1.0;
const double ATK_DANGER_FACTOR = 1.0;
const double DEF_DANGER_FACTOR = 0.7;
const double SPEED_DANGER_FACTOR = 0.5;
const double RANGE_DANGER_FACTOR = 0.9;

static Console *console = 0;

/********************************/
/*     Characters               */
/********************************/

template <class CampGroup, class CampUnit>
class Unit;
template <class CampGroup, class CampUnit>
class Group;

class EUnit;
class FUnit;
class EGroup;
class FGroup;

class Character // base class. default action
{
    // remember to use virtual member
public:
    int id, typeId;
    Character(int _id);

    virtual int get_group_range() const;

    virtual void attack(const EGroup &target);
    virtual void move(const Pos &p);
    virtual void mine(const EGroup &target);
};

/********************************/
/*     Group and Unit           */
/********************************/

template <class CampGroup, class CampUnit>
class Unit
{
    friend Group<CampGroup, CampUnit>;
    friend CampGroup;

protected:
    int id;
    Character character;
    CampGroup *belongs;

public:
    int get_id() const
    {
        return id;
    }

    const CampGroup *get_belongs() const
    {
        return belongs;
    }

    Unit(int _id);
    Unit(const Unit<CampGroup, CampUnit> &) = delete;
    Unit<CampGroup, CampUnit> &operator=(const Unit<CampGroup, CampUnit> &) = delete;
    Unit(Unit<CampGroup, CampUnit> &&) = delete;
    Unit<CampGroup, CampUnit> &operator=(Unit<CampGroup, CampUnit> &&) = delete;
};

template <class CampGroup, class CampUnit>
class Group
{
protected:
    std::vector<CampUnit*> member;
    std::set<int> idSet;

public:
    ~Group();

    bool idExist(int id) const
    {
        return idSet.count(id);
    }

    const std::vector<CampUnit*> &get_member() const
    {
        return member;
    }

    void add_member(CampUnit *unit);

    Group() {}
    Group(const Group<CampGroup, CampUnit> &other) = delete;
    Group<CampGroup, CampUnit> &operator=(const Group<CampGroup, CampUnit> &other) = delete;

    Group(Group<CampGroup, CampUnit> &&other)
        : member(std::move(other.member)), idSet(std::move(other.idSet))
    {
        for (CampUnit *u : member)
            u->belongs = (CampGroup*) this;
        other.member.clear();
    }

    Group<CampGroup, CampUnit> &operator=(Group<CampGroup, CampUnit> &&other)
    {
        member = std::move(other.member), idSet = std::move(other.idSet);
        for (CampUnit *u : member)
            u->belongs = (CampGroup*) this;
        other.member.clear();
        return *this;
    }
};

class EUnit : public Unit<EGroup, EUnit> // Enemy Unit
{
    friend EGroup;

public:
    EUnit(int _id);
    EUnit(const EUnit &) = delete;
    EUnit &operator=(const EUnit &) = delete;
    EUnit(EUnit &&) = delete;
    EUnit &operator=(EUnit &&) = delete;
};

class FUnit : public Unit<FGroup, FUnit> // Friend Unit
{
    friend FGroup;

    void attack(const EGroup &target);
    void move(const Pos &p);
    void mine(const EGroup &target);

public:
    FUnit(int _id);
    FUnit(const FUnit &) = delete;
    FUnit &operator=(const FUnit &) = delete;
    FUnit(FUnit &&) = delete;
    FUnit &operator=(FUnit &&) = delete;
};

class EGroup : public Group<EGroup, EUnit> // Enemy Group
{
public:
    void add_adj_members_recur(EUnit *unit);

    //functions below return in [0,1]
    double danger_factor() const;
    double mine_factor() const;
};

class FGroup : public Group<FGroup, FUnit> // Friend Group
{
public:
    void action();
};

/********************************/
/*     Conductor                */
/********************************/

// fake Conductor object
#define conductor (Conductor::get_instance())

class Conductor
{
    static Conductor instance[2];

    const PMap *map;
    const PPlayerInfo *info;
    PCommand *cmd;

    int hammerguardCnt, masterCnt, berserkerCnt, scouterCnt;

public:
    Conductor()
        : map(0), info(0), cmd(0), hammerguardCnt(0), masterCnt(0), berserkerCnt(0), scouterCnt(0) {}

    static Conductor &get_instance()
    {
        return instance[console->camp()];
    }

private:
    std::map<int, EUnit*> eUnitObj;
    std::map<int, FUnit*> fUnitObj;
    std::map<int, PUnit*> pUnits;

    std::vector<EGroup> eGroups;
    std::vector<FGroup> fGroups;

    // functions below return value in [0,1]
    double need_buy_hammerguard() const;
    double need_buy_master() const;
    double need_buy_berserker() const;
    double need_buy_scouter() const;
    double need_buy_hero() const;

    void enemy_make_groups();
    void check_buy_hero();
    void check_buyback_hero();
    void check_callback_hero();
    void check_update_hero();

public:
    const PMap &get_map() const
    {
        return *map;
    }

    const PPlayerInfo &get_info() const
    {
        return *info;
    }
    
    const PCommand &get_cmd() const
    {
        return *cmd;
    }

    EUnit *get_e_unit(int id)
    {
        if (! eUnitObj.count(id))
            eUnitObj[id] = new EUnit(id);
        return eUnitObj[id];
    }

    FUnit *get_f_unit(int id)
    {
        if (! fUnitObj.count(id))
            fUnitObj[id] = new FUnit(id);
        return fUnitObj[id];
    }
    
    PUnit *get_p_unit(int id)
    {
        return pUnits[id];
    }

    const std::vector<EGroup> &get_e_groups() const
    {
        return eGroups;
    }

    const std::vector<FGroup> &get_f_groups() const
    {
        return fGroups;
    }

    void init(const PMap &_map, const PPlayerInfo &_info, PCommand &_cmd);
    void work();
} Conductor::instance[2];

/********************************/
/*     Character Implement      */
/********************************/

int Character::get_group_range() const
{
    return conductor.get_p_unit(id)->range * ENEMY_GROUP_FACTOR;
}

void Character::attack(const EGroup &target)
{
    int targetId = -1, curdis2;
    for (const EUnit *e : target.get_member())
    {
        int newdis2 = dis2(conductor.get_p_unit(e->get_id())->pos, conductor.get_p_unit(id)->pos);
        if (! ~targetId || newdis2 < curdis2)
            curdis2 = newdis2, targetId = e->get_id();
    }
    if (! ~targetId)
        for (const EUnit *e : target.get_member())
        {
            std::vector<Pos> path;
            findShortestPath
                (conductor.get_map(), conductor.get_p_unit(id)->pos, conductor.get_p_unit(e->get_id())->pos, path);
            int newdis2 = length(path);
            if (! ~targetId || newdis2 < curdis2)
                curdis2 = newdis2, targetId = e->get_id();
        }
    console->attack(conductor.get_p_unit(targetId), conductor.get_p_unit(id));
}

void Character::move(const Pos &p)
{
    console->move(p, conductor.get_p_unit(id));
}

void Character::mine(const EGroup &target)
{
    auto member = target.get_member();
    if (member.size() == 1 && lowerCase(conductor.get_p_unit(member.front()->get_id())->name) == "mine")
        move(conductor.get_p_unit(member.front()->get_id())->pos);
    else
        attack(target);
}

/********************************/
/*     Unit Implement           */
/********************************/

template <class CampGroup, class CampUnit>
Unit<CampGroup, CampUnit>::Unit(int _id)
    : id(_id), character(id), belongs(0) {}

EUnit::EUnit(int _id)
    : Unit<EGroup, EUnit>(_id) {}

FUnit::FUnit(int _id)
    : Unit<FGroup, FUnit>(_id) {}

void FUnit::attack(const EGroup &target)
{
    character.attack(target);
}

void FUnit::move(const Pos &p)
{
    character.move(p);
}

void FUnit::mine(const EGroup &target)
{
    character.mine(target);
}

/********************************/
/*     Group Implement          */
/********************************/

template <class CampGroup, class CampUnit>
Group<CampGroup, CampUnit>::~Group()
{
    for (CampUnit *u : member)
        u->belongs = 0;
}

template <class CampGroup, class CampUnit>
void Group<CampGroup, CampUnit>::add_member(CampUnit *unit)
{
    member.push_back(unit);
    idSet.insert(unit->id);
    unit->belongs = (CampGroup*)this;
}

void EGroup::add_adj_members_recur(EUnit *unit)
{
    add_member(unit);
    UnitFilter filter;
    filter.setAreaFilter
        (new Circle(conductor.get_p_unit(unit->id)->pos, unit->character.get_group_range()), "a");
    // including mine
    for (const PUnit *item : console->enemyUnits(filter))
        if (! idExist(item->id))
            add_adj_members_recur(conductor.get_e_unit(item->id));
}

double EGroup::danger_factor() const
{
    double cur(0), max(0);
    for (const EUnit *e : member)
    {
        console->selectUnit(conductor.get_p_unit(e->id));

        // may consider recover rate
        if (lowerCase(conductor.get_p_unit(e->id)->name) == "observer")
        {
            cur += console->unitArg("hp","c") * HP_DANGER_FACTOR;
            max += console->unitArg("hp","m") * HP_DANGER_FACTOR;
            cur += console->unitArg("def","c") * DEF_DANGER_FACTOR;
            max += console->unitArg("def","m") * DEF_DANGER_FACTOR;
        } else
        {
            cur += console->unitArg("hp","c") * HP_DANGER_FACTOR;
            max += console->unitArg("hp","m") * HP_DANGER_FACTOR;
            if (conductor.get_p_unit(e->id)->isHero())
            {
                cur += console->unitArg("mp","c") * MP_DANGER_FACTOR;
                max += console->unitArg("mp","m") * MP_DANGER_FACTOR;
            }
            cur += console->unitArg("atk","c") * ATK_DANGER_FACTOR;
            max += console->unitArg("atk","m") * ATK_DANGER_FACTOR;
            cur += console->unitArg("def","c") * DEF_DANGER_FACTOR;
            max += console->unitArg("def","m") * DEF_DANGER_FACTOR;
            cur += console->unitArg("speed","c") * SPEED_DANGER_FACTOR;
            max += console->unitArg("speed","m") * SPEED_DANGER_FACTOR;
        }
        
        console->selectUnit(0);

    }
    return cur / max;
}

double EGroup::mine_factor() const
{
    for (const EUnit *_u : member)
    {
        const PUnit *p = conductor.get_p_unit(_u->id);
        if (lowerCase(p->name) == "mine" && console->unitArg("energy", "c", p) > 0)
            return 1.0;
    }
    return 0.0;
}

void FGroup::action()
{
    const EGroup *target = 0;
    double cur_factor = 0.0;
    for (const EGroup &g : conductor.get_e_groups())
    {
        double new_factor = g.mine_factor() / g.danger_factor();
        if (new_factor > cur_factor)
            cur_factor = new_factor, target = &g;
    }
    if (cur_factor)
        for (FUnit *u : member)
            u->mine(*target);
    else
        for (FUnit *u : member)
            u->move(MINE_POS[0]);
}

/********************************/
/*     Conductor Implement      */
/********************************/

inline Character::Character(int _id)
    : id(_id), typeId(conductor.get_p_unit(id)->typeId) {}

double Conductor::need_buy_hammerguard() const
{
    return exp(-hammerguardCnt);
}

double Conductor::need_buy_master() const
{
    return exp(-masterCnt);
}

double Conductor::need_buy_berserker() const
{
    return exp(-berserkerCnt);
}

double Conductor::need_buy_scouter() const
{
    return exp(-scouterCnt);
}

double Conductor::need_buy_hero() const
{
    return (double)console->gold() / console->property();
}

void Conductor::enemy_make_groups()
{
    eGroups.clear();
    for (const PUnit *item : console->enemyUnits())
        if (! get_e_unit(item->id)->get_belongs())
        {
            eGroups.push_back(EGroup());
            eGroups.back().add_adj_members_recur(get_e_unit(item->id));
        }
}

void Conductor::check_buy_hero()
{
    if (need_buy_hero() < BUY_HERO_THRESHOLD) return;
    double hammerguard = need_buy_hammerguard(),
           master = need_buy_master(),
           berserker = need_buy_berserker(),
           scouter = need_buy_scouter();
    double theMax = std::max(std::max(std::max(hammerguard,master),berserker),scouter);
    if (theMax == hammerguard && NEW_HAMMERGUARD_COST*(hammerguardCnt+1) < console->gold())
        console->chooseHero("hammerguard"), hammerguardCnt++;
    else if (theMax == master && NEW_MASTER_COST*(masterCnt+1) < console->gold())
        console->chooseHero("master"), masterCnt++;
    else if (theMax == berserker && NEW_BERSERKER_COST*(berserkerCnt+1) < console->gold())
        console->chooseHero("berserker"), berserkerCnt++;
    else if (theMax == scouter && NEW_SCOUTER_COST*(scouterCnt+1) < console->gold()) 
        console->chooseHero("scouter"), scouterCnt++;
}

void Conductor::check_buyback_hero()
{
    // TODO
}
void Conductor::check_callback_hero()
{
    // TODO
}

void Conductor::check_update_hero()
{
    // TODO
}

void Conductor::init(const PMap &_map, const PPlayerInfo &_info, PCommand &_cmd)
{
    map = &_map, info = &_info, cmd = &_cmd;
    pUnits.clear();
    for (const auto &u : info->units)
        pUnits[u.id] = const_cast<PUnit*>(&u);
    
    enemy_make_groups();
}

void Conductor::work()
{
    check_buy_hero();
    check_buyback_hero();
    check_callback_hero();
    check_update_hero();

    for (const PUnit *u : console->friendlyUnits())
    {
        FUnit *obj = conductor.get_f_unit(u->id);
        if (! obj->get_belongs())
        {
            fGroups.push_back(FGroup());
            fGroups.back().add_member(obj);
        }
    }
    
    for (FGroup &g : fGroups)
        g.action();
}

} // namespace rdai

/********************************/
/*     Main interface           */
/********************************/

void player_ai(const PMap &map, const PPlayerInfo &info, PCommand &cmd)
{
    using namespace rdai;

    console = new Console(map, info, cmd);

    try
    {
        conductor.init(map, info, cmd);
        conductor.work();
    } catch (const std::exception &e)
    {
    }

    delete console;
    console = 0;
}

#undef conductor

