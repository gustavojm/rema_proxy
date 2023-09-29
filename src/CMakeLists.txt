# Create the executable and compile all .cpp files in the source directory
file(GLOB SOURCES *.cpp)

add_executable(REMA_proxy ${SOURCES})

#target_link_libraries(
#  intro
#  PRIVATE myproject::myproject_options
#          myproject::myproject_warnings)

# Find Open3D
find_package(Open3D REQUIRED)

# Check if Open3D was found
if(Open3D_FOUND)
    # Open3D was found, and you can now use its targets and include directories
    message(STATUS "Open3D found. Including targets and headers.")
else()
    # Open3D was not found; you may want to handle this case appropriately
    message(FATAL_ERROR "Open3D not found. Please install Open3D.")
endif()

# Find Eigen3
find_package(Eigen3 REQUIRED)

# Check if Eigen3 was found
if(Eigen3_FOUND)
    # Eigen3 was found, and you can now use its targets and include directories
    message(STATUS "Eigen3 found. Including targets and headers.")
else()
    # Eigen3 was not found; you may want to handle this case appropriately
    message(FATAL_ERROR "Eigen3 not found. Please install Eigen3.")
endif()

# Find Restbed
find_package(Restbed REQUIRED)

# Check if Restbed was found
if(Restbed_FOUND)
     # Restbed was found, and you can now use its targets and include directories
    message(STATUS "Restbed found. Including targets and headers.")
else()
    # Restbed was not found; you may want to handle this case appropriately
    message(FATAL_ERROR "Restbed not found. Please install Restbed.")
endif()

find_package(Boost REQUIRED COMPONENTS program_options)

if(Boost_FOUND)
    message(STATUS "Boost found. Including targets and headers.")
else()
    message(FATAL_ERROR "Boost not found. Please install Boost.")
endif()

# Find OpenSSL package
find_package(OpenSSL REQUIRED)


target_include_directories(REMA_proxy PRIVATE
                      ${Open3D_INCLUDE_DIRS}
                      ${Restbed_INCLUDE_DIRS}
                      ${Boost_INCLUDE_DIRS}
                      "../inc"
                   )

target_link_system_libraries(
  REMA_proxy
  PRIVATE
          Open3D::Open3D
          Eigen3::Eigen        
          Boost::program_options
          ssl          
)

target_link_libraries(REMA_proxy PRIVATE 
                        ${Restbed_LIBRARIES}
                        ${OPENSSL_CRYPTO_LIBRARY} 
                        ${OPENSSL_SSL_LIBRARY}
                     )

message(STATUS "Restbed found. -------------------------> " ${Restbed_LIBRARIES})
