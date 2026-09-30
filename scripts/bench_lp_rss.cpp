// LP-01 benchmark (docs/contracts/sparse-lp-path.md §5): peak-RSS and wall
// delta between the retired dense dispatch shape (sparse canonical and dense
// conversion alive together — exactly what the pre-LP-01 engine_lp held while
// solving) and the sparse-first dispatch (no dense matrix anywhere). Each
// (instance, path) runs in a forked child so VmHWM starts from a clean
// process; the parent aggregates into one evidence JSON.
//
//   lp_rss_benchmark <repo-root> <output.json>
//
// Instances come from the frozen data/compare/netlib.txt set plus one
// synthetic sparse case whose dense form is hundreds of megabytes, so the
// retired conversion leaves a signal beyond noise. Statuses are reported as
// measured; the benchmark makes no speed, optimality, or memory-cap claim.
#include "markov_cero/io/mps.hpp"
#include "markov_cero/lp/reference/revised_simplex.hpp"
#include "markov_cero/transform/canonicalize.hpp"
#include "markov_cero/transform/sparse_canonical_model.hpp"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
using markov_cero::lp::reference::SolveStatus;

struct Metrics {
    std::string status = "not_run";
    double wall_ms = 0.0;
    std::size_t iterations = 0;
    long vmhwm_kb = -1;
    std::size_t rows = 0;
    std::size_t columns = 0;
    std::size_t nonzeros = 0;
    std::string error;
};

long proc_kb(const std::string& key) {
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.rfind(key, 0) == 0) {
            std::istringstream fields(line.substr(key.size()));
            long value = -1;
            fields >> value;
            return value;
        }
    }
    return -1;
}

// 4096x8000 structural with unbounded-above variables (a finite variable
// upper bound would become a canonical row): every row holds three unit
// entries, minimize sum(x) — feasible at the origin, bounded, canonical
// 4096x12096 → ~396 MB dense (the retired conversion's footprint, inside
// the legacy 4096x16384 cap).
markov_cero::model::Model make_synthetic() {
    markov_cero::model::Model m;
    m.name = "LP_RSS_SYNTHETIC_4096x8000";
    constexpr std::size_t rows = 4096;
    constexpr std::size_t columns = 8000;
    m.objective.assign(columns, 1.0);
    m.variable_type.assign(columns, markov_cero::model::VariableType::continuous);
    m.variable_lower.assign(columns, markov_cero::model::Bound::finite(0.0));
    m.variable_upper.assign(columns, markov_cero::model::Bound::positive_infinity());
    m.row_lower.assign(rows, markov_cero::model::Bound::negative_infinity());
    m.row_upper.assign(rows, markov_cero::model::Bound::finite(1.0));
    m.row_name.reserve(rows);
    m.variable_name.reserve(columns);
    for (std::size_t i = 0; i < rows; ++i) m.row_name.push_back("R" + std::to_string(i));
    for (std::size_t j = 0; j < columns; ++j) m.variable_name.push_back("X" + std::to_string(j));
    markov_cero::model::SparseMatrixBuilder builder(rows, columns);
    for (std::size_t i = 0; i < rows; ++i) {
        builder.add(i, (i * 7) % columns, 1.0);
        builder.add(i, (i * 7 + 1) % columns, 1.0);
        builder.add(i, (i * 7 + 31) % columns, 1.0);
    }
    m.matrix = builder.build();
    return m;
}

