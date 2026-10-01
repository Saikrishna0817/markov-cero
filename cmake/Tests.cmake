find_package(Python3 REQUIRED COMPONENTS Interpreter)
include(cmake/TestTargets.cmake)
add_test(NAME build_info COMMAND build_info_test)
add_test(NAME model_verifier COMMAND model_test)
add_test(NAME solve_context COMMAND solve_context_test)
add_test(NAME model_snapshot COMMAND model_snapshot_test)
add_test(NAME node_view COMMAND node_view_test)
add_test(NAME reference_materialisation COMMAND reference_materialisation_test)
add_test(NAME node_view_reference_identity COMMAND node_view_reference_identity_test)
add_test(NAME worker_context COMMAND worker_context_test)
add_test(NAME resource_failure COMMAND resource_failure_test)
set_tests_properties(resource_failure PROPERTIES TIMEOUT 120 WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
# Contract v1: tolerance boundaries and assurance labels.
add_test(NAME numerical_policy_boundaries COMMAND numerical_policy_boundary_test)
add_test(NAME assurance_labels COMMAND assurance_label_test)
# Resource contract v1: stop-reason attribution and completeness.
add_test(NAME stop_reason_boundary COMMAND stop_reason_test)
# MPS rim contract (docs/contracts/mps-input.md §3): objective-row RHS as the
# objective constant; the test also parses data/netlib/e226.mps and grow7.mps,
# so it needs MARKOV_CERO_SOURCE_DIR (set for the shared group below).
add_test(NAME mps_parser COMMAND mps_parser_test)
add_test(NAME lp_parser COMMAND lp_parser_test)
# W5: Milestone 1 — numerical accuracy
add_test(NAME ipm_large COMMAND ipm_large_test)
add_test(NAME pdlp_crossover COMMAND pdlp_crossover_test)
add_test(NAME qp_adaptive_rho COMMAND qp_adaptive_rho_test)
# QP-01 contract §6: witness attacks, edge statuses, cache reuse, disclosure.
add_test(NAME qp_kkt_attack COMMAND qp_kkt_attack_test)
# QP reporting: the JSON maximum_primal_violation field must be measured on
# QPLIB_0010, the BENCH-02 independent-re-check failure (data/qp fixture, so
# the test needs MARKOV_CERO_SOURCE_DIR).
add_test(NAME qp_primal_report COMMAND qp_primal_report_test)
# MIQP-01 contract §7.1/§7.2: supporting-bound algebra, fail-closed cases.
add_test(NAME miqp_supporting_bound COMMAND miqp_supporting_bound_test)
# MIQP-01 contract §7.3/§7.4: node overlays and blueprint §13 required cases.
add_test(NAME miqp_node_bound COMMAND miqp_node_bound_test)
# MIQP-01 contract §7.5: incumbent tamper rejection (engine gate + replay).
add_test(NAME miqp_incumbent COMMAND miqp_incumbent_test)
# MIQP-01 contract §7.6: quadratic-tree proof attacks.
add_test(NAME miqp_proof_attack COMMAND miqp_proof_attack_test)
# MIQP-01 contract §7.7: randomized enumeration differential.
add_test(NAME miqp_bruteforce COMMAND miqp_bruteforce_test)
add_test(NAME numerical_diagnostic COMMAND numerical_diagnostic_test)
add_test(NAME mps_fuzz_smoke COMMAND mps_fuzz_smoke)
add_test(NAME model_properties COMMAND model_property_test)
add_test(NAME dense_lu COMMAND dense_lu_test)
add_test(NAME primal_simplex COMMAND primal_simplex_test)
add_test(NAME primal_simplex_properties COMMAND primal_simplex_property_test)
add_test(NAME dual_simplex COMMAND dual_simplex_test)
add_test(NAME warm_start_properties COMMAND warm_start_property_test)
add_test(NAME sparse_basis COMMAND sparse_basis_test)
add_test(NAME sparse_fill_limit COMMAND sparse_fill_limit_test)
add_test(NAME sparse_lu_deadline COMMAND sparse_lu_deadline_test)
add_test(NAME sparse_update_properties COMMAND sparse_update_property_test)
add_test(NAME audit_regressions COMMAND regression_test)
add_test(NAME regression_backend_actually_used COMMAND regression_backend_actually_used)
add_test(NAME regression_simplex_scale200_pricing COMMAND regression_simplex_scale200_pricing)
set_tests_properties(regression_simplex_scale200_pricing PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME sparse_canonicalize COMMAND sparse_canonicalize_test)
add_test(NAME presolve COMMAND presolve_test)
add_test(NAME ruiz_scaling COMMAND ruiz_scaling_test)
add_test(NAME milp COMMAND milp_test)
add_test(NAME milp_heuristics COMMAND milp_heuristics_test)
add_test(NAME milp_cuts COMMAND milp_cuts_test)
# MIP-01 contract §5.3/§7.5: cut-row lattice guards and enumerated points.
add_test(NAME milp_cut_validity COMMAND milp_cut_validity_test)
# MIP-01 contract §7.1/§7.4: adversarial proof suite and replay cross-check.
add_test(NAME mip_adversarial COMMAND mip_adversarial_test)
# MIP-01 contract §7.2: brute-force enumeration cross-check.
add_test(NAME milp_bruteforce COMMAND milp_bruteforce_test)
# MIP-01 contract §7.3: blueprint edge cases (stop/gap/PDLP/P3/P10).
add_test(NAME milp_edge_cases COMMAND milp_edge_cases_test)
# MIP-01 contract §4: branch-partition certificate and empty-domain records.
add_test(NAME milp_branch_partition COMMAND milp_branch_partition_test)
add_test(NAME strong_branching COMMAND strong_branching_test)
add_test(NAME pdlp COMMAND pdlp_test)
add_test(NAME ipm COMMAND ipm_test)
# LP-01: sparse-first differential over the frozen netlib set
# (docs/contracts/sparse-lp-path.md §5). Several full solves per frozen
# instance, so it carries its own timeout instead of the shared 60s group.
add_test(NAME lp_sparse_differential COMMAND lp_sparse_differential_test)
set_tests_properties(lp_sparse_differential PROPERTIES
  TIMEOUT 180
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  ENVIRONMENT "MARKOV_CERO_SOURCE_DIR=${CMAKE_SOURCE_DIR}")
add_test(NAME parallel_tree_search COMMAND parallel_tree_search_test)
add_test(NAME gpu_buffer COMMAND gpu_buffer_test)
add_test(NAME equivalence COMMAND equivalence_test)
add_test(NAME gpu_reduction COMMAND gpu_reduction_test)
add_test(NAME gpu_pdhg_step COMMAND gpu_pdhg_step_test)
add_test(NAME gpu_pdhg_restart COMMAND gpu_pdhg_restart_test)
add_test(NAME gpu_pdhg_adaptive COMMAND gpu_pdhg_adaptive_test)
add_test(NAME gpu_pdhg_kkt COMMAND gpu_pdhg_kkt_test)
add_test(NAME gpu_pdhg_timing COMMAND gpu_pdhg_timing_test)
add_test(NAME gpu_admm COMMAND gpu_admm_test)
add_test(NAME gpu_fallback COMMAND gpu_fallback_test)
add_test(NAME gpu_qp COMMAND gpu_qp_test)
add_test(NAME qp COMMAND qp_test)
add_test(NAME json_records COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/scripts/check_json.py)
add_test(NAME sovereignty_guard
  COMMAND ${CMAKE_COMMAND} -E env python3
    ${CMAKE_SOURCE_DIR}/scripts/check-sovereignty.py
    ${CMAKE_SOURCE_DIR}
    --binary $<TARGET_FILE:markov-cero-solve>)
add_test(NAME cli_blend_optimal COMMAND markov-cero-solve ${CMAKE_SOURCE_DIR}/examples/blend.mps)
add_test(NAME cli_qp_portfolio
  COMMAND markov-cero-solve ${CMAKE_SOURCE_DIR}/examples/qp_portfolio.mps)
add_test(NAME cli_refinery_feasible COMMAND markov-cero-solve ${CMAKE_SOURCE_DIR}/examples/refinery/refinery-feasible.mps)
add_test(NAME cli_refinery_infeasible COMMAND markov-cero-solve ${CMAKE_SOURCE_DIR}/examples/refinery/refinery-infeasible.mps)
set_tests_properties(cli_refinery_infeasible PROPERTIES WILL_FAIL TRUE)
add_test(NAME cli_refinery_malformed COMMAND markov-cero-solve ${CMAKE_SOURCE_DIR}/examples/refinery/refinery-malformed.mps)
set_tests_properties(cli_refinery_malformed PROPERTIES WILL_FAIL TRUE)
add_test(NAME cli_refinery_limited COMMAND markov-cero-solve ${CMAKE_SOURCE_DIR}/examples/refinery/refinery-limited.mps --iteration-limit 1)
set_tests_properties(cli_refinery_limited PROPERTIES WILL_FAIL TRUE)
add_test(NAME cli_case_crude_oil COMMAND markov-cero-solve ${CMAKE_SOURCE_DIR}/examples/cases/crude_oil_blending.mps)
add_test(NAME cli_case_multiperiod COMMAND markov-cero-solve ${CMAKE_SOURCE_DIR}/examples/cases/multiperiod_production.mps)
add_test(NAME cli_case_supply_chain COMMAND markov-cero-solve ${CMAKE_SOURCE_DIR}/examples/cases/supply_chain_logistics.mps)
add_test(NAME cli_queued_node_capacity
  COMMAND markov-cero-solve ${CMAKE_SOURCE_DIR}/examples/phase3/tiny_milp.mps
    --engine milp --max-queued-nodes 1 --no-cuts --no-heuristics)
set_tests_properties(cli_queued_node_capacity PROPERTIES WILL_FAIL TRUE)
add_test(NAME cli_input_byte_limit
  COMMAND markov-cero-solve ${CMAKE_SOURCE_DIR}/examples/phase3/tiny_milp.mps
    --max-input-bytes 1)
set_tests_properties(cli_input_byte_limit PROPERTIES WILL_FAIL TRUE)
add_test(NAME domain_refinery_scheduling_large COMMAND markov-cero-solve ${CMAKE_SOURCE_DIR}/data/cases/refinery_scheduling_large.mps)
add_test(NAME domain_crude_blending_large COMMAND markov-cero-solve ${CMAKE_SOURCE_DIR}/data/cases/crude_blending_large.qps)
add_test(NAME domain_process_network_large COMMAND markov-cero-solve ${CMAKE_SOURCE_DIR}/data/cases/process_network_large.mps)
add_test(NAME domain_production_planning_large COMMAND markov-cero-solve ${CMAKE_SOURCE_DIR}/data/cases/production_planning_large.mps --mip-gap 0.05)
add_test(NAME domain_power_dispatch_dc_opf COMMAND markov-cero-solve ${CMAKE_SOURCE_DIR}/data/cases/power_dispatch_dc_opf.qps)
add_test(NAME domain_supply_chain_large COMMAND markov-cero-solve
  ${CMAKE_SOURCE_DIR}/data/cases/supply_chain_large.mps --time-limit 90)
add_test(NAME api_demo COMMAND api_demo)
add_test(NAME api_test COMMAND api_test)
add_test(NAME refinery_domain_and_iis COMMAND refinery_test)
add_test(NAME e2e_tier1_m1_features COMMAND e2e_tier1_m1_features)
add_test(NAME e2e_tier2_m1_boundaries COMMAND e2e_tier2_m1_boundaries)
add_test(NAME e2e_tier3_m1_combinations COMMAND e2e_tier3_m1_combinations)
add_test(NAME e2e_tier4_m1_scenarios COMMAND e2e_tier4_m1_scenarios)
add_test(NAME e2e_runner_harness
  COMMAND ${CMAKE_COMMAND} -E env python3 ${CMAKE_SOURCE_DIR}/scripts/run_e2e_tests.py --build-dir ${CMAKE_BINARY_DIR} --json ${CMAKE_BINARY_DIR}/reports/e2e_report.json)
add_test(NAME hosted_os_limits
  COMMAND ${CMAKE_COMMAND} -E env python3
    ${CMAKE_SOURCE_DIR}/web/backend/os_limits_test.py
    $<TARGET_FILE:markov-cero-solve>)
add_test(NAME hosted_service_limits
  COMMAND ${CMAKE_COMMAND} -E env python3
    ${CMAKE_SOURCE_DIR}/web/backend/service_limits_test.py
    $<TARGET_FILE:markov-cero-solve>)
set_tests_properties(
  build_info model_verifier mps_parser lp_parser mps_fuzz_smoke model_properties
  classifier nlp_sqp nlp_rosenbrock nlp_constrained minlp_basic nlobj_parser
  minlp02_proof minlp02_proof_attack minlp02_enumeration
  dense_lu primal_simplex primal_simplex_properties dual_simplex
  warm_start_properties sparse_basis sparse_fill_limit sparse_update_properties audit_regressions
  sparse_canonicalize presolve ruiz_scaling milp milp_heuristics   milp_cuts milp_cut_validity mip_adversarial milp_bruteforce milp_edge_cases
  milp_branch_partition
  strong_branching pdlp parallel_tree_search gpu_buffer equivalence gpu_reduction
  gpu_pdhg_step gpu_pdhg_restart gpu_pdhg_adaptive gpu_pdhg_kkt gpu_pdhg_timing
  gpu_admm gpu_fallback gpu_qp
  qp json_records sovereignty_guard
  ipm_large pdlp_crossover qp_adaptive_rho qp_kkt_attack qp_primal_report miqp_supporting_bound
  miqp_node_bound miqp_incumbent miqp_proof_attack miqp_bruteforce
  numerical_diagnostic
  e2e_tier1_m1_features e2e_tier2_m1_boundaries e2e_tier3_m1_combinations e2e_tier4_m1_scenarios e2e_runner_harness hosted_os_limits
  cli_blend_optimal cli_refinery_feasible cli_refinery_infeasible
  cli_refinery_malformed cli_refinery_limited
  cli_case_crude_oil cli_case_multiperiod cli_case_supply_chain
  api_demo
  api_test
  numerical_policy_boundaries assurance_labels stop_reason_boundary
  PROPERTIES
    TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    ENVIRONMENT "MARKOV_CERO_SOURCE_DIR=${CMAKE_SOURCE_DIR}")
set_tests_properties(
  domain_refinery_scheduling_large domain_crude_blending_large domain_process_network_large
  domain_production_planning_large domain_power_dispatch_dc_opf domain_supply_chain_large
  PROPERTIES
    # The solver's own default search limit is 60s; leave room for model load,
    # final verification, and result emission before CTest's outer watchdog.
    TIMEOUT 90
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    ENVIRONMENT "MARKOV_CERO_SOURCE_DIR=${CMAKE_SOURCE_DIR}")
set_tests_properties(domain_supply_chain_large PROPERTIES TIMEOUT 120)
# Device-only kernel tests: on a host that compiled CUDA but has no device
# (the hosted runners) they exit 77 from gpu/tests/device_skip.hpp instead of
# dying in cudaMalloc. CPU-only builds never take that path.
set_tests_properties(
  gpu_buffer equivalence gpu_reduction
  gpu_pdhg_step gpu_pdhg_restart gpu_pdhg_adaptive gpu_pdhg_kkt gpu_pdhg_timing
  PROPERTIES
    SKIP_RETURN_CODE 77)
if(MARKOV_CERO_BUILD_FUZZER)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "libFuzzer target requires Clang")
  endif()
  add_executable(mps_fuzz tests/fuzz/mps_fuzz.cpp)
  target_link_libraries(mps_fuzz PRIVATE markov_cero_core)
  target_compile_options(mps_fuzz PRIVATE -UNDEBUG -fsanitize=fuzzer,address,undefined)
  target_link_options(mps_fuzz PRIVATE -fsanitize=fuzzer,address,undefined)
  add_executable(sparse_basis_fuzz tests/fuzz/sparse_basis_fuzz.cpp)
  target_link_libraries(sparse_basis_fuzz PRIVATE markov_cero_core)
  target_compile_options(sparse_basis_fuzz PRIVATE -UNDEBUG -fsanitize=fuzzer,address,undefined)
  target_link_options(sparse_basis_fuzz PRIVATE -fsanitize=fuzzer,address,undefined)
