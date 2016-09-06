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
#ifndef HPHP_HHVM_SWOOLE_H_H
#define HPHP_HHVM_SWOOLE_H_H

#include "hphp/runtime/ext/extension.h"
#include "hphp/runtime/base/builtin-functions.h"
#include "hphp/runtime/vm/native-data.h"
#include "hphp/runtime/base/request-local.h"
#include "hphp/runtime/base/array-init.h"

#include "config.h"
#include "swoole/include/swoole.h"
#include "swoole/include/Server.h"

namespace HPHP
{
    struct TimerResource : SweepableResourceData
    {
        DECLARE_RESOURCE_ALLOCATION(TimerResource)
        CLASSNAME_IS("swoole_timer")
        explicit TimerResource(const Variant &callback, bool tick);
        ~TimerResource();

    public:
        Variant getCallback()
        {
            return m_callback;
        }

        swTimer_node *get()
        {
            return m_tnode;
        }

        void set(swTimer_node *tnode)
        {
            m_tnode = tnode;
        }

    private:
        bool m_tick;
        Variant m_callback;
        swTimer_node* m_tnode;
    };

    void php_swoole_check_reactor();
    void php_swoole_event_init(void);
    void php_swoole_event_wait();

    Variant HHVM_FUNCTION(swoole_timer_tick, long after_ms, const Variant &callback);
    Variant HHVM_FUNCTION(swoole_timer_after, long after_ms, const Variant &callback);
    bool HHVM_FUNCTION(swoole_timer_clear, long id);
    bool HHVM_FUNCTION(swoole_timer_exists, long id);
}

#endif //HPHP_HHVM_SWOOLE_H_H
