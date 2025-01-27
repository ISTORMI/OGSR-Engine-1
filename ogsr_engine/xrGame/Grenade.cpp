#include "stdafx.h"
#include "grenade.h"
#include "PhysicsShell.h"
#include "WeaponHUD.h"
#include "entity.h"
#include "ParticlesObject.h"
#include "actor.h"
#include "inventory.h"
#include "level.h"
#include "xrmessages.h"
#include "xr_level_controller.h"
#include "game_cl_base.h"
#include "xrserver_objects_alife.h"
#include "game_object_space.h"
#include "script_callback_ex.h"
#include "script_game_object.h"

#define GRENADE_REMOVE_TIME 30000
const float default_grenade_detonation_threshold_hit = 100;
CGrenade::CGrenade(void)
{
    m_destroy_callback.clear();
    m_eSoundCheckout = ESoundTypes(SOUND_TYPE_WEAPON_RECHARGING);
    contactBounce = true;
    contactBounceAngle = 0.f;
}

CGrenade::~CGrenade(void) { HUD_SOUND::DestroySound(sndCheckout); }

void CGrenade::Load(LPCSTR section)
{
    inherited::Load(section);
    CExplosive::Load(section);

    HUD_SOUND::LoadSound(section, "snd_checkout", sndCheckout,
                         m_eSoundCheckout);

    //////////////////////////////////////
    // время убирания оружия с уровня
    if (pSettings->line_exist(section, "grenade_remove_time"))
        m_dwGrenadeRemoveTime =
            pSettings->r_u32(section, "grenade_remove_time");
    else
        m_dwGrenadeRemoveTime = GRENADE_REMOVE_TIME;
    m_grenade_detonation_threshold_hit =
        READ_IF_EXISTS(pSettings, r_float, section, "detonation_threshold_hit",
                       default_grenade_detonation_threshold_hit);

    contactBounce =
        READ_IF_EXISTS(pSettings, r_bool, section, "contact_bounce", true);
    contactBounceAngle = READ_IF_EXISTS(pSettings, r_float, section,
                                        "contact_bounce_angle", 10.f);
    contactBounceAngle = deg2rad(contactBounceAngle);
}

void CGrenade::Hit(SHit* pHDS)
{
    if (ALife::eHitTypeExplosion == pHDS->hit_type && m_grenade_detonation_threshold_hit < pHDS->damage() && CExplosive::Initiator() == u16(-1))
    {
        CExplosive::SetCurrentParentID(pHDS->who->ID());
        Destroy();
    }
    inherited::Hit(pHDS);
}

BOOL CGrenade::net_Spawn(CSE_Abstract* DC)
{
    m_dwGrenadeIndependencyTime = 0;
    BOOL ret = inherited::net_Spawn(DC);
    Fvector box;
    BoundingBox().getsize(box);
    float max_size = _max(_max(box.x, box.y), box.z);
    box.set(max_size, max_size, max_size);
    box.mul(3.f);
    CExplosive::SetExplosionSize(box);
    m_thrown = false;
    return ret;
}

void CGrenade::net_Destroy()
{
    if (m_destroy_callback)
    {
        m_destroy_callback(this);
        m_destroy_callback = nullptr;
    }

    inherited::net_Destroy();
    CExplosive::net_Destroy();
}

void CGrenade::OnH_B_Independent(bool just_before_destroy) { inherited::OnH_B_Independent(just_before_destroy); }

void CGrenade::OnH_A_Independent()
{
    m_dwGrenadeIndependencyTime = Level().timeServer();
    inherited::OnH_A_Independent();
}

void CGrenade::OnH_A_Chield()
{
    m_dwGrenadeIndependencyTime = 0;
    m_dwDestroyTime = 0xffffffff;
    inherited::OnH_A_Chield();
}

void CGrenade::State(u32 state)
{
    switch (state)
    {
    case MS_THREATEN: {
        Fvector C;
        Center(C);
        PlaySound(sndCheckout, C);
    }
    break;
    case MS_HIDDEN: {
        if (m_thrown)
        {
            if (m_pPhysicsShell)
                m_pPhysicsShell->Deactivate();
            xr_delete(m_pPhysicsShell);
            m_dwDestroyTime = 0xffffffff;

            if (H_Parent())
                PutNextToSlot();

            if (Local())
            {
#ifdef DEBUG
                Msg("Destroying local grenade[%d][%d]", ID(), Device.dwFrame);
#endif
                DestroyObject();
            }
        };
    }
    break;
    };
    inherited::State(state);
}

