# Visual web app and optional solver API

`web/` contains the markov-cero presentation site and a small Python HTTP adapter for the native CLI. The React/Vite site explains the project through an interactive solver journey, capability pages, benchmark context, refinery case study and links to source records. The existing [logo](public/assets/markov-logo.jpeg) is shared with the repository README. The pages are a dated presentation of the [status register](../docs/project/STATUS.md) and [evidence index](../evidence/INDEX.md), not live benchmark telemetry.

The visual journey uses Three.js, GSAP and Lenis, with a fallback for mobile, reduced motion and unavailable WebGL. The workspace can inspect a sample model without an API; a live solve requires the adapter and its authentication configuration. Building the site alone does not deploy a solver.

## Run the site locally

From the repository root:

```sh
cd web
npm ci
cp .env.example .env.local
npm run dev
```

Set `VITE_SUPABASE_URL` and `VITE_SUPABASE_PUBLISHABLE_KEY` in `.env.local` for email sign-in. Add local and deployed site origins to Supabase Auth redirect URLs. `VITE_` variables are sent to browsers; do not put service-role or secret keys in them. `VITE_SOLVER_API_URL` is optional and points to the separately running adapter. Build the static site with `npm run build` and check it with `npm run preview`.

## Run the optional adapter

First build `markov-cero-solve` from the repository root. Then run from `web/` with a real local executable path and your configured authentication origin:

```sh
PORT=8080 \
SOLVER_BIN=/absolute/path/to/markov-cero-solve \
SUPABASE_URL=https://YOUR_PROJECT_REF.supabase.co \
SUPABASE_PUBLISHABLE_KEY=YOUR_PUBLISHABLE_KEY \
ALLOWED_ORIGIN=http://localhost:5173 \
python backend/server.py
```

`GET /health` reports whether the binary is available. `POST /solve` accepts MPS or LP text after validating the bearer token with Supabase Auth. The adapter applies an approximately 1 MB input cap, two concurrent solve slots, a 10-second CLI solve limit and a 15-second process timeout. Each solve child additionally runs under kernel-enforced OS limits (CPU, address space, file size, redirected output) with kill-on-timeout, declared in the [hosted limits contract](../docs/contracts/hosted-limits.md) and exercised by `backend/os_limits_test.py` (CTest `hosted_os_limits`). It uses the existing C++ CLI rather than implementing a second solver. Do not expose this adapter without its intended authentication and origin configuration.

## Deployment layout

- **Static frontend:** use `web/` as the project root, `npm run build` as the build command and `dist/` as the output directory.
- **Solver adapter:** use [`web/backend/Dockerfile`](backend/Dockerfile) with the repository root as Docker build context. The image compiles the CLI and copies the adapter; it does not include `.env.local`.
- **Connection:** set `VITE_SOLVER_API_URL` to the adapter origin, rebuild the static frontend, set the adapter's `ALLOWED_ORIGIN` to the frontend origin, and configure matching Supabase redirect URLs.

The project records no completed production deployment in this README. The [privacy page](public/privacy.html) describes the current browser data flow; the [repository guide](../docs/README.md) identifies the source of each project claim.
