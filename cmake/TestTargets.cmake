add_executable(build_info_test tests/build_info_test.cpp)
target_link_libraries(build_info_test PRIVATE markov_cero_core)
add_executable(model_test tests/model_test.cpp)
target_link_libraries(model_test PRIVATE markov_cero_core)
# W6: problem classification
add_executable(classifier_test tests/classifier_test.cpp)
target_link_libraries(classifier_test PRIVATE markov_cero_core)
add_test(NAME classifier COMMAND classifier_test)
# W2: ML branching (always built as fallback-contract test; the ONNX scorer
# path compiles in only under MARKOV_CERO_ENABLE_ML).
add_executable(ml_branching_test tests/ml_branching_test.cpp)
target_link_libraries(ml_branching_test PRIVATE markov_cero_core)
add_test(NAME ml_branching COMMAND ml_branching_test)
# W1: NLP + MINLP
add_executable(nlp_sqp_test tests/nlp_sqp_test.cpp)
target_link_libraries(nlp_sqp_test PRIVATE markov_cero_core)
add_executable(nlp_rosenbrock_test tests/nlp_rosenbrock_test.cpp)
target_link_libraries(nlp_rosenbrock_test PRIVATE markov_cero_core)
add_executable(nlp_constrained_test tests/nlp_constrained_test.cpp)
target_link_libraries(nlp_constrained_test PRIVATE markov_cero_core)
# NLP-01 contract §6: local semantics, derivative diagnostic, callback guards.
add_executable(nlp_local_semantics_test tests/nlp_local_semantics_test.cpp)
target_link_libraries(nlp_local_semantics_test PRIVATE markov_cero_core)
add_executable(nlp_derivative_check_test tests/nlp_derivative_check_test.cpp)
target_link_libraries(nlp_derivative_check_test PRIVATE markov_cero_core)
add_executable(nlp_callback_guard_test tests/nlp_callback_guard_test.cpp)
target_link_libraries(nlp_callback_guard_test PRIVATE markov_cero_core)
add_executable(nlp_restoration_test tests/nlp_restoration_test.cpp)
target_link_libraries(nlp_restoration_test PRIVATE markov_cero_core)
add_executable(minlp_basic_test tests/minlp_basic_test.cpp)
target_link_libraries(minlp_basic_test PRIVATE markov_cero_core)
# MINLP-01 contract §10: case A/oracles/F/G and cap counters; cut provenance,
# independent replay corruption and random feasible-point validation.
add_executable(minlp01_oa_test tests/minlp01_oa_test.cpp)
target_link_libraries(minlp01_oa_test PRIVATE markov_cero_core)
add_executable(minlp01_cut_replay_test tests/minlp01_cut_replay_test.cpp)
target_link_libraries(minlp01_cut_replay_test PRIVATE markov_cero_core)
# MINLP-02 contract §7: proof fixtures/downgrades/reader rejections, one
# mutation per proof field with seeded fuzz, and seeded enumeration oracles.
add_executable(minlp02_proof_test tests/minlp02_proof_test.cpp)
target_link_libraries(minlp02_proof_test PRIVATE markov_cero_core)
add_executable(minlp02_proof_attack_test tests/minlp02_proof_attack_test.cpp)
target_link_libraries(minlp02_proof_attack_test PRIVATE markov_cero_core)
add_executable(minlp02_enumeration_test tests/minlp02_enumeration_test.cpp)
target_link_libraries(minlp02_enumeration_test PRIVATE markov_cero_core)
add_executable(nlobj_parser_test tests/nlobj_parser_test.cpp)
target_link_libraries(nlobj_parser_test PRIVATE markov_cero_core)
add_test(NAME nlp_sqp COMMAND nlp_sqp_test)
add_test(NAME nlp_rosenbrock COMMAND nlp_rosenbrock_test)
add_test(NAME nlp_constrained COMMAND nlp_constrained_test)
# NLP-01 contract §6: local semantics (§6.2/§6.3/§6.6/§6.7), derivative
# diagnostic (§6.5), callback fail-closed guards and limit incumbents (§6.8/§6.9).
add_test(NAME nlp_local_semantics COMMAND nlp_local_semantics_test)
add_test(NAME nlp_derivative_check COMMAND nlp_derivative_check_test)
add_test(NAME nlp_callback_guard COMMAND nlp_callback_guard_test)
add_test(NAME nlp_restoration COMMAND nlp_restoration_test)
add_test(NAME minlp_basic COMMAND minlp_basic_test)
add_test(NAME minlp01_oa COMMAND minlp01_oa_test)
add_test(NAME minlp01_cut_replay COMMAND minlp01_cut_replay_test)
add_test(NAME minlp02_proof COMMAND minlp02_proof_test)
add_test(NAME minlp02_proof_attack COMMAND minlp02_proof_attack_test)
add_test(NAME minlp02_enumeration COMMAND minlp02_enumeration_test)
add_test(NAME nlobj_parser COMMAND nlobj_parser_test)
add_executable(mps_parser_test tests/mps_parser_test.cpp)
target_link_libraries(mps_parser_test PRIVATE markov_cero_core)
add_executable(mps_fuzz_smoke tests/fuzz/mps_fuzz_smoke.cpp)
target_link_libraries(mps_fuzz_smoke PRIVATE markov_cero_core)
add_executable(model_property_test tests/model_property_test.cpp)
target_link_libraries(model_property_test PRIVATE markov_cero_core)
add_executable(dense_lu_test tests/dense_lu_test.cpp)
target_link_libraries(dense_lu_test PRIVATE markov_cero_core)
add_executable(primal_simplex_test tests/primal_simplex_test.cpp)
target_link_libraries(primal_simplex_test PRIVATE markov_cero_core)
add_executable(primal_simplex_property_test tests/primal_simplex_property_test.cpp)
target_link_libraries(primal_simplex_property_test PRIVATE markov_cero_core)
add_executable(dual_simplex_test tests/dual_simplex_test.cpp)
target_link_libraries(dual_simplex_test PRIVATE markov_cero_core)
add_executable(warm_start_property_test tests/warm_start_property_test.cpp)
target_link_libraries(warm_start_property_test PRIVATE markov_cero_core)
add_executable(sparse_basis_test tests/sparse_basis_test.cpp)
target_link_libraries(sparse_basis_test PRIVATE markov_cero_core)
add_executable(sparse_fill_limit_test tests/sparse_fill_limit_test.cpp)
target_link_libraries(sparse_fill_limit_test PRIVATE markov_cero_core)
add_executable(sparse_lu_deadline_test tests/sparse_lu_deadline_test.cpp)
target_link_libraries(sparse_lu_deadline_test PRIVATE markov_cero_core)
add_executable(sparse_update_property_test tests/sparse_update_property_test.cpp)
target_link_libraries(sparse_update_property_test PRIVATE markov_cero_core)
add_executable(regression_test tests/regression_test.cpp)
target_link_libraries(regression_test PRIVATE markov_cero_core)
add_executable(regression_backend_actually_used tests/regression_backend_actually_used.cpp)
target_link_libraries(regression_backend_actually_used PRIVATE markov_cero_core)
add_executable(regression_simplex_scale200_pricing tests/regression_simplex_scale200_pricing.cpp)
target_link_libraries(regression_simplex_scale200_pricing PRIVATE markov_cero_core)
add_executable(sparse_canonicalize_test tests/sparse_canonicalize_test.cpp)
target_link_libraries(sparse_canonicalize_test PRIVATE markov_cero_core)
add_executable(presolve_test tests/presolve_test.cpp)
target_link_libraries(presolve_test PRIVATE markov_cero_core)
add_executable(ruiz_scaling_test tests/ruiz_scaling_test.cpp)
target_link_libraries(ruiz_scaling_test PRIVATE markov_cero_core)
add_executable(milp_test tests/milp_test.cpp tests/milp_test_test_node_selection_policies.cpp)
target_link_libraries(milp_test PRIVATE markov_cero_core)
add_executable(milp_heuristics_test tests/milp_heuristics_test.cpp)
target_link_libraries(milp_heuristics_test PRIVATE markov_cero_core)
add_executable(milp_cuts_test tests/milp_cuts_test.cpp)
target_link_libraries(milp_cuts_test PRIVATE markov_cero_core)
# MIP-01 contract §5.3/§7.5: cut-row lattice guards and enumerated-point
# validity for GMI/MIR/cover rows.
add_executable(milp_cut_validity_test tests/milp_cut_validity_test.cpp)
target_link_libraries(milp_cut_validity_test PRIVATE markov_cero_core)
# MIP-01 contract §7.1/§7.4: adversarial proof suite and production-vs-replay
# partition cross-check.
add_executable(mip_adversarial_test tests/mip_adversarial_test.cpp)
target_link_libraries(mip_adversarial_test PRIVATE markov_cero_core)
# MIP-01 contract §7.2: production solves matched against brute-force
# enumeration on fixed and seeded small integer programs.
add_executable(milp_bruteforce_test tests/milp_bruteforce_test.cpp)
target_link_libraries(milp_bruteforce_test PRIVATE markov_cero_core)
# MIP-01 contract §7.3: resource stop, nonzero-gap proof, infeasible-vs-
# resource, F1/P3/P10 edge cases.
add_executable(milp_edge_cases_test tests/milp_edge_cases_test.cpp)
target_link_libraries(milp_edge_cases_test PRIVATE markov_cero_core)
# MIP-01 contract §4: exhaustive branch-partition certificate, split statuses
# and empty-domain records for both engines.
add_executable(milp_branch_partition_test tests/milp_branch_partition_test.cpp)
target_link_libraries(milp_branch_partition_test PRIVATE markov_cero_core)
add_executable(strong_branching_test tests/strong_branching_test.cpp tests/strong_branching_test_test_strong_branching_and_domain_reduction.cpp)
target_link_libraries(strong_branching_test PRIVATE markov_cero_core)
add_executable(pdlp_test tests/pdlp_test.cpp)
target_link_libraries(pdlp_test PRIVATE markov_cero_core)
add_executable(ipm_test tests/ipm_test.cpp)
target_link_libraries(ipm_test PRIVATE markov_cero_core)
add_executable(lp_sparse_differential_test tests/lp_sparse_differential_test.cpp)
target_link_libraries(lp_sparse_differential_test PRIVATE markov_cero_core)
add_executable(parallel_tree_search_test tests/parallel_tree_search_test.cpp
  tests/parallel_tree_search_test_test_thread_safety_repeated_runs.cpp
  tests/parallel_tree_search_test_queue_capacity.cpp)
