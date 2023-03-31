#include "stdafx.h"
#include "netcore_monkey_test.h"

#include <functional>

#include <base/files/file_path.h>
#include <base/comm/path_service.h>
#include <base/files/file_util.h>
#include <base/log/logger.h>
#include <base/math/rand_util.h>
#include <base/comm/no_destructor.h>

#include <include/netcore/wrapper/netcore_wrapper.h>

static const char* g_logger_name = "netcore_monkey_test";

void on_call_monkey(netcore::NetResultType type, void* data, void* context)
{
    baselog::infox(g_logger_name, "[user_call] user on_called successed: type= {}, data= {}, context= {}",
        (int)type, data, context);
    auto header = reinterpret_cast<netcore::NetResultHeader*>(data);
    auto progress = reinterpret_cast<netcore::NetResultProgress*>(data);
    auto finish = reinterpret_cast<netcore::NetResultFinish*>(data);
    auto response = reinterpret_cast<netcore::NetResultWrite*>(data);

    switch (type) {
    case netcore::NetResultType::NRT_ONCB_HEADER:
        baselog::infox(g_logger_name, "[user_call] new header: {}", std::string(header->data, (std::size_t)header->data_len));
        break;
    case netcore::NetResultType::NRT_ONCB_PROGRESS:
        baselog::infox(g_logger_name, "[user_call] new progress: download= {}/{}@{:.2f} upload: {}/{}@{:.2f}",
            progress->download_transfered_size,
            progress->download_total_size,
            progress->download_speed,
            progress->upload_transfered_size,
            progress->upload_total_size,
            progress->upload_speed);
        break;
    case netcore::NetResultType::NRT_ONCB_WRITE:
        baselog::infox(g_logger_name, "[user_call] new response data: len= {}", response->data_len);
        break;
    case netcore::NetResultType::NRT_ONCB_FINISH:
        baselog::infox(g_logger_name, "[user_call] http finished message: url={}-{}.{}.{}.{}-clen={}-metric={}.{}.[{}|{}]",
            finish->request_url, (int)finish->result_code, finish->http_response_code,
            finish->http_header_len, finish->data_len, finish->content_length_download,
            finish->total_time_ms, finish->redirect_count,
            finish->app_average_speed, finish->download_speed_bytes_persecond);
        //baselog::infox(g_logger_name, "[user_call] http finished message: ");
        //baselog::infox(g_logger_name, "[user_call] url= {}", finish->request_url);
        //baselog::infox(g_logger_name, "[user_call] result_code= {}", (int)finish->result_code);
        //baselog::infox(g_logger_name, "[user_call] response_code= {}", finish->http_response_code);
        //baselog::infox(g_logger_name, "[user_call] content_type= {}", SAFE_STRING_PRINTF(finish->http_content_type));
        //baselog::infox(g_logger_name, "[user_call] content_length= {}", finish->content_length_download);
        //baselog::infox(g_logger_name, "[user_call] header_length= {}", finish->http_header_len);
        //baselog::infox(g_logger_name, "[user_call] data_length= {}", finish->data_len);
        //baselog::infox(g_logger_name, "[user_call] app_average_spped= {}", finish->app_average_speed);
        //baselog::infox(g_logger_name, "[user_call] download_spped= {}", finish->download_speed_bytes_persecond);
        //baselog::infox(g_logger_name, "[user_call] client ip string= {}", SAFE_STRING_PRINTF(finish->primary_ip_string));
        //baselog::infox(g_logger_name, "[user_call] namelookup time = {} ms", finish->namelookup_time_ms);
        //baselog::infox(g_logger_name, "[user_call] connected time = {} ms", finish->connected_time_ms);
        //baselog::infox(g_logger_name, "[user_call] appconnect time = {} ms", finish->app_connected_time_ms);
        //baselog::infox(g_logger_name, "[user_call] pretransfer time = {} ms", finish->pretransfer_time_ms);
        //baselog::infox(g_logger_name, "[user_call] starttransfer time = {} ms", finish->starttransfer_time_ms);
        //baselog::infox(g_logger_name, "[user_call] total time= {} ms", finish->total_time_ms);
        //baselog::infox(g_logger_name, "[user_call] redirect_count= {}", finish->redirect_count);
        //baselog::infox(g_logger_name, "[user_call] redirect_time= {} ms", finish->redirect_time_ms);
        break;
    default:
        break;
    }
}

