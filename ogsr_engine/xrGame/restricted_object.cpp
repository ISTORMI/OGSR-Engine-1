////////////////////////////////////////////////////////////////////////////
//	Module 		: restricted_object.cpp
//	Created 	: 18.08.2004
//  Modified 	: 23.08.2004
//	Author		: Dmitriy Iassenev
//	Description : Restricted object
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "restricted_object.h"
#include "space_restriction_manager.h"
#include "xrServer_Objects_ALife_Monsters.h"
#include "level.h"
#include "ai_space.h"
#include "level_graph.h"
#include "space_restriction.h"
#include "space_restriction_bridge.h"
#include "space_restriction_base.h"
#include "profiler.h"
#include "alife_simulator.h"
#include "alife_object_registry.h"
#include "game_graph.h"
#include "custommonster.h"

CRestrictedObject::~CRestrictedObject() {}

IC void construct_string(LPSTR result, u32 const result_size, xr_vector<ALife::_OBJECT_ID>& restrictions, CSE_ALifeMonsterAbstract* monster)
{
    u32 count = xr_strlen(result) ? _GetItemCount(result) : 0;

    xr_vector<ALife::_OBJECT_ID>::const_iterator iter;
    for (iter = restrictions.begin(); iter != restrictions.end();)
    {
        CSE_ALifeDynamicObject* object = ai().alife().objects().object(*iter);
        if (object)
        {
            if (ai().game_graph().vertex(object->m_tGraphID)->level_id() == ai().level_graph().level_id())
            {
                if (count)
                    xr_strcat(result, result_size, ",");
                xr_strcat(result, result_size, object->name_replace());
                ++count;
            }

            ++iter;
        }
        else
        {
            Msg("[%s]: remove invalid restriction with ID[%d] from %s", __FUNCTION__, *iter, monster->name_replace());
            iter = restrictions.erase(iter);
        }
    }
}

#if 0
IC	void construct_id_string					(LPSTR result, const xr_vector<ALife::_OBJECT_ID> &restrictions)
{
	xr_strcpy			(result,"");
	string16		temp;
	u32				count = 0;
	xr_vector<ALife::_OBJECT_ID>::const_iterator	I = restrictions.begin();
	xr_vector<ALife::_OBJECT_ID>::const_iterator	E = restrictions.end();
	for ( ; I != E; ++I) {
		if (count)
			xr_strcat	(result,",");
		xr_sprintf		(temp,"%d",*I);
		xr_strcat		(result,temp);
		++count;
	}
}
#endif

BOOL CRestrictedObject::net_Spawn(CSE_Abstract* data)
{
    CSE_Abstract* abstract = (CSE_Abstract*)(data);
    CSE_ALifeMonsterAbstract* monster = smart_cast<CSE_ALifeMonsterAbstract*>(abstract);
    VERIFY(monster);
    m_applied = false;
    m_removed = true;

    string8192 temp0{}, temp1{};

    xr_strcpy(temp0, *monster->m_out_space_restrictors);
    xr_strcpy(temp1, *monster->m_in_space_restrictors);

    if (ai().get_alife())
    {
        construct_string(temp0, sizeof(temp0), monster->m_dynamic_out_restrictions, monster);
        construct_string(temp1, sizeof(temp1), monster->m_dynamic_in_restrictions, monster);
    }

#if 0
	string8192					temp2;
	string8192					temp3;

	construct_id_string			(temp2,monster->m_dynamic_out_restrictions);
	construct_id_string			(temp3,monster->m_dynamic_in_restrictions);

	Msg							("Restricting object %s with",monster->name_replace());
	Msg							("STATIC OUT  : %s",*monster->m_out_space_restrictors);
	Msg							("STATIC IN   : %s",*monster->m_in_space_restrictors);
	Msg							("DYNAMIC OUT : %s",temp2);
	Msg							("DYNAMIC IN  : %s",temp3);
	Msg							("OUT         : %s",temp0);
	Msg							("IN          : %s",temp1);
#endif

    Level().space_restriction_manager().restrict(monster->ID, temp0, temp1);

    actual(true);

    return (TRUE);
}