void CGrenade::Throw()
{
    if (!m_fake_missile || m_thrown)
        return;

    CGrenade* pGrenade = smart_cast<CGrenade*>(m_fake_missile);
    VERIFY(pGrenade);

    if (pGrenade)
    {
        pGrenade->set_destroy_time(m_dwDestroyTimeMax);
        //установить ID того кто кинул гранату
        pGrenade->SetInitiator(H_Parent()->ID());
    }
    inherited::Throw();

    if (m_pCurrentInventory->GetOwner())
    {
        CActor* pActor = smart_cast<CActor*>(m_pCurrentInventory->GetOwner());
        if (pActor)
        {
            Actor()->set_state_wishful(Actor()->get_state_wishful() & (~mcSprint));
        }
    }

    m_fake_missile->processing_activate(); //@sliph
    m_thrown = true;

    // Real Wolf.Start.18.12.14
    auto parent = smart_cast<CGameObject*>(H_Parent());
    auto obj = smart_cast<CGameObject*>(m_fake_missile);
    if (parent && obj)
    {
        parent->callback(GameObject::eOnThrowGrenade)(obj->lua_game_object());
    }
    // Real Wolf.End.18.12.14
}

void CGrenade::Destroy()
{
    // Generate Expode event
    Fvector normal;

    if (m_destroy_callback)
    {
        m_destroy_callback(this);
        m_destroy_callback = nullptr;
    }

    FindNormal(normal);
    Fvector C;
    Center(C);
    CExplosive::GenExplodeEvent(C, normal);
}

bool CGrenade::Useful() const
{
    bool res = (/* !m_throw && */ m_dwDestroyTime == 0xffffffff && CExplosive::Useful() && TestServerFlag(CSE_ALifeObject::flCanSave));

    return res;
}

void CGrenade::OnEvent(NET_Packet& P, u16 type)
{
    inherited::OnEvent(P, type);
    CExplosive::OnEvent(P, type);
}

void CGrenade::PutNextToSlot()
{
    if (OnClient())
        return;
    VERIFY(!getDestroy());

    //выкинуть гранату из инвентаря
    if (m_pCurrentInventory)
    {
        NET_Packet P;
        m_pCurrentInventory->Ruck(this);

        this->u_EventGen(P, GEG_PLAYER_ITEM2RUCK, this->H_Parent()->ID());
        P.w_u16(this->ID());
        this->u_EventSend(P);

        CGrenade* pNext = smart_cast<CGrenade*>(m_pCurrentInventory->Same(this, true));
        if (!pNext)
            pNext = smart_cast<CGrenade*>(m_pCurrentInventory->SameSlot(GRENADE_SLOT, this, true));

        VERIFY(pNext != this);

        if (pNext && m_pCurrentInventory->Slot(pNext))
        {
            pNext->u_EventGen(P, GEG_PLAYER_ITEM2SLOT, pNext->H_Parent()->ID());
            P.w_u16(pNext->ID());
            pNext->u_EventSend(P);
            m_pCurrentInventory->SetActiveSlot(pNext->GetSlot());
        }
        /////	m_thrown				= false;
    }
}

void CGrenade::OnAnimationEnd(u32 state)
{
    switch (state)
    {
    case MS_END:
        SwitchState(MS_HIDDEN);
        break;
        //.	case MS_END: SwitchState(MS_RESTORE);	break;
    default: inherited::OnAnimationEnd(state);
    }
}

void CGrenade::UpdateCL()
{
    inherited::UpdateCL();
    CExplosive::UpdateCL();
}

