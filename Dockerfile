FROM python:3.12-slim

WORKDIR /workspace
ENV PLATFORMIO_CORE_DIR=/opt/platformio

RUN pip install --no-cache-dir platformio
COPY . /workspace

RUN pio run

CMD ["pio", "run"]
