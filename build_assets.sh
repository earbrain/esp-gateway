#!/bin/bash
# Build portal and generate embedded .S files for ESP32 firmware
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PORTAL_DIR="${SCRIPT_DIR}/portal"
PORTAL_DIST="${SCRIPT_DIR}/portal/dist"
OUTPUT_DIR="${SCRIPT_DIR}/portal_assets"

# Build portal
echo "Building portal..."
if [ ! -d "${PORTAL_DIR}/node_modules" ]; then
    echo "  Installing npm dependencies..."
    (cd "${PORTAL_DIR}" && npm install)
fi

echo "  Running npm build..."
(cd "${PORTAL_DIR}" && npm run build)

if [ ! -d "${PORTAL_DIST}" ]; then
    echo "Error: Portal build failed. ${PORTAL_DIST} directory not created."
    exit 1
fi

echo "  Portal build complete!"
echo ""

# Check if ESP-IDF is available
if [ -z "${IDF_PATH}" ]; then
    echo "Error: IDF_PATH is not set. Please source ESP-IDF environment first."
    echo "Run: . \$HOME/esp/esp-idf/export.sh"
    exit 1
fi

DATA_EMBED_SCRIPT="${IDF_PATH}/tools/cmake/scripts/data_file_embed_asm.cmake"

if [ ! -f "${DATA_EMBED_SCRIPT}" ]; then
    echo "Error: data_file_embed_asm.cmake not found at ${DATA_EMBED_SCRIPT}"
    exit 1
fi

# Create output directory
mkdir -p "${OUTPUT_DIR}"

# List of assets to embed
ASSETS=(
    "index.html"
    "app.js"
    "assets/index.css"
)

echo "Generating portal asset .S files..."

for asset in "${ASSETS[@]}"; do
    asset_path="${PORTAL_DIST}/${asset}"
    asset_name=$(basename "${asset}")
    output_file="${OUTPUT_DIR}/${asset_name}.S"

    if [ ! -f "${asset_path}" ]; then
        echo "Error: ${asset_path} not found. Run 'npm run build' in the portal directory first."
        exit 1
    fi

    echo "  Embedding ${asset}..."
    cmake -D DATA_FILE="${asset_path}" \
          -D SOURCE_FILE="${output_file}" \
          -D FILE_TYPE=TEXT \
          -P "${DATA_EMBED_SCRIPT}"
done

echo "Done! Generated .S files in ${OUTPUT_DIR}/"
echo ""
echo "Files generated:"
ls -lh "${OUTPUT_DIR}"
