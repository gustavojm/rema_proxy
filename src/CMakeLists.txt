# Create the executable and compile all .cpp files in the SOURCE directory
file(GLOB SOURCES *.cpp)

file (GLOB HEADERS
    ../inc/*.hpp
)

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

# Find Open3D
find_package(Open3D REQUIRED)
find_package(Eigen3 REQUIRED)
#find_package(Restbed REQUIRED)
find_package(Boost REQUIRED COMPONENTS program_options)
find_package(OpenSSL REQUIRED)


target_include_directories(${PROJECT_NAME} PRIVATE
                              ${Open3D_INCLUDE_DIRS}
#                              ${Restbed_INCLUDE_DIRS}
                              ${Boost_INCLUDE_DIRS}
                              "../inc"
                           )

target_link_libraries(${PROJECT_NAME} PRIVATE 
                          Open3D::Open3D
                          Eigen3::Eigen        
                          Boost::program_options
                          ssl          
                          restbed
#                          ${Restbed_LIBRARIES}
                          ${OPENSSL_CRYPTO_LIBRARY} 
                          ${OPENSSL_SSL_LIBRARY}
                     )

