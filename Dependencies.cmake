include(cmake/CPM.cmake)

# Done as a function so that updates to variables like
# CMAKE_CXX_FLAGS don't propagate out to other
# targets
function(myproject_setup_dependencies)

  # For each dependency, see if it's
  # already been provided to us by a parent project

  # if(NOT TARGET fmtlib::fmtlib)
  #   cpmaddpackage("gh:fmtlib/fmt#9.1.0")
  # endif()

  # if(NOT TARGET Open3D::Open3D)
  #   CPMAddPackage(
  #     NAME Open3D
  #     GIT_REPOSITORY https://github.com/intel-isl/Open3D.git
  #     GIT_TAG v0.17.0  # Specify the version/tag you want
  #   )
  # endif()

  
  # if(NOT TARGET Eigen::Eigen)
  #   CPMAddPackage(
  #     NAME Eigen
  #     GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
  #     #GIT_TAG 3.4.0  # Specify the version/tag you want
  #   )
  # endif()

  # if(NOT TARGET Restbed::Restbed)
  #   CPMAddPackage(
  #     NAME Restbed
  #     GIT_REPOSITORY https://github.com/corvusoft/restbed.git
  #     #GIT_TAG 4.7.0  # Specify the version/tag you want
  #   )
  # endif()

endfunction()
