
#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "lw/pp.hpp"
#include "lw/trait.hpp"

namespace lw {
namespace event {

// Implementation details for event emitters.
namespace _details {
    template<std::size_t I, typename EventId, typename EventsTuple>
    struct event_index_impl;

    template<
        std::size_t I,
        typename EventId,
        typename Event,
        typename... OtherEvents
    >
    struct event_index_impl<I, EventId, std::tuple<Event, OtherEvents...>> :
        public std::conditional<
            std::is_same<EventId, typename Event::event_id_type>::value,
            std::integral_constant<std::size_t, I>,
            event_index_impl<I + 1, EventId, std::tuple<OtherEvents...>>
        >::type
    {};

    template<std::size_t I, typename EventId>
    struct event_index_impl<I, EventId, std::tuple<>> {};

    template<typename EventId, typename... Events>
    struct event_index : public event_index_impl<0, EventId, std::tuple<Events...>> {};

    template<typename EventId, typename... Events>
    struct get_event {
        typedef event_index<EventId, Events...> event_index_type;
        typedef std::tuple<Events...> event_tuple_type;
        typedef typename std::tuple_element<event_index_type::value, event_tuple_type>::type event_type;

        static event_type& from(event_tuple_type& events){
            return std::get<event_index_type::value>(events);
        }

        static const event_type& from(const event_tuple_type& events){
            return std::get<event_index_type::value>(events);
        }
    };

    template<typename Listener, typename Event>
    struct listener_matches_event :
        public trait::is_tuple_callable<Listener(typename Event::event_argument_types)>
    {};
}

// ---------------------------------------------------------------------------------------------- //

/// @brief Maintains all information about a single event, including bound listeners.
///
/// @tparam EventArgs The arguments listeners expected with this event.
template<typename... EventArgs>
class Event {
public:
    typedef std::tuple<EventArgs...> event_argument_types;      ///< The event arguments.
    typedef std::function<void(EventArgs&&...)> listener_type;  ///< The type for storing listeners.

    /// @brief Adds a new event listener to the back of the list.
    ///
    /// @tparam Listener The event listener type which must conform to `listener_type`.
    ///
    /// @param listener The event listener being added.
    template<typename Listener>
    void emplace_back(Listener&& listener){
        m_listeners.emplace_back(std::forward<Listener>(listener));
    }

    /// @copydoc emplace_back
    template<typename Listener>
    void push_back(Listener&& listener){
        m_listeners.push_back(std::forward<Listener>(listener));
    }

    /// @brief Calls all the listeners with the provided arguments.
    ///
    /// @tparam CalledArgs
    ///     The types of the provided arguements. Must be compatible with `event_argument_types`.
    ///
    /// @param args The argument values being passed to each listener.
    template<typename... CalledArgs>
    void operator()(CalledArgs&&... args){
        for (auto&& listener : m_listeners) {
            listener(args...);
        }
    }

    /// @brief Removes any listeners for which the predicate returns true.
    ///
    /// @tparam Pred A unary predicate type which takes a `listener_type` instance.
    ///
    /// @param pred
    ///     The predicate receiving each listener in turn. If it returns `true` the listener will be
    ///     removed from the event.
    template<typename Pred>
    void remove_if(Pred&& pred){
        m_listeners.remove_if(std::forward<Pred>(pred));
    }

    /// @brief Removes any listeners equal to the one provided.
    ///
    /// @tparam Listener The listener type to remove.
    ///
    /// @param listener The value to check for.
    template<typename Listener>
    void remove(Listener&& listener){
        m_listeners.remove(std::forward<Listener>(listener));
    }

    /// @brief Removes all listeners from the event.
    void clear(void){
        m_listeners.clear();
    }

    /// @brief Returns the number of listeners bound to this event.
    std::size_t size(void) const {
        return m_listeners.size();
    }

    /// @brief Returns true if no listeners are bound to this event.
    bool empty(void) const {
        return m_listeners.empty();
    }

private:
    std::list<listener_type> m_listeners; ///< The list of event listeners.
};

template<typename T>
class IdEvent;

/// @brief Adds a type-safe method for identifying events.
///
/// These are the event types used by the `Emitter` class.
///
/// @tparam EventId The type used to identify this event.
template<typename EventId, typename... EventArgs>
class IdEvent<EventId(EventArgs...)> : public Event<EventArgs...> {
public:
    typedef EventId event_id_type; ///< The event Id type.
};

template<typename Events>
class Emitter;

/// @brief A class which can emit several different event types.
///
/// @tparam Events All the events this emitter will support. Must be derived from `IdEvent`.
template<typename... Events>
class Emitter<std::tuple<Events...>> {
public:
    typedef std::tuple<Events...> event_types; ///< The events supported by the emitter.

    /// @brief Adds a new listener for an event.
    ///
    /// @tparam EventId     The ID of the event this listener handles.
    /// @tparam Listener    A functor type that matches the event's call requirements.
    ///
    /// @param listener The functor to emplace in the back of the listeners for the event.
    template<
        typename EventId,
        typename Listener,
        typename = typename std::enable_if<
            _details::listener_matches_event<
                Listener, typename _details::get_event<EventId, Events...>::event_type
            >::value
        >::type
    >
    void on(const EventId&, Listener&& listener){
        _details::get_event<EventId, Events...>::from(m_events)
            .emplace_back(std::forward<Listener>(listener));
    }