void CRestrictedObject::net_Destroy() { Level().space_restriction_manager().unrestrict(m_object->ID()); }

u32 CRestrictedObject::accessible_nearest(const Fvector& position, Fvector& result) const
{
    START_PROFILE("Restricted Object/Accessible Nearest");
    VERIFY2(!accessible(position), make_string("[%s] [%f][%f][%f]", object().cName().c_str(), VPUSH(position)));

    return (Level().space_restriction_manager().accessible_nearest(object().ID(), position, result));
    STOP_PROFILE;
}

bool CRestrictedObject::accessible(const Fvector& position) const
{
    START_PROFILE("Restricted Object/Accessible");
    return (accessible(position, EPS_L));
    STOP_PROFILE;
}

bool CRestrictedObject::accessible(const Fvector& position, float radius) const
{
    START_PROFILE("Restricted Object/Accessible");
    Fsphere sphere;
    sphere.P = position;
    sphere.R = radius;
    return (Level().space_restriction_manager().accessible(object().ID(), sphere));
    STOP_PROFILE;
}

bool CRestrictedObject::accessible(u32 level_vertex_id) const
{
    START_PROFILE("Restricted Object/Accessible");
    VERIFY(ai().level_graph().valid_vertex_id(level_vertex_id));
    return (accessible(level_vertex_id, EPS_L));
    STOP_PROFILE;
}

bool CRestrictedObject::accessible(u32 level_vertex_id, float radius) const
{
    START_PROFILE("Restricted Object/Accessible");
#ifdef CRASH_ON_INVALID_VERTEX_ID
    if (!ai().level_graph().valid_vertex_id(level_vertex_id))
        return false;
#endif
    VERIFY(ai().level_graph().valid_vertex_id(level_vertex_id));
    return (Level().space_restriction_manager().accessible(object().ID(), level_vertex_id, radius));
    STOP_PROFILE;
}

void CRestrictedObject::add_border(u32 start_vertex_id, float radius) const
{
    START_PROFILE("Restricted Object/Add Border");

    VERIFY(ai().level_graph().valid_vertex_id(start_vertex_id));
    VERIFY(!m_applied);
    VERIFY(m_removed);
    m_removed = false;
    if (accessible(start_vertex_id))
    {
        m_applied = true;
        Level().space_restriction_manager().add_border(object().ID(), start_vertex_id, radius);
    }

    STOP_PROFILE;
}

void CRestrictedObject::add_border(const Fvector& start_position, const Fvector& dest_position) const
{
    START_PROFILE("Restricted Object/Add Border");

    VERIFY(!m_applied);
    VERIFY(m_removed);
    m_removed = false;
    if (accessible(start_position))
    {
        m_applied = true;
        Level().space_restriction_manager().add_border(object().ID(), start_position, dest_position);
    }

    STOP_PROFILE;
}

void CRestrictedObject::add_border(u32 start_vertex_id, u32 dest_vertex_id) const
{
    START_PROFILE("Restricted Object/Add Border");
    VERIFY(ai().level_graph().valid_vertex_id(start_vertex_id));
    VERIFY(!m_applied);
    VERIFY(m_removed);
    m_removed = false;
    if (accessible(start_vertex_id))
    {
        m_applied = true;
        Level().space_restriction_manager().add_border(object().ID(), start_vertex_id, dest_vertex_id);
    }
    STOP_PROFILE;
}

void CRestrictedObject::remove_border() const
{
    START_PROFILE("Restricted Object/Remove Border");

    VERIFY(!m_removed);
    m_removed = true;
    if (m_applied)
        Level().space_restriction_manager().remove_border(object().ID());
    m_applied = false;

    STOP_PROFILE;
}

shared_str CRestrictedObject::in_restrictions() const
{
    START_PROFILE("Restricted Object/in_restrictions")
    return (Level().space_restriction_manager().in_restrictions(object().ID()));
    STOP_PROFILE
}

shared_str CRestrictedObject::out_restrictions() const
{
    START_PROFILE("Restricted Object/out_restrictions")
    return (Level().space_restriction_manager().out_restrictions(object().ID()));
    STOP_PROFILE
}

