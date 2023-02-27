#ifndef CIAA_HPP
#define CIAA_HPP


#include <string>
#include <restbed>
#include <thread>
#include <chrono>
#include <boost/asio.hpp>
#include <boost/asio/high_resolution_timer.hpp>


using boost::asio::ip::tcp;

struct IO_Service {
    using error_code = boost::system::error_code;

    template<typename AllowTime, typename Cancel> void await_operation_ex(AllowTime const& deadline_or_duration, Cancel&& cancel) {
        using namespace boost::asio;

        ioservice.reset();
        {
            high_resolution_timer tm(ioservice, deadline_or_duration);
            tm.async_wait([&cancel](error_code ec) { if (ec != error::operation_aborted) std::forward<Cancel>(cancel)(); });
            ioservice.run_one();
        }
        ioservice.run();
    }

    template<typename AllowTime, typename ServiceObject> void await_operation(AllowTime const& deadline_or_duration, ServiceObject& so) {
        return await_operation_ex(deadline_or_duration, [&so]{ so.cancel(); });
    }

    boost::asio::io_service ioservice;
};


class ciaa
{
    public:
        static ciaa& get_instance()
        {
            static ciaa    instance; // Guaranteed to be destroyed.
                                  // Instantiated on first use.
            return instance;
        }

    	void tx_rx(const restbed::Bytes &tx_buffer, boost::asio::streambuf& rx_buffer);

    	void set_ip(const std::string ip) {
    		 this->ip = ip;
    	}

    	void set_port(std::string port) {
    		 this->port = port;
    	}

    	void set_port(int port) {
    		 this->port = std::to_string(port);
    	}

    	std::string get_ip() {
    		return ip;
    	}

    	std::string get_port() {
    		 return port;
    	}

    	void connect();
    	size_t receive(boost::asio::streambuf &rx_buffer);
    	void send(const restbed::Bytes &tx_buffer);

private:
    	std::string ip;
    	std::string port;
    	struct IO_Service serv;
    	std::unique_ptr<tcp::socket> socket;


        ciaa() {}                    // Constructor? (the {} brackets) are needed here.

        // C++ 11
        // =======
        // We can use the better technique of deleting the methods
        // we don't want.
    public:
        bool isConnected = false;
        ciaa(ciaa const&)            = delete;
        void operator=(ciaa const&)  = delete;

        // Note: Scott Meyers mentions in his Effective Modern
        //       C++ book, that deleted functions should generally
        //       be public as it results in better error messages
        //       due to the compilers behavior to check accessibility
        //       before deleted status
};

#endif 		// CIAA_HPP
