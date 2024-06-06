#ifndef NET_CLIENT_HPP
#define NET_CLIENT_HPP

#include <boost/asio.hpp>
#include <boost/asio/high_resolution_timer.hpp>
#include <boost/asio/io_context.hpp>

#include <chrono>
#include <restbed>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>

using boost::asio::ip::tcp;
using namespace std::placeholders;

class netClient {
public:

    netClient()
    : stopped_(false),
      socket_(io_context_),
      deadline_(io_context_)
    {}

    virtual ~netClient() {};

  // Called by the user of the client class to initiate the connection process.
  // The endpoint iterator will have been obtained using a tcp::resolver.
  void start() {
    auto endpoint_iter = tcp::resolver(io_context_).resolve(host, service);

    // Start the connect actor.
    start_connect(endpoint_iter);

    // Start the deadline actor. You will note that we're not setting any
    // particular deadline here. Instead, the connect and input actors will
    // update the deadline prior to each asynchronous operation.    
    deadline_.async_wait(std::bind(&netClient::check_deadline, this));
  }

void check_deadline()
  {
    if (stopped_)
      return;

    // Check whether the deadline has passed. We compare the deadline against
    // the current time since a new asynchronous operation may have moved the
    // deadline before this actor had a chance to run.
    if (deadline_.expires_at() <= boost::asio::deadline_timer::traits_type::now())
    {
      // The deadline has passed. The socket is closed so that any outstanding
      // asynchronous operations are cancelled.
      socket_.close();

      // There is no longer an active deadline. The expiry is set to positive
      // infinity so that the actor takes no action until a new deadline is set.
      deadline_.expires_at(boost::posix_time::pos_infin);
    }

    // Put the actor back to sleep.
    deadline_.async_wait(std::bind(&netClient::check_deadline, this));
  }


  // This function terminates all the actors to shut down the connection. It
  // may be called by the user of the client class, or by the class itself in
  // response to graceful termination or an unrecoverable error.
  void stop() {
    stopped_ = true;
    boost::system::error_code ignored_ec;
    socket_.close(ignored_ec);
    deadline_.cancel();
    is_connected = false;
  }

void disconnect() {
    io_context_.stop();
    // ...and its thread
    if (thrContext.joinable())
        thrContext.join();        

}
    virtual void handle_connect(const boost::system::error_code&,
        tcp::resolver::iterator) {};

  void start_connect(tcp::resolver::iterator endpoint_iter) {
    if (endpoint_iter != tcp::resolver::iterator())
    {
      std::cout << "Trying {}" << endpoint_iter->endpoint() << "\n";
 
      // Set a deadline for the connect operation.
      deadline_.expires_from_now(boost::posix_time::seconds(60));

      // Start the asynchronous connect operation.
      socket_.async_connect(endpoint_iter->endpoint(),
          std::bind(&netClient::handle_connect,
            this, std::placeholders::_1, endpoint_iter));
            //io_context_.run();
            thrContext = std::thread([this]() { io_context_.run(); });
    }
    else
    {
      // There are no more endpoints to try. Shut down the client.
      stop();
    }
  }

    void set_host(const std::string host_) {
        host = host_;
    }

    void set_service(std::string service_port) {
        service = service_port;
    }

    std::string get_host() {
        return host;
    }

    std::string get_service() {
        return service;
    }

    bool isConnected() {
        return is_connected;
    }

  public:
    bool is_connected = false;
    bool stopped_;
    boost::asio::io_context io_context_;
    tcp::socket socket_{io_context_};
    boost::asio::streambuf input_buffer_;
    boost::asio::deadline_timer deadline_;    

    boost::posix_time::time_duration timeout = boost::posix_time::seconds(1);
    std::string host;
    std::string service;
    std::mutex mtx;

    std::thread thrContext;

};

#endif // CIAA_HPP
