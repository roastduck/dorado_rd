#include <set>
#include <cmath>
#include <vector>
#include <string>
#include <fstream>
#include <typeinfo>
#include <exception>
#include <algorithm>
#include <unordered_map>
#include "sdk.h"
#include "const.h"
#include "filter.h"
#include "console.h"

namespace rdai {

/********************************/
/*     Cacher                   */
/********************************/

#ifdef CACHE_BEGIN
    #error CACHE_BEGIN defined
#endif
#ifdef CACHE_END
    #error CACHE_END defined
#endif

#define CACHE_BEGIN(type)     static std::unordered_map<const void*,type> cached_value_;     static int cached_round_ = -1, cached_camp_ = -1;     if (console->round() != cached_round_ || console->camp() != cached_camp_)     {         cached_round_ = console->round();         cached_camp_ = console->camp();         cached_value_.clear();     }     if (cached_value_.count(this))         return cached_value_[this];

#define CACHE_END(ret)     return cached_value_[this] = (ret);

// DO NOT USE BACKSLASH

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
    return atan(x * 2) / pi * 2;
}

template <class T>
inline T sqr(T x)
{
    return x*x;
}

/********************************/
/*     Global Variables         */
/********************************/

const double BUY_HERO_THRESHOLD = 0.4;

const double HP_STRENGTH_FACTOR = 1.0;
const double MP_STRENGTH_FACTOR = 1.0;
const double ATK_STRENGTH_FACTOR = 1.0;
const double DEF_STRENGTH_FACTOR = 0.7;
const double SPEED_STRENGTH_FACTOR = 0.5;
const double RANGE_STRENGTH_FACTOR = 0.9;
const double OBSERVER_FACTOR_RATE = 0.1;

const double HP_VALUE_FACTOR = 1.0;
const double DEF_VALUE_FACTOR = 0.9;
const double ATK_VALUE_FACTOR = 1.0;
const double MP_VALUE_FACTOR = 1.0;
const double DIZZY_VALUE_RATE = 1.5;
const double WAITREVIVE_VALUE_RATE = 5.0;
const double WINORDIE_VALUE_RATE = 5.0;
const double ISMINING_VALUE_RATE = 2.0;

const double DANGER_FACTOR = 1.0;
const double ABILITY_FACTOR = 1.0;

const double MINE_THRESHOLD = 0.0; // remember we have this
const double MINE_DIS_FACTOR = 0.1;

const int JOIN_DIS2_THRESHOLD = 100;
const int ENEMY_JOIN_DIS2 = 100;

const double GOBACK_THRESHOLD = 0.2;

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
    int id;
    Character(int _id);

    PUnit *get_entity();
    const PUnit *get_entity() const;

    virtual void attack(const EGroup &target);
    virtual void move(const Pos &p);
};

class HammerGuard : public Character
{
public:
    HammerGuard(int _id) : Character(_id) {}
    virtual void attack(const EGroup &target);
};

class Master : public Character
{
public:
    Master(int _id) : Character(_id) {}
    virtual void move(const Pos &p);
};

class Berserker : public Character
{
public:
    Berserker(int _id) : Character(_id) {}
    virtual void attack(const EGroup &target);
};

class Scouter : public Character
{
public:
    Scouter(int _id) : Character(_id) {}
    virtual void move(const Pos &p);
    virtual void attack(const EGroup &target);
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
    Character *character;
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
    ~Unit();
    
    double strength_factor() const;
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
    
    double danger_factor() const;
    double value_factor() const;
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
    
    double ability_factor() const;
    double health_factor() const;
};

class EGroup : public Group<EGroup, EUnit> // Enemy Group
{
public:
    void add_adj_members_recur(EUnit *unit);

    //functions below return in [0,1]
    double danger_factor() const;
    double value_factor() const;
    double mine_factor() const;
    
    void logMsg() const;
};

class FGroup : public Group<FGroup, FUnit> // Friend Group
{
    bool checkGoback();
    bool checkAttack();
    bool checkMine();
    bool checkSearch();

public:
    double ability_factor() const;

    void action();
    
    bool checkJoin();
    bool checkSplit();
    
    double health_factor() const;
};

/********************************/
/*     Conductor                */
/********************************/

// fake Conductor object
#ifdef conductor
    #error conductor defined
#endif
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
    std::unordered_map<int, EUnit*> eUnitObj;
    std::unordered_map<int, FUnit*> fUnitObj;
    std::unordered_map<int, PUnit*> pUnits;

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
    void check_upgrade_hero();

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
    
    double map_danger_factor(const Pos &p) const;

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

