//
// Created by htf on 16-9-2.
//

#include "hhvm_swoole.h"

namespace HPHP
{
    void php_swoole_event_init(void)
    {
//        SwooleG.main_reactor->setHandle(SwooleG.main_reactor, SW_FD_USER | SW_EVENT_READ, php_swoole_event_onRead);
//        SwooleG.main_reactor->setHandle(SwooleG.main_reactor, SW_FD_USER | SW_EVENT_WRITE, php_swoole_event_onWrite);
//        SwooleG.main_reactor->setHandle(SwooleG.main_reactor, SW_FD_USER | SW_EVENT_ERROR, php_swoole_event_onError);
    }

    void php_swoole_event_wait()
    {
        if (SwooleWG.in_client == 1 && SwooleWG.reactor_ready == 0 && SwooleG.running)
        {
            SwooleWG.reactor_ready = 1;

#ifdef HAVE_SIGNALFD
            if (SwooleG.main_reactor->check_signalfd)
            {
                swSignalfd_setup(SwooleG.main_reactor);
            }
#endif
            int ret = SwooleG.main_reactor->wait(SwooleG.main_reactor, NULL);
            if (ret < 0)
            {
                raise_error("reactor wait failed. Error: %s [%d]", strerror(errno), errno);
            }
        }
    }
    void php_swoole_check_reactor()
    {
        if (SwooleWG.reactor_init)
        {
            return;
        }

        if (swIsTaskWorker())
        {
            raise_error("cannot use async-io in task process.");
        }

        if (SwooleG.main_reactor == NULL)
        {
            swTraceLog(SW_TRACE_PHP, "init reactor");

            SwooleG.main_reactor = (swReactor *) malloc(sizeof(swReactor));
            if (SwooleG.main_reactor == NULL)
            {
                raise_error("malloc failed.");
            }
            if (swReactor_create(SwooleG.main_reactor, SW_REACTOR_MAXEVENTS) < 0)
            {
                raise_error("create reactor failed.");
            }
            //client, swoole_event_exit will set swoole_running = 0
            SwooleWG.in_client = 1;
            SwooleWG.reactor_wait_onexit = 1;
            SwooleWG.reactor_ready = 0;
            //only client side
            //php_swoole_at_shutdown("swoole_event_wait");
        }

        php_swoole_event_init();

        SwooleWG.reactor_init = 1;
    }
}