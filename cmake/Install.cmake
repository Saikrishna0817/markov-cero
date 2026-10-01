include(CMakePackageConfigHelpers)
install(TARGETS markov_cero_core EXPORT markov_ceroTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS markov-cero-iis markov-cero-verify-mip markov-cero-verify-minlp markov-cero-solve markov-cero-info markov-cero-mps-inspect
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/markov_cero gpu/include/markov_cero
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES LICENSE NOTICE docs/project/PROVENANCE.md DESTINATION ${CMAKE_INSTALL_DATADIR}/markov-cero)
configure_package_config_file(cmake/markov_ceroConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/markov_ceroConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/markov_cero)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/markov_ceroConfigVersion.cmake
  VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(EXPORT markov_ceroTargets NAMESPACE markov_cero::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/markov_cero)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/markov_ceroConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/markov_ceroConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/markov_cero)
