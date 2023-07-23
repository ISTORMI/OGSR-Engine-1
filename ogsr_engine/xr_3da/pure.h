#ifndef _PURE_H_AAA_
#define _PURE_H_AAA_

// messages
#define REG_PRIORITY_LOW 0x11111111ul
#define REG_PRIORITY_NORMAL 0x22222222ul
#define REG_PRIORITY_HIGH 0x33333333ul
#define REG_PRIORITY_CAPTURE 0x7ffffffful
#define REG_PRIORITY_INVALID 0xfffffffful

typedef void __fastcall RP_FUNC(void* obj);
#define DECLARE_MESSAGE_CL(name, calling) \
    extern ENGINE_API RP_FUNC rp_##name; \
    class ENGINE_API pure##name \
    { \
    public: \
        virtual void calling On##name(void) = 0; \
    }

#define DECLARE_MESSAGE(name) DECLARE_MESSAGE_CL(name, )
#define DECLARE_RP(name) \
    void __fastcall rp_##name(void* p) \
    { \
        ((pure##name*)p)->On##name(); \
    }

DECLARE_MESSAGE_CL(Frame, _BCL);

DECLARE_MESSAGE(Render);
DECLARE_MESSAGE(AppActivate);
DECLARE_MESSAGE(AppDeactivate);
DECLARE_MESSAGE(AppStart);
DECLARE_MESSAGE(AppEnd);
DECLARE_MESSAGE(DeviceReset);
DECLARE_MESSAGE(ScreenResolutionChanged);

// ENGINE_API extern int	__cdecl	_REG_Compare(const void *, const void *);

template <class T>
class CRegistrator // the registrator itself
{
    //-----------------------------------------------------------------------------
    struct _REG_INFO
    {
        T* Object;
        int Prio;
    };

public:
    xr_vector<_REG_INFO> R;

    // constructor
    struct
    {
        u32 in_process : 1;
        u32 changed : 1;
    };

    std::mutex m;

    CRegistrator()
    {
        in_process = false;
        changed = false;
    }

    constexpr void Add(T* object, const int priority = REG_PRIORITY_NORMAL)
    {
        Add({object, priority});
    }

    void Add(_REG_INFO&& newMessage)
    {
        std::scoped_lock lock(m);
        R.emplace_back(newMessage);

        if (in_process)
            changed = true;
        else
            Resort();
    }

    void Remove(T* obj)
    {
        std::scoped_lock lock(m);
        for (u32 i = 0; i < R.size(); i++)
        {
            if (R[i].Object == obj)
                R[i].Prio = REG_PRIORITY_INVALID;
        }

        if (in_process)
            changed = true;
        else
            Resort();
    }

    void Process(RP_FUNC* f)
    {
        in_process = true;

        _REG_INFO I{nullptr, 0, 0};
        u32 idx = 0;

        while (true)
        {
            bool found = false;
            {
                std::scoped_lock lock(m);
                if (idx < R.size())
                {
                    I = R[idx];
                    found = true;
                }
            }
            if (!found)
                break;
            int prio = I.Prio;
            if (prio != REG_PRIORITY_INVALID)
                f(I.Object);
            if (idx == 0 && prio == REG_PRIORITY_CAPTURE)
                break;
            idx++;
        }

        if (changed)
        {
            std::scoped_lock lock(m);
            Resort();
        }

        in_process = false;
    }

    void Resort(void)
    {
        if (!R.empty())
        {
            std::sort(
                std::begin(R), std::end(R),
                [](const auto& a, const auto& b) { return a.Prio > b.Prio; });
        }

        while (!R.empty() && R[R.size() - 1].Prio == REG_PRIORITY_INVALID)
        {
            R.pop_back();
        }

        if (R.empty())
            R.clear();

        changed = false;
    }
};

#endif
