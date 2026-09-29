FROM debian:bookworm AS solver-build
RUN apt-get update && apt-get install -y --no-install-recommends cmake g++ make ca-certificates && rm -rf /var/lib/apt/lists/*
WORKDIR /source
COPY CMakeLists.txt LICENSE NOTICE ./
RUN mkdir -p docs/project
COPY docs/project/PROVENANCE.md ./docs/project/PROVENANCE.md
COPY cmake ./cmake
COPY include ./include
COPY src ./src
COPY apps ./apps
COPY gpu ./gpu
RUN cmake -S . -B /tmp/markov-build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DMARKOV_CERO_WARNINGS_AS_ERRORS=OFF \
    && cmake --build /tmp/markov-build --target markov-cero-solve -j2

FROM python:3.12-slim
RUN apt-get update && apt-get install -y --no-install-recommends libstdc++6 && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY --from=solver-build /tmp/markov-build/markov-cero-solve /app/markov-cero-solve
COPY web/backend/server.py /app/server.py
ENV SOLVER_BIN=/app/markov-cero-solve
EXPOSE 8080
CMD ["python", "/app/server.py"]
