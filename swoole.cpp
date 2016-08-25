/*
 +----------------------------------------------------------------------+
 | HipHop for PHP                                                       |
 +----------------------------------------------------------------------+
 | Copyright (c) 2010-2013 Facebook, Inc. (http://www.facebook.com)     |
 +----------------------------------------------------------------------+
 | This source file is subject to version 3.01 of the PHP license,      |
 | that is bundled with this package in the file LICENSE, and is        |
 | available through the world-wide-web at the following url:           |
 | http://www.php.net/license/3_01.txt                                  |
 | If you did not receive a copy of the PHP license and are unable to   |
 | obtain it through the world-wide-web, please send a note to          |
 | license@php.net so we can mail you a copy immediately.               |
 +----------------------------------------------------------------------+
 */

#include "hphp/runtime/ext/extension.h"
#include "hphp/runtime/base/builtin-functions.h"
#include "hphp/runtime/vm/native-data.h"
#include "hphp/runtime/ext/sockets/ext_sockets.h"
#include "hphp/runtime/base/request-local.h"
#include "hphp/runtime/base/array-init.h"
#include "hphp/runtime/base/ini-setting.h"
#include "hphp/runtime/base/request-event-handler.h"
#include "hphp/runtime/base/zend-string.h"
#include <vector>

#include "config.h"
#include "swoole/include/swoole.h"
#include "swoole/include/Server.h"

enum php_swoole_server_callback_type
{
    //--------------------------Swoole\Server--------------------------
    SW_SERVER_CB_onConnect,        //worker(event)
    SW_SERVER_CB_onReceive,        //worker(event)
    SW_SERVER_CB_onClose,          //worker(event)
    SW_SERVER_CB_onPacket,         //worker(event)
    SW_SERVER_CB_onStart,          //master
    SW_SERVER_CB_onShutdown,       //master
    SW_SERVER_CB_onWorkerStart,    //worker(event & task)
    SW_SERVER_CB_onWorkerStop,     //worker(event & task)
    SW_SERVER_CB_onTask,           //worker(task)
    SW_SERVER_CB_onFinish,         //worker(event & task)
    SW_SERVER_CB_onWorkerError,    //manager
    SW_SERVER_CB_onManagerStart,   //manager
    SW_SERVER_CB_onManagerStop,    //manager
    SW_SERVER_CB_onPipeMessage,    //worker(evnet & task)
    //--------------------------Swoole\Http\Server----------------------
            SW_SERVER_CB_onRequest,        //http server
    //--------------------------Swoole\WebSocket\Server-----------------
            SW_SERVER_CB_onHandShake,      //worker(event)
    SW_SERVER_CB_onOpen,           //worker(event)
    SW_SERVER_CB_onMessage,        //worker(event)
    //-------------------------------END--------------------------------
};

#define PHP_SERVER_CALLBACK_NUM             (SW_SERVER_CB_onMessage+1)

using namespace std;

namespace HPHP
{
    static swServer *swoole_server_object;

    static int hhvm_swoole_onReceive(swServer *serv, swEventData *req)
    {
        auto this_ = (ObjectData *) serv->ptr2;
        auto args = make_packed_array(String("hello"), String("world"));
        const Variant callback = this_->o_get("receive");
        Variant retval = vm_call_user_func(callback, args);
        return 0;
    }

    static bool HHVM_METHOD(swoole_server, __construct, const String &host, int port, int mode = SW_MODE_PROCESS,
                            int type = SW_SOCK_TCP)
    {
        if (!SwooleGS)
        {
            swoole_init();
        }

        if (SwooleGS->start > 0)
        {
            raise_warning("server is already running. Unable to create swoole_server.");
            return false;
        }

        swoole_server_object = (swServer *) malloc(sizeof(swServer));
        swServer *serv = swoole_server_object;
        swServer_init(serv);
        serv->factory_mode = (uint8_t) mode;

        if (serv->factory_mode == SW_MODE_SINGLE)
        {
            serv->worker_num = 1;
            serv->max_request = 0;
        }

        swListenPort *ls = swServer_add_port(serv, type, (char *) host.c_str(), port);
        if (!ls)
        {
            raise_error("listen server port failed.");
        }

        return true;
    }

    static bool HHVM_METHOD(swoole_server, on, const String &event, const Variant &callback)
    {
        if (!is_callable(callback))
        {
            raise_warning("function [%s] is not callable.", callback.toString().c_str());
            return false;
        }

        if (SwooleGS->start > 0)
        {
            raise_warning("Server is running. Unable to set event callback now.");
            return false;
        }

        const char *callback_name[PHP_SERVER_CALLBACK_NUM] = {
                "Connect",
                "Receive",
                "Close",
                "Packet",
                "Start",
                "Shutdown",
                "WorkerStart",
                "WorkerStop",
                "Task",
                "Finish",
                "WorkerError",
                "ManagerStart",
                "ManagerStop",
                "PipeMessage",
                NULL,
                NULL,
                NULL,
                NULL,
        };

        bool isSupportType = false;
        for (int i = 0; i < PHP_SERVER_CALLBACK_NUM; i++)
        {
            if (callback_name[i] == NULL)
            {
                continue;
            }
            if (strncasecmp(callback_name[i], event.c_str(), event.length()) == 0)
            {
                this_->o_set(event, callback);
                isSupportType = true;
                break;
            }
        }

        if (!isSupportType)
        {
            raise_warning("Unknown event types[%s]", event.c_str());
            return false;
        }
        return true;
    }

    static bool HHVM_METHOD(swoole_server, set, const Array&)
    {
        return true;
    }

    static bool HHVM_METHOD(swoole_server, start)
    {
        if (SwooleGS->start > 0)
        {
            raise_warning("Server is running. Unable to execute swoole_server::start.");
            return false;
        }

        swServer *serv = swoole_server_object;
        /**
         * create swoole server
         */
        if (swServer_create(serv) < 0)
        {
            raise_error("create server failed. Error: %s", sw_error);
        }
        //-------------------------------------------------------------
        serv->onReceive = hhvm_swoole_onReceive;
        serv->onTask = NULL;
        serv->onPacket = NULL;
        serv->ptr2 = this_;
        if (swServer_start(serv) < 0)
        {
            raise_error("start server failed. Error: %s", sw_error);
        }
        return true;
    }

    static class SwooleExtension : public Extension
    {
    public:
        SwooleExtension() :
                Extension("swoole")
        {
        }

        virtual void moduleInit()
        {
            HHVM_ME(swoole_server, __construct);
            HHVM_ME(swoole_server, on);
            HHVM_ME(swoole_server, set);
            HHVM_ME(swoole_server, start);
            loadSystemlib();
        }
    } s_swoole_extension;

    HHVM_GET_MODULE(swoole)
}
