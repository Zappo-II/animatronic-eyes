# Animatronic Eyes
# Copyright (c) 2025 Zappo-II
# Licensed under CC BY-NC-SA 4.0
# https://github.com/Zappo-II/animatronic-eyes
#
# Docker build environment for ESP32 firmware

FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install dependencies
RUN apt-get update && apt-get install -y \
    curl \
    git \
    python3 \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

# Install arduino-cli
RUN curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
ENV PATH="/root/bin:${PATH}"

# Install ESP32 board support
RUN arduino-cli core update-index && \
    arduino-cli core install esp32:esp32@3.3.5

# Install libraries via arduino-cli
RUN arduino-cli lib install \
    "ESP32Servo@3.0.9" \
    "ArduinoJson@7.4.2"

# Install ESP32Async libraries (same versions as Arduino IDE)
RUN git clone --depth 1 --branch v3.9.4 https://github.com/ESP32Async/ESPAsyncWebServer.git /root/Arduino/libraries/ESP_Async_WebServer && \
    git clone --depth 1 --branch v3.4.10 https://github.com/ESP32Async/AsyncTCP.git /root/Arduino/libraries/Async_TCP

# Install mklittlefs
RUN curl -L https://github.com/earlephilhower/mklittlefs/releases/download/4.1.0/x86_64-linux-gnu-mklittlefs-42acb97.tar.gz \
    | tar -xz -C /usr/local/bin --strip-components=1 && chmod +x /usr/local/bin/mklittlefs

# Install esptool
RUN pip3 install esptool

# Make arduino directories readable for non-root users
RUN chmod -R a+rX /root

WORKDIR /src
