# NLP-02 contract nlp-restoration.md section 7.5: elastic restoration
# through the public Python binding — recovery on the frozen failure case,
# the elastic_restoration off switch, and the observability keys.
import markov_cero as mc


def _x2_minus_one() -> mc.NlpModel:
    m = mc.NlpModel()
    m.n_var = 1
    m.set_objective(
        lambda x: (x[0] - 1.0) ** 2,
        lambda x: [2.0 * (x[0] - 1.0)],
    )
    m.add_equality(lambda x: [x[0] * x[0] - 1.0], lambda x: [[2.0 * x[0]]])
    return m


def test_restoration_recovers_through_binding():
    res = _x2_minus_one().solve([0.0])
    assert res["status"] == "LocalStationary", res["message"]
    assert res["restoration_steps"] >= 1, res
    assert res["restoration_failures"] == 0, res
    assert abs(float(res["x"][0]) - 1.0) < 1e-6, res["x"]
    assert res["constraint_violation"] < 1e-6, res
    assert res["original_verified"] is True, res
    assert "elastic restoration steps=" in res["message"], res["message"]


def test_restoration_off_switch_keeps_old_path():
    res = _x2_minus_one().solve([0.0], elastic_restoration=False)
    assert res["status"] == "NumericalFailure", res
    assert "elastic restoration is disabled" in res["message"], res["message"]
    assert res["restoration_steps"] == 0, res
    assert res["restoration_failures"] == 0, res


def test_restoration_keys_present_on_every_solve():
    m = mc.NlpModel()
    m.n_var = 1
    m.set_objective(lambda x: x[0] ** 2, lambda x: [2.0 * x[0]])
    res = m.solve([1.0])
    assert res["restoration_steps"] == 0, res
    assert res["restoration_failures"] == 0, res
