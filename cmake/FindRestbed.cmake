find_path( Restbed_INCLUDE_DIRS restbed HINTS "${PROJECT_SOURCE_DIR}/../restbed/distribution/include" "usr/include" "/usr/local/include" "/opt/local/include" )

if ( Restbed_INCLUDE_DIRS )
    set( Restbed_FOUND TRUE )
    #set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DASIO_STANDALONE=YES" )
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" )

    message( STATUS "Found Restbed include at: ${Restbed_INCLUDE_DIRS}" )

    # Define Restbed include directories
    set(Restbed_INCLUDE_DIRS "/home/gustavo/CIAA/restbed/distribution/include")
    
    # Define Restbed library directory
    set(Restbed_LIBRARY_DIRS "/home/gustavo/CIAA/restbed/distribution/library")
    
    # Define the Restbed library to link against
    set(Restbed_LIBRARIES "/home/gustavo/CIAA/restbed/distribution/library/librestbed.a")
    
    # Provide the include directories to dependent targets
    set(Restbed_INCLUDE_DIRS ${Restbed_INCLUDE_DIRS} PARENT_SCOPE)
    set(Restbed_LIBRARY_DIRS ${Restbed_LIBRARY_DIRS} PARENT_SCOPE)
    set(Restbed_LIBRARIES ${Restbed_LIBRARIES} PARENT_SCOPE)
    
else ( )
    message( FATAL_ERROR "Failed to locate Restbed dependency." )
endif ( )


