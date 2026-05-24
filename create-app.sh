#!/usr/bin/env bash
# create-app.sh - Creates a basic Kastle 2 app.
# Requires: bash 3.2+, python3

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APPS_DIR="$SCRIPT_DIR/code/src/apps"
VSCODE_DIR="$SCRIPT_DIR/.vscode"
TODAY="$(date +"%Y-%m-%d")"
AUTHOR="$(id -un 2>/dev/null || echo "unknown")"

# -----------------------------------------------------------------------------
# Feature questions - add more entries here to extend the wizard.
#
# Parallel arrays (must stay in sync):
#   FEATURE_MARKERS      : Template block marker used in // IF $MARKER blocks
#   FEATURE_QUESTIONS    : Question shown to the user
#   FEATURE_INCLUDE_WHEN : "y" = include block when user answers yes
#                          "n" = include block when user answers no
# -----------------------------------------------------------------------------
FEATURE_MARKERS=("DISABLE_BASE_FEATURES")
FEATURE_QUESTIONS=("Disable 'base features'? Leave empty if you aren't sure.")
FEATURE_INCLUDE_WHEN=("y")
FEATURE_DEFAULT=("n")

# -----------------------------------------------------------------------------
# Utilities
# -----------------------------------------------------------------------------

pascal_to_kebab() {
    echo "$1" | sed -E 's/([A-Z])/-\1/g' | sed 's/^-//' | tr '[:upper:]' '[:lower:]'
}

validate_name() {
    if [ -z "$1" ]; then
        echo "Error: App name cannot be empty." >&2
        return 1
    fi
    if ! echo "$1" | grep -qE '^[A-Za-z]+$'; then
        echo "Error: App name must contain only letters (e.g. MySynth)." >&2
        return 1
    fi
    if ! echo "$1" | grep -qE '^[A-Z]'; then
        echo "Error: App name must start with an uppercase letter (e.g. MySynth)." >&2
        return 1
    fi
    return 0
}

# -----------------------------------------------------------------------------
# Help
# -----------------------------------------------------------------------------

usage() {
    cat <<EOF
Usage: $(basename "$0") [OPTIONS] [APP_NAME]

Creates a new basic Kastle 2 app.

Arguments:
  APP_NAME    Name of the app in PascalCase, letters only (e.g. MySynth).
              If omitted, the script will prompt for it interactively.

Options:
  -h, --help  Show this help message and exit.

What this script does:
  1. Creates code/src/apps/<APP_NAME>/ with CMakeLists.txt, a .hpp, .cpp, and main.cpp.
  2. Adds the new build target to .vscode/launch.json and .vscode/tasks.json.
  3. Runs configure.sh to regenerate the build system.

Examples:
  $(basename "$0") MySynth
  $(basename "$0")
EOF
}


# -----------------------------------------------------------------------------
# Begin
# -----------------------------------------------------------------------------

echo ""
echo "Welcome to the Kastle 2 app creation wizard!"

# -----------------------------------------------------------------------------
# Collect app name (from first argument or interactively)
# -----------------------------------------------------------------------------

_initial_name=""
if [ $# -ge 1 ]; then
    case "$1" in
        -h|--help) usage; exit 0 ;;
    esac
    _initial_name="$1"
fi

APP_NAME=""
while true; do
    if [ -n "$_initial_name" ]; then
        APP_NAME="$_initial_name"
        _initial_name=""
    else
        echo ""
        printf "Enter app name (PascalCase, letters only, e.g. MySynth): "
        read -r APP_NAME
    fi
    validate_name "$APP_NAME" || continue
    if [ -d "$APPS_DIR/$APP_NAME" ]; then
        echo "Error: '$APP_NAME' already exists, choose a different name." >&2
        continue
    fi
    break
done

KEBAB_NAME="$(pascal_to_kebab "$APP_NAME")"
APP_CLASS_NAME="App${APP_NAME}"
APP_DIR="$APPS_DIR/$APP_NAME"

echo ""
echo "Creating app '$APP_NAME' ($KEBAB_NAME) ..."
echo ""

# -----------------------------------------------------------------------------
# Collect feature selections interactively
# -----------------------------------------------------------------------------

FEATURE_INCLUDE=()
for _i in "${!FEATURE_MARKERS[@]}"; do
    FEATURE_INCLUDE+=("0")
done

for i in "${!FEATURE_MARKERS[@]}"; do
    marker="${FEATURE_MARKERS[$i]}"
    question="${FEATURE_QUESTIONS[$i]}"
    include_when="${FEATURE_INCLUDE_WHEN[$i]}"
    default="${FEATURE_DEFAULT[$i]}"
    while true; do
        printf "%s (y/n) [%s]: " "$question" "$default"
        read -r answer
        answer="$(echo "$answer" | tr '[:upper:]' '[:lower:]')"
        [ -z "$answer" ] && answer="$default"
        if [ "$answer" = "y" ] || [ "$answer" = "n" ]; then break; fi
        echo "Please answer y or n."
    done
    if [ "$answer" = "$include_when" ]; then
        FEATURE_INCLUDE[$i]="1"
    fi
done

echo ""

