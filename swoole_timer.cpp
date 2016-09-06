/*
  +----------------------------------------------------------------------+
  | Swoole                                                               |
  +----------------------------------------------------------------------+
  | This source file is subject to version 2.0 of the Apache license,    |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.apache.org/licenses/LICENSE-2.0.html                      |
  | If you did not receive a copy of the Apache2.0 license and are unable|
  | to obtain it through the world-wide-web, please send a note to       |
  | license@swoole.com so we can mail you a copy immediately.            |
  +----------------------------------------------------------------------+
  | Author: Tianfeng Han  <mikan.tenny@gmail.com>                        |
  +----------------------------------------------------------------------+
*/

#include "hhvm_swoole.h"

namespace HPHP
{
    class TimerResource : public ResourceData
    {
    public:
        explicit TimerResource(const Variant &callback, bool tick);
        ~TimerResource() {};

        Variant getCallback()
        {
            return m_callback;
        }

        swTimer_node *getNode()
        {
            return m_tnode;
        }

        void setNode(swTimer_node *tnode)
        {
            m_tnode = tnode;
        }

    private:
        bool m_tick;
        Variant m_callback;
        swTimer_node* m_tnode;
    };


    TimerResource::TimerResource(const Variant &callback, bool tick)
    {
        m_callback = callback;
        m_tick = tick;
        m_tnode = NULL;
    }

    static Array timer_map;

    int del_timer(swTimer_node *tnode)
    {
        if (!timer_map.exists(tnode->id))
        {
            return SW_ERR;
        }
        timer_map.remove(tnode->id);
        tnode->id = -1;
        return SW_OK;
    }

    void hhvm_swoole_onTimeout(swTimer *timer, swTimer_node *tnode)
    {
        auto args = Array();
        auto tm_res = dyn_cast_or_null<TimerResource>(timer_map[tnode->id].toResource());
        if (tm_res == nullptr)
        {
            raise_warning("supplied argument is not a valid cURL handle resource");
            return;
        }

        auto callback = tm_res->getCallback();

        args.append(Variant(tnode->id));
        timer->_current_id = tnode->id;
        vm_call_user_func(callback, args);
        timer->_current_id = -1;
        del_timer(tnode);
    }

    void hhvm_swoole_onInterval(swTimer *timer, swTimer_node *tnode)
    {
        auto args = Array();
        auto tm_res = dyn_cast_or_null<TimerResource>(timer_map[tnode->id].toResource());
        if (tm_res == nullptr)
        {
            raise_warning("supplied argument is not a valid cURL handle resource");
            return;
        }
        auto callback = tm_res->getCallback();

        args.append(Variant(tnode->id));
        timer->_current_id = tnode->id;
        vm_call_user_func(callback, args);
        timer->_current_id = -1;

        if (tnode->remove)
        {
            del_timer(tnode);
        }
    }

    void timer_init(int msec)
    {
        swTimer_init(msec);
        SwooleG.timer.onAfter = hhvm_swoole_onTimeout;
        SwooleG.timer.onTick = hhvm_swoole_onInterval;
        timer_map = Array::Create();
    }

    long add_timer(int ms, const Variant &callback, bool tick)
    {
        if (SwooleG.serv && swIsMaster())
        {
            raise_warning("cannot use timer in master process.");
            return SW_ERR;
        }
        if (ms > 86400000)
        {
            raise_warning("The given parameters is too big.");
            return SW_ERR;
        }
        if (ms <= 0)
        {
            raise_warning("Timer must be greater than 0");
            return SW_ERR;
        }
        if (!is_callable(callback))
        {
            raise_warning("function [%s] is not callable.", callback.toString().c_str());
            return false;
        }

        if (!swIsTaskWorker())
        {
            php_swoole_check_reactor();
        }
        if (SwooleG.timer.fd == 0)
        {
            timer_init(ms);
        }

        auto res = req::make<TimerResource>(callback, tick);
        swTimer_node *tnode = swTimer_add(&SwooleG.timer, ms, tick ? 1 : 0, NULL);
        if (tnode == NULL)
        {
            raise_warning("addtimer failed.");
            return SW_ERR;
        }
        else
        {
            res->setNode(tnode);
            timer_map.set(tnode->id, Variant(res));
            return tnode->id;
        }
    }

    Variant HHVM_FUNCTION(swoole_timer_tick, long after_ms, const Variant &callback)
    {
        long timer_id = add_timer(after_ms, callback, true);
        if (timer_id < 0)
        {
            return Variant(false);
        }
        else
        {
            return Variant(timer_id);
        }
    }

    Variant HHVM_FUNCTION(swoole_timer_after, long after_ms, const Variant &callback)
    {
        long timer_id = add_timer(after_ms, callback, false);
        if (timer_id < 0)
        {
            return Variant(false);
        }
        else
        {
            return Variant(timer_id);
        }
    }

    bool HHVM_FUNCTION(swoole_timer_clear, long id)
    {
        if (!SwooleG.timer.set)
        {
            raise_warning( "no timer");
            return false;
        }

        auto tm_res = dyn_cast_or_null<TimerResource>(timer_map[id].toResource());
        if (tm_res == nullptr)
        {
            return false;
        }

        auto callback = tm_res->getCallback();
        swTimer_node *tnode = tm_res->getNode();
        if (tnode->id == SwooleG.timer._current_id)
        {
            tnode->remove = 1;
            return true;
        }
        if (del_timer(tnode) < 0)
        {
            return false;
        }
        else
        {
            swTimer_del(&SwooleG.timer, tnode);
            return true;
        }
    }

    bool HHVM_FUNCTION(swoole_timer_exists, long id)
    {
        if (!SwooleG.timer.set)
        {
            raise_warning("no timer");
            return false;
        }
        return timer_map.exists(id);
    }
}
