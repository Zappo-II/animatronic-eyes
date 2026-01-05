# Animatronic Eyes
# Copyright (c) 2025 Zappo-II
# Licensed under CC BY-NC-SA 4.0
# https://github.com/Zappo-II/animatronic-eyes
#
# Build system for ESP32 firmware and UI

DOCKER_IMAGE = animatronic-eyes-builder
PORT ?= /dev/ttyUSB0
BAUD ?= 115200
BUILD_DIR = build
SKETCH_NAME = animatronic-eyes
UID := $(shell id -u)
GID := $(shell id -g)
DISCOVER_FILTER ?= animatronic-eyes|LookIntoMyEyes

# Docker run command with source mounted (directory must match sketch name)
DOCKER_RUN = docker run --rm -v $(PWD):/src/$(SKETCH_NAME) -w /src/$(SKETCH_NAME) $(DOCKER_IMAGE)
DOCKER_RUN_TTY = docker run --rm -it -v $(PWD):/src/$(SKETCH_NAME) -w /src/$(SKETCH_NAME) --device=$(PORT) $(DOCKER_IMAGE)

.PHONY: docker build build-firmware build-ui flash flash-ui flash-all monitor clean release help discover deploy-firmware deploy-ui

help:
	@echo "Animatronic Eyes - Build System"
	@echo ""
	@echo "Targets:"
	@echo "  docker                   - Build the Docker image (one-time)"
	@echo "  build                    - Compile firmware and ui.bin"
	@echo "  flash                    - Flash firmware to ESP32 via USB"
	@echo "  flash-ui                 - Flash ui.bin to ESP32 via USB"
	@echo "  flash-all                - Flash both firmware and ui.bin via USB"
	@echo "  discover                 - Find devices on network (mDNS)"
	@echo "  deploy-firmware          - Upload firmware via OTA"
	@echo "  deploy-ui                - Upload ui.bin via OTA"
	@echo "  monitor                  - Open serial monitor (picocom)"
	@echo "  clean                    - Remove build directory"
	@echo "  release                  - Create GitHub release (requires V=x.y.z)"
	@echo ""
	@echo "Variables:"
	@echo "  PORT                     - Serial port (default: $(PORT))"
	@echo "  BAUD                     - Baud rate (default: $(BAUD))"
	@echo "  DEVICE                   - Device IP/hostname (auto-read from .device)"
	@echo "  PIN                      - Admin PIN for OTA (optional)"
	@echo "  V                        - Version for release (e.g., V=1.0.1)"
	@echo "  DISCOVER_FILTER          - mDNS filter pattern (default: $(DISCOVER_FILTER))"
	@echo ""
	@echo "Requirements (Arch/Manjaro: pacman -S docker picocom github-cli avahi):"
	@echo "  docker                   - Build environment (Target: docker, build, flash...)"
	@echo "  picocom                  - Serial monitor (Target: monitor)"
	@echo "  gh                       - GitHub CLI (Target: release)"
	@echo "  avahi-browse             - mDNS discovery (Target: discover)"
	@echo "  curl                     - HTTP client (Target: deploy-...)"
	@echo ""
	@echo "Get started:"
	@echo "  1. make docker           # Build Docker image (one-time)"
	@echo "  2. make build            # Compile firmware and ui.bin"
	@echo "  3. make flash-all        # Flash to ESP32"
	@echo "  4. make monitor          # Serial debug"
	@echo ""
	@echo "Release workflow:"
	@echo "  make build && make release V=1.0.1"
	@echo ""
	@echo "OTA deploy workflow:"
	@echo "  make discover            # Find device, save IP to .device"
	@echo "  make deploy-firmware     # Upload firmware (uses .device)"
	@echo "  make deploy-ui PIN=1234  # Upload UI with PIN"

# Build Docker image
docker:
	docker build -t $(DOCKER_IMAGE) .