void Character::attack(const EGroup &target)
{
    const EUnit *targetUnit(0);
    double val(-INFINITY);
    if (get_entity()->findSkill("attack")->cd == 0)
        for (const EUnit *e : target.get_member())
        {
            if (dis2(get_entity()->pos, e->get_entity()->pos) > get_entity()->range) continue;
            double _val = e->value_factor();
            if (_val > val)
                val = _val, targetUnit = e;
        }
    if (! targetUnit)
        for (const EUnit *e : target.get_member())
        {
            double _val = e->value_factor();
            if (_val > val)
                val = _val, targetUnit = e;
        }
    if (targetUnit)
    {
        mylog << "UnitAction : Unit " << id << " : attack unit " << targetUnit->get_id() << std::endl;
        console->attack(targetUnit->get_entity(), get_entity());
    }
}

void Character::move(const Pos &p)
{
    mylog << "UnitAction : Unit " << id << " : move " << p << std::endl;
    console->move(p, get_entity());
}

void HammerGuard::attack(const EGroup &target)
{
    if (get_entity()->mp >= HAMMERATTACK_MP && get_entity()->findSkill("hammerattack")->cd == 0)
    {
        const EUnit *targetUnit(0);
        double val(-INFINITY);
        for (const EUnit *e : target.get_member())
        {
            double _val = e->danger_factor();
            if (_val > val && dis2(get_entity()->pos, e->get_entity()->pos) <= HAMMERATTACK_RANGE)
                val = _val, targetUnit = e;
        }
        if (targetUnit)
        {
            mylog << "UnitAction : Unit " << id << " : hammerattack unit " << targetUnit->get_id() << std::endl;
            console->useSkill("hammerattack", targetUnit->get_entity(), get_entity());
        } else
            Character::attack(target);
    } else
        Character::attack(target);
}

void Master::move(const Pos &p)
{
    if (
        conductor.get_f_unit(id)->get_belongs()->get_member().size() == 1 && 
        get_entity()->mp >= BLINK_MP && get_entity()->findSkill("blink")->cd == 0
       )
    {
        Pos q(get_entity()->pos), _p(q+(p-q)*std::min(1, (int)(sqrt(BLINK_RANGE)/dis(p,q))));
        mylog << "UnitAction : Unit " << id << " : blink " << _p << std::endl;
        console->useSkill("blink", _p, get_entity());
    } else
        Character::move(p);
}

void Berserker::attack(const EGroup &target)
{
    if (! console->getBuff("beattacked", get_entity()) && get_entity()->findSkill("sacrifice")->cd == 0)
    {
        const EUnit *targetUnit(0);
        double val(-INFINITY);
        if (get_entity()->findSkill("attack")->cd == 0)
            for (const EUnit *e : target.get_member())
            {
                if (dis2(get_entity()->pos, e->get_entity()->pos) > get_entity()->range) continue;
                double _val = e->value_factor();
                if (_val > val)
                    val = _val, targetUnit = e;
            }
        if (! targetUnit)
            for (const EUnit *e : target.get_member())
            {
                double _val = e->value_factor();
                if (_val > val)
                    val = _val, targetUnit = e;
            }
        if (targetUnit && dis2(targetUnit->get_entity()->pos, get_entity()->pos) <= get_entity()->range)
        {
            mylog << "UnitAction : Unit " << id << " : sacrifice" << std::endl;
            console->useSkill("sacrifice", NULL, get_entity());
        } else
            Character::attack(target);
    } else
        Character::attack(target);
}

void Scouter::move(const Pos &p)
{
    if (get_entity()->mp >= SET_OBSERVER_MP && get_entity()->findSkill("setobserver")->cd == 0)
    {
        UnitFilter filter;
        filter.setAreaFilter(new Circle(get_entity()->pos, SET_OBSERVER_RANGE), "a");
        filter.setTypeFilter("mine", "a");
        auto mines = console->enemyUnits(filter);
        if (! mines.empty())
        {
            Pos _p;
            do
                _p = console->randPosInArea(mines.front()->pos, MINING_RANGE);
            while (! Circle(get_entity()->pos, SET_OBSERVER_RANGE).contain(_p));
            mylog << "UnitAction : Unit " << id << " : set observer " << _p << std::endl;
            console->useSkill("setobserver", _p, get_entity());
        } else
            Character::move(p);
    } else
        Character::move(p);
}

