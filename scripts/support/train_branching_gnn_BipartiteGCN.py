from __future__ import annotations
from .train_branching_gnn_config import (
    HIDDEN, ml_metrics, nn, np, torch
)

class BipartiteGCN(nn.Module):
    """Two message-passing layers over fractional-variable/active-row nodes."""
    def __init__(self, n_var_features=6, n_row_features=4, hidden=HIDDEN):
        super().__init__()
        self.var_to_row = nn.Linear(n_var_features, hidden, bias=False)
        self.row_self = nn.Linear(n_row_features, hidden)
        self.var_self = nn.Linear(n_var_features, hidden)
        self.row_to_var = nn.Linear(hidden, hidden, bias=False)
        self.var_out = nn.Sequential(nn.Linear(hidden, hidden), nn.ReLU(), nn.Linear(hidden, 1))

    def forward(self, variables, rows, edge_index, edge_weight):
        vi, ri = edge_index[0], edge_index[1]
        row_messages = self.var_to_row(variables[vi]) * edge_weight.unsqueeze(1)
        row_aggregate = variables.new_zeros((rows.shape[0], row_messages.shape[1]))
        row_index = ri.unsqueeze(1).expand(-1, row_messages.shape[1])
        row_aggregate = row_aggregate.scatter_add(0, row_index, row_messages)
        row_hidden = torch.relu(self.row_self(rows) + row_aggregate)

        var_messages = self.row_to_var(row_hidden[ri]) * edge_weight.unsqueeze(1)
        var_aggregate = variables.new_zeros((variables.shape[0], var_messages.shape[1]))
        var_index = vi.unsqueeze(1).expand(-1, var_messages.shape[1])
        var_aggregate = var_aggregate.scatter_add(0, var_index, var_messages)
        var_hidden = torch.relu(self.var_self(variables) + var_aggregate)
        return self.var_out(var_hidden).squeeze(-1)

class RawInputExport(nn.Module):
    """Expose raw solver features at the ONNX boundary; scalers are buffers."""
    def __init__(self, model, scalers):
        super().__init__()
        self.model = model
        xc, xs, rc, rs = scalers
        self.register_buffer("variable_center", torch.as_tensor(xc, dtype=torch.float32))
        self.register_buffer("variable_scale", torch.as_tensor(xs, dtype=torch.float32))
        self.register_buffer("row_center", torch.as_tensor(rc, dtype=torch.float32))
        self.register_buffer("row_scale", torch.as_tensor(rs, dtype=torch.float32))

    def forward(self, variables, rows, edge_index, edge_weight):
        variables = (variables - self.variable_center) / self.variable_scale
        rows = (rows - self.row_center) / self.row_scale
        return self.model(variables, rows, edge_index, edge_weight)

def normalized_edges(record):
    edges = record["edges"]
    coeff = np.asarray(record["edge_coefficients"], dtype=np.float32)
    if len(edges) == 0:
        return np.empty((2, 0), np.int64), np.empty((0,), np.float32)
    vi, ri = edges[:, 0], edges[:, 1]
    dv = np.bincount(vi, weights=np.abs(coeff), minlength=len(record["X"]))
    dr = np.bincount(ri, weights=np.abs(coeff), minlength=len(record["R"]))
    norm = coeff / np.sqrt(np.maximum(dv[vi], 1e-12) * np.maximum(dr[ri], 1e-12))
    return np.stack((vi, ri)).astype(np.int64), norm.astype(np.float32)

def graph_tensors(record, x_center, x_scale, r_center, r_scale):
    edge_index, edge_weight = normalized_edges(record)
    X = (record["X"] - x_center) / x_scale
    R = (record["R"] - r_center) / r_scale
    return (torch.as_tensor(X, dtype=torch.float32),
            torch.as_tensor(R, dtype=torch.float32),
            torch.as_tensor(edge_index, dtype=torch.long),
            torch.as_tensor(edge_weight, dtype=torch.float32))

def rank_loss(pred, target):
    if pred.numel() < 2:
        return pred.sum() * 0.0
    i, j = torch.triu_indices(pred.numel(), pred.numel(), offset=1, device=pred.device)
    delta = target[i] - target[j]
    keep = delta != 0
    if not torch.any(keep):
        return pred.sum() * 0.0
    sign = torch.sign(delta[keep])
    return torch.nn.functional.softplus(-(pred[i[keep]] - pred[j[keep]]) * sign).mean()

def predict_records(model, records, scalers):
    model.eval()
    out = []
    with torch.no_grad():
        for rec in records:
            args = graph_tensors(rec, *scalers)
            out.append(model(*args).cpu().numpy().astype(np.float64))
    return out

def evaluate(records, predictions):
    return ml_metrics.evaluate_records(
        list(zip([r["X"] for r in records], [r["S"] for r in records])),
        lambda _x, it=iter(predictions): next(it), cross_check_scipy=False)
