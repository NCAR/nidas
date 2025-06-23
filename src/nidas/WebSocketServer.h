#ifndef WEBSOCKETSERVER_H
#define WEBSOCKETSERVER_H

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <set>
#include <mutex>


namespace boost { namespace beast { class error_code; } }
class Session;

class WebSocketServer {
public:
    WebSocketServer();
    ~WebSocketServer();

    // Starts the server listening on a port.
    void run(const std::string& address, unsigned short port, int num_threads = 1);

    // Stops the server
    void stop();

    // Sends a string message to all connected clients
    void broadcast(const std::string& message);

    // Friend class to allow Session to add/remove itself from the server's list
    friend class Session;

private:
    void accept_loop();

    boost::asio::io_context ioc_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::vector<std::thread> threads_;

    // We need to manage the set of active sessions to broadcast to them.
    std::set<std::shared_ptr<Session>> sessions_;
    std::mutex mutex_; // To protect access to the sessions set
};

#endif // WEBSOCKETSERVER_H