#ifndef __CERBERUS_PROXY_HPP__
#define __CERBERUS_PROXY_HPP__

#include <vector>

#include "utils/pointer.h"
#include "command.hpp"
#include "slot_map.hpp"
#include "fdutil.hpp"

namespace cerb {

    class Proxy;

    class Connection
        : public FDWrapper
    {
    public:
        explicit Connection(int fd)
            : FDWrapper(fd)
        {}

        virtual ~Connection() {}

        virtual void triggered(int events) = 0;
        virtual void event_handled(std::set<Connection*>&) {}
        virtual void close() = 0;
    };

    class ProxyConnection
        : public Connection
    {
    protected:
        bool _closed;
    public:
        explicit ProxyConnection(int fd)
            : Connection(fd)
            , _closed(false)
        {}

        void event_handled(std::set<Connection*>&);
        void close();
    };

    class Acceptor
        : public Connection
    {
        Proxy* const _proxy;
    public:
        Acceptor(int fd, Proxy* p)
            : Connection(fd)
            , _proxy(p)
        {}

        void triggered(int events);
        void close();
    };

    class Server
        : public ProxyConnection
    {
        Proxy* const _proxy;
        Buffer _buffer;

        std::vector<util::sref<Command>> _commands;
        std::vector<util::sref<Command>> _ready_commands;

        void _send_to();
        void _recv_from();
    public:
        Server(std::string const& host, int port, Proxy* p);

        void triggered(int events);
        void event_handled(std::set<Connection*>&);

        void push_client_command(util::sref<Command> cmd);
        void pop_client(Client* cli);
        std::vector<util::sref<Command>> deliver_commands();
    };

    class Client
        : public ProxyConnection
    {
        void _write_response();
        void _read_request();

        Proxy* const _proxy;
        std::set<Server*> _peers;
        std::vector<util::sptr<CommandGroup>> _parsed_groups;
        std::vector<util::sptr<CommandGroup>> _awaiting_groups;
        std::vector<util::sptr<CommandGroup>> _ready_groups;
        int _awaiting_count;
        Buffer _buffer;

        void _process();
        void _response_ready();
    public:
        Client(int fd, Proxy* p)
            : ProxyConnection(fd)
            , _proxy(p)
            , _awaiting_count(0)
        {}

        ~Client();

        void triggered(int events);
        void group_responsed();
        void add_peer(Server* svr);
        void reactivate(util::sref<Command> cmd);
        void push_command(util::sptr<CommandGroup> g);
        void stat_proccessed(Interval cmd_elapse);
    };

    class SlotsMapUpdater
        : public Connection
    {
        Proxy* _proxy;
        std::map<slot, util::Address> _updated_map;
        Buffer _rsp;

        void _send_cmd();
        void _recv_rsp();
        void _await_data();
    public:
        SlotsMapUpdater(util::Address const& addr, Proxy* p);

        void triggered(int events);
        void close();

        bool success() const
        {
            return !_updated_map.empty();
        }

        std::map<slot, util::Address> deliver_map()
        {
            return std::move(_updated_map);
        }
    };

    class Proxy {
        int _clients_count;

        SlotMap<Server> _server_map;
        std::vector<util::sptr<SlotsMapUpdater>> _slot_updaters;
        std::vector<util::sptr<SlotsMapUpdater>> _finished_slot_updaters;
        int _active_slot_updaters_count;
        std::vector<util::sref<Command>> _retrying_commands;
        Interval _total_cmd_elapse;
        long _total_cmd;
        bool _server_closed;

        bool _should_update_slot_map() const;
        void _retrieve_slot_map();
        void _set_slot_map(std::map<slot, util::Address> map);
        void _update_slot_map();
        void _loop();
    public:
        int epfd;

        explicit Proxy(util::Address const& remote);
        ~Proxy();

        Proxy(Proxy const&) = delete;

        int clients_count() const
        {
            return _clients_count;
        }

        long total_cmd() const
        {
            return _total_cmd;
        }

        Interval total_cmd_elapse() const
        {
            return _total_cmd_elapse;
        }

        util::Address const& random_addr() const
        {
            return _server_map.random_addr();
        }

        Server* get_server_by_slot(slot key_slot);
        void notify_slot_map_updated();
        void server_closed();
        void retry_move_ask_command_later(util::sref<Command> cmd);
        void run(int listen_port);
        void accept_from(int listen_fd);
        void pop_client(Client* cli);
        void stat_proccessed(Interval cmd_elapse);
    };

}

#endif /* __CERBERUS_PROXY_HPP__ */
