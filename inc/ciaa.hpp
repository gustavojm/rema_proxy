#ifndef CIAA_HPP
#define CIAA_HPP


#include <string>
#include <restbed>
#include "asio.hpp"

class ciaa
{
    public:
        static ciaa& get_instance()
        {
            static ciaa    instance; // Guaranteed to be destroyed.
                                  // Instantiated on first use.
            return instance;
        }

    	asio::error_code tx_rx(const restbed::Bytes &tx_buffer, asio::streambuf& rx_buffer);

    	void set_ip(const asio::ip::address ip) {
    		 this->ip = ip;
    	}

    	void set_ip(const std::string ip) {
    		 this->ip = asio::ip::address::from_string(ip);
    	}

    	void set_port(int port) {
    		 this->port = port;
    	}

    	 asio::ip::address get_ip() {
    		return ip;
    	}

    	int get_port() {
    		 return port;
    	}


private:
    	asio::ip::address ip;
    	int port;

        ciaa() {}                    // Constructor? (the {} brackets) are needed here.

        // C++ 11
        // =======
        // We can use the better technique of deleting the methods
        // we don't want.
    public:
        ciaa(ciaa const&)            = delete;
        void operator=(ciaa const&)  = delete;

        // Note: Scott Meyers mentions in his Effective Modern
        //       C++ book, that deleted functions should generally
        //       be public as it results in better error messages
        //       due to the compilers behavior to check accessibility
        //       before deleted status
};

#endif 		// CIAA_HPP
