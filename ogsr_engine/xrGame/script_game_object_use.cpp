#include "stdafx.h"
#include "script_game_object.h"
#include "UsableScriptObject.h"
#include "GameObject.h"
#include "script_engine.h"
#include "stalker_planner.h"
#include "ai/stalker/ai_stalker.h"
#include "searchlight.h"
#include "script_callback_ex.h"
#include "game_object_space.h"
#include "memory_manager.h"
#include "enemy_manager.h"
#include "movement_manager.h"
#include "patrol_path_manager.h"
#include "PHCommander.h"
#include "PHScriptCall.h"
#include "PHSimpleCalls.h"
#include "phworld.h"
#include "doors_manager.h"

void CScriptGameObject::SetTipText(LPCSTR tip_text)
{
    CUsableScriptObject* l_tpUseableScriptObject = smart_cast<CUsableScriptObject*>(&object());
    if (!l_tpUseableScriptObject)
        ai().script_engine().script_log(ScriptStorage::eLuaMessageTypeError, "SetTipText. Reason: the object is not usable");
    else
        l_tpUseableScriptObject->set_tip_text(tip_text);
}

void CScriptGameObject::SetTipTextDefault()
{
    CUsableScriptObject* l_tpUseableScriptObject = smart_cast<CUsableScriptObject*>(&object());
    if (!l_tpUseableScriptObject)
        ai().script_engine().script_log(ScriptStorage::eLuaMessageTypeError, "SetTipTextDefault . Reason: the object is not usable");
    else
        l_tpUseableScriptObject->set_tip_text_default();
}

void CScriptGameObject::SetNonscriptUsable(bool nonscript_usable)
{
    CUsableScriptObject* l_tpUseableScriptObject = smart_cast<CUsableScriptObject*>(&object());
    if (!l_tpUseableScriptObject)
        ai().script_engine().script_log(ScriptStorage::eLuaMessageTypeError, "SetNonscriptUsable . Reason: the object is not usable");
    else
        l_tpUseableScriptObject->set_nonscript_usable(nonscript_usable);
}

Fvector CScriptGameObject::GetCurrentDirection()
{
    CProjector* obj = smart_cast<CProjector*>(&object());
    if (!obj)
    {
        ai().script_engine().script_log(ScriptStorage::eLuaMessageTypeError, "Script Object : cannot access class member GetCurrentDirection!");
        return Fvector().set(0.f, 0.f, 0.f);
    }
    return obj->GetCurrentDirection();
}

CProjector* CScriptGameObject::GetProjector()
{
    CProjector* obj = smart_cast<CProjector*>(&object());
    if (!obj)
    {
        ai().script_engine().script_log(ScriptStorage::eLuaMessageTypeError, "Script Object : cannot access class member GetCurrentDirection!");
        return nullptr;
    }
    return obj;
}

void CScriptGameObject::SwitchProjector(bool _on)
{
    CProjector* obj = smart_cast<CProjector*>(&object());
    if (!obj)
    {
        ai().script_engine().script_log(ScriptStorage::eLuaMessageTypeError, "Script Object : cannot access class member GetCurrentDirection!");
        return;
    }
    if (_on)
        obj->TurnOn();
    else
        obj->TurnOff();
}

CScriptGameObject::CScriptGameObject(CGameObject* game_object) : m_game_object(game_object), m_door(0) { R_ASSERT2(m_game_object, "Null actual object passed!"); }

CScriptGameObject::~CScriptGameObject()
{
    if (!m_door)
        return;

    unregister_door();
}

CScriptGameObject* CScriptGameObject::Parent() const
{
    CGameObject* l_tpGameObject = smart_cast<CGameObject*>(object().H_Parent());
    if (l_tpGameObject)
        return (l_tpGameObject->lua_game_object());
    else
        return (0);
}

int CScriptGameObject::clsid() const { return (object().clsid()); }

LPCSTR CScriptGameObject::Name() const { return (*object().cName()); }

shared_str CScriptGameObject::cName() const { return (object().cName()); }

LPCSTR CScriptGameObject::Section() const { return (*object().cNameSect()); }

void CScriptGameObject::Kill(CScriptGameObject* who)
{
    CEntity* l_tpEntity = smart_cast<CEntity*>(&object());
    if (!l_tpEntity)
    {
        ai().script_engine().script_log(ScriptStorage::eLuaMessageTypeError, "%s cannot access class member Kill!", *object().cName());
        return;
    }
    if (!l_tpEntity->AlreadyDie())
        l_tpEntity->KillEntity(who ? who->object().ID() : object().ID());
    else
        ai().script_engine().script_log(ScriptStorage::eLuaMessageTypeError, "attempt to kill dead object %s", *object().cName());
}

bool CScriptGameObject::Alive() const
{
    CEntity* entity = smart_cast<CEntity*>(&object());
    if (!entity)
    {
        ai().script_engine().script_log(ScriptStorage::eLuaMessageTypeError, "CSciptEntity : cannot access class member Alive!");
        return (false);
    }
    return (!!entity->g_Alive());
}

ALife::ERelationType CScriptGameObject::GetRelationType(CScriptGameObject* who)
{
    CEntityAlive* l_tpEntityAlive1 = smart_cast<CEntityAlive*>(&object());
    if (!l_tpEntityAlive1)
    {
        ai().script_engine().script_log(ScriptStorage::eLuaMessageTypeError, "%s cannot access class member GetRelationType!", *object().cName());
        return ALife::eRelationTypeDummy;
    }

    CEntityAlive* l_tpEntityAlive2 = smart_cast<CEntityAlive*>(&who->object());
    if (!l_tpEntityAlive2)
    {
        ai().script_engine().script_log(ScriptStorage::eLuaMessageTypeError, "%s cannot apply GetRelationType method for non-alive object!", *who->object().cName());
        return ALife::eRelationTypeDummy;
    }

    return l_tpEntityAlive1->tfGetRelationType(l_tpEntityAlive2);
}