NetcoreMonkeyTest& NetcoreMonkeyTest::Instance()
{
    static base::NoDestructor<NetcoreMonkeyTest> ins;
    return *ins.get();
}

bool NetcoreMonkeyTest::init(const base::FilePath& path)
{
    base::FilePath conf_path(path);
    if (conf_path.empty()) {
        base::PathService::Get(base::FILE_MODULE, &conf_path);
        conf_path = conf_path.DirName().AppendASCII("urls.txt");
    }

    if (!base::PathExists(conf_path)) {
        return false;
    }

    if (!_file_loadder.load(conf_path.AsUTF8Unsafe().c_str())) {
        return false;;
    }

    if (!baselog::initialize()) {
        return false;
    }
    if (!baselog::create_exclusive_sink(nullptr, baselog::log_sink::windebug_sink, nullptr)) {
        return false;
    }
    if (!baselog::create_logger(g_logger_name, baselog::log_sink::windebug_sink, nullptr)) {
        return false;
    }

    if (!NetcoreWrapper::Instance().Initialize()) {
        return false;
    }

    std::vector<std::string> urls;
    for (std::uint32_t i = 0; i < _file_loadder.max(); ++i) {
        urls.push_back(std::string(_file_loadder.get_buf(i), _file_loadder.get_buf_len(i)));
    }

    baselog::infox(g_logger_name, "init successed");
    return true;
}

bool NetcoreMonkeyTest::run()
{
    {
        std::lock_guard<std::mutex> lg(_mutex);
        _is_stopped = false;
    }
    _net_thread_create = std::move(std::thread(&NetcoreMonkeyTest::thrad_func_create, this));
    _net_thread_remove = std::move(std::thread(&NetcoreMonkeyTest::thrad_func_remove, this));
    _net_thread_request = std::move(std::thread(&NetcoreMonkeyTest::thrad_func_request, this));
    _net_thread_stop = std::move(std::thread(&NetcoreMonkeyTest::thrad_func_stop, this));

    baselog::infox(g_logger_name, "running successed");
    return true;
}

bool NetcoreMonkeyTest::stop()
{
    {
        std::lock_guard<std::mutex> lg(_mutex);
        _is_stopped = true;
    }

    if (_net_thread_create.joinable()) {
        _net_thread_create.join();
    }
    if (_net_thread_remove.joinable()) {
        _net_thread_remove.join();
    }
    if (_net_thread_request.joinable()) {
        _net_thread_request.join();
    }
    if (_net_thread_stop.joinable()) {
        _net_thread_stop.join();
    }

    baselog::infox(g_logger_name, "stoping successed"); 
    if (!baselog::uninitialize()) {
        return false;
    }
    return NetcoreWrapper::Instance().UnInitialize();
}

void NetcoreMonkeyTest::thrad_func_create()
{
    while (1) {
        static auto srv_ptr = NetcoreWrapper::Instance().NetServiceInstance();
        {
            std::lock_guard<std::mutex> lg(_mutex);
            if (_is_stopped) {
                break;
            }
        }

        auto chan = srv_ptr->create_channel();
        if (chan == nullptr) {
            IMMEDIATE_CRASH();
        }

        {
            std::lock_guard<std::mutex> lg(_mutex);
            _channel_pool.push_back(reinterpret_cast<void*>(chan));
        }

        baselog::infox(g_logger_name, "create channel successed: {}", (void*)chan);

        auto sleep_timems = base::RandInt(500, 5000);
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_timems));
    }
}