    /// @copydoc lw::event::Emitter::on
    template<typename EventId, typename Listener>
    void emplace(const EventId& e, Listener&& listener){
        on(e, std::forward<Listener>(listener));
    }

    /// @brief Adds a new listener for an event.
    ///
    /// @tparam EventId     The ID of the event this listener handles.
    /// @tparam Listener    A functor type that matches the event's call requirements.
    ///
    /// @param listener The functor to insert in the back of the listeners for the event.
    template<
        typename EventId,
        typename Listener,
        typename = typename std::enable_if<
            _details::listener_matches_event<
                Listener, typename _details::get_event<EventId, Events...>::event_type
            >::value
        >::type
    >
    void insert(const EventId&, Listener&& listener){
        _details::get_event<EventId, Events...>::from(m_events)
            .push_back(std::forward<Listener>(listener));
    }

    /// @brief Removes any listeners from the specified event for which the predicate returns true.
    ///
    /// @tparam EventId
    ///     The ID of the event to remove listeners from.
    /// @tparam Pred
    ///     A functor type taking a const reference to a std::function for the listener and
    ///     returning a boolean value.
    ///
    /// @param pred The functor to test every listener with.
    template<typename EventId, typename Pred>
    void remove_if(const EventId&, Pred&& pred){
        _details::get_event<EventId, Events...>::from(m_events).remove_if(std::forward<Pred>(pred));
    }

    /// @brief Removes a single listener which matches the one provided.
    ///
    /// @tparam EventId     The ID of the event to remove the listener from.
    /// @tparam Listener    The type of the listener to remove.
    ///
    /// @param listener The listener to remove from the event.
    template<typename EventId, typename Listener>
    void remove(const EventId&, Listener&& listener){
        _details::get_event<EventId, Events...>::from(m_events)
            .remove(std::forward<Listener>(listener));
    }

    /// @brief Removes all listeners for the given event.
    ///
    /// @tparam EventId The ID of the event to remove all listeners from.
    template<typename EventId>
    void clear(const EventId&){
        _details::get_event<EventId, Events...>::from(m_events).clear();
    }

    /// @brief Removes all event listeners from all events.
    void clear(void){
        trait::for_each(m_events, [](auto& event){ event.clear(); });
    }

    /// @brief Checks if the given event has any listeners.
    ///
    /// @tparam EventId The ID of the event to check for listeners on.
    ///
    /// @return True if the event has no listeners, otherwise false.
    template<typename EventId>
    bool empty(const EventId&){
        return _details::get_event<EventId, Events...>::from(m_events).empty();
    }

    /// @brief Checks if any event has any event listeners.
    ///
    /// @return True if there are no event listeners on any event, otherwise false.
    bool empty(void) const {
        bool empty = true;
        trait::for_each(m_events, [&empty](const auto& event){ empty = empty && event.empty(); });
        return empty;
    }

    /// @brief Gets the number of listeners bound to given event.
    ///
    /// @return The number of events bound to the specified event.
    template<typename EventId>
    std::size_t size(const EventId&) const {
        return _details::get_event<EventId, Events...>::from(m_events).size();
    }

    /// @brief Gets the number of listeners bound all of the events.
    ///
    /// @return The number of events bound to all the events.
    std::size_t size(void) const {
        std::size_t size = 0;
        trait::for_each(m_events, [&size](const auto& event){ size += event.size(); });
        return size;
    }

    /// @brief Calls all listeners bound to the given event.
    ///
    /// @tparam Args Types that are convertable to the events expecte argument types.
    ///
    /// @param args The event values being emitted.
    template<typename EventId, typename... Args>
    void emit(const EventId&, Args&&... args){
        _details::get_event<EventId, Events...>::from(m_events)(std::forward<Args>(args)...);
    }

private:
    event_types m_events; ///< The events.
};

// ---------------------------------------------------------------------------------------------- //

// Declares event types.
#define LW_DECLARE_EVENT(_event_name) struct LW_CONCAT(_event_name, _t) {};
#define LW_DECLARE_EVENTS(...) LW_FOR_EACH(LW_DECLARE_EVENT, __VA_ARGS__)

// Creates an event emitter class using the given events.
#define _LW_EVENT_TYPE_IMPL(_event_name, ...) \
    ::lw::event::IdEvent<LW_CONCAT(_event_name, _t)(__VA_ARGS__)>,
#define _LW_EVENT_TYPE(x) _LW_EVENT_TYPE_IMPL x
#define _LW_IMPORT_EVENT_IMPL(_event_name, ...) \
    const LW_CONCAT(_event_name, _t) LW_CONCAT(_event_name, _event){};
#define _LW_IMPORT_EVENT(x) _LW_IMPORT_EVENT_IMPL x
#define LW_DEFINE_EMITTER(_emitter_name, ...)                                       \
    class _emitter_name :                                                           \
        public ::lw::event::Emitter<                                                \
            trait::remove_type<                                                     \
                std::nullptr_t,                                                     \
                std::tuple<LW_FOR_EACH(_LW_EVENT_TYPE, __VA_ARGS__) std::nullptr_t> \
            >::type                                                                 \
        >                                                                           \
    {                                                                               \
    public:                                                                         \
        LW_FOR_EACH(_LW_IMPORT_EVENT, __VA_ARGS__);                                 \
    }

}
}
