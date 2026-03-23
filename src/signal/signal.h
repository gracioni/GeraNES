//Leon Colijn c++11 simple signal/slot implementation

#pragma once

#include <memory>
#include <functional>
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <mutex>
#include <queue>
#include <thread>

#include <vector>
#include <list>

namespace SigSlot {

class Binding;
class SigSlotBase;


/*
 * @brief Base class for objects wishing to receive signals (i.e. have slots)
 */
class SigSlotBase 
{
    public:
        SigSlotBase();
        virtual ~SigSlotBase();

        void add_binding(const std::shared_ptr<Binding>& b);
        virtual void erase_binding(const std::shared_ptr<Binding>& b);

        void move_to_current_thread();
        bool in_owner_thread() const;
        void enqueue_call(std::function<void()> fn);
        size_t dispatch_queued_calls(size_t maxCalls = static_cast<size_t>(-1));
    private:
        std::list<std::shared_ptr<Binding>> _bindings;
        std::thread::id _ownerThreadId;
        mutable std::mutex _queuedCallsMutex;
        std::queue<std::function<void()>> _queuedCalls;
};


class Binding: public std::enable_shared_from_this<Binding>
{
    public:
        Binding() = delete;
        Binding(const Binding& o) = delete;
        virtual ~Binding();

        Binding& operator=(const Binding& other) = delete;

        static std::shared_ptr<Binding> create(SigSlotBase* em, SigSlotBase* recv);

        void unbind();
        bool is_active() const;

    private:
        Binding(SigSlotBase* emitter, SigSlotBase* receiver);

        SigSlotBase* _emitter;
        SigSlotBase* _receiver;
        std::atomic<bool> _active {true};
};

template <typename... _ArgTypes>
class Signal: public SigSlotBase
{
    typedef std::function<void(_ArgTypes...)> _Fun;

    typedef std::pair<std::shared_ptr<Binding>, _Fun> _Binding_Fun;

    public:
        /*
         * @brief bind a new slot to this signal
         *
         * @param slot the method to bind to
         * @param inst the instance of the class to bind to
         *
         * @return void
         */
        template <typename _Class>
        void bind(void(_Class::* slot)(_ArgTypes...), _Class* inst)
        {
            std::shared_ptr<Binding> binding = Binding::create(this, inst);

            _slots.push_back(_Binding_Fun(
                       binding, [=](_ArgTypes... args){(inst->*slot)(args...);}));

            inst->add_binding(binding);
            add_binding(binding);
        }

        template <typename _Class>
        void bind_queued(void(_Class::* slot)(_ArgTypes...), _Class* inst)
        {
            std::shared_ptr<Binding> binding = Binding::create(this, inst);
            std::weak_ptr<Binding> weakBinding = binding;

            _slots.push_back(_Binding_Fun(
                binding,
                [=](_ArgTypes... args) {
                    inst->enqueue_call([=]() {
                        auto locked = weakBinding.lock();
                        if(!locked || !locked->is_active()) return;
                        (inst->*slot)(args...);
                    });
                }));

            inst->add_binding(binding);
            add_binding(binding);
        }

        template <typename _Class>
        void bind_auto(void(_Class::* slot)(_ArgTypes...), _Class* inst)
        {
            std::shared_ptr<Binding> binding = Binding::create(this, inst);
            std::weak_ptr<Binding> weakBinding = binding;

            _slots.push_back(_Binding_Fun(
                binding,
                [=](_ArgTypes... args) {
                    if(inst->in_owner_thread()) {
                        auto locked = weakBinding.lock();
                        if(!locked || !locked->is_active()) return;
                        (inst->*slot)(args...);
                        return;
                    }

                    inst->enqueue_call([=]() {
                        auto locked = weakBinding.lock();
                        if(!locked || !locked->is_active()) return;
                        (inst->*slot)(args...);
                    });
                }));

            inst->add_binding(binding);
            add_binding(binding);
        }

        //TODO: create unbind method


        /*
         * @brief Emit the signal
         *
         * @param args Arguments to the slots
         *
         * @return void
         *
         * @see operator()
         */
        void _emit(_ArgTypes... args)
        {
            for(auto& slot: _slots) {
                std::get<1>(slot)(args...);
            }
        }


        /*
         * @brief Emit the signal
         *
         * @param args Arguments to the slots
         *
         * @return void
         *
         * @see emit 
         */
        void operator()(_ArgTypes... args)
        {
            _emit(args...);
        }

    protected:

        void erase_binding(const std::shared_ptr<Binding>& b) override
        {
            SigSlotBase::erase_binding(b);

            auto it = std::find_if(_slots.begin(), _slots.end(), [&b](_Binding_Fun r) -> bool {
                    return std::get<0>(r) == b;});

            if( it != _slots.end()) _slots.erase(it);
        }

    private:
        std::list<_Binding_Fun> _slots;
};

} // namespace