endif()

add_executable(readiness_correctness_test tests/readiness_correctness_test.cpp)
target_link_libraries(readiness_correctness_test PRIVATE markov_cero_core)
add_test(NAME readiness_correctness COMMAND readiness_correctness_test)

if(MARKOV_CERO_ENABLE_BENCHMARK_TESTS)
  include(cmake/BenchmarkTests.cmake)
endif()
if(MARKOV_CERO_ENABLE_PYTHON_TESTS)
  execute_process(COMMAND ${Python3_EXECUTABLE} -c "import markov_cero, pytest"
    RESULT_VARIABLE _python_import_status)
  if(NOT _python_import_status EQUAL 0)
    message(FATAL_ERROR "Python tests require the installed extension and pytest in Python3_EXECUTABLE")
  endif()
  add_test(NAME python_bindings COMMAND ${Python3_EXECUTABLE} -m pytest
    ${PROJECT_SOURCE_DIR}/python/tests -q)
endif()

add_test(NAME cli_public_fawley COMMAND markov-cero-solve ${CMAKE_SOURCE_DIR}/examples/refinery/fawley-public.mps)
add_executable(mip_proof_test tests/mip_proof_test.cpp)
target_link_libraries(mip_proof_test PRIVATE markov_cero_core)
add_test(NAME mip_proof COMMAND mip_proof_test)

if(Python3_Interpreter_FOUND)
  add_test(NAME source_limits COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/scripts/check_source_limits.py)
endif()

add_executable(readiness_edge_cases_test tests/readiness_edge_cases_test.cpp)
target_link_libraries(readiness_edge_cases_test PRIVATE markov_cero_core)
add_test(NAME readiness_edge_cases COMMAND readiness_edge_cases_test)
if(Python3_Interpreter_FOUND)
  add_test(NAME repository_tools COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/repository_tools_test.py
    $<TARGET_FILE:markov-cero-solve> $<TARGET_FILE:markov-cero-verify-mip> $<TARGET_FILE:markov-cero-verify-minlp>)
  # BENCH-01 contract section 10: fabricated records only, no solver, no data.
  add_test(NAME bench01_harness_selftest COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/bench01_harness_selftest.py)
endif()

add_test(NAME worker_kill_demo
  COMMAND ${CMAKE_COMMAND} -E env MARKOV_CERO_SOLVE_BIN=$<TARGET_FILE:markov-cero-solve>
          ${CMAKE_SOURCE_DIR}/scripts/worker_kill_demo.sh)
