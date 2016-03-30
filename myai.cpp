#include <set>
#include <map>
#include <cmath>
#include <vector>
#include <fstream>
#include <exception>
#include <algorithm>
#include "sdk.h"
#include "const.h"
#include "filter.h"
#include "console.h"

namespace rdai {

/********************************/
/*     Logger                   */
/********************************/

#ifdef RD_LOG_FILE
    std::ofstream mylog("mylog.txt");
#else
    class NullBuffer : public std::streambuf
    {
    public:
        int overflow(int c) { return c; }
    } bull_buff;
    std::ostream mylog(&bull_buff);
#endif // RD_LOG_FILE    

/********************************/
/*     Math Helper              */
/********************************/

const double pi = acos(-1);

inline double inf_1(double x)
{
    return atan(x) / pi * 2;
}

/********************************/
/*     Global Variables         */
/********************************/

const double ENEMY_GROUP_FACTOR = 1.5;

const double BUY_HERO_THRESHOLD = 0.4;

const double HP_STRENGTH_FACTOR = 1.0;
const double MP_STRENGTH_FACTOR = 1.0;
const double ATK_STRENGTH_FACTOR = 1.0;
const double DEF_STRENGTH_FACTOR = 0.7;
const double SPEED_STRENGTH_FACTOR = 0.5;
const double RANGE_STRENGTH_FACTOR = 0.9;

const double DANGER_FACTOR = 1.0;
const double ABILITY_FACTOR = 1.0;

const double MINE_THRESHOLD = 0.2;
const double MINE_DIS_FACTOR = 0.1;

const double JOIN_THRESHOLD = 0.4;
const double JOIN_DIS_FACTOR = 0.4;
const int JOIN_DIS2_THRESHOLD = 100;

const double SPLIT_THRESHOLD = 0.9;

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

    PUnit *get_entity();
    const PUnit *get_entity() const;

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

    PUnit *get_entity();
    const PUnit *get_entity() const;

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
    static int groupIdCnt;

protected:
    std::vector<CampUnit*> member;
    std::set<int> idSet;

public:
    int groupId;
    
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

    Group() : groupId(++groupIdCnt) {}
    Group(const Group<CampGroup, CampUnit> &other) = delete;
    Group<CampGroup, CampUnit> &operator=(const Group<CampGroup, CampUnit> &other) = delete;

    Group(Group<CampGroup, CampUnit> &&other)
        : member(std::move(other.member)), idSet(std::move(other.idSet)), groupId(other.groupId)
    {
        for (CampUnit *u : member)
            u->belongs = (CampGroup*) this;
        other.member.clear();
    }

    Group<CampGroup, CampUnit> &operator=(Group<CampGroup, CampUnit> &&other)
    {
        member = std::move(other.member), idSet = std::move(other.idSet), groupId = other.groupId;
        for (CampUnit *u : member)
            u->belongs = (CampGroup*) this;
        other.member.clear();
        return *this;
    }

    Pos center() const
    {
        Pos ret(0, 0);
        for (const CampUnit *u : member)
            ret = ret + u->get_entity()->pos;
        return Pos(ret.x / member.size(), ret.y / member.size());
    }

    double strength_factor() const;
    void logMsg() const;
};

template <class CampGroup, class CampUnit>
int Group<CampGroup, CampUnit>::groupIdCnt;

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
    
    int madeAction; // = round

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
    bool checkAttack();
    bool checkMine();
    bool checkSearch();

public:
    double ability_factor() const;

    void action();
    
    bool checkJoin();
    bool checkSplit();
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

inline PUnit *Character::get_entity()
{
    return conductor.get_p_unit(id);
}

inline const PUnit *Character::get_entity() const
{
    return conductor.get_p_unit(id);
}

int Character::get_group_range() const
{
    return get_entity()->range * ENEMY_GROUP_FACTOR;
}