Metrics measure(const std::string& path, bool dense_shape) {
    Metrics out;
    const auto model = path.empty() ? make_synthetic()
                                    : [&path] {
                                          std::ifstream input(path);
                                          if (!input) throw std::runtime_error("cannot open " + path);
                                          return markov_cero::io::parse_mps(input);
                                      }();
    auto sparse = markov_cero::transform::sparse_canonicalize(model);
    out.rows = sparse.matrix.rows;
    out.columns = sparse.matrix.columns;
    out.nonzeros = sparse.matrix.values.size();

    markov_cero::lp::reference::Options options;
    options.iteration_limit = path.empty() ? 200 : 500000;
    options.time_limit_seconds = path.empty() ? 60.0 : 600.0;

    const auto start = Clock::now();
    SolveStatus status = SolveStatus::numerical_failure;
    if (dense_shape) {
        // Pre-LP-01 dispatch shape: both representations alive during solve.
        const auto canonical = sparse.to_dense();
        const auto result = markov_cero::lp::reference::solve(canonical, options);
        status = result.status;
        out.iterations = result.phase_one_iterations + result.phase_two_iterations;
    } else {
        const auto result = markov_cero::lp::reference::solve(sparse, options);
        status = result.status;
        out.iterations = result.phase_one_iterations + result.phase_two_iterations;
    }
    out.wall_ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    out.status = markov_cero::lp::reference::to_string(status);
    out.vmhwm_kb = proc_kb("VmHWM:");
    return out;
}

std::string encode(const Metrics& m) {
    std::ostringstream line;
    if (!m.error.empty()) {
        line << "ERROR\t" << m.error;
        return line.str();
    }
    line << m.status << '\t' << std::fixed << std::setprecision(3) << m.wall_ms << '\t'
         << m.iterations << '\t' << m.vmhwm_kb << '\t' << m.rows << '\t' << m.columns << '\t'
         << m.nonzeros;
    return line.str();
}

Metrics decode(const std::string& text) {
    std::istringstream line(text);
    std::string status;
    line >> status;
    Metrics m;
    if (status == "ERROR") {
        std::getline(line, m.error);
        return m;
    }
    std::string rest;
    std::getline(line, rest);
    std::istringstream fields(rest);
    m.status = status;
    fields >> m.wall_ms >> m.iterations >> m.vmhwm_kb >> m.rows >> m.columns >> m.nonzeros;
    if (fields.fail()) throw std::runtime_error("malformed child metrics: " + text);
    return m;
}

Metrics run_forked(const std::string& path, bool dense_shape) {
    int fds[2];
    if (pipe(fds) != 0) throw std::runtime_error("pipe failed");
    const pid_t pid = fork();
    if (pid < 0) throw std::runtime_error("fork failed");
    if (pid == 0) {
        close(fds[0]);
        std::string line;
        try {
            line = encode(measure(path, dense_shape));
        } catch (const std::exception& e) {
            line = std::string("ERROR\t") + e.what();
        }
        std::size_t written = 0;
        while (written < line.size()) {
            const auto n = ::write(fds[1], line.data() + written, line.size() - written);
            if (n <= 0) _exit(2);
            written += static_cast<std::size_t>(n);
        }
        close(fds[1]);
        _exit(0);
    }
    close(fds[1]);
    std::string text;
    char buffer[4096];
    ssize_t got = 0;
    while ((got = read(fds[0], buffer, sizeof(buffer))) > 0) text.append(buffer, static_cast<std::size_t>(got));
    close(fds[0]);
    int child_status = 0;
    waitpid(pid, &child_status, 0);
    if (!WIFEXITED(child_status) || WEXITSTATUS(child_status) != 0 || text.empty()) {
        throw std::runtime_error("child failed for " + path);
    }
    Metrics m = decode(text);
    if (!m.error.empty()) throw std::runtime_error(m.error);
    return m;
}

struct Instance {
    std::string name;
    std::string path;
};

std::vector<Instance> frozen_set(const std::string& root) {
    std::ifstream list(root + "/data/compare/netlib.txt");
    if (!list) throw std::runtime_error("cannot open " + root + "/data/compare/netlib.txt");
    std::vector<Instance> out;
    std::string line;
    while (std::getline(list, line)) {
        if (line.empty() || line[0] == '#') continue;
        const auto tab = line.find('\t');
        if (tab == std::string::npos) continue;
        out.push_back({line.substr(0, tab), root + "/" + line.substr(tab + 1)});
    }
    return out;
}

std::string json_escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"' || c == '\\') out.push_back('\\');
        if (c == '\n') { out += "\\n"; continue; }
        out.push_back(c);
    }
    return out;
}

