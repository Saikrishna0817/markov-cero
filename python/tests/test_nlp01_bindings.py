"""NLP-01 contract nlp-local-sqp.md §2.4/§3/§6.9: callback telemetry,
x0 projection disclosure, limit incumbents and the derivative diagnostic,
through the installed Python binding.
"""
import pytest

import markov_cero as mc  # noqa: E402


def test_nlp_callback_telemetry_and_derivative_diagnostic():
    # NLP-01 contract nlp-local-sqp.md §2.4/§4.5/§6.9 and §3: evaluation
    # counts, x0 projection disclosure, a retained feasible incumbent on a
    # limit exit, and the developer-only finite-difference diagnostic.
    nlp = mc.NlpModel()
    nlp.n_var = 2
    nlp.set_objective(
        lambda x: (x[0] - 1.0) ** 2 + (x[1] - 2.0) ** 2,
        lambda x: [2.0 * (x[0] - 1.0), 2.0 * (x[1] - 2.0)],
    )
    nlp.set_bounds([0.0, 0.0], [10.0, 10.0])
    res = nlp.solve([-4.0, 0.5], max_iterations=1)
    assert res["status"] == "IterationLimit"
    assert res["callback_evaluations"] > 0
    assert res["x0_projection_norm"] == pytest.approx(4.0)
    assert res.get("best_feasible_x") is not None
    best = res["best_feasible_x"]
    assert res.get("best_feasible_objective") == pytest.approx(
        (best[0] - 1.0) ** 2 + (best[1] - 2.0) ** 2
    )

    good = mc.check_derivatives(nlp, [2.0, 1.0])
    assert good["passed"]
    assert good["gradient_ok"]
    assert good["gradient_checks"] == 2

    wrong = mc.NlpModel()
    wrong.n_var = 2
    wrong.set_objective(
        lambda x: x[0] ** 2 + 3.0 * x[0] * x[1],
        lambda x: [2.0 * x[0], 3.0 * x[0]],  # drops the 3*x1 term
    )
    bad = mc.check_derivatives(wrong, [1.0, 2.0])
    assert not bad["passed"]
    assert not bad["gradient_ok"]
    assert bad["worst_gradient_error"] > 1e-3
