#!/usr/bin/env python3
"""Measure cold, one-shot warm, and session solves in build_item8.

The C++ driver is generated in build_item8 so no build-system edits are needed.
All accepted results are independently verified inside the timed operation.
"""

import argparse
import json
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
DRIVER = r'''
#include "markov_cero/lp/dual/dual_simplex.hpp"
#include "markov_cero/qp/admm_solver.hpp"
#include "markov_cero/qp/verifier.hpp"
#include "markov_cero/verify/reference_lp_verifier.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace markov_cero;
using Clock = std::chrono::steady_clock;
template<class F> double timed(F&& f) {
    auto t = Clock::now(); f();
    return std::chrono::duration<double, std::milli>(Clock::now()-t).count();
}
void check(bool ok, const char* msg) { if (!ok) throw std::runtime_error(msg); }
transform::CanonicalModel lp_model(int blocks, int step, bool rhs) {
    transform::CanonicalModel m;
    m.matrix.rows = 2*blocks; m.matrix.columns = 3*blocks;
    m.matrix.values.assign(m.matrix.rows*m.matrix.columns, 0);
    m.rhs.resize(m.matrix.rows); m.objective.assign(m.matrix.columns, 0);
    for (int b=0; b<blocks; ++b) {
        const int r=2*b, c=3*b;
        m.matrix.values[r*m.matrix.columns+c]=-1;
        m.matrix.values[r*m.matrix.columns+c+1]=1;
        m.matrix.values[(r+1)*m.matrix.columns+c]=1;
        m.matrix.values[(r+1)*m.matrix.columns+c+2]=1;
        const double delta = rhs ? (step%5)*0.1 : (step%5)*0.15;
        m.rhs[r]=-(1.0+delta+(b%3)*0.1);
        m.rhs[r+1]=4.0+(rhs ? 0.0 : delta)+(b%4)*0.1;
        m.objective[c]=1;
    }
    m.record.objective_sign=1;
    m.record.structural_variables=blocks;
    m.record.variables.resize(blocks);
    m.validate(); return m;
}
qp::QuadraticModel qp_model(int n, int step, bool rhs) {
    qp::QuadraticModel q;
    q.P.dimension=n; q.P.column_offsets.resize(n+1);
    q.A.rows=n; q.A.columns=n; q.A.column_offsets.resize(n+1);
    q.q.resize(n); q.l.resize(n); q.u.resize(n);
    for (int i=0;i<n;++i) {
        q.P.column_offsets[i]=i; q.P.row_indices.push_back(i); q.P.values.push_back(2);
        q.A.column_offsets[i]=i; q.A.row_indices.push_back(i); q.A.values.push_back(1);
        q.q[i]=-2.0-(i%3)*0.1;
        double delta=(step%5)*0.01;
        q.l[i]=rhs ? 0.5+delta : 0.5;
        q.u[i]=rhs ? 2.0+delta : 2.0-delta;
    }
    q.P.column_offsets[n]=n; q.A.column_offsets[n]=n;
    q.validate(); return q;
}
struct Stat { double ms=0; std::size_t iterations=0, refactors=0, reuse=0, verified=0; };
void print(const std::string& name, const std::string& mode, const Stat& s) {
    std::cout << name << ' ' << mode << ' ' << s.ms << ' ' << s.iterations << ' '
              << (name.rfind("lp_", 0)==0 && mode=="cold" ? -1LL :
                  static_cast<long long>(s.refactors)) << ' '
              << s.reuse << ' ' << s.verified << '\n';
}
void bench_lp(int blocks, int steps, bool rhs) {
    const std::string name=rhs ? "lp_rhs" : "lp_bounds";
    Stat cold, warm, repeat;
    auto session=lp::dual::make_session();
    auto first=lp_model(blocks, 0, rhs);
    auto bootstrap=session.resolve(first);
    check(bootstrap.verified, "LP bootstrap not verified");
    auto warm_basis=bootstrap.basis_state;
    for (int step=1;step<=steps;++step) {
        auto m=lp_model(blocks,step,rhs);
        lp::dual::Result a,b,c;
        cold.ms += timed([&]{ a=lp::dual::solve(m); });
        warm.ms += timed([&]{ b=lp::dual::solve(m,{},warm_basis); });
        repeat.ms += timed([&]{ c=session.resolve(m); });
        check(a.verified && b.verified && c.verified, "LP solve not verified");
        check(verify::verify_reference_result(m,a.solution).accepted &&
              verify::verify_reference_result(m,b.solution).accepted &&
              verify::verify_reference_result(m,c.solution).accepted, "LP witness rejected");
        check(a.solution.status==b.solution.status && a.solution.status==c.solution.status &&
              std::abs(a.solution.objective-c.solution.objective)<1e-7 &&
              std::abs(b.solution.objective-c.solution.objective)<1e-7, "LP parity");
        cold.iterations+=a.solution.phase_one_iterations+a.solution.phase_two_iterations;
        warm.iterations+=b.telemetry.size();
        repeat.iterations+=c.telemetry.size();
        cold.refactors+=a.refactorizations; warm.refactors+=b.refactorizations;
        repeat.refactors+=c.refactorizations;
        cold.verified++; warm.verified++; repeat.verified++;
        repeat.reuse+=c.factor_reused;
        warm_basis=c.basis_state;
    }
    print(name,"cold",cold); print(name,"warm",warm); print(name,"session",repeat);
}
void bench_qp(int n, int steps, bool rhs) {
    const std::string name=rhs ? "qp_rhs" : "qp_bounds";
    Stat cold, repeat;
    qp::QpOptions options; options.adaptive_rho=false;
    options.absolute_tolerance=1e-5; options.relative_tolerance=1e-5;
    qp::AdmmQpSolver session;
    for (int step=0;step<steps;++step) {
        auto model=qp_model(n,step,rhs);
        qp::QpSolution a,b;
        cold.ms += timed([&]{ a=qp::solve_qp(model,options); });
        repeat.ms += timed([&]{ b=qp::solve_qp(model,options,session,false); });
        check(a.status==qp::QpStatus::optimal && b.status==a.status &&
              a.verified && b.verified, "QP status or verification parity");
        check(std::abs(a.objective_value-b.objective_value)<1e-7, "QP objective parity");
        check(qp::verify_qp_solution(model,a).passed &&
              qp::verify_qp_solution(model,b).passed, "QP witness rejected");
        cold.iterations+=a.iterations; repeat.iterations+=b.iterations;
        cold.refactors+=1+a.refactorization_count;
        repeat.refactors+=1+b.refactorization_count;
        repeat.reuse+=b.kkt_symbolic_reuse;
        cold.verified++; repeat.verified++;
    }
    print(name,"cold",cold); print(name,"session",repeat);
}
int main(int argc,char** argv) {
    const int steps=argc>1 ? std::stoi(argv[1]) : 12;
    bench_lp(100,steps,true); bench_lp(100,steps,false);
    bench_qp(120,steps,true); bench_qp(120,steps,false);
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--steps", type=int, default=12)
    parser.add_argument("--build-dir", type=pathlib.Path, default=ROOT / "build_item8")
    parser.add_argument("--output", type=pathlib.Path)
    args = parser.parse_args()
    build = args.build_dir.resolve()
    source = build / "bench_repeated_solve.cpp"
    binary = build / "bench_repeated_solve"
    source.write_text(DRIVER)
    subprocess.run(["c++", "-std=c++20", "-O3", "-DNDEBUG", "-I", str(ROOT / "include"),
                    "-I", str(ROOT / "gpu/include"), str(source),
                    str(build / "libmarkov_cero_core.a"), "-o", str(binary)], check=True)
    raw = subprocess.check_output([str(binary), str(args.steps)], text=True)
    result = {"steps": args.steps, "units": "ms", "cases": {}}
    for line in raw.splitlines():
        name, mode, ms, iterations, refactors, reuse, verified = line.split()
        result["cases"].setdefault(name, {})[mode] = {
            "wall_ms": float(ms), "iterations": int(iterations),
            "refactorizations": None if int(refactors) < 0 else int(refactors),
            "cache_reuses": int(reuse),
            "verified_results": int(verified)}
    rendered = json.dumps(result, indent=2)
    if args.output:
        args.output.write_text(rendered + "\n")
    print(rendered)


if __name__ == "__main__":
    main()
