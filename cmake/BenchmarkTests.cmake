if(NOT EXISTS "${MARKOV_CERO_COMPARE_PYTHON}")
  message(FATAL_ERROR "Benchmark tests require MARKOV_CERO_COMPARE_PYTHON with highspy installed")
endif()
execute_process(COMMAND "${MARKOV_CERO_COMPARE_PYTHON}" -c "import highspy"
  RESULT_VARIABLE _oracle_status)
if(NOT _oracle_status EQUAL 0)
  message(FATAL_ERROR "Comparison Python cannot import highspy")
endif()
add_test(NAME netlib_benchmarks COMMAND ${CMAKE_COMMAND} -E env ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/run_netlib.py --solver $<TARGET_FILE:markov-cero-solve> --instances afiro adlittle sc50a sc50b sc105 share2b recipe)
  add_test(NAME miplib_benchmarks COMMAND ${CMAKE_COMMAND} -E env ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/run_miplib.py --solver $<TARGET_FILE:markov-cero-solve> --instances stein9 stein15 flugpl)
  add_test(NAME cut_effectiveness COMMAND ${CMAKE_COMMAND} -E env ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/measure_cut_effectiveness.py --solver $<TARGET_FILE:markov-cero-solve> --min-passing 1)
add_test(NAME compare_harness
  COMMAND ${CMAKE_COMMAND} -E env
    MARKOV_COMPARE_PYTHON=${MARKOV_CERO_COMPARE_PYTHON}
    ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/run_compare.py
    --solver $<TARGET_FILE:markov-cero-solve>
    --highs-python ${MARKOV_CERO_COMPARE_PYTHON}
    --suites netlib miplib --repeat 1 --timeout 60)
add_test(NAME gpu_benchmarks
  COMMAND ${CMAKE_COMMAND} -E env ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/run_gpu.py
    --solver $<TARGET_FILE:markov-cero-solve>
    --instances afiro blend sc50a sc50b
    --output ${CMAKE_BINARY_DIR}/reports/gpu_benchmark.csv)
add_test(NAME gpu_profiling
  COMMAND ${CMAKE_COMMAND} -E env ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/profile_gpu.py
    --solver $<TARGET_FILE:markov-cero-solve>
    --model ${CMAKE_SOURCE_DIR}/examples/blend.mps
    --output-json ${CMAKE_BINARY_DIR}/reports/profile_summary.json
    --output-md ${CMAKE_BINARY_DIR}/reports/profile_analysis.md)
set_tests_properties(compare_harness cut_effectiveness miplib_benchmarks
  PROPERTIES
    TIMEOUT 600
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    ENVIRONMENT "MARKOV_CERO_SOURCE_DIR=${CMAKE_SOURCE_DIR}")
set_tests_properties(miplib_benchmarks
  PROPERTIES
    TIMEOUT 180
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    ENVIRONMENT "MARKOV_CERO_SOURCE_DIR=${CMAKE_SOURCE_DIR}")
set_tests_properties(netlib_benchmarks gpu_benchmarks gpu_profiling PROPERTIES
  TIMEOUT 180 WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