target_link_libraries(parallel_tree_search_test PRIVATE markov_cero_core)
add_executable(parallel_proof_events_test tests/parallel_proof_events_test.cpp)
target_link_libraries(parallel_proof_events_test PRIVATE markov_cero_core)
add_test(NAME parallel_proof_events COMMAND parallel_proof_events_test)
add_executable(gpu_buffer_test gpu/tests/gpu_buffer_test.cpp gpu/tests/gpu_buffer_test_test_model_sparse_matrix_roundtrip.cpp)
target_link_libraries(gpu_buffer_test PRIVATE markov_cero_core)
add_executable(equivalence_test gpu/tests/equivalence_test.cpp gpu/tests/equivalence_test_test_project_bounds_equivalence.cpp)
target_link_libraries(equivalence_test PRIVATE markov_cero_core)
add_executable(gpu_reduction_test gpu/tests/reduction_test.cpp)
target_link_libraries(gpu_reduction_test PRIVATE markov_cero_core)
add_executable(gpu_pdhg_step_test gpu/tests/pdhg_step_test.cpp)
target_link_libraries(gpu_pdhg_step_test PRIVATE markov_cero_core)
add_executable(gpu_pdhg_restart_test gpu/tests/pdhg_restart_test.cpp)
target_link_libraries(gpu_pdhg_restart_test PRIVATE markov_cero_core)
add_executable(gpu_pdhg_adaptive_test gpu/tests/pdhg_adaptive_test.cpp)
target_link_libraries(gpu_pdhg_adaptive_test PRIVATE markov_cero_core)
add_executable(gpu_pdhg_kkt_test gpu/tests/pdhg_kkt_test.cpp)
target_link_libraries(gpu_pdhg_kkt_test PRIVATE markov_cero_core)
add_executable(gpu_pdhg_timing_test gpu/tests/pdhg_timing_test.cpp)
target_link_libraries(gpu_pdhg_timing_test PRIVATE markov_cero_core)
# W3/W4: GPU ADMM step + Tier-1 CPU fallback
add_executable(gpu_admm_test gpu/tests/admm_gpu_test.cpp)
target_link_libraries(gpu_admm_test PRIVATE markov_cero_core)
add_executable(gpu_fallback_test tests/gpu_fallback_test.cpp)
target_link_libraries(gpu_fallback_test PRIVATE markov_cero_core)
add_executable(gpu_qp_test tests/gpu_qp_test.cpp)
target_link_libraries(gpu_qp_test PRIVATE markov_cero_core)
add_executable(qp_test tests/qp_test.cpp tests/qp_scenarios_0.cpp tests/qp_scenarios_1.cpp)
target_link_libraries(qp_test PRIVATE markov_cero_core)
add_executable(api_demo examples/api_demo.cpp)
target_link_libraries(api_demo PRIVATE markov_cero_core)
add_executable(api_test tests/api_test.cpp)
target_link_libraries(api_test PRIVATE markov_cero_core)
add_executable(refinery_test tests/refinery_test.cpp)
target_link_libraries(refinery_test PRIVATE markov_cero_core)
target_compile_definitions(refinery_test PRIVATE MARKOV_CERO_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_executable(lp_parser_test tests/lp_parser_test.cpp)
target_link_libraries(lp_parser_test PRIVATE markov_cero_core)
# W5: Milestone 1 numerical accuracy tests
add_executable(ipm_large_test tests/ipm_large_test.cpp)
target_link_libraries(ipm_large_test PRIVATE markov_cero_core)
add_executable(pdlp_crossover_test tests/pdlp_crossover_test.cpp)
target_link_libraries(pdlp_crossover_test PRIVATE markov_cero_core)
add_executable(qp_adaptive_rho_test tests/qp_adaptive_rho_test.cpp)
target_link_libraries(qp_adaptive_rho_test PRIVATE markov_cero_core)
# QP-01 contract §6: attack suite, edge statuses, symbolic-cache reuse and
# CPU/GPU disclosure (docs/contracts/convex-qp.md).
add_executable(qp_kkt_attack_test tests/qp_kkt_attack_test.cpp)
target_link_libraries(qp_kkt_attack_test PRIVATE markov_cero_core)
# QP reporting: primal_report must carry the measured row violation the JSON
# "maximum_primal_violation" field publishes (BENCH-02 QPLIB_0010 shape).
add_executable(qp_primal_report_test tests/qp_primal_report_test.cpp)
target_link_libraries(qp_primal_report_test PRIVATE markov_cero_core)
# MIQP-01 contract §7.1/§7.2: supporting-bound algebra against an independent
# rational restatement of docs/contracts/miqp-node-bounds.md §2.2.
add_executable(miqp_supporting_bound_test tests/miqp_supporting_bound_test.cpp)
target_link_libraries(miqp_supporting_bound_test PRIVATE markov_cero_core)
# MIQP-01 contract §7.3/§7.4: node overlays, infinite-endpoint fail-closed
# search and the blueprint §13 required MIQP cases.
add_executable(miqp_node_bound_test tests/miqp_node_bound_test.cpp)
target_link_libraries(miqp_node_bound_test PRIVATE markov_cero_core)
# MIQP-01 contract §7.5: incumbent tamper rejection at the engine gate and
# at proof replay, for minimize and maximize quadratic models.
add_executable(miqp_incumbent_test tests/miqp_incumbent_test.cpp)
target_link_libraries(miqp_incumbent_test PRIVATE markov_cero_core)
# MIQP-01 contract §7.6: proof attacks on quadratic trees — altered primal,
# multiplier, objective, Farkas certificate, status labels, splits and
# incumbents must all be rejected.
add_executable(miqp_proof_attack_test tests/miqp_proof_attack_test.cpp)
target_link_libraries(miqp_proof_attack_test PRIVATE markov_cero_core)
# MIQP-01 contract §7.7: seeded random small integer boxes with quadratic
# objectives differentially checked against exhaustive enumeration.
add_executable(miqp_bruteforce_test tests/miqp_bruteforce_test.cpp)
target_link_libraries(miqp_bruteforce_test PRIVATE markov_cero_core)
add_executable(numerical_diagnostic_test tests/numerical_diagnostic_test.cpp)
target_link_libraries(numerical_diagnostic_test PRIVATE markov_cero_core)
add_executable(e2e_tier1_m1_features tests/e2e/test_tier1_m1_features.cpp tests/e2e/test_tier1_m1_features_t1_f03_01_pdlpprimalfeasibilityverification.cpp tests/e2e/test_tier1_m1_features_t1_f06_03_sparsebasisstatisticstracking.cpp)
target_link_libraries(e2e_tier1_m1_features PRIVATE markov_cero_core)
target_include_directories(e2e_tier1_m1_features PRIVATE tests/e2e)
add_executable(e2e_tier2_m1_boundaries tests/e2e/test_tier2_m1_boundaries.cpp tests/e2e/test_tier2_m1_boundaries_t2_f04_03_admmprimalinfeasibledetection.cpp tests/e2e/test_tier2_m1_boundaries_t2_f07_04_presolvedisableddiagnosticmatch.cpp)
target_link_libraries(e2e_tier2_m1_boundaries PRIVATE markov_cero_core)
target_include_directories(e2e_tier2_m1_boundaries PRIVATE tests/e2e)
add_executable(e2e_tier3_m1_combinations tests/e2e/test_tier3_m1_combinations.cpp)
target_link_libraries(e2e_tier3_m1_combinations PRIVATE markov_cero_core)
target_include_directories(e2e_tier3_m1_combinations PRIVATE tests/e2e)
add_executable(e2e_tier4_m1_scenarios tests/e2e/test_tier4_m1_scenarios.cpp)
target_link_libraries(e2e_tier4_m1_scenarios PRIVATE markov_cero_core)
target_include_directories(e2e_tier4_m1_scenarios PRIVATE tests/e2e)
add_executable(node_frontier_memory_benchmark scripts/bench_node_frontier_memory.cpp)
target_link_libraries(node_frontier_memory_benchmark PRIVATE markov_cero_core)
add_executable(solve_frontier_memory_benchmark scripts/bench_solve_frontier_memory.cpp)
target_link_libraries(solve_frontier_memory_benchmark PRIVATE markov_cero_core)
# Contract v1: verifier overhead against a full production solve (NUM-01).
add_executable(verifier_overhead_benchmark scripts/bench_verifier_overhead.cpp)
target_link_libraries(verifier_overhead_benchmark PRIVATE markov_cero_core)
# LP-01 contract §5: retired dense dispatch shape vs sparse-first peak RSS.
add_executable(lp_rss_benchmark scripts/bench_lp_rss.cpp)
target_link_libraries(lp_rss_benchmark PRIVATE markov_cero_core)
# QP-01 contract §6: convexity classification, KKT fill and ADMM iteration
# behavior on the tracked QPLIB subset.
add_executable(qp_kkt_benchmark scripts/bench_qp_kkt.cpp)
target_link_libraries(qp_kkt_benchmark PRIVATE markov_cero_core)
# MINLP-01 contract §11: restricted convex quadratic MINLP stratum with
# known optima and OA counters.
add_executable(minlp01_oa_benchmark scripts/bench_minlp01_oa.cpp)
target_link_libraries(minlp01_oa_benchmark PRIVATE markov_cero_core)
# MINLP-02 contract §8: independent OA proof on/off stratum record.
add_executable(minlp02_proof_benchmark scripts/bench_minlp02_proof.cpp)
target_link_libraries(minlp02_proof_benchmark PRIVATE markov_cero_core)
# W02/W01 contracts: shared SolveContext, ModelSnapshot and NodeView.
add_executable(solve_context_test tests/solve_context_test.cpp)
target_link_libraries(solve_context_test PRIVATE markov_cero_core)
add_executable(model_snapshot_test tests/model_snapshot_test.cpp)
target_link_libraries(model_snapshot_test PRIVATE markov_cero_core)
add_executable(node_view_test tests/node_view_test.cpp)
target_link_libraries(node_view_test PRIVATE markov_cero_core)
# W01/IR-19: bounded reference-materialisation comparator and identity tests.
add_executable(reference_materialisation_test tests/reference_materialisation_test.cpp)
target_link_libraries(reference_materialisation_test PRIVATE markov_cero_core)
add_executable(node_view_reference_identity_test tests/node_view_reference_identity_test.cpp)
target_link_libraries(node_view_reference_identity_test PRIVATE markov_cero_core)
add_executable(worker_context_test tests/worker_context_test.cpp)
target_link_libraries(worker_context_test PRIVATE markov_cero_core)
add_executable(resource_failure_test tests/resource_failure_test.cpp)
target_link_libraries(resource_failure_test PRIVATE markov_cero_core)
# Contract v1 (docs/contracts/numerical-policy.md): tolerance boundaries and
# the SolveResult::assurance label derivation.
add_executable(numerical_policy_boundary_test tests/numerical_policy_boundary_test.cpp)
target_link_libraries(numerical_policy_boundary_test PRIVATE markov_cero_core)
add_executable(assurance_label_test tests/assurance_label_test.cpp)
target_link_libraries(assurance_label_test PRIVATE markov_cero_core)
# Resource contract (docs/contracts/resource-limits.md): boundary stop-reason
# attribution and the resource_limit completeness invariant.
add_executable(stop_reason_test tests/stop_reason_test.cpp)
target_link_libraries(stop_reason_test PRIVATE markov_cero_core)
# These two tests include tests/support/failing_new.hpp, which REPLACES the
# global operator new/delete. Clang's static TSan runtime force-loads its own
# copies (libclang_rt.tsan_cxx.a via --whole-archive, ahead of the test
# objects), so under clang+TSan the definitions cannot coexist - and the
# runtime's would win anyway, leaving the harness unable to fail an
# allocation. gcc's libtsan defines no replacements; every other build is
# unaffected. Allow the link, compile the skip in, and let main() exit 77.
if(MARKOV_CERO_ENABLE_TSAN AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  foreach(_mc_injection_test IN ITEMS resource_failure_test stop_reason_test)
    target_link_options(${_mc_injection_test} PRIVATE -Wl,--allow-multiple-definition)
    target_compile_definitions(${_mc_injection_test} PRIVATE
      MARKOV_CERO_TEST_CLANG_TSAN_INJECTION_UNAVAILABLE=1)
  endforeach()
endif()
# Test executables must keep assert() checks alive in EVERY build type:
# Release/RelWithDebInfo define NDEBUG, which compiles assert() to a no-op and
# silently disables the assert-based test files (CI's ASan/UBSan and TSan jobs
# build RelWithDebInfo). Production code contains no assert() calls, so only
# non-app targets undefine NDEBUG; the three markov-cero-* apps and the core
# library keep the configured flags.
get_property(_mc_all_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(_tgt IN LISTS _mc_all_targets)
  get_target_property(_mc_type ${_tgt} TYPE)
  if(_mc_type STREQUAL "EXECUTABLE" AND NOT _tgt MATCHES "^markov-cero-")
    target_compile_options(${_tgt} PRIVATE -UNDEBUG)
  endif()
endforeach()
unset(_mc_all_targets)
unset(_tgt)