void Scouter::attack(const EGroup &target)
{
    if (get_entity()->mp >= SET_OBSERVER_MP && get_entity()->findSkill("setobserver")->cd == 0)
    {
        UnitFilter filter;
        filter.setAreaFilter(new Circle(get_entity()->pos, SET_OBSERVER_RANGE), "a");
        filter.setTypeFilter("mine", "a");
        auto mines = console->enemyUnits(filter);
        if (! mines.empty())
        {
            Pos _p;
            do
                _p = console->randPosInArea(mines.front()->pos, MINING_RANGE);
            while (! Circle(get_entity()->pos, SET_OBSERVER_RANGE).contain(_p));
            mylog << "UnitAction : Unit " << id << " : set observer " << _p << std::endl;
            console->useSkill("setobserver", _p, get_entity());
        } else
            Character::attack(target);
    } else
        Character::attack(target);
}

/********************************/
/*     Unit Implement           */
/********************************/

template <class CampGroup, class CampUnit>
Unit<CampGroup, CampUnit>::Unit(int _id)
    : id(_id), belongs(0)
{
    std::string name = lowerCase(conductor.get_p_unit(id)->name);
    if (name == "hammerguard")
        character = new HammerGuard(id);
    else if (name == "master")
        character = new Master(id);
    else if (name == "berserker")
        character = new Berserker(id);
    else if (name == "scouter")
        character = new Scouter(id);
    else
        character = new Character(id);
}

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

template <class CampGroup, class CampUnit>
Unit<CampGroup, CampUnit>::~Unit()
{
    delete character;
}

template <class CampGroup, class CampUnit>
double Unit<CampGroup, CampUnit>::strength_factor() const
{
    CACHE_BEGIN(double);
    if (lowerCase(get_entity()->name) == "mine") return 0;
    
    double val(0), ava(0), tot(0);

    console->selectUnit(get_entity());

    if (lowerCase(get_entity()->name) == "observer")
    {
        ava += console->unitArg("hp","c") * HP_STRENGTH_FACTOR * OBSERVER_FACTOR_RATE;
        tot += console->unitArg("hp","m") * HP_STRENGTH_FACTOR;
        val += console->unitArg("def","c") * DEF_STRENGTH_FACTOR;
    } else
    {
        ava += console->unitArg("hp","c") * HP_STRENGTH_FACTOR;
        tot += console->unitArg("hp","m") * HP_STRENGTH_FACTOR;
        if (get_entity()->isHero())
        {
            ava += console->unitArg("mp","c") * MP_STRENGTH_FACTOR;
            tot += console->unitArg("mp","m") * MP_STRENGTH_FACTOR;
        }
        val += console->unitArg("atk","c") * ATK_STRENGTH_FACTOR;
        val += console->unitArg("def","c") * DEF_STRENGTH_FACTOR;
        if (! get_entity()->isBase() && ! get_entity()->isMine())
            val += console->unitArg("speed","c") * SPEED_STRENGTH_FACTOR;
    }
    console->selectUnit(0);

    CACHE_END(val * ava / tot);
}

double EUnit::value_factor() const
{
    CACHE_BEGIN(double);
    if (lowerCase(get_entity()->name) == "mine") return 0;
    
    double danger(0), hp(0), def(0);

    console->selectUnit(get_entity());

    // may consider recover rate
    if (lowerCase(get_entity()->name) == "observer")
    {
        hp += console->unitArg("hp","c") * HP_VALUE_FACTOR;
        def += console->unitArg("def","c") * DEF_VALUE_FACTOR;
    } else
    {
        hp += console->unitArg("hp","c") * HP_VALUE_FACTOR;
        if (get_entity()->isHero())
        {
            danger += console->unitArg("mp","c") * MP_VALUE_FACTOR;
        }
        danger += console->unitArg("atk","c") * ATK_VALUE_FACTOR;
        def += console->unitArg("def","c") * DEF_VALUE_FACTOR;
    }
    if (console->getBuff("dizzy", get_entity())) danger *= DIZZY_VALUE_RATE;
    if (console->getBuff("waitrevive", get_entity())) danger *= WAITREVIVE_VALUE_RATE;
    if (console->getBuff("winordie", get_entity())) danger *= WINORDIE_VALUE_RATE;
    if (console->getBuff("ismining", get_entity())) danger *= ISMINING_VALUE_RATE;

    console->selectUnit(0);

    CACHE_END(danger / inf_1(hp * def / 5000));
}

inline double EUnit::danger_factor() const
{
    return strength_factor() * DANGER_FACTOR;
}

inline double FUnit::ability_factor() const
{
    return strength_factor() * ABILITY_FACTOR;
}