void Character::attack(const EGroup &target)
{
    int targetId = -1, curdis2;
    for (const EUnit *e : target.get_member())
    {
        int newdis2 = dis2(e->get_entity()->pos, get_entity()->pos);
        if (! ~targetId || newdis2 < curdis2)
            curdis2 = newdis2, targetId = e->get_id();
    }
    if (! ~targetId)
        for (const EUnit *e : target.get_member())
        {
            std::vector<Pos> path;
            findShortestPath
                (conductor.get_map(), get_entity()->pos, e->get_entity()->pos, path);
            int newdis2 = length(path);
            if (! ~targetId || newdis2 < curdis2)
                curdis2 = newdis2, targetId = e->get_id();
        }
    console->attack(conductor.get_p_unit(targetId), get_entity());
}

void Character::move(const Pos &p)
{
    console->move(p, get_entity());
}

void Character::mine(const EGroup &target)
{
    auto member = target.get_member();
    if (member.size() == 1 && lowerCase(member.front()->get_entity()->name) == "mine")
        move(member.front()->get_entity()->pos);
    else
        attack(target);
}

/********************************/
/*     Unit Implement           */
/********************************/

template <class CampGroup, class CampUnit>
Unit<CampGroup, CampUnit>::Unit(int _id)
    : id(_id), character(id), belongs(0) {}

template <class CampGroup, class CampUnit>
inline PUnit *Unit<CampGroup, CampUnit>::get_entity()
{
    return conductor.get_p_unit(id);
}

template <class CampGroup, class CampUnit>
inline const PUnit *Unit<CampGroup, CampUnit>::get_entity() const
{
    return conductor.get_p_unit(id);
}

EUnit::EUnit(int _id)
    : Unit<EGroup, EUnit>(_id) {}

FUnit::FUnit(int _id)
    : Unit<FGroup, FUnit>(_id), madeAction(-1) {}

void FUnit::attack(const EGroup &target)
{
    if (madeAction == console->round()) return;
    madeAction = console->round();
    character.attack(target);
}

void FUnit::move(const Pos &p)
{
    if (madeAction == console->round()) return;
    madeAction = console->round();
    character.move(p);
}

void FUnit::mine(const EGroup &target)
{
    if (madeAction == console->round()) return;
    madeAction = console->round();
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
        (new Circle(unit->get_entity()->pos, unit->character.get_group_range()), "a");
    // including mine
    for (const PUnit *item : console->enemyUnits(filter))
        if (! idExist(item->id))
            add_adj_members_recur(conductor.get_e_unit(item->id));
}

template <class CampGroup, class CampUnit>
double Group<CampGroup, CampUnit>::strength_factor() const
{
    double ret(0);
    for (const CampUnit *e : member)
    {
        double val(0), ava(0), tot(0);

        console->selectUnit(e->get_entity());

        // may consider recover rate
        if (lowerCase(e->get_entity()->name) == "observer")
        {
            ava += console->unitArg("hp","c") * HP_STRENGTH_FACTOR;
            tot += console->unitArg("hp","m") * HP_STRENGTH_FACTOR;
            val += console->unitArg("def","c") * DEF_STRENGTH_FACTOR;
        } else
        {
            ava += console->unitArg("hp","c") * HP_STRENGTH_FACTOR;
            tot += console->unitArg("hp","m") * HP_STRENGTH_FACTOR;
            if (e->get_entity()->isHero())
            {
                ava += console->unitArg("mp","c") * MP_STRENGTH_FACTOR;
                tot += console->unitArg("mp","m") * MP_STRENGTH_FACTOR;
            }
            val += console->unitArg("atk","c") * ATK_STRENGTH_FACTOR;
            val += console->unitArg("def","c") * DEF_STRENGTH_FACTOR;
            if (! e->get_entity()->isBase() && ! e->get_entity()->isMine())
                val += console->unitArg("speed","c") * SPEED_STRENGTH_FACTOR;
        }
        ret += val * ava / tot;

        console->selectUnit(0);
    }
    return inf_1(ret);
}

template <class CampGroup, class CampUnit>
void Group<CampGroup, CampUnit>::logMsg() const
{
    mylog << "Action : Group " << groupId << " : { ";
    for (const CampUnit *u : member)
        mylog << u->id << ", ";
    mylog << "}" << std::endl;
}

