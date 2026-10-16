FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake python3 python3-matplotlib util-linux \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY CMakeLists.txt common.h sequential.c pthreads.c openmp.c bench.py make_results.py make_figures.py entrypoint.sh ./
RUN chmod +x entrypoint.sh \
    && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build

VOLUME ["/data", "/results"]
ENTRYPOINT ["/app/entrypoint.sh"]
CMD ["all"]
