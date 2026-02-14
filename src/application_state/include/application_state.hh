#pragma once

#include "generated_application_state.hh"
#include "semaphore.hh"

#include <array>
#include <atomic>
#include <etl/bitset.h>
#include <etl/mutex.h>
#include <etl/vector.h>
#include <string_view>

constexpr auto kInvalidIconHash = 0;

using ParameterBitset = etl::bitset<AS::kLastIndex + 1, uint32_t>;

namespace AS::storage
{

template <class... T>
struct partial_state : public T...
{
    template <typename S>
    auto& GetRef()
    {
        return static_cast<S&>(*this).template GetRef<S>();
    }
};

} // namespace AS::storage

class ApplicationState
{
public:
    class IListener
    {
    public:
        virtual ~IListener() = default;
    };


    class ReadOnlyState
    {
    public:
        friend class ApplicationState;

        template <typename T>
        auto Get()
        {
            return m_parent.Get<T>();
        }

    protected:
        explicit ReadOnlyState(ApplicationState& parent);

        ApplicationState& m_parent;
    };

    class ReadWriteState : public ReadOnlyState
    {
    public:
        friend class ApplicationState;

        template <typename T>
        void Set(const auto& value)
        {
            m_parent.Set<T>(value);
        }

    private:
        using ReadOnlyState::ReadOnlyState;
    };


    template <class... T>
    class PartialSnapshot
    {
    public:
        friend class ApplicationState;

        ~PartialSnapshot()
        {
            // TODO: Lock and do each write in a single transaction
            (void)std::initializer_list<int> {
                ((m_changed.test(AS::IndexOf<T>()) ? (m_parent.Set<T>(Get<T>()), 0) : 0))...};
        }

        template <typename S>
        auto Get()
        {
            // If S not in T, return the global state
            if constexpr (!std::disjunction_v<std::is_same<S, T>...>)
            {
                return m_parent.Get<S>();
            }
            else
            {
                // Never a shared_ptr
                return m_state.template GetRef<S>();
            }
        }

        template <typename S>
        void Set(const auto& value)
        {
            if constexpr (!std::disjunction_v<std::is_same<S, T>...>)
            {
                m_parent.Set<S>(value);
            }
            else
            {
                if (value == Get<S>())
                {
                    return;
                }

                m_state.template GetRef<S>() = value;

                m_changed.set(AS::IndexOf<S>());
            }
        }

    private:
        explicit PartialSnapshot(ApplicationState& parent)
            : m_parent(parent)
        {
            (void)std::initializer_list<int> {
                (m_state.template GetRef<T>() = m_parent.GetValue<T>(), 0)...};
        }

        ApplicationState& m_parent;
        AS::storage::partial_state<T...> m_state;

        ParameterBitset m_changed;
    };


    ApplicationState();


    template <class... T>
    std::unique_ptr<IListener> AttachListener(os::binary_semaphore& semaphore)
    {
        ParameterBitset interested;

        (void)std::initializer_list<int> {(interested.set<AS::IndexOf<T>()>(), 0)...};

        return DoAttachListener(interested, semaphore);
    }

    // Checkout a local copy of the global state. Rewritten when the unique ptr is released
    ReadWriteState CheckoutReadWrite();

    ReadOnlyState CheckoutReadonly();


    template <class... T>
    auto CheckoutPartialSnapshot()
    {
        return PartialSnapshot<T...>(*this);
    }

private:
    class ListenerImpl;
    class StateImpl;

    template <typename T>
    auto Get()
    {
        if constexpr (T::IsAtomic())
        {
            return m_global_state.GetRef<T>();
        }
        else
        {
            auto ptr = m_global_state.GetRef<T>();

            using element_type = typename decltype(ptr)::element_type;
            return std::make_shared<const element_type>(*ptr);
        }
    }

    template <typename T>
    auto GetValue()
    {
        if constexpr (T::IsAtomic())
        {
            return m_global_state.GetRef<T>();
        }
        else
        {
            return *m_global_state.GetRef<T>();
        }
    }


    template <typename T>
    void Set(const auto& value)
    {
        std::lock_guard lock(m_mutex);

        if constexpr (T::IsAtomic())
        {
            if (value == Get<T>())
            {
                return;
            }
            m_global_state.GetRef<T>() = value;
        }
        else
        {
            if (value == *Get<T>())
            {
                return;
            }

            auto& ref = m_global_state.GetRef<T>();

            m_global_state.GetRef<T>() = std::make_shared<std::decay_t<decltype(*ref)>>(value);
        }

        NotifyChange(AS::IndexOf<T>());
    }

    std::unique_ptr<IListener> DoAttachListener(const ParameterBitset& interested,
                                                os::binary_semaphore& semaphore);
    void DetachListener(const ListenerImpl* impl);
    void NotifyChange(unsigned index);

    AS::storage::state m_global_state;

    etl::mutex m_mutex;

    std::array<std::vector<ListenerImpl*>, AS::kLastIndex + 1> m_listeners;
};
