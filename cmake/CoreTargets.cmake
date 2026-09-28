find_package(Threads REQUIRED)
# W7/D-11: the static core is linked into the Python extension module, so every
# object must be position-independent (C5: single static library preserved).
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_library(markov_cero_core STATIC
  src/api/api.cpp
  src/api/mip_certificate.cpp
  src/verify/mip_proof.cpp src/verify/mip_proof_relaxation.cpp
  src/verify/mip_proof_io.cpp
  src/verify/mip_proof_builder.cpp
  src/api/dispatch.cpp
  src/api/engine_nonlinear.cpp
  src/api/engine_parallel.cpp
  src/api/engine_pdlp.cpp
  src/api/engine_qp.cpp
  src/api/engine_milp.cpp
  src/api/engine_lp.cpp
  src/foundation/build_info.cpp
  src/model/model.cpp
  src/model/classifier.cpp
  src/io/mps.cpp
  src/io/mps_records.cpp
  src/io/mps_extensions.cpp
  src/io/mps_model.cpp
  src/io/lp_lexer.cpp
  src/io/lp_objective.cpp
  src/io/lp_constraints.cpp
  src/io/lp_bounds.cpp
  src/io/lp_model.cpp
  src/io/lp_parser.cpp
  src/io/nlobj_parser.cpp
  src/io/nlp_callbacks.cpp
  src/nlp/lbfgs.cpp
  src/nlp/nlp_model.cpp
  src/nlp/nlp_verifier.cpp
  src/nlp/sqp_solver_subproblem.cpp
  src/nlp/sqp_solver.cpp
  src/minlp/minlp_solver.cpp src/minlp/minlp_solver_iterate_outer_approximation.cpp src/minlp/minlp_solver_solve_minlp.cpp
  src/verify/primal_verifier.cpp
  src/verify/linear_certificate.cpp
  src/linalg/dense_lu.cpp
  src/linalg/sparse_basis_ordering.cpp
  src/linalg/sparse_basis_matrix.cpp
  src/linalg/sparse_basis_factor.cpp
  src/linalg/sparse_basis_solve.cpp
  src/linalg/sparse_basis.cpp
  src/transform/canonicalize.cpp
  src/transform/sparse_canonicalize_model.cpp
  src/transform/sparse_canonicalize.cpp
  src/presolve/presolve.cpp src/presolve/eliminate_singletons.cpp src/presolve/eliminate_duplicates.cpp src/presolve/compact.cpp src/presolve/postsolve.cpp
  src/scale/ruiz_scaling.cpp
  src/lp/reference/revised_simplex_pricing.cpp
  src/lp/reference/revised_simplex_iteration.cpp
  src/lp/reference/revised_simplex_workspace.cpp
  src/lp/reference/revised_simplex.cpp
  src/verify/reference_lp_verifier.cpp
  src/verify/sparse_lp_verifier.cpp
  src/lp/dual/dual_simplex.cpp src/lp/dual/dual_simplex_select_entering_column.cpp src/lp/dual/dual_simplex_solve.cpp
  src/lp/interior/ipm.cpp src/lp/interior/ipm_crossover_basis_sparse.cpp src/lp/interior/ipm_solve.cpp src/lp/interior/ipm_solve_overload.cpp
  src/lp/first_order/pdlp.cpp src/lp/first_order/pdlp_degenerate.cpp src/lp/first_order/pdlp_try_dual_simplex_crossover.cpp src/lp/first_order/pdlp_iterate_pdlp.cpp src/lp/first_order/pdlp_solve_pdlp.cpp
  src/milp/branch_selector.cpp
  src/milp/branch_selector_features.cpp
  src/milp/cut_pool.cpp
  src/milp/gomory.cpp
  src/milp/heuristics.cpp
  src/milp/heuristics_pump.cpp
  src/milp/heuristics_repair.cpp
  src/milp/milp_solver.cpp
  src/milp/search_initialize.cpp
  src/milp/search_root_relaxation.cpp
  src/milp/search_root_branching.cpp
  src/milp/search_node_relaxation.cpp
  src/milp/search_separate_cuts.cpp
  src/milp/search_branch.cpp
  src/milp/search_finish.cpp
  src/milp/node_lp.cpp
  src/milp/node_qp.cpp
  src/milp/mir.cpp
  src/milp/cover.cpp
  src/milp/parallel_tree_search.cpp src/milp/parallel_tree_search_solve_integer_parallel.cpp src/milp/parallel_tree_search_solve_parallel.cpp
  src/milp/shared_incumbent.cpp
  src/milp/strong_branching.cpp
  src/milp/work_queue.cpp
  src/qp/model_matrix.cpp
  src/qp/model_convexity.cpp
  src/qp/model.cpp
  src/qp/kkt_factor.cpp
  src/qp/kkt_assembly.cpp
  src/qp/kkt.cpp
  src/qp/admm_solver.cpp src/qp/admm_solver_solve.cpp
  src/qp/equilibrate.cpp src/qp/supporting_bound.cpp
  src/qp/verifier.cpp
  src/qp/certificates.cpp
  src/refinery/synthetic_config.cpp
  src/refinery/refinery_model.cpp
  src/refinery/pooling_slp.cpp
  src/analysis/iis_analyzer.cpp
  gpu/src/buffer.cpp
  gpu/src/csr.cpp
  gpu/src/device.cpp
  gpu/src/admm_matvec.cpp
  gpu/src/pdhg_step.cpp gpu/src/pdhg_iteration.cpp gpu/src/pdhg_residuals.cpp gpu/src/pdhg_solver.cpp
  gpu/src/reduce.cpp
  gpu/src/spmv.cpp
  gpu/src/vector_ops.cpp
)
add_library(markov_cero::core ALIAS markov_cero_core)
set_target_properties(markov_cero_core PROPERTIES EXPORT_NAME core)
target_include_directories(markov_cero_core PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/gpu/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(markov_cero_core PUBLIC cxx_std_20)
target_link_libraries(markov_cero_core PUBLIC Threads::Threads)
if(MARKOV_CERO_ENABLE_CUDA)
  # CUDA runtime headers are also included by the ordinary C++ GPU wrappers,
  # not only by .cu translation units. The imported target supplies both the
  # headers and the selected static runtime to downstream links.
  target_link_libraries(markov_cero_core PUBLIC CUDA::cudart_static)
  target_compile_definitions(markov_cero_core PUBLIC MARKOV_CERO_HAS_CUDA=1)
  target_sources(markov_cero_core PRIVATE
    gpu/kernels/pdhg_step.cu
    gpu/kernels/reduce.cu
    gpu/kernels/spmv.cu
    gpu/kernels/vector_ops.cu
    gpu/kernels/admm_step.cu
  )
endif()
if(MARKOV_CERO_ENABLE_ML)
  target_compile_definitions(markov_cero_core PUBLIC MARKOV_CERO_ENABLE_ML=1)
  target_sources(markov_cero_core PRIVATE
    src/milp/ml_branching/onnx_scorer.cpp src/milp/ml_branching/onnx_scorer_score_graph.cpp
  )
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(markov_cero_core PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra;-Wpedantic>)
  if(MARKOV_CERO_WARNINGS_AS_ERRORS)
    target_compile_options(markov_cero_core PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Werror>)
  endif()
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(markov_cero_core PRIVATE
      $<$<COMPILE_LANGUAGE:CXX>:-Wno-maybe-uninitialized>)
  endif()
  if(MARKOV_CERO_ENABLE_ASAN_UBSAN)
    target_compile_options(markov_cero_core PRIVATE
      $<$<COMPILE_LANGUAGE:CXX>:-fsanitize=address,undefined;-fno-omit-frame-pointer>)
    target_link_options(markov_cero_core PUBLIC -fsanitize=address,undefined)
  elseif(MARKOV_CERO_ENABLE_TSAN)
    target_compile_options(markov_cero_core PRIVATE
      $<$<COMPILE_LANGUAGE:CXX>:-fsanitize=thread;-fno-omit-frame-pointer>)
    target_link_options(markov_cero_core PUBLIC -fsanitize=thread)
  endif()
endif()
add_executable(markov-cero-info apps/markov_cero_info.cpp)
target_link_libraries(markov-cero-info PRIVATE markov_cero_core)
add_executable(markov-cero-mps-inspect apps/markov_cero_mps_inspect.cpp)
target_link_libraries(markov-cero-mps-inspect PRIVATE markov_cero_core)
add_executable(markov-cero-solve apps/markov_cero_solve.cpp)
target_link_libraries(markov-cero-solve PRIVATE markov_cero_core)


add_executable(markov-cero-verify-mip apps/markov_cero_verify_mip.cpp)
target_link_libraries(markov-cero-verify-mip PRIVATE markov_cero_core)

add_executable(markov-cero-iis apps/markov_cero_iis.cpp)
target_link_libraries(markov-cero-iis PRIVATE markov_cero_core)
