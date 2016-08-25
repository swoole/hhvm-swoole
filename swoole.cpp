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
#include "hphp/runtime/base/request-local.h"
#include "hphp/runtime/base/array-init.h"

#include "config.h"
#include "swoole/include/swoole.h"
#include "swoole/include/Server.h"

#include <sys/stat.h>

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

const StaticString s_SWOOLE_BASE("SWOOLE_BASE");
const StaticString s_SWOOLE_PROCESS("SWOOLE_PROCESS");
const StaticString s_SWOOLE_SOCK_TCP("SWOOLE_SOCK_TCP");
const StaticString s_SWOOLE_SOCK_TCP6("SWOOLE_SOCK_TCP6");
const StaticString s_SWOOLE_SOCK_UDP("SWOOLE_SOCK_UDP");
const StaticString s_SWOOLE_SOCK_UDP6("SWOOLE_SOCK_UDP6");
const StaticString s_SWOOLE_SOCK_UNIX_DGRAM("SWOOLE_SOCK_UNIX_DGRAM");
const StaticString s_SWOOLE_SOCK_UNIX_STREAM("SWOOLE_SOCK_UNIX_STREAM");

static int task_id = 2;

static int check_task_param(int dst_worker_id)
{
    if (SwooleG.task_worker_num < 1)
    {
        raise_warning("Task method cannot use, Please set task_worker_num.");
        return SW_ERR;
    }
    if (dst_worker_id >= SwooleG.task_worker_num)
    {
        raise_warning("worker_id must be less than serv->task_worker_num.");
        return SW_ERR;
    }
    if (!swIsWorker())
    {
        raise_warning("The method can only be used in the worker process.");
        return SW_ERR;
    }
    return SW_OK;
}

static int task_pack(swEventData *task, const Variant &data)
{
    task->info.type = SW_EVENT_TASK;
    //field fd save task_id
    task->info.fd = task_id++;
    //field from_id save the worker_id
    task->info.from_id = SwooleWG.id;
    swTask_type(task) = 0;

    String _data;
    //need serialize
    if (!data.isString())
    {
        //serialize
        swTask_type(task) |= SW_TASK_SERIALIZE;
        _data = f_serialize(data);
    }
    else
    {
        _data = data.toString();
    }

    if (_data.length() >= SW_IPC_MAX_SIZE - sizeof(task->info))
    {
        if (swTaskWorker_large_pack(task, (char *) _data.c_str(), _data.length()) < 0)
        {
            raise_warning("large task pack failed()");
            return SW_ERR;
        }
    }
    else
    {
        memcpy(task->data, (char *) _data.c_str(), _data.length());
        task->info.len = _data.length();
    }
    return task->info.fd;
}

static Variant task_unpack(swEventData *task_result)
{
    char *result_data_str;
    int result_data_len = 0;

    int data_len;
    char *data_str = NULL;

    /**
     * Large result package
     */
    if (swTask_type(task_result) & SW_TASK_TMPFILE)
    {
        swTaskWorker_large_unpack(task_result, malloc, data_str, data_len);
        /**
         * unpack failed
         */
        if (data_len == -1)
        {
            if (data_str)
            {
                free(data_str);
            }
            return null_variant;
        }
        result_data_str = data_str;
        result_data_len = data_len;
    }
    else
    {
        result_data_str = task_result->data;
        result_data_len = task_result->info.len;
    }

    Variant data;
    if (swTask_type(task_result) & SW_TASK_SERIALIZE)
    {
        data = unserialize_from_buffer(result_data_str, result_data_len);
    }
    else
    {
        data = Variant(String(result_data_str, result_data_len, CopyString));
    }
    if (data_str)
    {
        free(data_str);
    }
    return data;
}

static bool task_finish(Variant result)
{
    int flags = 0;
    int ret;
    String str;
    //need serialize
    if (!result.isString())
    {
        flags |= SW_TASK_SERIALIZE;
        str = f_serialize(result);
    }
    else
    {
        str = result.toString();
    }
    ret = swTaskWorker_finish(swoole_server_object, (char *) str.c_str(), str.length(), flags);
    return ret == 0 ? true : false;
}

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