# Build everything
build: build-firmware build-ui
	@echo ""
	@echo "Build complete:"
	@ls -lh $(BUILD_DIR)/*.bin

# Compile firmware
build-firmware:
	@mkdir -p $(BUILD_DIR)
	$(DOCKER_RUN) arduino-cli compile \
		--fqbn esp32:esp32:esp32 \
		--output-dir $(BUILD_DIR) \
		.
	$(DOCKER_RUN) chown -R $(UID):$(GID) $(BUILD_DIR)

# Build LittleFS image
build-ui:
	@mkdir -p $(BUILD_DIR)
	$(DOCKER_RUN) mklittlefs \
		-c data \
		-p 256 \
		-b 4096 \
		-s 0x160000 \
		$(BUILD_DIR)/ui.bin
	$(DOCKER_RUN) chown -R $(UID):$(GID) $(BUILD_DIR)

# Flash firmware
flash:
	$(DOCKER_RUN_TTY) esptool.py \
		--chip esp32 \
		--port $(PORT) \
		--baud 460800 \
		write_flash 0x10000 $(BUILD_DIR)/animatronic-eyes.ino.bin

# Flash UI
flash-ui:
	$(DOCKER_RUN_TTY) esptool.py \
		--chip esp32 \
		--port $(PORT) \
		--baud 460800 \
		write_flash 0x290000 $(BUILD_DIR)/ui.bin

# Flash both
flash-all: flash flash-ui

# Serial monitor
monitor:
	@echo "Using picocom as serial monitor"
	@echo "- Exit with Ctrl+A then Ctrl+X."
	picocom $(PORT) -b $(BAUD)

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)

# Create GitHub release
release:
ifndef V
	$(error Version required. Usage: make release V=1.0.1)
endif
	@echo "Creating release v$(V)..."
	git tag -a $(V) -m "Release v$(V)"
	git push origin $(V)
	gh release create $(V) \
		$(BUILD_DIR)/animatronic-eyes.ino.bin \
		$(BUILD_DIR)/ui.bin \
		--title "v$(V)" \
		--generate-notes
	@echo ""
	@echo "Release v$(V) created: https://github.com/Zappo-II/animatronic-eyes/releases/tag/$(V)"

# Discover devices on network via mDNS (saves first found IP to .device)
discover:
	@echo "Searching for devices matching '$(DISCOVER_FILTER)'..."
	@avahi-browse -t -r _http._tcp 2>/dev/null | grep -E -A3 "$(DISCOVER_FILTER)" > .device.tmp; \
		cat .device.tmp; \
		grep "address" .device.tmp | head -1 | sed 's/.*\[\(.*\)\]/\1/' > .device; \
		rm -f .device.tmp; \
		if [ -s .device ]; then echo ""; echo "Saved to .device: $$(cat .device)"; else echo "No devices found. Use DEVICE=<ip> or create .device manually."; rm -f .device; fi

# Read device from .device file if not specified
DEVICE ?= $(shell cat .device 2>/dev/null)

# Deploy firmware via OTA
deploy-firmware:
	@if [ -z "$(DEVICE)" ]; then echo "DEVICE required. Run 'make discover' first or specify DEVICE=192.168.1.100"; exit 1; fi
	@echo "Deploying firmware to $(DEVICE)..."
ifdef PIN
	@echo "Authenticating with PIN..."
	@curl -s -X POST -H "Content-Type: application/json" -d '{"pin":"$(PIN)"}' http://$(DEVICE)/api/unlock | grep -q "OK" && echo "Authenticated" || (echo "Authentication failed"; exit 1)
endif
	@echo "Uploading firmware... (device reboots after upload, may take a moment)"
	@HTTP_CODE=$$(curl -s --max-time 30 -o .deploy.tmp -w "%{http_code}" -X POST -F "firmware=@$(BUILD_DIR)/animatronic-eyes.ino.bin" http://$(DEVICE)/update 2>&1); \
		CURL_EXIT=$$?; \
		RESPONSE=$$(cat .deploy.tmp 2>/dev/null); \
		rm -f .deploy.tmp; \
		if echo "$$RESPONSE" | grep -q "FAIL\|Admin lock"; then \
			echo "FAILED: $$RESPONSE"; exit 1; \
		elif [ "$$HTTP_CODE" = "200" ] || [ $$CURL_EXIT -eq 52 ] || [ $$CURL_EXIT -eq 56 ] || [ $$CURL_EXIT -eq 28 ]; then \
			echo "Firmware deployed. Device will reboot."; \
		else \
			echo "FAILED (HTTP $$HTTP_CODE, curl exit $$CURL_EXIT): $$RESPONSE"; exit 1; \
		fi

# Deploy UI via OTA
deploy-ui:
	@if [ -z "$(DEVICE)" ]; then echo "DEVICE required. Run 'make discover' first or specify DEVICE=192.168.1.100"; exit 1; fi
	@echo "Deploying UI to $(DEVICE)..."
ifdef PIN
	@echo "Authenticating with PIN..."
	@curl -s -X POST -H "Content-Type: application/json" -d '{"pin":"$(PIN)"}' http://$(DEVICE)/api/unlock | grep -q "OK" && echo "Authenticated" || (echo "Authentication failed"; exit 1)
endif
	@echo "Uploading UI... (device reboots after upload, may take a moment)"
	@HTTP_CODE=$$(curl -s --max-time 30 -o .deploy.tmp -w "%{http_code}" -X POST -F "file=@$(BUILD_DIR)/ui.bin" http://$(DEVICE)/api/upload-ui 2>&1); \
		CURL_EXIT=$$?; \
		RESPONSE=$$(cat .deploy.tmp 2>/dev/null); \
		rm -f .deploy.tmp; \
		if echo "$$RESPONSE" | grep -q "FAIL\|Admin lock"; then \
			echo "FAILED: $$RESPONSE"; exit 1; \
		elif [ "$$HTTP_CODE" = "200" ] || [ $$CURL_EXIT -eq 52 ] || [ $$CURL_EXIT -eq 56 ] || [ $$CURL_EXIT -eq 28 ]; then \
			echo "UI deployed. Device will reboot."; \
		else \
			echo "FAILED (HTTP $$HTTP_CODE, curl exit $$CURL_EXIT): $$RESPONSE"; exit 1; \
		fi
