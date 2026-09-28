from .plot_crossover_config import (
    argparse, os, sys
)
from .plot_crossover_generate_svg import generate_svg
from .plot_crossover_print_ascii_summary import parse_crossover_csv
from .plot_crossover_print_ascii_summary import print_ascii_summary

def main():
    parser = argparse.ArgumentParser(
        description="markov-cero Scale Crossover Plot Generator"
    )
    parser.add_argument(
        "--input", default="evidence/benchmarks/crossover_study.csv",
        help="Input crossover CSV (long schema)"
    )
    parser.add_argument(
        "--output-svg", default="evidence/benchmarks/crossover_plot.svg",
        help="Output SVG plot path"
    )
    parser.add_argument(
        "--output-evidence", default="",
        help="Optional second copy of the SVG (evidence benchmark path)"
    )
    parser.add_argument(
        "--crossover-nnz", type=int, default=None,
        help="Override the empirical crossover marker (nonzeros)"
    )
    parser.add_argument(
        "--crossover-row", type=int, default=None,
        help=argparse.SUPPRESS)  # legacy flag; rows are not in the schema
    args = parser.parse_args()

    if not os.path.isfile(args.input):
        print(f"Error: input file {args.input} not found.", file=sys.stderr)
        sys.exit(1)

    records = parse_crossover_csv(args.input)
    if not records:
        print(f"Error: no data rows in {args.input}.", file=sys.stderr)
        sys.exit(1)
    print_ascii_summary(records)

    crossover_nnz = args.crossover_nnz
    generate_svg(records, args.output_svg, crossover_nnz=crossover_nnz)
    if args.output_evidence and args.output_evidence != args.output_svg:
        generate_svg(records, args.output_evidence, crossover_nnz=crossover_nnz)