void NetcoreMonkeyTest::thrad_func_remove()
{
    while (1) {
        static auto srv_ptr = NetcoreWrapper::Instance().NetServiceInstance();

        {
            std::lock_guard<std::mutex> lg(_mutex);
            if (_is_stopped) {
                break;
            }
        }

        bool is_odd = base::RandUint64() % 2 == 0;
        netcore::INetChannel *channel = nullptr;
        {
            std::lock_guard<std::mutex> lg(_mutex);
            if (!_channel_pool.empty()) {
                channel = is_odd ? (reinterpret_cast<netcore::INetChannel*>(_channel_pool.front())) :
                    (reinterpret_cast<netcore::INetChannel*>(_channel_pool.back()));
                is_odd ? (_channel_pool.pop_front()) :(_channel_pool.pop_back());
            }
        }

        if (channel != nullptr) {
            baselog::infox(g_logger_name, "removing channel: {}", (void*)channel); 
            srv_ptr->remove_channel(channel);
            baselog::infox(g_logger_name, "removed channel done: {}", (void*)channel); 
        }

        auto sleep_timems = base::RandInt(1000, 4000);
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_timems));
    }
}

void NetcoreMonkeyTest::thrad_func_request()
{
    while (1) {
        static auto srv_ptr = NetcoreWrapper::Instance().NetServiceInstance();

        {
            std::lock_guard<std::mutex> lg(_mutex);
            if (_is_stopped) {
                break;
            }
        }

        netcore::INetChannel* channel = nullptr;
        static std::string url;
        auto url_index = base::RandInt(0, _file_loadder.max() - 1);
        url.assign(this->_file_loadder.get_buf(url_index), _file_loadder.get_buf_len(url_index));
        {
            std::lock_guard<std::mutex> lg(_mutex);
            if (!_channel_pool.empty()) {
                channel = reinterpret_cast<netcore::INetChannel*>(_channel_pool.front());
                _channel_pool.pop_front();
            }
        }

        if (channel != nullptr) {
            if (url_index % 2 == 0) {
                baselog::infox(g_logger_name, "new request posted: channel= {} url= {}", (void*)channel, url); 
                auto ret = channel->post_request(url,
                    std::bind(on_call_monkey,
                        std::placeholders::_1,
                        std::placeholders::_2,
                        std::placeholders::_3),
                    (void*)channel);
                baselog::infox(g_logger_name, "new request posted done: result = {}", ret); 
            } else { 
                baselog::infox(g_logger_name, "new request posted: channel= {} url= {}", (void*)channel, url); 
                auto ret = channel->post_request(url,
                    std::bind(on_call_monkey,
                        std::placeholders::_1,
                        std::placeholders::_2,
                        std::placeholders::_3),
                    (void*)channel);
                baselog::infox(g_logger_name, "new request posted done: result = {}", ret); 
            }

            {
                std::lock_guard<std::mutex> lg(_mutex);
                _channel_pool.push_back(channel);
            }
        }

        auto sleep_timems = base::RandInt(300, 900);
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_timems));
    }
}

void NetcoreMonkeyTest::thrad_func_stop()
{
    while (1) {
        static auto srv_ptr = NetcoreWrapper::Instance().NetServiceInstance();

        {
            std::lock_guard<std::mutex> lg(_mutex);
            if (_is_stopped) {
                break;
            }
        }

        netcore::INetChannel* channel = nullptr;
        {
            std::lock_guard<std::mutex> lg(_mutex);
            if (!_channel_pool.empty()) {
                channel = reinterpret_cast<netcore::INetChannel*>(_channel_pool.front());
                _channel_pool.pop_front();
            }
        }

        if (channel != nullptr) {
            if (base::RandUint64() % 2 == 0) {
                channel->post_stop();
                baselog::infox(g_logger_name, "channel has stopped posted: {}", (void*)channel);
            } else {
                channel->send_stop();
                baselog::infox(g_logger_name, "channel has stopped sent: {}", (void*)channel);
            }

            {
                std::lock_guard<std::mutex> lg(_mutex);
                _channel_pool.push_back(channel);
            }
        }

        auto sleep_timems = base::RandInt(1000, 20000);
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_timems));
    }
}
