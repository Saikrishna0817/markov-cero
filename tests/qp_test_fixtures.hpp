#pragma once
#include "markov_cero/io/mps.hpp"
#include "markov_cero/milp/milp_solver.hpp"
#include "markov_cero/qp/admm_solver.hpp"
#include "markov_cero/qp/kkt.hpp"
#include "markov_cero/qp/model.hpp"
#include "markov_cero/qp/verifier.hpp"

#include <cmath>
#include <chrono>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

inline void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}


using namespace markov_cero;
using namespace markov_cero::qp;
void qp_scenario_0();
void qp_scenario_1();
void qp_scenario_2();
void qp_scenario_3();
void qp_scenario_4();
void qp_scenario_5();
void qp_scenario_6();
void qp_scenario_7();
void qp_scenario_8();
