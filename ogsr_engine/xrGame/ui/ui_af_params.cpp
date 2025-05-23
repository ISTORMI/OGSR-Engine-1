#include "stdafx.h"
#include "ui_af_params.h"
#include "UIStatic.h"
#include "../object_broker.h"
#include "UIXmlInit.h"

LPCSTR af_item_sect_names[] = {
    "health_restore_speed",
    "radiation_restore_speed",
    "satiety_restore_speed",
    "thirst_restore_speed",
    "power_restore_speed",
    "bleeding_restore_speed",
    "psy_health_restore_speed",
    "additional_inventory_weight",
    "additional_inventory_weight2",

    "burn_immunity",
    "strike_immunity",
    "shock_immunity",
    "wound_immunity",
    "radiation_immunity",
    "telepatic_immunity",
    "chemical_burn_immunity",
    "explosion_immunity",
    "fire_wound_immunity",
};

LPCSTR af_item_param_names[] = {
    "ui_inv_health",
    "ui_inv_radiation",
    "ui_inv_satiety",
    "ui_inv_thirst",
    "ui_inv_power",
    "ui_inv_bleeding",
    "ui_inv_psy_health",
    "ui_inv_additional_weight",
    "ui_inv_additional_weight2",

    "ui_inv_outfit_burn_protection", // "(burn_imm)",
    "ui_inv_outfit_strike_protection", // "(strike_imm)",
    "ui_inv_outfit_shock_protection", // "(shock_imm)",
    "ui_inv_outfit_wound_protection", // "(wound_imm)",
    "ui_inv_outfit_radiation_protection", // "(radiation_imm)",
    "ui_inv_outfit_telepatic_protection", // "(telepatic_imm)",
    "ui_inv_outfit_chemical_burn_protection", // "(chemical_burn_imm)",
    "ui_inv_outfit_explosion_protection", // "(explosion_imm)",
    "ui_inv_outfit_fire_wound_protection", // "(fire_wound_imm)",
};

LPCSTR af_actor_param_names[] = {"satiety_health_v", "radiation_v", "satiety_v", "thirst_v", "satiety_power_v", "wound_incarnation_v", "psy_health_v"};

CUIArtefactImmunity::CUIArtefactImmunity()
{
    m_name = nullptr;
    m_value = nullptr;
}

CUIArtefactImmunity::~CUIArtefactImmunity()
{
    xr_delete(m_name);
    xr_delete(m_value);
}

void CUIArtefactImmunity::InitFromXml(CUIXml& xml_doc, LPCSTR base_str, u32 hit_type)
{
    string256 buf;

    m_name = xr_new<CUIStatic>();
    m_name->SetAutoDelete(false);
    AttachChild(m_name);
    strconcat(sizeof(buf), buf, base_str, ":static_", af_item_sect_names[hit_type]);
    CUIXmlInit::InitWindow(xml_doc, buf, 0, this);
    CUIXmlInit::InitStatic(xml_doc, buf, 0, m_name);

    strconcat(sizeof(buf), buf, base_str, ":static_", af_item_sect_names[hit_type], ":static_value");
    if (xml_doc.NavigateToNode(buf, 0))
    {
        m_value = xr_new<CUIStatic>();
        m_value->SetAutoDelete(false);
        m_value->SetTextComplexMode(true);
        AttachChild(m_value);
        CUIXmlInit::InitStatic(xml_doc, buf, 0, m_value);
    }
}

CUIArtefactParams::CUIArtefactParams() { Memory.mem_fill(m_info_items, 0, sizeof(m_info_items)); }

CUIArtefactParams::~CUIArtefactParams()
{
    for (u32 i = _item_start; i < _max_item_index; ++i)
        xr_delete(m_info_items[i]);
}

void CUIArtefactParams::InitFromXml(CUIXml& xml_doc)
{
    LPCSTR _base = "af_params";
    if (!xml_doc.NavigateToNode(_base, 0))
        return;

    string256 _buff;
    CUIXmlInit::InitWindow(xml_doc, _base, 0, this);

    for (u32 i = _item_start; i < _max_item_index; ++i)
    {
        strconcat(sizeof(_buff), _buff, _base, ":static_", af_item_sect_names[i]);

        if (xml_doc.NavigateToNode(_buff, 0))
        {
            m_info_items[i] = xr_new<CUIArtefactImmunity>();
            m_info_items[i]->InitFromXml(xml_doc, _base, i);
        }
    }
}

bool CUIArtefactParams::Check(const shared_str& af_section) { return !!pSettings->line_exist(af_section, "af_actor_properties"); }
#include "../string_table.h"
void CUIArtefactParams::SetInfo(const shared_str& af_section)
{
    string128 _buff;
    float _h = 0.0f;
    DetachAll();
    for (u32 i = _item_start; i < _max_item_index; ++i)
    {
        if (!m_info_items[i])
            continue;

        float _val;
        if (i == _item_additional_inventory_weight)
        {
            _val = READ_IF_EXISTS(pSettings, r_float, af_section, af_item_sect_names[i], 0.f);
            if (fis_zero(_val))
                continue;
            float _val2 = READ_IF_EXISTS(pSettings, r_float, af_section, af_item_sect_names[_item_additional_inventory_weight2], 0.f);
            if (fsimilar(_val, _val2))
                continue;
        }
        else if (i == _item_additional_inventory_weight2)
        {
            _val = READ_IF_EXISTS(pSettings, r_float, af_section, af_item_sect_names[i], 0.f);
            if (fis_zero(_val))
                continue;
        }
        else if (i < _max_item_index1)
        {
            float _actor_val = pSettings->r_float("actor_condition", af_actor_param_names[i]);
            _val = READ_IF_EXISTS(pSettings, r_float, af_section, af_item_sect_names[i], 0.f);

            if (fis_zero(_val))
                continue;

            _val = (_val / _actor_val) * 100.0f;
        }
        else
        {
            shared_str _sect = pSettings->r_string(af_section, "hit_absorbation_sect");
            _val = pSettings->r_float(_sect, af_item_sect_names[i]);
            if (fsimilar(_val, 1.0f))
                continue;
            _val = (1.0f - _val);
            _val *= 100.0f;
        }
        LPCSTR _sn = "%";
        if (i == _item_radiation_restore_speed || i == _item_power_restore_speed)
        {
            _val /= 100.0f;
            _sn = "";
        }
        else if (i == _item_additional_inventory_weight || i == _item_additional_inventory_weight2)
            _sn = "";

        LPCSTR _color = (_val > 0) ? "%c[green]" : "%c[red]";

        if (i == _item_bleeding_restore_speed)
            _val *= -1.0f;

        if (i == _item_bleeding_restore_speed || i == _item_radiation_restore_speed)
            _color = (_val > 0) ? "%c[red]" : "%c[green]";

        if (m_info_items[i]->m_value)
        {
            m_info_items[i]->m_name->SetText(CStringTable().translate(af_item_param_names[i]).c_str());
            sprintf_s(_buff, "%s%+.0f%s", _color, _val, _sn);
            m_info_items[i]->m_value->SetText(_buff);
        }
        else
        {
            sprintf_s(_buff, "%s %s %+.0f%s", CStringTable().translate(af_item_param_names[i]).c_str(), _color, _val, _sn);
            m_info_items[i]->m_name->SetText(_buff);
        }
        m_info_items[i]->SetWndPos(m_info_items[i]->GetWndPos().x, _h);
        _h += m_info_items[i]->GetWndSize().y;
        AttachChild(m_info_items[i]);
    }
    SetHeight(_h);
}