FEATURE_ARGS=()
for i in "${!FEATURE_MARKERS[@]}"; do
    FEATURE_ARGS+=("${FEATURE_MARKERS[$i]}=${FEATURE_INCLUDE[$i]}")
done

# -----------------------------------------------------------------------------
# Template engine
#   process_blocks  - removes or keeps // IF $MARKER … // ENDIF blocks
#   substitute_vars - replaces app-name placeholders and $YOUR_NAME / $TODAY_DATE
#   render_template - pipes template through both stages into a file
# -----------------------------------------------------------------------------

if ! command -v python3 >/dev/null 2>&1; then
    echo "Error: python3 is required but not found in PATH." >&2
    exit 1
fi

# Python: reads template from stdin, processes conditional blocks.
# Args: MARKER=1 (include block) or MARKER=0 (remove block).
PYTHON_BLOCK_PROCESSOR='
import re, sys
content = sys.stdin.read()
for arg in sys.argv[1:]:
    marker, val = arg.split("=", 1)
    include = val == "1"
    pattern = r"[ \t]*// IF \$" + re.escape(marker) + r"[^\n]*\n(.*?)[ \t]*// ENDIF[^\n]*\n"
    if include:
        content = re.sub(pattern, r"\1", content, flags=re.DOTALL)
    else:
        content = re.sub(pattern, "", content, flags=re.DOTALL)
sys.stdout.write(content)
'

process_blocks() {
    python3 -c "$PYTHON_BLOCK_PROCESSOR" "$@"
}

substitute_vars() {
    sed \
        -e 's/\$APP_CLASS_NAME/'"App${APP_NAME}"'/g' \
        -e 's/\$APP_NAME_PASCAL_CASE/'"${APP_NAME}"'/g' \
        -e 's/\$APP_NAME_KEBAB_CASE/'"${KEBAB_NAME}"'/g' \
        -e 's/\$YOUR_NAME/'"${AUTHOR}"'/g' \
        -e 's/\$TODAY_DATE/'"${TODAY}"'/g'
}

render_template() {
    # $1 = template string; remaining args = feature flag args (MARKER=0/1)
    local tmpl="$1"
    shift
    printf '%s\n' "$tmpl" | process_blocks "$@" | substitute_vars
}

# -----------------------------------------------------------------------------
# Embedded file templates
# Note: placeholders $APP_CLASS_NAME, $APP_NAME_PASCAL_CASE, $APP_NAME_KEBAB_CASE, $YOUR_NAME, $TODAY_DATE, etc.
# are replaced at render time by substitute_vars.
# -----------------------------------------------------------------------------

TPL_CMAKE=$(
    cat <<'TEMPLATE_EOF'
# App definition
create_kastle2_app(
    APP_NAME "$APP_NAME_KEBAB_CASE"
    APP_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/$APP_CLASS_NAME.cpp
)
TEMPLATE_EOF
)

TPL_HPP=$(
    cat <<'TEMPLATE_EOF'
#pragma once

#include <cstddef>
#include <cstdint>
#include "common/core/App.hpp"
#include "common/core/Kastle2.hpp"
#include "common/dsp/math/qmath.hpp"

namespace kastle2
{

/**
 * @class $APP_CLASS_NAME
 * @ingroup apps
 * @brief App description
 * @author $YOUR_NAME
 * @date $TODAY_DATE
 */
class $APP_CLASS_NAME : public virtual App
{
public:
    /**
     * @brief Initializes all the parameters, memory, etc.
     */
    void Init();

    /**
     * @brief Deinitializes the app, stops all effects, etc.
     */
    void DeInit();

    /**
     * @brief Called each interrupt loop. Implements all the audio processing.
     * @param input Input buffer.
     * @param output Output buffer.
     * @param size Number of sample pairs in the buffer (real size of the buffer is 2*size).
     */
    FASTCODE void AudioLoop(q15_t *input, q15_t *output, size_t size);

    /**
     * @brief Called each time AudioLoop isn't busy.
     */
    void UiLoop();

    /**
     * @brief Called when the app is first loaded - initializes the memory values.
     */
    void MemoryInitialization() {}

    /**
     * @brief Returns the app ID.
     * @return The app ID.
     */
    uint8_t GetId()
    {
        return Kastle2::kDefaultAppId;
    }

private:
    // Put private variables here
};
}
TEMPLATE_EOF
)

TPL_CPP=$(
    cat <<'TEMPLATE_EOF'
#include "$APP_CLASS_NAME.hpp"
#include "common/core/Kastle2.hpp"
#include "common/utils.hpp"

using namespace kastle2;

void $APP_CLASS_NAME::Init()
{
    // IF $DISABLE_BASE_FEATURES
    // Disable all "base features"
    Kastle2::base.SetFeatureEnabled(Base::Feature::BASE, false);
    // ENDIF

}

void $APP_CLASS_NAME::DeInit()
{
}

FASTCODE void $APP_CLASS_NAME::AudioLoop(q15_t *input, q15_t *output, size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        // read
        q15_t left = input[2 * i];
        q15_t right = input[2 * i + 1];

        // code that runs each sample

        // output
        output[2 * i] = left;
        output[2 * i + 1] = right;
    }
}