bool CGrenade::Action(s32 cmd, u32 flags)
{
    if (inherited::Action(cmd, flags))
        return true;

    switch (cmd)
    {
    //переключение типа гранаты
    case kWPN_NEXT: {
        if (flags & CMD_START)
        {
            const u32 state = GetState();
            if (state == MS_HIDDEN || state == MS_IDLE || state == MS_PLAYING || state == MS_IDLE_SPRINT || state == MS_IDLE_MOVING)
            {
                if (m_pCurrentInventory)
                {
                    TIItemContainer::iterator it = m_pCurrentInventory->m_ruck.begin();
                    TIItemContainer::iterator it_e = m_pCurrentInventory->m_ruck.end();
                    /*	for(;it!=it_e;++it)
                        {
                            CGrenade *pGrenade = smart_cast<CGrenade*>(*it);
                            if(pGrenade && xr_strcmp(pGrenade->cNameSect(), cNameSect()))
                            {
                                m_pCurrentInventory->Ruck(this);
                                m_pCurrentInventory->SetActiveSlot(NO_ACTIVE_SLOT);
                                m_pCurrentInventory->Slot(pGrenade);
                                return true;
                            }
                        }*/
                    xr_map<shared_str, CGrenade*> tmp;
                    tmp.insert(mk_pair(cNameSect(), this));
                    for (; it != it_e; ++it)
                    {
                        CGrenade* pGrenade = smart_cast<CGrenade*>(*it);
                        if (pGrenade && (tmp.find(pGrenade->cNameSect()) == tmp.end()))
                            tmp.insert(mk_pair(pGrenade->cNameSect(), pGrenade));
                    }
                    xr_map<shared_str, CGrenade*>::iterator curr_it = tmp.find(cNameSect());
                    curr_it++;
                    CGrenade* tgt;
                    if (curr_it != tmp.end())
                        tgt = curr_it->second;
                    else
                        tgt = tmp.begin()->second;
                    m_pCurrentInventory->Ruck(this);
                    m_pCurrentInventory->SetActiveSlot(NO_ACTIVE_SLOT);
                    m_pCurrentInventory->Slot(tgt);
                }
            }
        }
        return true;
    };
    }
    return false;
}

bool CGrenade::NeedToDestroyObject() const { return (TimePassedAfterIndependant() > m_dwGrenadeRemoveTime); }

ALife::_TIME_ID CGrenade::TimePassedAfterIndependant() const
{
    if (!H_Parent() && m_dwGrenadeIndependencyTime != 0)
        return Level().timeServer() - m_dwGrenadeIndependencyTime;
    else
        return 0;
}

BOOL CGrenade::UsedAI_Locations()
{
#pragma todo("Dima to Yura : It crashes, because on net_Spawn object doesn't use AI locations, but on net_Destroy it does use them")
    return TRUE; // m_dwDestroyTime == 0xffffffff;
}

void CGrenade::net_Relcase(CObject* O)
{
    CExplosive::net_Relcase(O);
    inherited::net_Relcase(O);
}

void CGrenade::Deactivate(bool now)
{
    // Drop grenade if primed
    m_pHUD->StopCurrentAnimWithoutCallback();
    CEntityAlive* entity = smart_cast<CEntityAlive*>(m_pCurrentInventory->GetOwner());
    if (!entity->g_Alive() && !GetTmpPreDestroy() && Local() && (GetState() == MS_THREATEN || GetState() == MS_READY || GetState() == MS_THROW))
    {
        if (m_fake_missile)
        {
            CGrenade* pGrenade = smart_cast<CGrenade*>(m_fake_missile);
            if (pGrenade)
            {
                if (m_pCurrentInventory->GetOwner())
                {
                    CActor* pActor = smart_cast<CActor*>(m_pCurrentInventory->GetOwner());
                    if (pActor)
                    {
                        if (!pActor->g_Alive())
                        {
                            m_constpower = false;
                            m_fThrowForce = 0;
                        }
                    }
                }
                Throw();
            };
        };
    };

    inherited::Deactivate(now || (GetState() == MS_THREATEN || GetState() == MS_READY || GetState() == MS_THROW));
}

void CGrenade::GetBriefInfo(xr_string& str_name, xr_string& icon_sect_name,
                            xr_string& str_count)
{
    str_name = NameShort();
    u32 ThisGrenadeCount =
        m_pCurrentInventory->dwfGetSameItemCount(*cNameSect(), true);
    string16 stmp;
    sprintf_s(stmp, "%d", ThisGrenadeCount);
    str_count = stmp;
    icon_sect_name = *cNameSect();
}

void CGrenade::Contact(CPhysicsShellHolder* obj, Fvector vUp)
{
    inherited::Contact(obj, vUp);

    if (contactBounce || !actorWasParent())
        return;

    Fvector up = {0.f, 1.f, 0.f};
    if (angle_between_vectors(vUp, up) > contactBounceAngle)
        return;

    Fvector vel;
    PPhysicsShell()->get_LinearVel(vel);
    vel.x = 0.f;
    vel.z = 0.f;
    PPhysicsShell()->set_LinearVel(vel);
}

bool CGrenade::actorWasParent()
{
    if (PPhysicsShell() &&
        smart_cast<CActor*>(
            (CPhysicsShellHolder*)PPhysicsShell()->get_CallbackData()))
        return true;
    return false;
}
