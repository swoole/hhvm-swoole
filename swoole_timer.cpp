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
    enum TimerType
    {
        SW_TIMER_TICK, SW_TIMER_AFTER,
    };

    typedef struct _swTimer_callback
    {
        Variant &callback;
        int type;
    } swTimer_callback;

    static swHashMap *timer_map;

    static void php_swoole_onTimeout(swTimer *timer, swTimer_node *tnode);
    static void php_swoole_onInterval(swTimer *timer, swTimer_node *tnode);
    static long php_swoole_add_timer(int ms, const Variant &callback, int is_tick);
    static int php_swoole_del_timer(swTimer_node *tnode);
    static void php_swoole_check_timer(int msec);

    static long php_swoole_add_timer(int ms, const Variant &callback, int is_tick)
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

        php_swoole_check_timer(ms);
        swTimer_callback *cb = (swTimer_callback *) malloc(sizeof(swTimer_callback));
        if (cb == NULL)
        {
            raise_warning("malloc(%ld) failed.", sizeof(swTimer_callback));
            return false;
        }
        cb->callback = callback;
        if (is_tick)
        {
            cb->type = SW_TIMER_TICK;
        }
        else
        {
            cb->type = SW_TIMER_AFTER;
        }

        swTimer_node *tnode = swTimer_add(&SwooleG.timer, ms, is_tick, cb);
        if (tnode == NULL)
        {
            raise_warning("addtimer failed.");
            return SW_ERR;
        }
        else
        {
            swHashMap_add_int(timer_map, tnode->id, tnode);
            return tnode->id;
        }
    }

    static int php_swoole_del_timer(swTimer_node *tnode)
    {
        if (swHashMap_del_int(timer_map, tnode->id) < 0)
        {
            return SW_ERR;
        }
        tnode->id = -1;
        swTimer_callback *cb = (swTimer_callback *) tnode->data;
        if (!cb)
        {
            return SW_ERR;
        }
        free(cb);
        return SW_OK;
    }

    static void php_swoole_onTimeout(swTimer *timer, swTimer_node *tnode)
    {
        swTimer_callback *cb = (swTimer_callback *) tnode->data;
        auto args = Array();
        const Variant callback = cb->callback;
        args.append(Variant(tnode->id));
        timer->_current_id = tnode->id;
        vm_call_user_func(callback, args);
        timer->_current_id = -1;
        php_swoole_del_timer(tnode);
    }

    static void php_swoole_onInterval(swTimer *timer, swTimer_node *tnode)
    {
        swTimer_callback *cb = (swTimer_callback *)tnode->data;
        auto args = Array();
        const Variant callback = cb->callback;
        args.append(Variant(tnode->id));
        timer->_current_id = tnode->id;
        vm_call_user_func(callback, args);
        timer->_current_id = -1;

        if (tnode->remove)
        {
            php_swoole_del_timer(tnode);
        }
    }

    static void php_swoole_check_timer(int msec)
    {
        if (SwooleG.timer.fd == 0)
        {
            swTimer_init(msec);
            SwooleG.timer.onAfter = php_swoole_onTimeout;
            SwooleG.timer.onTick = php_swoole_onInterval;
            timer_map = swHashMap_new(SW_HASHMAP_INIT_BUCKET_N, NULL);
        }
    }

    Variant HHVM_FUNCTION(swoole_timer_tick, long after_ms, const Variant &callback)
    {
        long timer_id = php_swoole_add_timer(after_ms, callback, 1);
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
        long timer_id = php_swoole_add_timer(after_ms, callback, 0);
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

        swTimer_node *tnode = (swTimer_node *) swHashMap_find_int(timer_map, id);
        if (tnode == NULL)
        {
            raise_warning("timer#%ld is not found.", id);
            return false;
        }

        //current timer, cannot remove here.
        if (tnode->id == SwooleG.timer._current_id)
        {
            tnode->remove = 1;
            return true;
        }

        if (php_swoole_del_timer(tnode) < 0)
        {
            return false;
        } else
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
        swTimer_node *tnode = (swTimer_node *) swHashMap_find_int(timer_map, id);
        return tnode != NULL;
    }
}