shared_str CRestrictedObject::base_in_restrictions() const
{
    START_PROFILE("Restricted Object/base_in_restrictions")
    return (Level().space_restriction_manager().base_in_restrictions(object().ID()));
    STOP_PROFILE
}

shared_str CRestrictedObject::base_out_restrictions() const
{
    START_PROFILE("Restricted Object/out_restrictions")
    return (Level().space_restriction_manager().base_out_restrictions(object().ID()));
    STOP_PROFILE
}

IC void CRestrictedObject::add_object_restriction(ALife::_OBJECT_ID id, const RestrictionSpace::ERestrictorTypes& restrictor_type)
{
    NET_Packet net_packet;
    object().u_EventGen(net_packet, GE_ADD_RESTRICTION, object().ID());
    net_packet.w(&id, sizeof(id));
    net_packet.w(&restrictor_type, sizeof(restrictor_type));
    Level().Send(net_packet, net_flags(TRUE, TRUE));
}

IC void CRestrictedObject::remove_object_restriction(ALife::_OBJECT_ID id, const RestrictionSpace::ERestrictorTypes& restrictor_type)
{
    NET_Packet net_packet;
    object().u_EventGen(net_packet, GE_REMOVE_RESTRICTION, object().ID());
    net_packet.w(&id, sizeof(id));
    net_packet.w(&restrictor_type, sizeof(restrictor_type));
    Level().Send(net_packet, net_flags(TRUE, TRUE));
}

template <typename P, bool value>
IC void CRestrictedObject::construct_restriction_string(LPSTR temp_restrictions, u32 const temp_restrictions_size, const xr_vector<ALife::_OBJECT_ID>& restrictions,
                                                        shared_str current_restrictions, const P& p)
{
    xr_vector<std::string> cur_restrs;
    string256 tmp;
    int cnt = _GetItemCount(current_restrictions.c_str());
    for (int i = 0; i < cnt; ++i)
    {
        _GetItem(current_restrictions.c_str(), i, tmp);
        cur_restrs.emplace_back(tmp);
    }

    xr_vector<std::string> new_restrs;
    *temp_restrictions = 0;
    xr_vector<ALife::_OBJECT_ID>::const_iterator I = restrictions.begin();
    xr_vector<ALife::_OBJECT_ID>::const_iterator E = restrictions.end();
    for (; I != E; ++I)
    {
        CObject* object = Level().Objects.net_Find(*I);
        if (!object)
            continue;
        std::string s(object->cName().c_str());
        if ((std::find(cur_restrs.begin(), cur_restrs.end(), s) != cur_restrs.end()) == value)
            continue;
        if (std::find(new_restrs.begin(), new_restrs.end(), s) == new_restrs.end())
            new_restrs.emplace_back(object->cName().c_str());

        p(this, object->ID());
    }

    if (!new_restrs.empty())
    {
        std::string s;
        for (const auto& it : new_restrs)
        {
            if (!s.empty())
                s += ",";
            s += it;
        }
        ASSERT_FMT(s.length() < temp_restrictions_size, "[%s]: resulted string too long: object[%s] temp_restrictions_size[%u] s.length[%u]", __FUNCTION__,
                   object().cName().c_str(), temp_restrictions_size, s.length());
        xr_strcat(temp_restrictions, temp_restrictions_size, s.c_str());
    }
}

template <bool add>
struct CRestrictionPredicate
{
    RestrictionSpace::ERestrictorTypes m_restrictor_type;

    IC CRestrictionPredicate(RestrictionSpace::ERestrictorTypes restrictor_type) { m_restrictor_type = restrictor_type; }

    IC void operator()(CRestrictedObject* object, ALife::_OBJECT_ID id) const
    {
        if (add)
            object->add_object_restriction(id, m_restrictor_type);
        else
            object->remove_object_restriction(id, m_restrictor_type);
    }
};

