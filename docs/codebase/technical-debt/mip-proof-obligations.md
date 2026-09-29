# MIP cut and propagation audit notes

Proof format 3 can carry optional `MipObligation` records after the cut-free
tree. A cut note stores its coefficients, right-hand side, and the LP point's
observed left-hand side. A singleton propagation note stores the source row,
variable, coefficient, source bound, derived bound, and which side of the row
was used. The recording API checks finite scalars, a violated cut left-hand
side, and the singleton division. The reader budgets and parses the notes.

These records are **audit annotations**, not certificates of cut validity.
In particular, a violated LP point does not prove a cut is valid for every
integer solution, and a note alone does not prove that its source row belongs
to the model at that node. `verify_mip_proof` ignores the notes when deciding
acceptance and assurance tier. It independently checks the original-domain,
cut-free branch tree and every leaf witness. Annotations can be removed without
changing that replay result; the format-3 reader also accepts a missing trailer.

The serial optimizer puts applied singleton and verified cut events in
`milp::Result::obligations`. `build_mip_proof` accepts these as an optional
argument. The API currently discards the MILP result after copying summary
fields, so its proof artifact does not yet receive the notes. Parallel worker
events also need a synchronized collector after the fenced call site is wired.
The note's node ID identifies the optimizer tree, which differs from the
independently built proof tree. Independent validity checking of cut
derivations would require more than the current audit note fields.
