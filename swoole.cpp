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

    static Variant get_recv_data(swEventData *req, char *header, uint32_t header_length)
    {
        char *data_ptr = NULL;
        int data_len;

#ifdef SW_USE_RINGBUFFER
        swPackage package;
        if (req->info.type == SW_EVENT_PACKAGE)
        {
            memcpy(&package, req->data, sizeof (package));

            data_ptr = package.data;
            data_len = package.length;
        }
#else
        if (req->info.type == SW_EVENT_PACKAGE_END)
        {
            swString *worker_buffer = swWorker_get_buffer(SwooleG.serv, req->info.from_id);
            data_ptr = worker_buffer->str;
            data_len = worker_buffer->length;
        }
#endif
        else
        {
            data_ptr = req->data;
            data_len = req->info.len;
        }

        Variant retval;
        if (header_length >= data_len)
        {
            retval = Variant("");
        }
        else
        {
            retval = Variant(String(data_ptr + header_length, data_len - header_length, CopyString));
        }

        if (header_length > 0)
        {
            memcpy(header, data_ptr, header_length);
        }

#ifdef SW_USE_RINGBUFFER
        if (req->info.type == SW_EVENT_PACKAGE)
        {
            swReactorThread *thread = swServer_get_thread(SwooleG.serv, req->info.from_id);
            thread->buffer_input->free(thread->buffer_input, data_ptr);
        }
#endif
        return retval;
    }

    static int hhvm_swoole_onReceive(swServer *serv, swEventData *req)
    {
        auto this_ = (ObjectData *) serv->ptr2;
        auto args = Array();
        args.append(Variant(this_));
        args.append(Variant(req->info.fd));
        args.append(Variant(req->info.from_id));
        args.append(get_recv_data(req, nullptr, 0));
        const Variant callback = this_->o_get("onReceive");
        vm_call_user_func(callback, args);
        return 0;
    }

    static void hhvm_swoole_onConnect(swServer *serv, swDataHead *info)
    {
        auto this_ = (ObjectData *) serv->ptr2;
        auto args = Array();
        args.append(Variant(this_));
        args.append(Variant(info->fd));
        args.append(Variant(info->from_id));
        const Variant callback = this_->o_get("onConnect");
        vm_call_user_func(callback, args);
    }

    static void hhvm_swoole_onClose(swServer *serv, swDataHead *info)
    {
        auto this_ = (ObjectData *) serv->ptr2;
        auto args = Array();
        args.append(Variant(this_));
        args.append(Variant(info->fd));
        args.append(Variant(info->from_id));
        const Variant callback = this_->o_get("onClose");
        vm_call_user_func(callback, args);
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

        const String callback_name[PHP_SERVER_CALLBACK_NUM] = {
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
        };

        bool isSupportType = false;
        for (int i = 0; i < PHP_SERVER_CALLBACK_NUM; i++)
        {
            if (strcasecmp(event.c_str(), callback_name[i].c_str()) == 0)
            {
                this_->o_set("on" + callback_name[i], callback);
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
        if (!this_->o_get("onConnect", false).isNull())
        {
            serv->onConnect = hhvm_swoole_onConnect;
        }
        if (!this_->o_get("onClose", false).isNull())
        {
            serv->onClose = hhvm_swoole_onClose;
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

    static bool HHVM_METHOD(swoole_server, send, int fd, const String &data)
    {
        if (SwooleGS->start == 0)
        {
            raise_warning("Server is not running.");
            return false;
        }
        if (data.length() <= 0)
        {
            raise_warning("data is empty.");
            return false;
        }
        return swServer_tcp_send(swoole_server_object, fd, (char *)data.c_str(), data.length()) == SW_OK ? true : false;
    }

    static bool HHVM_METHOD(swoole_server, close, int fd, bool reset = false)
    {
        if (SwooleGS->start == 0)
        {
            raise_warning("Server is not running.");
            return false;
        }

        if (swIsMaster())
        {
            raise_warning("Cannot close connection in master process.");
            return false;
        }

        swConnection *conn = swServer_connection_verify_no_ssl(swoole_server_object, fd);
        if (!conn)
        {
            return false;
        }

        //Reset send buffer, Immediately close the connection.
        if (reset)
        {
            conn->close_reset = 1;
        }

        int ret;
        if (!swIsWorker())
        {
            swWorker *worker = swServer_get_worker(swoole_server_object, conn->fd % swoole_server_object->worker_num);
            swDataHead ev;
            ev.type = SW_EVENT_CLOSE;
            ev.fd = fd;
            ev.from_id = conn->from_id;
            ret = swWorker_send2worker(worker, &ev, sizeof(ev), SW_PIPE_MASTER);
        }
        else
        {
            ret = swoole_server_object->factory.end(&swoole_server_object->factory, fd);
        }
        return ret == SW_OK ? true : false;
    }

    static class SwooleExtension : public Extension
    {
    public:
        SwooleExtension() : Extension("swoole")
        {
        }

        virtual void moduleInit()
        {
            HHVM_ME(swoole_server, __construct);
            HHVM_ME(swoole_server, on);
            HHVM_ME(swoole_server, set);
            HHVM_ME(swoole_server, send);
            HHVM_ME(swoole_server, close);
            HHVM_ME(swoole_server, start);
            loadSystemlib();
        }
    } s_swoole_extension;

    HHVM_GET_MODULE(swoole)
}
