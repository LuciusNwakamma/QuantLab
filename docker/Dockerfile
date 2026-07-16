# ---- Build stage ----
FROM ubuntu:24.04 AS cpp-builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git ninja-build ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY CMakeLists.txt .
COPY cpp/ cpp/
COPY tests/cpp/ tests/cpp/

RUN cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DQUANTLAB_BUILD_TESTS=ON \
    -G Ninja \
  && cmake --build build --parallel

# ---- Python research stage ----
FROM python:3.12-slim AS python-research

WORKDIR /app
COPY python/requirements.txt .

RUN pip install --no-cache-dir -r requirements.txt

COPY python/ python/
COPY research/ research/

# ---- Final combined image ----
FROM python:3.12-slim

WORKDIR /app

# Copy C++ binaries
COPY --from=cpp-builder /app/build/tests/cpp/ /app/bin/

# Copy Python layer
COPY --from=python-research /usr/local/lib/python3.12/site-packages/ \
     /usr/local/lib/python3.12/site-packages/
COPY --from=python-research /app/ .

ENV PYTHONPATH=/app

CMD ["python", "-c", "import python; print('QuantLab ready.')"]
