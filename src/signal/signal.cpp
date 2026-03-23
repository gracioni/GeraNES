//Leon Colijn c++11 simple signal/slot implementation

#include "signal.h"

#include <cassert>

namespace SigSlot
{

SigSlotBase::SigSlotBase()
    : _ownerThreadId(std::this_thread::get_id())
{
}

SigSlotBase::~SigSlotBase()
{
    while(!_bindings.empty()) {
        _bindings.front()->unbind();
    }

}

void SigSlotBase::add_binding(const std::shared_ptr<Binding>& b)
{
    _bindings.push_back(b);
}

void SigSlotBase::erase_binding(const std::shared_ptr<Binding>& b)
{
    auto pos = std::find(_bindings.begin(), _bindings.end(), b);
    if(pos == _bindings.end()) {
        return;
    }
    _bindings.erase(pos);
}

void SigSlotBase::move_to_current_thread()
{
    _ownerThreadId = std::this_thread::get_id();
}

bool SigSlotBase::in_owner_thread() const
{
    return _ownerThreadId == std::this_thread::get_id();
}

void SigSlotBase::enqueue_call(std::function<void()> fn)
{
    if(!fn) return;

    std::scoped_lock lock(_queuedCallsMutex);
    _queuedCalls.push(std::move(fn));
}

size_t SigSlotBase::dispatch_queued_calls(size_t maxCalls)
{
    size_t dispatched = 0;

    while(dispatched < maxCalls) {
        std::function<void()> fn;
        {
            std::scoped_lock lock(_queuedCallsMutex);
            if(_queuedCalls.empty()) break;
            fn = std::move(_queuedCalls.front());
            _queuedCalls.pop();
        }

        if(fn) {
            fn();
            ++dispatched;
        }
    }

    return dispatched;
}




Binding::Binding(SigSlotBase* emitter, SigSlotBase* receiver): _emitter(emitter), _receiver(receiver)
{
    assert(_emitter != nullptr);
    assert(_receiver != nullptr);
}

Binding::~Binding()
{
    unbind();
}

std::shared_ptr<Binding> Binding::create(SigSlotBase* em, SigSlotBase* recv)
{
    return std::shared_ptr<Binding>(new Binding(em, recv));
}

void Binding::unbind()
{
    _active.store(false, std::memory_order_release);

    if(_emitter) {
        SigSlotBase* em = _emitter;
        _emitter = nullptr;
        em->erase_binding(shared_from_this());
    }
    if(_receiver) {
        SigSlotBase* recv = _receiver;
        _receiver = nullptr;
        recv->erase_binding(shared_from_this());
    }
}

bool Binding::is_active() const
{
    return _active.load(std::memory_order_acquire);
}

}
