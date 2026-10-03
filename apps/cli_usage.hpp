#pragma once
#include <ostream>
namespace markov_cero::apps {
inline void cli_usage(std::ostream& out) {
        out << "usage: markov-cero-solve MODEL.mps [options]\n"
            << "options:\n"
            << "  --output result.json     Write output JSON to file\n"
            << "  --engine primal|dual|ipm|pdlp|milp|parallel|qp|miqp|sqp|outer_approx|auto "
            << "Select solver engine (default: auto)\n"
            << "  --threads N              Worker threads for parallel tree search (default: 4)\n"
            << "  --branching most_fractional|pseudo_cost|strong_branching|reliability|ml_gnn "
               "Branching variable selection rule (default: pseudo_cost; ml_gnn needs "
               "MARKOV_CERO_ENABLE_ML + model file, else falls back to pseudo_cost)\n"
            << "  --iteration-limit N      Maximum simplex iterations\n"
            << "  --max-nodes N            Maximum branch-and-cut search nodes (default: 50000)\n"
            << "  --max-queued-nodes N     Maximum queued B&B nodes (default: 50000)\n"
            << "  --max-input-bytes N      Override the MPS/LP input byte cap (max: 1 GiB)\n"
            << "  --memory-limit-bytes N   Budget instrumented solver allocations\n"
            << "  --device-memory-limit-bytes N  Budget gpu device-buffer allocations "
               "(backend=gpu)\n"
            << "  --node-selection best-bound|depth-first|dive  Node selection policy "
               "(default: best-bound)\n"
            << "  --time-limit SEC         Maximum solve wall-clock time in seconds (default: 60.0)\n"
            << "  --mip-gap TOL            Relative MIP gap tolerance (default: 1e-4)\n"
            << "  --proof-time-limit SEC   Independent MIP proof generation and replay budget in seconds (default: 5)\n"
            << "  --proof-max-nodes N      Maximum nodes in generated/replayed proof (default: 10000)\n"
            << "  --proof-max-values N     Maximum witness values in proof (default: 4000000)\n"
            << "  --cuts, --no-cuts        Enable or disable Gomory & MIR mixed-integer cuts "
               "(default: enabled)\n"
            << "  --heuristics, --no-heuristics Enable or disable primal heuristics (default: "
               "enabled)\n"
            << "  --warm-start FILE        Load warm-start basis from file (dual engine)\n"
            << "  --save-basis FILE        Save optimal basis to file\n"
            << "  --presolve, --no-presolve Enable or disable presolve reductions (default: "
               "enabled)\n"
            << "  --scale, --no-scale       Enable or disable Ruiz matrix scaling (default: "
               "enabled)\n"
            << "  --max-presolve-passes N   Maximum presolve passes (default: 5)\n"
            << "  --ruiz-iterations N       Maximum Ruiz equilibration iterations (default: 10)\n"
            << "  --tolerance TOL          Relative KKT tolerance for PDLP (default: 1e-4)\n"
            << "  --backend cpu|gpu        PDLP execution backend (default: cpu)\n"
            << "  --help, -h               Show this help\n";
}

inline void mip_verify_usage(std::ostream& out) {
    out << "usage: markov-cero-verify-mip MODEL.mps PROOF.txt [options]\n"
        << "  --time-limit SEC  Maximum proof parsing/replay time (default: 5)\n"
        << "  --max-nodes N     Maximum proof tree nodes (default: 10000)\n"
        << "  --max-values N    Maximum witness values (default: 4000000)\n"
        << "  --relative-gap T  Accepted relative gap (default: 0)\n";
}

// MINLP-02 (minlp-proof-replay.md §6.3): same flag surface as the MIP
// verifier — the certified artifact is an embedded MILP tree.
inline void minlp_verify_usage(std::ostream& out) {
    out << "usage: markov-cero-verify-minlp MODEL.mps PROOF.txt [options]\n"
        << "  --time-limit SEC  Maximum proof parsing/replay time (default: 5)\n"
        << "  --max-nodes N     Maximum proof tree nodes (default: 10000)\n"
        << "  --max-values N    Maximum witness values (default: 4000000)\n"
        << "  --relative-gap T  Accepted relative gap (default: 0)\n";
}
}
