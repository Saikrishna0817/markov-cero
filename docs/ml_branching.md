# ML assisted branching (W2)

## Model and labels

Each training sample is a branch-and-bound node. Variable nodes are the
fractional integer columns at that node. Constraint nodes are rows active at
the LP solution. An edge connects candidate variable `j` to active row `i` and
carries `A_ij`. Its message weight is

\[
\widehat A_{ij}=\frac{A_{ij}}
 {\sqrt{(\sum_k |A_{ik}|)(\sum_l |A_{lj}|)}}.
\]

The variable vector has six entries: fractionality, objective coefficient,
up/down pseudo-cost ratios, bound width, and column density. The row vector
has normalized finite side, normalized primal activity, signed dual
multiplier divided by the objective coefficient scale, and row density.
Bounds and primal activity are kept separately because a binding row is not
identified by its right-hand side alone. Node LP duals are mapped from the
canonicalized rows back to original model rows before logging.

The two message-passing layers are

\[
h_i^{row}=\operatorname{ReLU}\left(W_r r_i+
 \sum_{j:(j,i)\in E}\widehat A_{ij}W_{v\to r}x_j\right),
\qquad
h_j^{var}=\operatorname{ReLU}\left(W_v x_j+
 \sum_{i:(j,i)\in E}\widehat A_{ij}W_{r\to v}h_i^{row}\right).
\]

A two-layer dense head maps each variable embedding to one branching score.
Training labels are the solver's strong-branching product scores
`(delta_down + epsilon) * (delta_up + epsilon)`. The loss is pairwise
softplus ranking loss over unequal label pairs. This learns the ranking used
by the selector; it does not treat score magnitudes as calibrated
probabilities. A candidate is logged only when both child LPs terminate with
an optimal relaxation or a proven infeasibility result. Iteration/resource-
limited child solves are unresolved and omitted; a stopped LP's primal
objective is not a certified minimization bound. Feature pseudocosts are
captured before the current node's probes, and probe-only gains do not update
the collector's pseudocost history.

## Data and validation

`scripts/ml/collect_training_data.py` runs our own solver and writes
MCONLOG3 records. The binary contains ordered variable features, active-row
features, edge endpoints and coefficients, and aligned strong-branching
labels. The reader accepts older MCONLOG1/MCONLOG2 logs for inspection, but
the trainer rejects records without the current four row features.

The trainer partitions by instance into 70/15/15 splits before fitting robust
scalers on training nodes only. It trains for 50 epochs with Adam at `1e-3`
and batch size 32. `scripts/ml/export_onnx.py` writes a standard ONNX graph
whose inputs are raw solver features, checks the graph, and compares its
outputs against ONNX Runtime.

## C++ boundary and open acceptance gates

The C++ scorer reads the standard ONNX protobuf's initializers and evaluates
the fixed exported GCN architecture directly, without linking PyTorch or an
ONNX Runtime shared library. It validates tensor names and dimensions and
rejects unsupported model layouts. This is a specialized interpreter for
this architecture, not a general ONNX Runtime implementation. C++ scores
must match ONNX Runtime before a model is considered usable.

ML remains opt-in (`--branching ml_gnn`), compiled only with
`MARKOV_CERO_ENABLE_ML=ON`, and activates only above 200 fractional integer
candidates. Until an adequately sized self-collected dataset, quantized
model parity, and a genuine ML-on node-count win are recorded, the feature is
not accepted for release. In particular, `stein15` has too few candidates to
exercise the ML path and cannot serve as that acceptance case.
