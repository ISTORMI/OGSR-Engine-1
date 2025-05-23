////////////////////////////////////////////////////////////////////////////
//	Module 		: squad_hierarchy_holder.cpp
//	Created 	: 12.11.2001
//  Modified 	: 03.09.2004
//	Author		: Dmitriy Iassenev, Oles Shishkovtsov, Aleksandr Maksimchuk
//	Description : Squad hierarchy holder
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "squad_hierarchy_holder.h"
#include "group_hierarchy_holder.h"
#include "object_broker.h"
#include "seniority_hierarchy_space.h"
#include "memory_space.h"
#include "team_hierarchy_holder.h"

CSquadHierarchyHolder::~CSquadHierarchyHolder() { delete_data(m_groups); }

CGroupHierarchyHolder& CSquadHierarchyHolder::group(u32 group_id) const
{
    if (group_id >= m_groups.size())
    {
        MsgIfDbg("* [%s]: [team:%u][squad:%u] group_id[%u]: resize m_groups: %u -> %u", __FUNCTION__, team().id(), id(), group_id, m_groups.size(), group_id + 1);
        m_groups.resize(group_id + 1, nullptr);
    }
    if (!m_groups[group_id])
        m_groups[group_id] = xr_new<CGroupHierarchyHolder>(const_cast<CSquadHierarchyHolder*>(this), group_id);
    return (*m_groups[group_id]);
}

#ifdef SQUAD_HIERARCHY_HOLDER_USE_LEADER
void CSquadHierarchyHolder::update_leader()
{
    m_leader = 0;
    GROUP_REGISTRY::const_iterator I = m_groups.begin();
    GROUP_REGISTRY::const_iterator E = m_groups.end();
    for (; I != E; ++I)
        if (*I && (*I)->leader())
        {
            leader((*I)->leader());
            break;
        }
}
#endif // SQUAD_HIERARCHY_HOLDER_USE_LEADER