inline double FUnit::health_factor() const
{
    return (double)console->unitArg("hp","c",get_entity()) / console->unitArg("hp","m",get_entity());
}

EUnit::EUnit(int _id)
    : Unit<EGroup, EUnit>(_id) {}

FUnit::FUnit(int _id)
    : Unit<FGroup, FUnit>(_id), madeAction(-1) {}

void FUnit::attack(const EGroup &target)
{
    if (madeAction == console->round()) return;
    
    character->attack(target);
    
    madeAction = console->round();
}

void FUnit::move(const Pos &p)
{
    if (madeAction == console->round()) return;
    
    character->move(p);
    
    madeAction = console->round();
}

void FUnit::mine(const EGroup &target)
{
    if (madeAction == console->round()) return;
    
    auto member = target.get_member();
    if (member.size() == 1 && lowerCase(member.front()->get_entity()->name) == "mine")
        move(member.front()->get_entity()->pos);
    else
        attack(target);
    
    madeAction = console->round();
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
        (new Circle(unit->get_entity()->pos, ENEMY_JOIN_DIS2), "a");
    // including mine
    for (const PUnit *item : console->enemyUnits(filter))
        if (! idExist(item->id))
            add_adj_members_recur(conductor.get_e_unit(item->id));
}

template <class CampGroup, class CampUnit>
void Group<CampGroup, CampUnit>::logMsg() const
{
    mylog << "GroupMember : " << typeid(CampGroup).name() << " : Group " << groupId << " : { ";
    for (const CampUnit *u : member)
        mylog << u->id << ", ";
    mylog << "}" << std::endl;
}

void EGroup::logMsg() const
{
    Group<EGroup, EUnit>::logMsg();
    mylog << "GroupStatus : EGroup : Group " << groupId << " mine_factor = " << mine_factor() << std::endl;
}

double EGroup::danger_factor() const
{
    CACHE_BEGIN(double);
    double ret(0);
    for (const EUnit *e : member)
        ret += e->danger_factor();
    CACHE_END(inf_1(ret / 200));
}

double FGroup::ability_factor() const
{
    CACHE_BEGIN(double);
    double ret(0);
    for (const FUnit *e : member)
        ret += e->ability_factor();
    CACHE_END(inf_1(ret / 200));
}

double FGroup::health_factor() const
{
    CACHE_BEGIN(double);
    double tc(0), tm(0);
    for (const FUnit *e : member)
    {
        tc += console->unitArg("hp","c",e->get_entity());
        tm += console->unitArg("hp","m",e->get_entity());
    }
    CACHE_END(tc / tm);
}

