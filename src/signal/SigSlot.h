//Leon Colijn c++11 simple signal/slot implementation

#ifndef SIGNAL_H_
#define SIGNAL_H_

#include <memory>
#include <functional>
#include <algorithm>

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
        virtual ~SigSlotBase();

        void add_binding(const std::shared_ptr<Binding>& b);
        virtual void erase_binding(const std::shared_ptr<Binding>& b);
    private:
        std::list<std::shared_ptr<Binding>> _bindings;
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

    private:
        Binding(SigSlotBase* emitter, SigSlotBase* receiver);

        SigSlotBase* _emitter;
        SigSlotBase* _receiver;
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

}


#endif /* signal.h */