void $APP_CLASS_NAME::UiLoop()
{
    Kastle2::hw.SetLed(Hardware::Led::LED_1, WS2812::GREEN);
    Kastle2::hw.SetLed(Hardware::Led::LED_2, WS2812::BLUE);
    // IF $DISABLE_BASE_FEATURES
    Kastle2::hw.SetLed(Hardware::Led::LED_3, WS2812::PINK);
    // ENDIF
}
TEMPLATE_EOF
)

TPL_MAIN=$(
    cat <<'TEMPLATE_EOF'

#include "common/core/Kastle2.hpp"
#include "apps/$APP_NAME_PASCAL_CASE/$APP_CLASS_NAME.hpp"

using namespace kastle2;

/**
 * @file main.cpp
 * @brief Main entry point for Kastle 2 firmware.
 * @author $YOUR_NAME
 * @date $TODAY_DATE
 */

$APP_CLASS_NAME app;

static void process_audio(q15_t *input, q15_t *output, size_t size)
{
    app.AudioLoop(input, output, size);
}

static void midi_callback(midi::Message *msg)
{
    app.MidiCallback(msg);
}

int main()
{
    // Initializes the hardware
    Kastle2::Init();

    // Register it with the Kastle2
    // Clears the EEPROM app space if the ID is different
    Kastle2::RegisterApp(&app);

    // Initialize the app
    app.Init();

    // Start I2S
    Kastle2::StartAudio(process_audio);

    // Set the MIDI callback
    Kastle2::SetAppMidiCallback(midi_callback);

    // Infinite program loop
    while (true)
    {
        Kastle2::ReadInputs();
        app.UiLoop();
    }

    return 0;
}
TEMPLATE_EOF
)

# -----------------------------------------------------------------------------
# Create app directory and render files
# -----------------------------------------------------------------------------

mkdir -p "$APP_DIR"

render_template "$TPL_CMAKE" "${FEATURE_ARGS[@]}" >"$APP_DIR/CMakeLists.txt"
render_template "$TPL_HPP" "${FEATURE_ARGS[@]}" >"$APP_DIR/${APP_CLASS_NAME}.hpp"
render_template "$TPL_CPP" "${FEATURE_ARGS[@]}" >"$APP_DIR/${APP_CLASS_NAME}.cpp"
render_template "$TPL_MAIN" "${FEATURE_ARGS[@]}" >"$APP_DIR/main.cpp"

echo "Created: code/src/apps/${APP_NAME}/"
echo "  CMakeLists.txt"
echo "  ${APP_CLASS_NAME}.hpp"
echo "  ${APP_CLASS_NAME}.cpp"
echo "  main.cpp"

# -----------------------------------------------------------------------------
# Update .vscode/launch.json and .vscode/tasks.json
#
# Surgically inserts the new kebab-case target into the "target" pickString
# options array, preserving all other content (comments, trailing commas, etc.)
# -----------------------------------------------------------------------------

PYTHON_JSON_UPDATER='
import sys, re

filename = sys.argv[1]
new_target = sys.argv[2]

with open(filename, "r") as f:
    lines = f.readlines()

if any("\"" + new_target + "\"" in line for line in lines):
    print("  (already present in " + filename + ")")
    sys.exit(0)

in_target = False
in_options = False
insert_idx = -1
insert_indent = "                "

for i, line in enumerate(lines):
    stripped = line.strip()
    if "\"id\": \"target\"" in line:
        in_target = True
    if in_target and "\"options\"" in line and "[" in line:
        in_options = True
    if in_options:
        m = re.match(r"(\s*)\],?", line)
        if m and stripped in ("]", "],"):
            insert_indent = m.group(1) + "    "
            insert_idx = i
            in_options = False
            in_target = False
            break

if insert_idx < 0:
    print("Warning: could not find target options array in " + filename, file=sys.stderr)
    sys.exit(1)

prev = insert_idx - 1
while prev >= 0 and not lines[prev].strip():
    prev -= 1
if prev >= 0 and lines[prev].rstrip("\n\r").rstrip().endswith("\""):
    lines[prev] = lines[prev].rstrip("\n\r").rstrip() + ",\n"

lines.insert(insert_idx, insert_indent + "\"" + new_target + "\"\n")

with open(filename, "w") as f:
    f.writelines(lines)

print("  Added \"" + new_target + "\" to " + filename)
'

update_vscode_json() {
    local file="$1"
    if [ -f "$file" ]; then
        python3 -c "$PYTHON_JSON_UPDATER" "$file" "$KEBAB_NAME"
    else
        echo "  Warning: $file not found, skipping." >&2
    fi
}

echo ""
echo "Updating .vscode JSON files ..."
update_vscode_json "$VSCODE_DIR/launch.json"
update_vscode_json "$VSCODE_DIR/tasks.json"

echo ""
echo "Done! App '$APP_NAME' created successfully."
echo "Running configure.sh ..."
echo ""
"$SCRIPT_DIR/configure.sh"
echo ""
echo "You can now go to code/build and run 'make $KEBAB_NAME' to build your app."
echo ""

