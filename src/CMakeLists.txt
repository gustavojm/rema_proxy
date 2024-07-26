# Create the executable and compile all .cpp files in the SOURCE directory
set(HEADERS_DIR ../inc)

file(GLOB SOURCES *.cpp)

file (GLOB HEADERS
    ${HEADERS_DIR}/*.hpp
)

set(SOURCES ${SOURCES} PARENT_SCOPE)
set(HEADERS ${HEADERS} PARENT_SCOPE)

if(${PROJECT_NAME}_BUILD_EXECUTABLE)
  add_executable(${PROJECT_NAME} ${SOURCES})

  if(${PROJECT_NAME}_VERBOSE_OUTPUT)
    verbose_message("Found the following SOURCES:")
    foreach(SOURCE IN LISTS SOURCES)
      verbose_message("* ${SOURCE}")
    endforeach()
  endif()

  if(${PROJECT_NAME}_ENABLE_UNIT_TESTING)
    add_library(${PROJECT_NAME}_LIB ${HEADERS} ${SOURCES})

    if(${PROJECT_NAME}_VERBOSE_OUTPUT)
      verbose_message("Found the following HEADERS:")
      foreach(HEADER IN LISTS HEADERS)
        verbose_message("* ${HEADER}")
      endforeach()
    endif()
  endif()
elseif(${PROJECT_NAME}_BUILD_HEADERS_ONLY)
  add_library(${PROJECT_NAME} INTERFACE)

  if(${PROJECT_NAME}_VERBOSE_OUTPUT)
    verbose_message("Found the following HEADERS:")
    foreach(HEADER IN LIST HEADERS)
      verbose_message("* ${HEADER}")
    endforeach()
  endif()
else()
  add_library(
    ${PROJECT_NAME}
    ${HEADERS}
    ${SOURCES}
  )

  if(${PROJECT_NAME}_VERBOSE_OUTPUT)
    verbose_message("Found the following SOURCES:")
    foreach(SOURCE IN LISTS SOURCES)
      verbose_message("* ${SOURCE}")
    endforeach()
    verbose_message("Found the following HEADERS:")
    foreach(HEADER IN LISTS HEADERS)
      verbose_message("* ${HEADER}")
    endforeach()
  endif()
endif()


#target_link_libraries(
#  intro
#  PRIVATE myproject::myproject_options
#          myproject::myproject_warnings)

# Dependencies resolved by vcpkg. See vcpkg.json
find_package(Eigen3 REQUIRED)
find_package(unofficial-restbed CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS program_options)
find_package(OpenSSL REQUIRED)
find_package(spdlog REQUIRED)
find_package(protobuf CONFIG REQUIRED)

# Find Open3D (not found in vcpkg repo)
find_package(Open3D REQUIRED)

# Include restbed headers
include_directories(${RESTBED_DIR}/include)

target_include_directories(${PROJECT_NAME} PRIVATE
                              ${Open3D_INCLUDE_DIRS}
                              ${Boost_INCLUDE_DIRS}
                              ${HEADERS_DIR}
                           )

target_link_libraries(${PROJECT_NAME} PRIVATE 
                          Open3D::Open3D
                          Eigen3::Eigen
                          Boost::program_options
                          spdlog::spdlog
                          unofficial::restbed::restbed
                          OpenSSL::SSL
                          OpenSSL::Crypto
                          protobuf::libprotoc
                          protobuf::libprotobuf
                          protobuf::libprotobuf-lite
                     )

