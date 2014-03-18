/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (C) 2014 Named Data Networking Project
 * See COPYING for copyright and distribution information.
 */

#include "common.hpp"

#include "scheduler.hpp"

namespace ndn {

struct EventIdImpl
{
  EventIdImpl(const Scheduler::EventQueue::iterator& event)
    : m_event(event)
    , m_isValid(true)
  {
  }

  void
  invalidate()
  {
    m_isValid = false;
  }

  bool
  isValid() const
  {
    return m_isValid;
  }

  operator const Scheduler::EventQueue::iterator&() const
  {
    return m_event;
  }

  void
  reset(const Scheduler::EventQueue::iterator& newIterator)
  {
    m_event = newIterator;
    m_isValid = true;
  }

private:
  Scheduler::EventQueue::iterator m_event;
  bool m_isValid;
};

Scheduler::EventInfo::EventInfo(const time::nanoseconds& after,
                                const time::nanoseconds& period,
                                const Event& event)
  : m_scheduledTime(time::steady_clock::now() + after)
  , m_period(period)
  , m_event(event)
{
}

Scheduler::EventInfo::EventInfo(const time::steady_clock::TimePoint& when, const EventInfo& previousEvent)
  : m_scheduledTime(when)
  , m_period(previousEvent.m_period)
  , m_event(previousEvent.m_event)
  , m_eventId(previousEvent.m_eventId)
{
}

time::nanoseconds
Scheduler::EventInfo::expiresFromNow() const
{
  time::steady_clock::TimePoint now = time::steady_clock::now();
  if (now > m_scheduledTime)
    return time::seconds(0); // event should be scheduled ASAP
  else
    return m_scheduledTime - now;
}


Scheduler::Scheduler(boost::asio::io_service& ioService)
  : m_ioService(ioService)
  , m_scheduledEvent(m_events.end())
  , m_deadlineTimer(ioService)
  , m_isEventExecuting(false)
{
}

EventId
Scheduler::scheduleEvent(const time::nanoseconds& after,
                         const Event& event)
{
  return schedulePeriodicEvent(after, time::nanoseconds(-1), event);
}

EventId
Scheduler::schedulePeriodicEvent(const time::nanoseconds& after,
                                 const time::nanoseconds& period,
                                 const Event& event)
{
  EventQueue::iterator i = m_events.insert(EventInfo(after, period, event));
  i->m_eventId = make_shared<EventIdImpl>(boost::cref(i));

  if (!m_isEventExecuting)
    {
      if (m_scheduledEvent == m_events.end() ||
          *i < *m_scheduledEvent)
        {
          m_deadlineTimer.expires_from_now(after);
          m_deadlineTimer.async_wait(bind(&Scheduler::onEvent, this, _1));
          m_scheduledEvent = i;
        }
    }

  return i->m_eventId;
}
  
void
Scheduler::cancelEvent(const EventId& eventId)
{
  if (!static_cast<bool>(eventId) || !eventId->isValid())
    return; // event already fired or cancelled
  
  if (static_cast<EventQueue::iterator>(*eventId) != m_scheduledEvent) {
    m_events.erase(*eventId);
    eventId->invalidate();
    return;
  }
  
  m_deadlineTimer.cancel();
  m_events.erase(static_cast<EventQueue::iterator>(*eventId));
  eventId->invalidate();

  if (!m_isEventExecuting)
    {
      if (!m_events.empty())
        {
          m_deadlineTimer.expires_from_now(m_events.begin()->expiresFromNow());
          m_deadlineTimer.async_wait(bind(&Scheduler::onEvent, this, _1));
          m_scheduledEvent = m_events.begin();
        }
      else
        {
          m_scheduledEvent = m_events.end();
        }
    }
}

void
Scheduler::onEvent(const boost::system::error_code& error)
{
  if (error) // e.g., cancelled
    {
      return;
    }

  m_isEventExecuting = true;

  // process all expired events
  time::steady_clock::TimePoint now = time::steady_clock::now();
  while(!m_events.empty() && m_events.begin()->m_scheduledTime <= now)
    {
      EventQueue::iterator head = m_events.begin();
      
      Event event = head->m_event;
      if (head->m_period < time::nanoseconds::zero())
        {
          head->m_eventId->invalidate();
          m_events.erase(head);
        }
      else
        {
          // "reschedule" and update EventId data of the event
          EventInfo event(now + head->m_period, *head);
          EventQueue::iterator i = m_events.insert(event);
          i->m_eventId->reset(i);
          m_events.erase(head);
        }

      event();
    }

  if (!m_events.empty())
    {
      m_deadlineTimer.expires_from_now(m_events.begin()->m_scheduledTime - now);
      m_deadlineTimer.async_wait(bind(&Scheduler::onEvent, this, _1));
      m_scheduledEvent = m_events.begin();
    }
  else
    {
      m_scheduledEvent = m_events.end();
    }

  m_isEventExecuting = false;
}


} // namespace ndn