double EGroup::value_factor() const
{
    CACHE_BEGIN(double);
    double ret(0);
    for (const EUnit *e : member)
        ret += e->value_factor();
    CACHE_END(inf_1(ret / 200));
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

bool FGroup::checkGoback()
{
    if (health_factor() >= GOBACK_THRESHOLD) return false;
    for (FUnit *u : member)
        u->move(MILITARY_BASE_POS[console->camp()]);
    mylog << "GroupAction : Group " << groupId << " : go back " << std::endl;
    return true;
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
    mylog << "GroupAction : Group " << groupId << " : mine Group " << target->groupId << std::endl;
    return true;
}

bool FGroup::checkJoin()
{
    mylog << "GroupStatus : FGroup : Group " << groupId << " : health_factor() = " << health_factor() << std::endl;
    if (member.empty() || health_factor() < GOBACK_THRESHOLD) return false;
    FGroup *target = 0;
    for (FGroup &g : const_cast<std::vector<FGroup>&>(conductor.get_f_groups()))
        if (
            ! g.member.empty() && g.groupId != groupId &&
            conductor.map_danger_factor(g.center()) > conductor.map_danger_factor(center()) &&
            dis2(g.center(), center()) <= JOIN_DIS2_THRESHOLD
           )
        {
            target = &g;
            break;
        }
    if (! target) return false;
    for (FUnit *u : member)
        target->add_member(u);
    member.clear(), idSet.clear();
    mylog << "GroupAction : Group " << groupId << " : join Group " << target->groupId << std::endl;
    return true;
}

bool FGroup::checkSplit()
{
    if (member.size() < 2 || health_factor() < GOBACK_THRESHOLD) return false;
    FGroup newGroup;
    std::vector<FUnit*> _member;
    for (int i=member.size()/2; i>0; i--)
    {
        if (member.back()->health_factor() < GOBACK_THRESHOLD)
        {
            newGroup.add_member(member.back());
            idSet.erase(member.back()->id);
            mylog << "UnitStatus : Unit " << member.back()->id
                  << " : health_factor() = " << member.back()->health_factor()
                  << " , hp = " << member.back()->get_entity()->hp << std::endl;
        }
        else
            _member.push_back(member.back());
        member.pop_back();
    }
    for (auto u : _member)
        member.push_back(u);
    if (newGroup.member.empty()) return false;
    mylog << "GroupAction : Group " << groupId << " : split " << std::endl;
    const_cast<std::vector<FGroup>&>(conductor.get_f_groups()).push_back(std::move(newGroup)); // keep this the last line
    return true;
}

bool FGroup::checkSearch()
{
    for (FUnit *u : member)
        u->move(MINE_POS[0]);
    mylog << "GroupAction : Group " << groupId << " : search " << std::endl;
    return true;
}

void FGroup::action()
{
    logMsg();
    if (checkGoback()) return;
    if (checkAttack()) return;
    if (checkMine()) return;
    if (checkSearch()) return;
}

/********************************/
/*     Conductor Implement      */
/********************************/

inline Character::Character(int _id)
    : id(_id) {}

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

double Conductor::map_danger_factor(const Pos &p) const
{
    double x(p.x), y(p.y), tot(0);
    tot += 2.0 / pow(dis2(Pos(x,y), MILITARY_BASE_POS[1-console->camp()]), 0.1);
    for (int i=0; i<4; i++)
        tot += 0.7 / pow(dis2(Pos(x,y), Dragon_pos[i]), 0.2);
    for (int i=0; i<2; i++)
        tot += 0.7 / pow(dis2(Pos(x,y), Roshan_pos[i]), 0.2);
    return log(inf_1(pow(tot, 3.0) / 2)) * 4;
}

void Conductor::enemy_make_groups()
{
    eGroups.clear();
    for (const PUnit *item : console->enemyUnits())
        if (! get_e_unit(item->id)->get_belongs())
        {
            eGroups.push_back(EGroup());
            eGroups.back().add_adj_members_recur(get_e_unit(item->id));
            eGroups.back().logMsg();
        }
}

void Conductor::check_buy_hero()
{
    while (true)
    {
        if (need_buy_hero() < BUY_HERO_THRESHOLD) return;
        double hammerguard = need_buy_hammerguard(),
               master = need_buy_master(),
               berserker = need_buy_berserker(),
               scouter = need_buy_scouter();
        double theMax = std::max(std::max(std::max(hammerguard,master),berserker),scouter);
        if (theMax == hammerguard && NEW_HAMMERGUARD_COST*(hammerguardCnt+1) <= console->gold() - console->goldCostCurrentRound())
            console->chooseHero("hammerguard"), hammerguardCnt++;
        else if (theMax == master && NEW_MASTER_COST*(masterCnt+1) <= console->gold() - console->goldCostCurrentRound())
            console->chooseHero("master"), masterCnt++;
        else if (theMax == berserker && NEW_BERSERKER_COST*(berserkerCnt+1) <= console->gold() - console->goldCostCurrentRound())
            console->chooseHero("berserker"), berserkerCnt++;
        else if (theMax == scouter && NEW_SCOUTER_COST*(scouterCnt+1) <= console->gold() - console->goldCostCurrentRound()) 
            console->chooseHero("scouter"), scouterCnt++;
        else
            break;
    }
}

void Conductor::check_buyback_hero()
{
    // TODO
}

void Conductor::check_callback_hero()
{
    // TODO
}

void Conductor::check_upgrade_hero()
{
    while (true)
    {
        UnitFilter filter;
        filter.setAreaFilter
            (new Circle(MILITARY_BASE_POS[console->camp()], LEVELUP_RANGE), "a");
        filter.setAvoidFilter("militarybase");
        const PUnit *target(0);
        int cost;
        for (const PUnit *item : console->friendlyUnits(filter))
        {
            int _cost = LEVELUP_COST_PER_LEVEL * item->level + LEVELUP_COST_BASE;
            if (! target || _cost < cost)
                cost = _cost, target = item;
        }
        if (cost < console->gold() - console->goldCostCurrentRound())
            console->buyHeroLevel(target);
        else
            break;
    }
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
    check_upgrade_hero();

    UnitFilter filter;
    filter.setAvoidFilter("militarybase");
    filter.setAvoidFilter("observer");
    for (const PUnit *u : console->friendlyUnits(filter))
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
#undef CACHE_BEGIN
#undef CACHE_END