void record(std::ostream& out, const std::string& name, const Instance* inst,
            const Metrics& dense, const Metrics& sparse, bool comma) {
    const std::size_t rows = sparse.rows ? sparse.rows : dense.rows;
    const std::size_t columns = sparse.columns ? sparse.columns : dense.columns;
    out << (comma ? "," : "") << "\n  {\"name\":\"" << json_escape(name) << "\""
        << ",\"source\":\"" << (inst ? json_escape(inst->path) : "synthetic") << "\""
        << ",\"rows\":" << rows << ",\"columns\":" << columns
        << ",\"nonzeros\":" << (sparse.nonzeros ? sparse.nonzeros : dense.nonzeros)
        << ",\"dense_bytes\":" << static_cast<std::uint64_t>(rows) * columns * 8
        << ",\"retired_dense_path\":{\"status\":\"" << dense.status
        << "\",\"wall_ms\":" << dense.wall_ms << ",\"iterations\":" << dense.iterations
        << ",\"vmhwm_kb\":" << dense.vmhwm_kb << "}"
        << ",\"sparse_path\":{\"status\":\"" << sparse.status
        << "\",\"wall_ms\":" << sparse.wall_ms << ",\"iterations\":" << sparse.iterations
        << ",\"vmhwm_kb\":" << sparse.vmhwm_kb << "}"
        << ",\"vmhwm_delta_kb\":" << (dense.vmhwm_kb - sparse.vmhwm_kb) << "}";
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 3) {
            std::cerr << "usage: lp_rss_benchmark <repo-root> <output.json>\n";
            return 2;
        }
        const std::string root = argv[1];
        const auto instances = frozen_set(root);
        std::ostringstream rows_json;
        std::size_t count = 0;
        for (const auto& inst : instances) {
            const Metrics dense = run_forked(inst.path, true);
            const Metrics sparse = run_forked(inst.path, false);
            record(rows_json, inst.name, &inst, dense, sparse, count++ != 0);
            std::cout << inst.name << " dense=" << dense.status
                      << " hwm_kb=" << dense.vmhwm_kb << " sparse=" << sparse.status
                      << " hwm_kb=" << sparse.vmhwm_kb
                      << " delta_kb=" << (dense.vmhwm_kb - sparse.vmhwm_kb) << "\n";
        }
        const Metrics synth_dense = run_forked("", true);
        const Metrics synth_sparse = run_forked("", false);
        record(rows_json, "synthetic_4096x8000", nullptr, synth_dense, synth_sparse, count++ != 0);
        std::cout << "synthetic dense hwm_kb=" << synth_dense.vmhwm_kb
                  << " sparse hwm_kb=" << synth_sparse.vmhwm_kb
                  << " delta_kb=" << (synth_dense.vmhwm_kb - synth_sparse.vmhwm_kb) << "\n";

        std::ostringstream now;
        std::time_t t = std::time(nullptr);
        char stamp[32];
        std::strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%S%z", std::localtime(&t));
        now << stamp;

        std::ofstream out(argv[2]);
        if (!out) throw std::runtime_error(std::string("cannot write ") + argv[2]);
        out << "{\n \"date\":\"" << now.str().substr(0, 10) << "\""
            << ",\n \"run_started\":\"" << now.str() << "\""
            << ",\n \"purpose\":\"LP-01: peak-RSS / wall delta of the retired dense LP "
               "dispatch shape versus the sparse-first path (sparse-lp-path.md §5).\""
            << ",\n \"method\":\"fork per (instance, path); VmHWM from /proc/self/status; "
               "reference simplex on both entry shapes; no speed or optimality claim.\""
            << ",\n \"contract\":\"docs/contracts/sparse-lp-path.md\""
            << ",\n \"frozen_set\":\"data/compare/netlib.txt\""
            << ",\n \"instances\":[" << rows_json.str() << "]\n}\n";
        std::cout << "wrote " << argv[2] << " (" << count << " records)\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[-] Error: " << e.what() << "\n";
        return 1;
    }
}