double EGroup::danger_factor() const
{
    return strength_factor() * DANGER_FACTOR;
}

double FGroup::ability_factor() const
{
    return strength_factor() * ABILITY_FACTOR;
}

double EGroup::mine_factor() const
{
    for (const EUnit *_u : member)
    {
        const PUnit *p = _u->get_entity();
        if (lowerCase(p->name) == "mine" && console->unitArg("energy", "c", p) > 0)
            return 1.0;
    }
    return 0.0;
}

bool FGroup::checkAttack()
{
    // TODO
    return false;
}

bool FGroup::checkMine()
{
    const EGroup *target = 0;
    double cur_factor = 0.0;
    for (const EGroup &g : conductor.get_e_groups())
    {
        double new_factor = g.mine_factor() / g.danger_factor();
        new_factor = inf_1(new_factor / inf_1(dis2(center(), g.center()) * MINE_DIS_FACTOR));
        if (new_factor > MINE_THRESHOLD && new_factor > cur_factor)
            cur_factor = new_factor, target = &g;
    }
    if (! cur_factor) return false;
    for (FUnit *u : member)
        u->mine(*target);
    mylog << "Action : Group " << groupId << " : mine Group " << target->groupId << std::endl;
    return true;
}

bool FGroup::checkJoin()
{
    FGroup *target = 0;
    double this_ability = ability_factor();
    double cur_factor = 0.0;
    for (FGroup &g : const_cast<std::vector<FGroup>&>(conductor.get_f_groups()))
        if (! g.member.empty())
        {
            double new_factor = inf_1(g.ability_factor() * this_ability);
            new_factor = inf_1(new_factor / inf_1(dis2(center(), g.center()) * JOIN_DIS_FACTOR));
            if (new_factor < JOIN_THRESHOLD && new_factor > cur_factor)
                cur_factor = new_factor, target = &g;
        }
    if (! cur_factor) return false;
    std::vector<FUnit*> _member;
    for (FUnit *u : member)
        if (dis2(u->get_entity()->pos, target->center()) < JOIN_DIS2_THRESHOLD)
            target->add_member(u);
        else
            _member.push_back(u), u->move(target->center());
    member = std::move(_member);
    mylog << "Action : Group " << groupId << " : join Group " << target->groupId << std::endl;
    return true;
}

bool FGroup::checkSplit()
{
    if (ability_factor() < SPLIT_THRESHOLD || member.size() < 2) return false;
    FGroup newGroup;
    for (int i=member.size()/2; i>0; i--)
    {
        newGroup.add_member(member.back());
        member.pop_back();
    }
    const_cast<std::vector<FGroup>&>(conductor.get_f_groups()).push_back(std::move(newGroup));
    mylog << "Action : Group " << groupId << " : split " << std::endl;
    return true;
}

bool FGroup::checkSearch()
{
    for (FUnit *u : member)
        u->move(MINE_POS[0]);
    mylog << "Action : Group " << groupId << " : search " << std::endl;
    return true;
}

void FGroup::action()
{
    logMsg();
    if (checkAttack()) return;
    if (checkMine()) return;
    if (checkSearch()) return;
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

    UnitFilter filterAvoidBase;
    filterAvoidBase.setAvoidFilter("militarybase");
    for (const PUnit *u : console->friendlyUnits(filterAvoidBase))
    {
        FUnit *obj = conductor.get_f_unit(u->id);
        if (! obj->get_belongs())
        {
            fGroups.push_back(FGroup());
            fGroups.back().add_member(obj);
        }
    }
    
    for (size_t i=0; i<fGroups.size(); i++) // do use id
        if (! fGroups[i].get_member().empty())
            if (! fGroups[i].checkJoin()) fGroups[i].checkSplit();
    for (auto i=fGroups.begin(); i!=fGroups.end(); i++)
        if (i->get_member().empty())
        {
            std::vector<FGroup> _fGroups;
            for (FGroup &g : fGroups)
                if (! g.get_member().empty())
                    _fGroups.push_back(std::move(g));
            fGroups = std::move(_fGroups);
            break;
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
    
    mylog << "Round " << console->round() << std::endl;

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