void CRestrictedObject::add_restrictions(const xr_vector<ALife::_OBJECT_ID>& out_restrictions, const xr_vector<ALife::_OBJECT_ID>& in_restrictions)
{
    if (out_restrictions.empty() && in_restrictions.empty())
        return;

    START_PROFILE("Restricted Object/Add Restrictions");

    string8192 temp_out_restrictions{}, temp_in_restrictions{};

    construct_restriction_string<CRestrictionPredicate<true>, true>(temp_out_restrictions, sizeof(temp_out_restrictions), out_restrictions, this->out_restrictions(),
                                                                    CRestrictionPredicate<true>(RestrictionSpace::eRestrictorTypeOut));
    construct_restriction_string<CRestrictionPredicate<true>, true>(temp_in_restrictions, sizeof(temp_in_restrictions), in_restrictions, this->in_restrictions(),
                                                                    CRestrictionPredicate<true>(RestrictionSpace::eRestrictorTypeIn));

    Level().space_restriction_manager().add_restrictions(object().ID(), temp_out_restrictions, temp_in_restrictions);

    actual(false);

    STOP_PROFILE;
}

void CRestrictedObject::remove_restrictions(const xr_vector<ALife::_OBJECT_ID>& out_restrictions, const xr_vector<ALife::_OBJECT_ID>& in_restrictions)
{
    if (out_restrictions.empty() && in_restrictions.empty())
        return;

    START_PROFILE("Restricted Object/Remove Restrictions");

    string8192 temp_out_restrictions{}, temp_in_restrictions{};

    construct_restriction_string<CRestrictionPredicate<false>, false>(temp_out_restrictions, sizeof(temp_out_restrictions), out_restrictions, this->out_restrictions(),
                                                                      CRestrictionPredicate<false>(RestrictionSpace::eRestrictorTypeOut));
    construct_restriction_string<CRestrictionPredicate<false>, false>(temp_in_restrictions, sizeof(temp_in_restrictions), in_restrictions, this->in_restrictions(),
                                                                      CRestrictionPredicate<false>(RestrictionSpace::eRestrictorTypeIn));

    Level().space_restriction_manager().remove_restrictions(object().ID(), temp_out_restrictions, temp_in_restrictions);

    actual(false);

    STOP_PROFILE;
}

void CRestrictedObject::add_restrictions(const shared_str& out_restrictions, const shared_str& in_restrictions)
{
    if (!out_restrictions.size() && !in_restrictions.size())
        return;

    START_PROFILE("Restricted Object/Add Restrictions");

    Level().space_restriction_manager().add_restrictions(object().ID(), *out_restrictions, *in_restrictions);

    actual(false);

    STOP_PROFILE;
}

void CRestrictedObject::remove_restrictions(const shared_str& out_restrictions, const shared_str& in_restrictions)
{
    if (!out_restrictions.size() && !in_restrictions.size())
        return;

    START_PROFILE("Restricted Object/Remove Restrictions");

    Level().space_restriction_manager().remove_restrictions(object().ID(), *out_restrictions, *in_restrictions);

    actual(false);

    STOP_PROFILE;
}

void CRestrictedObject::remove_all_restrictions(const RestrictionSpace::ERestrictorTypes& restrictor_type)
{
    NET_Packet net_packet;
    object().u_EventGen(net_packet, GE_REMOVE_ALL_RESTRICTIONS, object().ID());
    net_packet.w(&restrictor_type, sizeof(restrictor_type));
    Level().Send(net_packet, net_flags(TRUE, TRUE));
}

void CRestrictedObject::remove_all_restrictions()
{
    START_PROFILE("Restricted Object/Remove Restrictions");

    remove_all_restrictions(RestrictionSpace::eRestrictorTypeOut);
    remove_all_restrictions(RestrictionSpace::eRestrictorTypeIn);

    if (Level().space_restriction_manager().in_restrictions(object().ID()).size() || Level().space_restriction_manager().out_restrictions(object().ID()).size())
        actual(false);

    Level().space_restriction_manager().restrict(object().ID(), "", "");

    STOP_PROFILE;
}

void CRestrictedObject::actual(bool value)
{
    m_actual = value;
    if (!actual())
        m_object->on_restrictions_change();
}