static int hhvm_swoole_onPacket(swServer *serv, swEventData *req)
{
    auto this_ = (ObjectData *) serv->ptr2;
    swDgramPacket *packet;
    auto args = Array();
    auto clientInfo = Array();
    Variant data;

    swString *buffer = swWorker_get_buffer(serv, req->info.from_id);
    packet = (swDgramPacket*) buffer->str;

    clientInfo.set(String("server_socket"), Variant(req->info.from_fd));

    //udp ipv4
    if (req->info.type == SW_EVENT_UDP)
    {
        struct in_addr sin_addr;
        sin_addr.s_addr = packet->addr.v4.s_addr;
        char *address = inet_ntoa(sin_addr);
        clientInfo.set(String("address"), String(address));
        clientInfo.set(String("port"), Variant(packet->port));
        data = Variant(String(packet->data, packet->length, CopyString));
    }
    //udp ipv6
    else if (req->info.type == SW_EVENT_UDP6)
    {
        char tmp[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &packet->addr.v6, tmp, sizeof(tmp));
        clientInfo.set(String("address"), String(tmp));
        clientInfo.set(String("port"), Variant(packet->port));
        data = Variant(String(packet->data, packet->length, CopyString));
    }
    //unix dgram
    else if (req->info.type == SW_EVENT_UNIX_DGRAM)
    {
        clientInfo.set(String("address"), String(packet->data, packet->addr.un.path_length, CopyString));
        clientInfo.set(String("port"), Variant(packet->port));
        data = Variant(String(packet->data + packet->addr.un.path_length, packet->length - packet->addr.un.path_length, CopyString));
    }

    args.append(Variant(this_));
    args.append(data);
    args.append(Variant(clientInfo));
    const Variant callback = this_->o_get("onPacket");
    vm_call_user_func(callback, args);
    return SW_OK;
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

static int hhvm_swoole_onTask(swServer *serv, swEventData *req)
{
    auto this_ = (ObjectData *) serv->ptr2;
    auto args = Array();
    Variant data;
    sw_atomic_fetch_sub(&SwooleStats->tasking_num, 1);

    args.append(Variant(this_));
    args.append(Variant(req->info.fd));
    args.append(Variant(req->info.from_id));

    char *data_ptr;
    uint32_t data_length;
    bool free_memory = false;

    if (swTask_type(req) & SW_TASK_TMPFILE)
    {
        int data_len;
        char *buf = NULL;
        swTaskWorker_large_unpack(req, malloc, buf, data_len);
        if (data_len == -1)
        {
            if (buf)
            {
                free(buf);
            }
            return SW_OK;
        }
        data_ptr = buf;
        data_length = data_len;
        free_memory = true;
    }
    else
    {
        data_ptr = req->data;
        data_length = req->info.len;
    }

    if (swTask_type(req) & SW_TASK_SERIALIZE)
    {
        data = unserialize_from_buffer(data_ptr, data_length);
    }
    else
    {
        data = String(data_ptr, data_length, CopyString);
    }
    if (free_memory)
    {
        free(data_ptr);
    }
    args.append(data);
    const Variant callback = this_->o_get("onTask");
    auto retval = vm_call_user_func(callback, args);

    if (!retval.isNull())
    {
        task_finish(retval);
    }
    return SW_OK;
}

static int hhvm_swoole_onFinish(swServer *serv, swEventData *req)
{
    auto this_ = (ObjectData *) serv->ptr2;
    auto args = Array();
    auto data = task_unpack(req);
    if (data.isNull())
    {
        return SW_OK;
    }

    args.append(Variant(this_));
    args.append(Variant(req->info.fd));
    args.append(data);
    Variant callback;
    if (swTask_type(req) & SW_TASK_CALLBACK)
    {
        callback = this_->o_get("task_callbacks").asArrRef()[req->info.fd];
    }
    else
    {
        callback = this_->o_get("onFinish");
    }
    vm_call_user_func(callback, args);
    if (swTask_type(req) & SW_TASK_CALLBACK)
    {
        this_->o_get("task_callbacks").asArrRef().remove(req->info.fd);
    }
    return SW_OK;
}

static void hhvm_swoole_onWorkerStart(swServer *serv, int worker_id)
{
    auto this_ = (ObjectData *) serv->ptr2;
    auto args = Array();
    args.append(Variant(this_));
    args.append(Variant(worker_id));
    const Variant callback = this_->o_get("onWorkerStart", false);

    this_->o_set("master_pid", Variant(SwooleGS->master_pid));
    this_->o_set("manager_pid", Variant(SwooleGS->manager_pid));
    this_->o_set("worker_id", Variant(worker_id));

    if (!callback.isNull())
    {
        vm_call_user_func(callback, args);
    }
}

static void hhvm_swoole_onWorkerStop(swServer *serv, int worker_id)
{
    auto this_ = (ObjectData *) serv->ptr2;
    auto args = Array();
    args.append(Variant(this_));
    args.append(Variant(worker_id));
    const Variant callback = this_->o_get("onWorkerStop");
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

static bool HHVM_METHOD(swoole_server, set, const Array& setting)
{
    if (SwooleGS->start > 0)
    {
        raise_warning("Server is running. Unable to set event callback now.");
        return false;
    }
    //chroot
    if (setting.exists(String("chroot")))
    {
        SwooleG.chroot = strdup(setting[String("chroot")].toString().c_str());
    }
    //user
    if (setting.exists(String("user")))
    {
        SwooleG.chroot = strdup(setting[String("user")].toString().c_str());
    }
    //group
    if (setting.exists(String("group")))
    {
        SwooleG.chroot = strdup(setting[String("group")].toString().c_str());
    }
    auto serv = swoole_server_object;
    //daemonize
    if (setting.exists(String("daemonize")))
    {
        swoole_server_object->daemonize = setting[String("group")].toBoolean() ? 1 : 0;
    }
    //reactor thread num
    if (setting.exists(String("reactor_num")))
    {
        serv->reactor_num =  setting[String("reactor_num")].toInt16();
        if (serv->reactor_num <= 0)
        {
            serv->reactor_num = SwooleG.cpu_num;
        }
    }
    //worker_num
    if (setting.exists(String("worker_num")))
    {
        serv->worker_num = setting[String("worker_num")].toInt16();
        if (serv->worker_num <= 0)
        {
            serv->worker_num = SwooleG.cpu_num;
        }
    }
    //dispatch_mode
    if (setting.exists(String("dispatch_mode")))
    {
        serv->dispatch_mode = setting[String("group")].toInt16();
    }
    //log_file
    if (setting.exists(String("log_file")))
    {
        SwooleG.chroot = strdup(setting[String("log_file")].toString().c_str());
    }
    //log_level
    if (setting.exists(String("log_level")))
    {
        SwooleG.log_level = setting[String("group")].toInt16();
    }
    /**
     * for dispatch_mode = 1/3
     */
    if (setting.exists(String("discard_timeout_request")))
    {
        serv->discard_timeout_request = setting[String("discard_timeout_request")].toBoolean() ? 1 : 0;
    }
    //onConnect/onClose event
    if (setting.exists(String("enable_unsafe_event")))
    {
        serv->enable_unsafe_event = setting[String("enable_unsafe_event")].toBoolean() ? 1 : 0;
    }
    //port reuse
    if (setting.exists(String("enable_port_reuse")))
    {
        SwooleG.reuse_port = setting[String("enable_port_reuse")].toBoolean() ? 1 : 0;
    }
    //delay receive
    if (setting.exists(String("enable_delay_receive")))
    {
        serv->enable_delay_receive = setting[String("enable_delay_receive")].toBoolean() ? 1 : 0;
    }
    //task_worker_num
    if (setting.exists(String("task_worker_num")))
    {
        SwooleG.task_worker_num = setting[String("task_worker_num")].toInt16();
        this_->o_set("task_callbacks", Array::Create());
    }
    //task ipc mode, 1,2,3
    if (setting.exists(String("task_ipc_mode")))
    {
        SwooleG.task_ipc_mode = setting[String("task_ipc_mode")].toInt16();
    }
    /**
     * Temporary file directory for task_worker
     */
    if (setting.exists(String("task_tmpdir")))
    {
        SwooleG.task_tmpdir = (char *) malloc(SW_TASK_TMPDIR_SIZE);
        SwooleG.task_tmpdir_len = snprintf(SwooleG.task_tmpdir, SW_TASK_TMPDIR_SIZE, "%s/task.XXXXXX", setting[String("task_tmpdir")].toString().c_str());
        if (SwooleG.task_tmpdir_len > SW_TASK_TMPDIR_SIZE - 1)
        {
            raise_error("task_tmpdir is too long, max size is %d.", SW_TASK_TMPDIR_SIZE - 1);
        }
    }
    else
    {
        SwooleG.task_tmpdir = strndup(SW_TASK_TMP_FILE, sizeof (SW_TASK_TMP_FILE));
        SwooleG.task_tmpdir_len = sizeof (SW_TASK_TMP_FILE);
    }
    //task_max_request
    if (setting.exists(String("task_max_request")))
    {
        SwooleG.task_max_request = setting[String("task_max_request")].toInt32();
    }
    //max_connection
    if (setting.exists(String("max_connection")))
    {
        serv->max_connection = setting[String("max_connection")].toInt32();
    }
    else if (setting.exists(String("max_conn")))
    {
        serv->max_connection = setting[String("max_conn")].toInt32();
    }
    //heartbeat_check_interval
    if (setting.exists(String("heartbeat_check_interval")))
    {
        serv->heartbeat_check_interval = setting[String("heartbeat_check_interval")].toInt32();
    }
    //heartbeat idle time
    if (setting.exists(String("heartbeat_idle_time")))
    {
        serv->heartbeat_idle_time = setting[String("heartbeat_idle_time")].toInt32();
        if (serv->heartbeat_check_interval > serv->heartbeat_idle_time)
        {
            raise_warning("heartbeat_idle_time must be greater than heartbeat_check_interval.");
            serv->heartbeat_check_interval = serv->heartbeat_idle_time / 2;
        }
    }
    else if (serv->heartbeat_check_interval > 0)
    {
        serv->heartbeat_idle_time = serv->heartbeat_check_interval * 2;
    }
    //max_request
    if (setting.exists(String("max_request")))
    {
        serv->max_request = setting[String("max_request")].toInt32();
    }
    //cpu affinity
    if (setting.exists(String("open_cpu_affinity")))
    {
        serv->open_cpu_affinity = setting[String("open_cpu_affinity")].toBoolean() ? 1 : 0;
    }
    //paser x-www-form-urlencoded form data
    if (setting.exists(String("http_parse_post")))
    {
        serv->http_parse_post = setting[String("http_parse_post")].toBoolean() ? 1 : 0;
    }
    /**
     * buffer input size
     */
    if (setting.exists(String("buffer_input_size")))
    {
        serv->buffer_input_size = setting[String("buffer_input_size")].toInt32();
    }
    /**
     * buffer output size
     */
    if (setting.exists(String("buffer_output_size")))
    {
        serv->buffer_output_size = setting[String("buffer_output_size")].toInt32();
    }
    /**
     * set pipe memory buffer size
     */
    if (setting.exists(String("pipe_buffer_size")))
    {
        serv->pipe_buffer_size = setting[String("pipe_buffer_size")].toInt32();
    }
    //message queue key
    if (setting.exists(String("message_queue_key")))
    {
        serv->message_queue_key =  setting[String("message_queue_key")].toInt32();
    }
    this_->o_set("setting", setting);
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
    if (!this_->o_get("onWorkerStop", false).isNull())
    {
        serv->onWorkerStop = hhvm_swoole_onWorkerStop;
    }
    if (!this_->o_get("onPacket", false).isNull())
    {
        serv->onPacket = hhvm_swoole_onPacket;
    }
    if (!this_->o_get("onTask", false).isNull())
    {
        serv->onTask = hhvm_swoole_onTask;
    }
    if (!this_->o_get("onFinish", false).isNull())
    {
        serv->onFinish = hhvm_swoole_onFinish;
    }
    serv->onReceive = hhvm_swoole_onReceive;
    serv->onWorkerStart = hhvm_swoole_onWorkerStart;
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

static bool HHVM_METHOD(swoole_server, sendto, const String &ip, int port, const String &data, int server_socket = -1)
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
    bool ipv6 = false;
    if (strchr(ip.c_str(), ':'))
    {
        ipv6 = true;
    }

    if (ipv6 && swoole_server_object->udp_socket_ipv6 <= 0)
    {
        raise_warning("You must add an UDP6 listener to server before using sendto.");
        return false;
    }
    else if (swoole_server_object->udp_socket_ipv4 <= 0)
    {
        raise_warning("You must add an UDP listener to server before using sendto.");
        return false;
    }

    if (server_socket < 0)
    {
        server_socket = ipv6 ?  swoole_server_object->udp_socket_ipv6 : swoole_server_object->udp_socket_ipv4;
    }

    int ret;
    if (ipv6)
    {
        ret = swSocket_udp_sendto6(server_socket, (char*)ip.c_str(), port, (char*)data.c_str(), data.length());
    }
    else
    {
        ret = swSocket_udp_sendto(server_socket, (char*)ip.c_str(), port,(char*)data.c_str(), data.length());
    }
    return ret == SW_OK ? true : false;
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

static int HHVM_METHOD(swoole_server, task, const Variant &data, int dst_worker_id, const Variant &callback)
{
    if (SwooleGS->start == 0)
    {
        raise_warning("Server is not running.");
        return false;
    }
    swEventData buf;

    if (check_task_param(dst_worker_id) < 0)
    {
        return false;
    }

    if (task_pack(&buf, data) < 0)
    {
        return false;
    }

    if (!callback.isNull())
    {
        if (!is_callable(callback))
        {
            raise_warning("Function '%s' is not callable", callback.toString().c_str());
            return false;
        }
        swTask_type(&buf) |= SW_TASK_CALLBACK;
        auto name = this_->o_realProp("task_callbacks", ObjectData::RealPropUnchecked);
        name->asArrRef().add(buf.info.fd, callback);
    }

    swTask_type(&buf) |= SW_TASK_NONBLOCK;
    if (swProcessPool_dispatch(&SwooleGS->task_workers, &buf, &dst_worker_id) >= 0)
    {
        sw_atomic_fetch_add(&SwooleStats->tasking_num, 1);
        return buf.info.fd;
    }
    else
    {
        return false;
    }
}

static bool HHVM_METHOD(swoole_server, exist, int fd)
{
    if (SwooleGS->start == 0)
    {
        raise_warning("Server is not running.");
        return false;
    }

    swConnection *conn = swServer_connection_verify_no_ssl(swoole_server_object, fd);
    if (!conn || conn->active == 0 || conn->closed)
    {
        return false;
    }
    else
    {
        return true;
    }
}

static bool HHVM_METHOD(swoole_server, sendfile, int fd, const String &file, long offset = 0)
{
    if (SwooleGS->start == 0)
    {
        raise_warning("Server is not running.");
        return false;
    }

    //check fd
    if (fd <= 0 || fd > SW_MAX_SOCKET_ID)
    {
        swoole_error_log(SW_LOG_WARNING, SW_ERROR_SESSION_INVALID_ID, "invalid fd[%d].", fd);
        return false;
    }

    swConnection *conn = swServer_connection_verify_no_ssl(swoole_server_object, fd);
    if (!conn || conn->active == 0 || conn->closed)
    {
        return false;
    }
    else
    {
        struct stat file_stat;
        if (stat(file.c_str(), &file_stat) < 0)
        {
            raise_warning("stat(%s) failed.", file.c_str());
            return false;
        }

        if (file_stat.st_size <= offset)
        {
            raise_warning("file[offset=%ld] is empty.", offset);
            return false;
        }

        return swServer_tcp_sendfile(swoole_server_object, fd, (char *)file.c_str(), file.length(), offset) == SW_OK ? true : false;
    }
}

static Variant HHVM_METHOD(swoole_server, getClientInfo, int fd, int reactorId, bool noCheckConnection)
{
    if (SwooleGS->start == 0)
    {
        raise_warning("Server is not running.");
        return Variant(false);
    }

    swConnection *conn = swServer_connection_verify(swoole_server_object, fd);
    if (!conn)
    {
        return Variant(false);
    }
    //connection is closed
    if (conn->active == 0 && !noCheckConnection)
    {
        return Variant(false);
    }
    else
    {
        auto retval = Array();

        if (swoole_server_object->dispatch_mode == SW_DISPATCH_UIDMOD)
        {
            retval.set(String("uid"), Variant(conn->uid));
        }

        swListenPort *port = swServer_get_port(swoole_server_object, conn->fd);
        if (port && port->open_websocket_protocol)
        {
            retval.set(String("websocket_status"), Variant(conn->websocket_status));
        }
#ifdef SW_USE_OPENSSL
        if (conn->ssl_client_cert.length > 0)
        {
            retval.set(String("ssl_client_cert"), Variant(String(conn->ssl_client_cert.str, conn->ssl_client_cert.length - 1, CopyString)));
        }
#endif
        //server socket
        swConnection *from_sock = swServer_connection_get(swoole_server_object, conn->from_fd);
        if (from_sock)
        {
            retval.set(String("server_port"), Variant(swConnection_get_port(from_sock)));
        }
        retval.set(String("server_fd"), Variant(conn->from_fd));
        retval.set(String("socket_type"), Variant(conn->socket_type));
        retval.set(String("remote_port"), Variant(swConnection_get_port(conn)));
        retval.set(String("remote_ip"), Variant(String(swConnection_get_ip(conn))));
        retval.set(String("from_id"), Variant( conn->from_id));
        retval.set(String("connect_time"), Variant(conn->connect_time));
        retval.set(String("last_time"), Variant(conn->last_time));
        retval.set(String("close_errno"), Variant(conn->close_errno));
        return Variant(retval);
    }
}

static class SwooleExtension : public Extension
{
public:
    SwooleExtension() : Extension("swoole")
    {
    }

    virtual void moduleInit()
    {
        Native::registerConstant<KindOfInt64>(
                s_SWOOLE_BASE.get(), SW_MODE_BASE
        );
        Native::registerConstant<KindOfInt64>(
                s_SWOOLE_PROCESS.get(), SW_MODE_PROCESS
        );
        Native::registerConstant<KindOfInt64>(
                s_SWOOLE_SOCK_TCP.get(), SW_SOCK_TCP
        );
        Native::registerConstant<KindOfInt64>(
                s_SWOOLE_SOCK_TCP6.get(), SW_SOCK_TCP6
        );
        Native::registerConstant<KindOfInt64>(
                s_SWOOLE_SOCK_UDP.get(), SW_SOCK_UDP
        );
        Native::registerConstant<KindOfInt64>(
                s_SWOOLE_SOCK_UDP6.get(), SW_SOCK_UDP6
        );
        Native::registerConstant<KindOfInt64>(
                s_SWOOLE_SOCK_UNIX_DGRAM.get(), SW_SOCK_UNIX_DGRAM
        );
        Native::registerConstant<KindOfInt64>(
                s_SWOOLE_SOCK_UNIX_STREAM.get(), SW_SOCK_UNIX_STREAM
        );
        HHVM_ME(swoole_server, __construct);
        HHVM_ME(swoole_server, on);
        HHVM_ME(swoole_server, set);
        HHVM_ME(swoole_server, send);
        HHVM_ME(swoole_server, sendto);
        HHVM_ME(swoole_server, close);
        HHVM_ME(swoole_server, getClientInfo);
        HHVM_ME(swoole_server, exist);
        HHVM_ME(swoole_server, sendfile);
        HHVM_ME(swoole_server, task);
        HHVM_ME(swoole_server, start);
        loadSystemlib();
    }
} s_swoole_extension;

HHVM_GET_MODULE(swoole)
}
