#include "stdafx.h"

#include "../xr_3da/cl_intersect.h"
#include "../xr_3da/gamemtllib.h"
#include "SoundRender_Core.h"
#include "SoundRender_Emitter.h"
#include "SoundRender_Target.h"
#include "SoundRender_Source.h"

CSoundRender_Emitter* CSoundRender_Core::i_play(ref_sound* S, BOOL _loop,
                                                float delay)
{
    VERIFY(S->_p->feedback == 0);
    CSoundRender_Emitter* E = xr_new<CSoundRender_Emitter>();
    S->_p->feedback = E;
    E->start(S, _loop, delay);
    s_emitters.push_back(E);
    return E;
}

void CSoundRender_Core::update(const Fvector& P, const Fvector& D,
                               const Fvector& N)
{
    u32 it;

    if (0 == bReady)
        return;
    bLocked = TRUE;
    Timer.time_factor(psSoundTimeFactor); //--#SM+#--
    float new_tm = Timer.GetElapsed_sec();
    fTimer_Delta = new_tm - fTimer_Value;
    //.	float dt					= float(Timer_Delta)/1000.f;
    float dt_sec = fTimer_Delta;
    fTimer_Value = new_tm;

    s_emitters_u++;

    // Firstly update emitters, which are now being rendered
    // Msg	("! update: r-emitters");
    for (it = 0; it < s_targets.size(); it++)
    {
        CSoundRender_Target* T = s_targets[it];
        CSoundRender_Emitter* E = T->get_emitter();
        if (E)
        {
            E->update(dt_sec);
            E->marker = s_emitters_u;
            E = T->get_emitter(); // update can stop itself
            if (E)
            {
                T->priority = E->priority();
            }
            else
                T->priority = -1;
        }
        else
        {
            T->priority = -1;
        }
    }

    // Update emmitters
    // Msg	("! update: emitters");
    for (it = 0; it < s_emitters.size(); it++)
    {
        CSoundRender_Emitter* pEmitter = s_emitters[it];
        if (pEmitter->marker != s_emitters_u)
        {
            pEmitter->update(dt_sec);
            pEmitter->marker = s_emitters_u;
        }
        if (!pEmitter->isPlaying())
        {
            // Stopped
            xr_delete(pEmitter);
            s_emitters.erase(s_emitters.begin() + it);
            it--;
        }
    }

    // Get currently rendering emitters
    // Msg	("! update: targets");
    s_targets_defer.clear();
    s_targets_pu++;
    // u32 PU				= s_targets_pu%s_targets.size();
    for (it = 0; it < s_targets.size(); it++)
    {
        CSoundRender_Target* T = s_targets[it];
        if (T->get_emitter())
        {
            // Has emmitter, maybe just not started rendering
            if (T->get_Rendering())
            {
                /*if	(PU == it)*/ T->fill_parameters(filter);
                T->update();
            }
            else
                s_targets_defer.push_back(T);
        }
    }

    // Commit parameters from pending targets
    if (!s_targets_defer.empty())
    {
        // Msg	("! update: start render - commit");
        s_targets_defer.erase(
            std::unique(s_targets_defer.begin(), s_targets_defer.end()),
            s_targets_defer.end());
        for (it = 0; it < s_targets_defer.size(); it++)
            s_targets_defer[it]->fill_parameters(filter);
    }

    // update EAX or EFX
    if (psSoundFlags.test(ss_EAX) && (bEAX || bEFX))
    {
        if (bListenerMoved)
        {
            bListenerMoved = FALSE;
            e_target = *get_environment(P);
        }

        e_current.lerp(e_current, e_target, dt_sec);

        if (bEAX)
        {
            i_eax_listener_set(&e_current);
            i_eax_commit_setting();
        }
        else
        {
            i_efx_listener_set(&e_current);
            bEFX = i_efx_commit_setting();
        }
    }

    // update listener
    update_listener(P, D, N, dt_sec);

    // Start rendering of pending targets
    if (!s_targets_defer.empty())
    {
        // Msg	("! update: start render");
        for (it = 0; it < s_targets_defer.size(); it++)
            s_targets_defer[it]->render();
    }

    // Events
    update_events();

    bLocked = FALSE;
}

static u32 g_saved_event_count = 0;
void CSoundRender_Core::update_events()
{
    g_saved_event_count = s_events.size();
    for (u32 it = 0; it < s_events.size(); it++)
    {
        event& E = s_events[it];
        Handler(E.first, E.second);
    }
    s_events.clear_not_free();
}

void CSoundRender_Core::statistic(CSound_stats* dest, CSound_stats_ext* ext)
{
    if (dest)
    {
        dest->_rendered = 0;
        for (u32 it = 0; it < s_targets.size(); it++)
        {
            CSoundRender_Target* T = s_targets[it];
            if (T->get_emitter() && T->get_Rendering())
                dest->_rendered++;
        }
        dest->_simulated = s_emitters.size();
        dest->_cache_hits = cache._stat_hit;
        dest->_cache_misses = cache._stat_miss;
        dest->_events = g_saved_event_count;
        cache.stats_clear();
    }
    if (ext)
    {
        for (u32 it = 0; it < s_emitters.size(); it++)
        {
            CSoundRender_Emitter* _E = s_emitters[it];
            CSound_stats_ext::SItem _I;
            _I._3D = !_E->b2D;
            _I._rendered = !!_E->target;
            _I.params = _E->p_source;
            _I.volume = _E->smooth_volume;
            if (_E->owner_data)
            {
                _I.name = _E->source()->fname;
                _I.game_object = _E->owner_data->g_object;
                _I.game_type = _E->owner_data->g_type;
                _I.type = _E->owner_data->s_type;
            }
            else
            {
                _I.game_object = 0;
                _I.game_type = 0;
                _I.type = st_Effect;
            }
            ext->append(_I);
        }
    }
}

