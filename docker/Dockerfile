FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake python3 python3-matplotlib util-linux \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY c/ ./c/
COPY python/ ./python/
COPY tests/ ./tests/
COPY runners/entrypoint.sh ./
RUN chmod +x entrypoint.sh tests/edge_cases.sh \
    && cmake -S c -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build

VOLUME ["/data", "/results"]
ENTRYPOINT ["/app/entrypoint.sh"]
CMD ["all"]
