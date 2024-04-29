#ifndef UPLOAD_HPP
#define UPLOAD_HPP

#include <vector>
#include <restbed>
#include <functional>
#include <ctime>
#include <thread>
#include <chrono>
#include <fstream>
#include <iostream>
#include <exception>
#include <vector>
#include <memory>
#include <cstdint>
#include <spdlog/spdlog.h>

void upload_create_endpoints(restbed::Service &service);

#endif // UPLOAD_HPP