bool CScriptGameObject::IsRelationEnemy(CScriptGameObject* who)
{
    CEntityAlive* l_tpEntityAlive1 = smart_cast<CEntityAlive*>(&object());
    if (!l_tpEntityAlive1)
    {
        ai().script_engine().script_log(ScriptStorage::eLuaMessageTypeError, "%s cannot access class member GetRelationType!", *object().cName());
        return false;
    }

    CEntityAlive* l_tpEntityAlive2 = smart_cast<CEntityAlive*>(&who->object());
    if (!l_tpEntityAlive2)
    {
        ai().script_engine().script_log(ScriptStorage::eLuaMessageTypeError, "%s cannot apply GetRelationType method for non-alive object!", *who->object().cName());
        return false;
    }

    return l_tpEntityAlive1->is_relation_enemy(l_tpEntityAlive2);
}

template <typename T>
IC T* CScriptGameObject::action_planner()
{
    auto manager = smart_cast<CAI_Stalker*>(&object());
    if (!manager)
    {
        Msg("!!CAI_Stalker : cannot access class member action_planner! Object: [%s]", object().Name());
        return nullptr;
    }
    return &manager->brain();
}

CScriptActionPlanner* script_action_planner(CScriptGameObject* obj) { return obj->action_planner<CScriptActionPlanner>(); }

void CScriptGameObject::set_enemy_callback(const luabind::functor<bool>& functor)
{
    CCustomMonster* monster = smart_cast<CCustomMonster*>(&object());
    if (!monster)
    {
        ai().script_engine().script_log(ScriptStorage::eLuaMessageTypeError, "CCustomMonster : cannot access class member set_enemy_callback!");
        return;
    }
    monster->memory().enemy().useful_callback().set(functor);
}

void CScriptGameObject::set_enemy_callback(const luabind::functor<bool>& functor, const luabind::object& object)
{
    CCustomMonster* monster = smart_cast<CCustomMonster*>(&this->object());
    if (!monster)
    {
        ai().script_engine().script_log(ScriptStorage::eLuaMessageTypeError, "CCustomMonster : cannot access class member set_enemy_callback!");
        return;
    }
    monster->memory().enemy().useful_callback().set(functor, object);
}

void CScriptGameObject::set_enemy_callback()
{
    CCustomMonster* monster = smart_cast<CCustomMonster*>(&object());
    if (!monster)
    {
        ai().script_engine().script_log(ScriptStorage::eLuaMessageTypeError, "CCustomMonster : cannot access class member set_enemy_callback!");
        return;
    }
    monster->memory().enemy().useful_callback().clear();
}

void CScriptGameObject::SetCallback(GameObject::ECallbackType type, const luabind::functor<void>& functor)
{
    if (auto it = this->object().m_callbacks.find(type); it != this->object().m_callbacks.end())
        (*it).second->m_callback.set(functor);
    else
    {
        auto callback = std::make_unique<GOCallbackInfo>();
        callback->m_callback.set(functor);
        this->object().m_callbacks[type] = std::move(callback);
    }
}

void CScriptGameObject::SetCallback(GameObject::ECallbackType type, const luabind::functor<void>& functor, const luabind::object& object)
{
    if (auto it = this->object().m_callbacks.find(type); it != this->object().m_callbacks.end())
        (*it).second->m_callback.set(functor, object);
    else
    {
        auto callback = std::make_unique<GOCallbackInfo>();
        callback->m_callback.set(functor, object);
        this->object().m_callbacks[type] = std::move(callback);
    }
}

void CScriptGameObject::SetCallback(GameObject::ECallbackType type)
{
    if (auto it = this->object().m_callbacks.find(type); it != this->object().m_callbacks.end())
        (*it).second->m_callback.clear();
}

void CScriptGameObject::set_fastcall(const luabind::functor<bool>& functor, const luabind::object& object)
{
    CPHScriptGameObjectCondition* c = xr_new<CPHScriptGameObjectCondition>(object, functor, m_game_object);
    CPHDummiAction* a = xr_new<CPHDummiAction>();
    CPHSriptReqGObjComparer cmpr(m_game_object);
    Level().ph_commander_scripts().remove_calls(&cmpr);
    Level().ph_commander_scripts().add_call(c, a);
}

void CScriptGameObject::set_const_force(const Fvector& dir, float value, u32 time_interval)
{
    CPhysicsShell* shell = object().cast_physics_shell_holder()->PPhysicsShell();
    if (!ph_world)
    {
        ai().script_engine().script_log(ScriptStorage::eLuaMessageTypeError, "set_const_force : ph_world do not exist!");
        return;
    }
    if (!shell)
    {
        ai().script_engine().script_log(ScriptStorage::eLuaMessageTypeError, "set_const_force : object %s has no physics shell!", *object().cName());
        return;
    }

    Fvector force;
    force.set(dir);
    force.mul(value);
    CPHConstForceAction* a = xr_new<CPHConstForceAction>(shell, force);
    CPHExpireOnStepCondition* cn = xr_new<CPHExpireOnStepCondition>();
    cn->set_time_interval(time_interval);
    ph_world->AddCall(cn, a);
}
