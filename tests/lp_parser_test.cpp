#include "markov_cero/io/lp_parser.hpp"
#include "markov_cero/api/solve.hpp"

#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

static void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error("Assertion failed: " + message);
    }
}

int main() {
    std::cout << "Running lp_parser_test...\n";

    // 1. Basic LP test
    {
        const std::string lp_text =
            "\\ Test linear program\n"
            "Minimize\n"
            "  obj: 2 x1 + 3 x2\n"
            "Subject To\n"
            "  c1: x1 + x2 >= 5\n"
            "  c2: 2 x1 - x2 <= 10\n"
            "Bounds\n"
            "  0 <= x1 <= 10\n"
            "  x2 >= 1\n"
            "End\n";

        const auto model = markov_cero::io::parse_lp_string(lp_text);
        require(model.objective_sense == markov_cero::model::ObjectiveSense::minimize, "sense must be minimize");
        require(model.row_name.size() == 2, "row count must be 2");
        require(model.variable_name.size() == 2, "variable count must be 2");
        require(model.row_name[0] == "c1", "row 0 name is c1");
        require(model.row_name[1] == "c2", "row 1 name is c2");

        // Row bounds: c1: [5, +inf), c2: (-inf, 10]
        require(model.row_lower[0].value == 5.0 && !model.row_upper[0].is_finite(), "c1 bounds");
        require(!model.row_lower[1].is_finite() && model.row_upper[1].value == 10.0, "c2 bounds");

        // Variable bounds: x1: [0, 10], x2: [1, +inf)
        require(model.variable_lower[0].value == 0.0 && model.variable_upper[0].value == 10.0, "x1 bounds");
        require(model.variable_lower[1].value == 1.0 && !model.variable_upper[1].is_finite(), "x2 bounds");

        // Objective
        require(model.objective[0] == 2.0, "obj x1");
        require(model.objective[1] == 3.0, "obj x2");

        // Solve model
        markov_cero::api::SolveOptions options;
        options.engine = "primal_simplex";
        auto res = markov_cero::api::solve_model(model, options);
        require(res.status == markov_cero::lp::reference::SolveStatus::optimal, "basic LP should be optimal");
        // Min 2 x1 + 3 x2 s.t. x1 + x2 >= 5, 2 x1 - x2 <= 10, x1 in [0,10], x2 >= 1
        // Optimal is x1 = 4, x2 = 1 -> 2(4) + 3(1) = 11? Wait, x1 + x2 >= 5, so with x2=1, x1>=4.
        // Or if x1=0, x2=5 -> 2(0) + 3(5) = 15. If x1=4, x2=1 -> obj = 11.
        // Wait, 2(4) - 1 = 7 <= 10. So x1=4, x2=1 is feasible with obj 11!
        require(std::abs(res.objective - 11.0) < 1e-4, "optimal value should be 11.0");
    }

    // 2. MILP with Binary and General variables
    {
        const std::string milp_text =
            "Maximize\n"
            "  cost: 5 x1 + 4 x2 - 2 y\n"
            "Subject To\n"
            "  con1: x1 + 2 x2 + y <= 14\n"
            "  con2: 3 x1 - x2 <= 10\n"
            "  con3: x1 - y == 0\n"
            "Bounds\n"
            "  x2 free\n"
            "  0 <= y <= 3\n"
            "Binary\n"
            "  x1\n"
            "General\n"
            "  y\n"
            "End\n";

        const auto model = markov_cero::io::parse_lp_string(milp_text);
        require(model.objective_sense == markov_cero::model::ObjectiveSense::maximize, "sense maximize");
        require(model.variable_name.size() == 3, "3 variables");

        // Variable types
        std::size_t x1_idx = 0, x2_idx = 1, y_idx = 2;
        for (std::size_t i = 0; i < model.variable_name.size(); ++i) {
            if (model.variable_name[i] == "x1") x1_idx = i;
            if (model.variable_name[i] == "x2") x2_idx = i;
            if (model.variable_name[i] == "y") y_idx = i;
        }

        require(model.variable_type[x1_idx] == markov_cero::model::VariableType::binary, "x1 is binary");
        require(model.variable_type[x2_idx] == markov_cero::model::VariableType::continuous, "x2 is continuous");
        require(model.variable_type[y_idx] == markov_cero::model::VariableType::integer, "y is integer");

        // Bounds: x2 free
        require(!model.variable_lower[x2_idx].is_finite() && !model.variable_upper[x2_idx].is_finite(), "x2 is free");
        // y in [0, 3]
        require(model.variable_lower[y_idx].value == 0.0 && model.variable_upper[y_idx].value == 3.0, "y in [0, 3]");

        // Solve MILP
        markov_cero::api::SolveOptions options;
        options.engine = "milp";
        auto res = markov_cero::api::solve_model(model, options);
        require(res.status == markov_cero::lp::reference::SolveStatus::optimal, "MILP should be optimal");
    }

    // 3. File I/O and solve_file dispatch
    {
        const std::string tmp_path = "/tmp/test_markov_cero.lp";
        std::ofstream out(tmp_path);
        out << "Minimize\n"
            << "  obj: x + y\n"
            << "Subject To\n"
            << "  c1: x + y >= 10\n"
            << "Bounds\n"
            << "  0 <= x <= 6\n"
            << "  0 <= y <= 6\n"
            << "End\n";
        out.close();

        const auto model = markov_cero::io::parse_lp_file(tmp_path);
        require(model.variable_name.size() == 2, "tmp file parsed 2 vars");

        markov_cero::api::SolveOptions options;
        auto res = markov_cero::api::solve_file(tmp_path, options);
        require(res.status == markov_cero::lp::reference::SolveStatus::optimal, "solve_file should be optimal");
        require(std::abs(res.objective - 10.0) < 1e-4, "optimal value should be 10.0");
    }

    // 4. Quadratic objective parsing
    {
        const std::string qp_text =
            "Minimize\n"
            "  x1 + x2 + [ 2 x1 ^ 2 + 2 x1 * x2 + 2 x2 ^ 2 ] / 2\n"
            "Subject To\n"
            "  x1 + x2 >= 2\n"
            "End\n";

        const auto model = markov_cero::io::parse_lp_string(qp_text);
        require(model.has_quadratic_objective, "has quadratic objective");
        require(model.quadratic_matrix.column_count == 2, "2x2 quadratic matrix");
    }

    // 5. Error handling
    {
        bool threw = false;
        try {
            (void)markov_cero::io::parse_lp_string("");
        } catch (const std::exception&) {
            threw = true;
        }
        require(threw, "empty string should throw");

        threw = false;
        try {
            (void)markov_cero::io::parse_lp_string("InvalidHeader\n  x1 + x2\nEnd\n");
        } catch (const std::exception&) {
            threw = true;
        }
        require(threw, "invalid header should throw");
    }

    // 6. Configurable parser resource limits fail before model construction.
    {
        using markov_cero::io::LpLimits;
        using markov_cero::io::parse_lp_string;
        const std::string linear =
            "Minimize\n x + y\nSubject To\n r1: x + y <= 4\n r2: x <= 2\nEnd\n";
        const auto rejects = [](const std::string& text, LpLimits limits) {
            try {
                (void)parse_lp_string(text, limits);
            } catch (const std::length_error&) {
                return true;
            }
            return false;
        };
        auto limits = LpLimits{};
        limits.maximum_bytes = linear.size() - 1;
        require(rejects(linear, limits), "byte budget must be enforced");
        limits = LpLimits{};
        limits.maximum_tokens = 4;
        require(rejects(linear, limits), "token budget must be enforced");
        limits = LpLimits{};
        limits.maximum_rows = 1;
        require(rejects(linear, limits), "row budget must be enforced");
        limits = LpLimits{};
        limits.maximum_columns = 1;
        require(rejects(linear, limits), "column budget must be enforced");
        limits = LpLimits{};
        limits.maximum_nonzeros = 1;
        require(rejects(linear, limits), "coefficient budget must be enforced");
        const std::string limited_path = "/tmp/markov_cero_lp_input_limit.lp";
        {
            std::ofstream limited_file(limited_path);
            limited_file << linear;
        }
        limits = LpLimits{};
        limits.maximum_bytes = 1;
        bool file_limited = false;
        try {
            (void)markov_cero::io::parse_lp_file(limited_path, limits);
        } catch (const std::length_error&) {
            file_limited = true;
        }
        require(file_limited, "file reader must enforce byte budget while reading");

        const std::string quadratic =
            "Minimize\n x + [ x^2 + y^2 ] / 2\nSubject To\n x + y <= 1\nEnd\n";
        limits = LpLimits{};
        limits.maximum_quadratic_terms = 1;
        require(rejects(quadratic, limits), "quadratic term budget must be enforced");
    }

    std::cout << "All lp_parser tests passed successfully!\n";
    return 0;
}
