#include "WebSocketServer.h"
#include <nidas/util/Logger.h> 

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

// Represents a single client connection.
class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket&& socket, WebSocketServer& server)
        : ws_(std::move(socket)), server_(server) {}

    void run() {
        ws_.async_accept(
            beast::bind_front_handler(&Session::on_accept, shared_from_this()));
    }

    void send(const std::shared_ptr<const std::string>& message) {
        // Post our work to the strand, this ensures thread safety.
        net::post(ws_.get_executor(),
            beast::bind_front_handler(&Session::on_send, shared_from_this(), message));
    }

private:
    void on_accept(beast::error_code ec) {
        if (ec) {
            ELOG("WebSocket accept error: " << ec.message());
            return;
        }

        ILOG("WebSocket client connected.");

        // Add this session to the server's list of active sessions
        {
            std::lock_guard<std::mutex> lock(server_.mutex_);
            server_.sessions_.insert(shared_from_this());
        }

        // Start reading messages from the client
        do_read();
    }

    void do_read() {
        ws_.async_read(buffer_,
            beast::bind_front_handler(&Session::on_read, shared_from_this()));
    }

    void on_read(beast::error_code ec, std::size_t) {
        if (ec == websocket::error::closed) {
            ILOG("WebSocket client disconnected.");
            std::lock_guard<std::mutex> lock(server_.mutex_);
            server_.sessions_.erase(shared_from_this());
            return;
        }
        if (ec) {
            ELOG("WebSocket read error: " << ec.message());
            std::lock_guard<std::mutex> lock(server_.mutex_);
            server_.sessions_.erase(shared_from_this());
            return;
        }
        
        // We don't expect any messages from clients, so just clear the buffer
        buffer_.consume(buffer_.size());
        // Wait for the next message (or the close message)
        do_read();
    }

    void on_send(const std::shared_ptr<const std::string>& message) {
        // If we are already writing, add to queue. For simplicity, we just write.
        ws_.async_write(net::buffer(*message),
            beast::bind_front_handler(&Session::on_write, shared_from_this()));
    }
    
    void on_write(beast::error_code ec, std::size_t) {
        if (ec) {
            ELOG("WebSocket write error: " << ec.message());
            std::lock_guard<std::mutex> lock(server_.mutex_);
            server_.sessions_.erase(shared_from_this());
        }
    }

    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;
    WebSocketServer& server_;
};

// --- WebSocketServer Implementation ---
WebSocketServer::WebSocketServer() : ioc_(), acceptor_(ioc_) {}

WebSocketServer::~WebSocketServer() {
    stop();
}

void WebSocketServer::run(const std::string& address, unsigned short port, int num_threads) {
    try {
        auto const endpoint = tcp::endpoint{net::ip::make_address(address), port};
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(net::socket_base::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen(net::socket_base::max_listen_connections);
    } catch (const beast::system_error& e) {
        ELOG("WebSocket server setup failed: " << e.what());
        return;
    }
    
    ILOG("WebSocket server starting on port " << port);
    accept_loop();
    
    threads_.reserve(num_threads);
    for(int i = 0; i < num_threads; ++i) {
        threads_.emplace_back([this]{ ioc_.run(); });
    }
}

void WebSocketServer::accept_loop() {
    acceptor_.async_accept(
        [this](beast::error_code ec, tcp::socket socket) {
            if (!ec) {
                std::make_shared<Session>(std::move(socket), *this)->run();
            } else {
                ELOG("WebSocket accept error in loop: " << ec.message());
            }
            // Continue accepting new connections unless the io_context is stopped
            if (acceptor_.is_open()) {
                accept_loop();
            }
        });
}

void WebSocketServer::stop() {
    if (acceptor_.is_open()) {
        ioc_.post([this]() { acceptor_.close(); });
    }
    for(auto& t : threads_) {
        if(t.joinable()) {
            t.join();
        }
    }
    ILOG("WebSocket server stopped.");
}

void WebSocketServer::broadcast(const std::string& message) {
    auto const shared_message = std::make_shared<const std::string>(message);
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& session : sessions_) {
        session->send(shared_message);
    }
}