float CSoundRender_Core::get_occlusion_to(const Fvector& hear_pt,
                                          const Fvector& snd_pt,
                                          float dispersion)
{
    Occ occ;
    return calc_occlusion(hear_pt, snd_pt, dispersion, &occ);
}

float CSoundRender_Core::get_occlusion(const Fvector& P, float R, Occ* occ,
                                       CSoundRender_Emitter* E)
{
    return calc_occlusion(listener_position(), P, R, occ, E);
}

float CSoundRender_Core::calc_occlusion(const Fvector& base, const Fvector& P,
                                        float R, Occ* occ,
                                        CSoundRender_Emitter* E)
{
    if (occ->lastFrame == s_emitters_u)
        return occ->occ_value;
    float occ_value = 1.f;

    // Если источник звука находится вплотную к слушателю - нечего и заглушать.
    if (P.similar(base, 0.2f))
    {
        occ->lastFrame = s_emitters_u;
        occ->occ_value = occ_value;
        return occ_value;
    }

    // Calculate RAY params
    Fvector pos = P, dir;
    // pos.random_dir();
    // pos.mul(R);
    // pos.add(P);
    dir.sub(base, pos);
    float range = dir.magnitude();
    dir.div(range);

    if (0 != geom_MODEL)
    {
        occ->lastFrame = s_emitters_u;
        bool bNeedFullTest = true;

        // 1. Check cached polygon
        float _u, _v, _range;
        if (occ->valid &&
            CDB::TestRayTri(pos, dir, occ->occ, _u, _v, _range, false))
        {
            if (_range > 0 && _range < range)
            {
                occ_value = occ->occ_value;
                bNeedFullTest = false;
            }
        }

        // 2. Polygon doesn't picked up - real database query
        if (bNeedFullTest)
        {
            // Проверяем препятствие в направлении от звука к камере
            occ_value = occRayTestMtl(pos, dir, range, occ, E);
            if (occ->checkReverse && occ_value < 1.f)
            {
                // Проверяем препятствие в обратном направлении, от камеры к
                // звуку и выбираем максимальное поглощение звука из этой и
                // предыдущей проверок. Бывают ситуации, когда на локации в
                // прямом и обратном направлениях находятся материалы с разными
                // fSndOcclusionFactor.
                Fvector reverseDir;
                reverseDir.sub(pos, base).normalize();
                Occ reverseOcc;
                float reverseVal =
                    occRayTestMtl(base, reverseDir, range, &reverseOcc, E);
                if (reverseVal < occ_value)
                    occ_value = reverseVal;
            }
            occ->occ_value = occ_value;
        }
    }

    if (0 != geom_SOM && !fis_zero(occ_value))
        occ_value *= occRayTestSom(pos, dir, range);

    return occ_value;
}

float CSoundRender_Core::occRayTestMtl(const Fvector& pos, const Fvector& dir,
                                       float range, Occ* occ,
                                       CSoundRender_Emitter* E)
{
    float occ_value = 1.f;
    occ->valid = false;

    geom_DB.ray_options(CDB::OPT_CULL);
    geom_DB.ray_query(geom_MODEL, pos, dir, range);

    if (geom_DB.r_count() > 0)
    {
        for (size_t i = 0; i < geom_DB.r_count(); i++)
        {
            CDB::RESULT* R = geom_DB.r_begin() + i;
            const CDB::TRI& T = geom_MODEL->get_tris()[R->id];
            if (!occ->valid)
            {
                const Fvector* V = geom_MODEL->get_verts();
                occ->occ[0].set(V[T.verts[0]]);
                occ->occ[1].set(V[T.verts[1]]);
                occ->occ[2].set(V[T.verts[2]]);
                occ->valid = true;
            }
            // Если `pos` находится на поверхности, то из-за ограничений float,
            // точка может оказаться как за поверхностью, так и перед ней, ведь
            // поверхности в игре не имеют толщины. Поэтому будем сравнивать
            // расстояние до пересечения с расстоянием до `pos`, с точностью
            // 0.01. Если совпало, не будет засчитывать это пересечение. Будем
            // считать, что точка находится перед поверхность и это пересечение
            // не должно менять звук.
            if (fsimilar(R->range, range, 0.01f))
                continue;
            u16 material_idx = T.material;
            SGameMtl* mtl = get_material(material_idx);
            if (mtl->fSndOcclusionFactor < 1.f)
            {
                if (fis_zero(mtl->fSndOcclusionFactor))
                    occ_value = 0.f;
                else
                    occ_value *= mtl->fSndOcclusionFactor;
            }

            if (fis_zero(occ_value))
                break;
        }
    }

    return occ_value;
}

float CSoundRender_Core::occRayTestSom(const Fvector& pos, const Fvector& dir,
                                       float range)
{
    float occ_value = 1.f;

    geom_DB.ray_options(0);
    geom_DB.ray_query(geom_SOM, pos, dir, range);
    u32 r_cnt = u32(geom_DB.r_count());
    CDB::RESULT* _B = geom_DB.r_begin();

    if (r_cnt > 0)
    {
        for (u32 k = 0; k < r_cnt; k++)
        {
            CDB::RESULT* R = _B + k;
            occ_value *= *(float*)&R->dummy;
            if (fis_zero(occ_value))
                break;
        }
    }

    return occ_value;
}
