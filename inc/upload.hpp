#pragma once

#include <chrono>
#include <cstdint>
#include <ctime>
#include <exception>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <restbed>
#include <spdlog/spdlog.h>
#include <vector>

void upload_create_endpoints(restbed::Service &service);
