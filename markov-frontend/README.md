# Markov-cero frontend

The site tells the solver story across eleven labeled states. A fixed desktop Three.js canvas changes from a feasible polytope to a branch tree, solver pipeline, presolve/QP sketches, sparse GPU grid, and convergence point as GSAP ScrollTrigger and Lenis follow the page. The network canvas remains the mobile, reduced-motion, and WebGL-unavailable fallback. Three.js renders at no more than 1.5 DPR and skips rendering once the journey leaves the viewport. The interface uses off-white, navy, blue, orange, and warm highlights from the supplied images.

The landing page links to local Status & Roadmap, Verification, synthetic Refinery Case Study, Sovereignty, Benchmarks, Algorithms Lab, GPU & Research, Docs / API, Team, Security, Changelog, and Glossary pages. Claims are snapshots of the repository's checked-in `STATUS.md`, changelog, and evidence files as of 2026-09-29; they are not live solver telemetry. The refinery page reports one measured local CLI run and labels its runtime accordingly.

## Run locally

```bash
cd markov-frontend
npm install
cp .env.example .env.local
npm run dev
```

Set `VITE_SUPABASE_URL` and `VITE_SUPABASE_PUBLISHABLE_KEY` in `.env.local`. The app uses Supabase Auth for email signup and sign-in. Supabase stores account data in its managed Auth schema; this frontend does not create a separate users table. Add the local and deployed frontend origins to **Authentication → URL Configuration → Redirect URLs** in Supabase so confirmation links return to the site. Keep email confirmation enabled. Never put a Supabase secret or service-role key in a `VITE_` variable.

The optional `VITE_SOLVER_API_URL` points to the HTTP adapter in `backend/`. Without it, the workspace accepts a sample model for inspection and clearly says that live solving is pending.

## Solver API

The adapter is a separate Python standard-library service. It verifies the Supabase bearer token with `/auth/v1/user` before running the existing C++ CLI. It accepts `.mps` or `.lp` model text at `POST /solve`, with a 1 MB input cap, at most two simultaneous solves, a 10-second solver time limit, and a 15-second process timeout. `GET /health` reports whether the solver executable is available.

For local development, build the solver from the repository root and run:

```bash
cd markov-frontend
PORT=8080 \
SOLVER_BIN=/absolute/path/to/markov-cero-solve \
SUPABASE_URL=https://YOUR_PROJECT_REF.supabase.co \
SUPABASE_PUBLISHABLE_KEY=sb_publishable_YOUR_KEY \
ALLOWED_ORIGIN=http://localhost:5173 \
python backend/server.py
```

The backend Dockerfile is at `markov-frontend/backend/Dockerfile` and expects the repository root as its build context. The final container includes the solver binary and adapter, not the frontend's local environment file.

## Deployment layout

- **Vercel:** static Vite frontend; project root `markov-frontend`, build command `npm run build`, output `dist`.
- **Railway or Render:** solver API; Dockerfile `markov-frontend/backend/Dockerfile`, repository-root build context. Set `SUPABASE_URL`, `SUPABASE_PUBLISHABLE_KEY`, and `ALLOWED_ORIGIN` to the public frontend origin.
- After the API is live, set `VITE_SOLVER_API_URL` in the frontend deployment and rebuild. Add that frontend origin to Supabase redirect URLs.

The FAQ and detailed footer link to project status, verification, research limits, documentation, and a factual data and privacy page. This directory is kept local; the deployment notes above are instructions only and do not publish it.
