#include "markov_cero/io/mps.hpp"
#include "markov_cero/verify/mip_proof.hpp"
#include <sstream>
#include <stdexcept>
using namespace markov_cero;
void require(bool condition) { if (!condition) throw std::runtime_error("MIP proof regression"); }
int main() {
    auto model = io::parse_mps_string(
        "NAME TEST\nROWS\n N COST\n L CAP\nCOLUMNS\n X COST -1 CAP 1\n"
        "RHS\n R CAP 1.5\nBOUNDS\n LI B X 0\n UI B X 2\nENDATA\n");
    auto proof = verify::build_mip_proof(model, {1}, -1, false);
    require(verify::verify_mip_proof(model, proof).accepted);
    require(proof.nodes.size() == 3);
    std::stringstream stream; verify::write_mip_proof(stream, proof);
    auto decoded = verify::read_mip_proof(stream);
    require(verify::verify_mip_proof(model, decoded).accepted);
    auto corrupt = proof; corrupt.nodes[0].up = corrupt.nodes[0].down;
    require(!verify::verify_mip_proof(model, corrupt).accepted);
    corrupt = proof; corrupt.nodes[0].split_value = .5;
    require(!verify::verify_mip_proof(model, corrupt).accepted);
    corrupt = proof; corrupt.nodes[0].down = 0;
    require(!verify::verify_mip_proof(model, corrupt).accepted);
    corrupt = proof; corrupt.incumbent[0] = 1.5;
    require(!verify::verify_mip_proof(model, corrupt).accepted);
    corrupt = proof; corrupt.nodes.emplace_back();
    require(!verify::verify_mip_proof(model, corrupt).accepted);
    corrupt = proof;
    for (auto& node : corrupt.nodes) node.relaxation.dual.clear();
    require(!verify::verify_mip_proof(model, corrupt).accepted);
    corrupt = proof; corrupt.nodes[1].kind = verify::MipProofKind::empty_domain;
    require(!verify::verify_mip_proof(model, corrupt).accepted);
    verify::MipProofOptions small; small.maximum_nodes = 1;
    require(!verify::verify_mip_proof(model, proof, small).accepted);
    const auto no_proof = verify::build_mip_proof(model, {0}, 0, false);
    require(!verify::verify_mip_proof(model, no_proof).accepted);
    model.row_lower[0] = model::Bound::finite(.5);
    model.row_upper[0] = model::Bound::finite(.5);
    const auto infeasible = verify::build_mip_proof(model, {}, 0, true);
    require(verify::verify_mip_proof(model, infeasible).accepted);

    const auto quadratic = io::parse_mps_string(
        "NAME QINT\nROWS\n N COST\nCOLUMNS\n X COST -1\nBOUNDS\n LI B X 0\n UI B X 2\n"
        "QUADOBJ\n X X 2\nENDATA\n");
    auto qp_proof = verify::build_mip_proof(quadratic, {0}, 0, false);
    auto qp_report = verify::verify_mip_proof(quadratic, qp_proof);
    if (!qp_report.accepted) throw std::runtime_error(qp_report.message);
    require(qp_proof.nodes.size() == 3);
    std::stringstream qp_stream; verify::write_mip_proof(qp_stream, qp_proof);
    require(verify::verify_mip_proof(quadratic, verify::read_mip_proof(qp_stream)).accepted);
    for (auto& node : qp_proof.nodes) node.relaxation.dual.assign(node.relaxation.dual.size(), 100);
    require(!verify::verify_mip_proof(quadratic, qp_proof).accepted);
    verify::MipProofOptions expired;
    expired.deadline = std::chrono::steady_clock::now();
    require(!verify::verify_mip_proof(model, proof, expired).accepted);
    for (const auto text : {"MARKOV_MIP_PROOF 1 0 0 999999999", "MARKOV_MIP_PROOF 1 0 nan 1"}) {
        bool rejected = false;
        try { std::stringstream bad(text); (void)verify::read_mip_proof(bad); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected);
    }
}
