#!/usr/bin/env bash

# Exit immediately if a command exits with a non-zero status,
# if an undefined variable is used, or if a piped command fails.
set -euo pipefail

# Display usage instructions
print_usage() {
    echo "Usage: ${0##*/} <module name> [optional class name]"
    echo "Description: Creates a new module in the source code with included boilerplate"
    echo ""
    echo "Options:"
    echo "  -h, --help    Show this help message and exit"
}

# Check if help is requested
if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    print_usage
    exit 0
fi

# Validate that exactly one argument is provided
if [ "$#" -ne 1 ] && [ "$#" -ne 2 ]; then
    echo "Error: Invalid number of arguments." >&2
    print_usage >&2
    exit 1
fi

# Assign the argument to a meaningful variable name
readonly MOD_NAME="$1"
HAS_CLASS=0
if [ -n "${2:-}" ]; then
    readonly CLASS_NAME="$2"
    readonly CLASS_REGISTER="ClassDB::register_class<$CLASS_NAME>();"
    readonly CLASS_INCLUDE="#include \"$MOD_NAME.h\""
    HAS_CLASS=1
else
    readonly CLASS_NAME=""
    readonly CLASS_REGISTER="// Register classes here"
    readonly CLASS_INCLUDE=""
fi

if [ -d "modules/$MOD_NAME" ]; then
    echo "Error: Module $MOD_NAME already exists."
    exit 1
fi

mkdir -p "modules/$MOD_NAME"

echo "
def can_build(env, platform):
    return True

def configure(env):
    pass
" > "modules/$MOD_NAME/config.py"

echo "
Import('env')

module_env = env.Clone()
module_env.add_source_files(env.modules_sources, \"*.cpp\")
" > "modules/$MOD_NAME/SCsub"

echo "
#pragma once

#include \"modules/register_module_types.h\"

void initialize_${MOD_NAME}_module(ModuleInitializationLevel p_level);
void uninitialize_${MOD_NAME}_module(ModuleInitializationLevel p_level);
" > "modules/$MOD_NAME/register_types.h"

echo "
#include \"register_types.h\"
#include \"core/object/class_db.h\"
$CLASS_INCLUDE

void initialize_${MOD_NAME}_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    $CLASS_REGISTER
}

void uninitialize_${MOD_NAME}_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    // Perform cleanup if necessary.
}
" > "modules/$MOD_NAME/register_types.cpp"

if [[ $HAS_CLASS -eq 1 ]]; then
    echo "
#pragma once

#include \"core/object/ref_counted.h\"

class $CLASS_NAME : public RefCounted {
    GDCLASS($CLASS_NAME, RefCounted);

private:
    // Private vars/methods

protected:
    static void _bind_methods();
    // Protected vars/methods

public:
    // Public vars/methods
    // int add(int a, int b) // example

    $CLASS_NAME();
    // ~$CLASS_NAME(); // Destructor
};
" > "modules/$MOD_NAME/$MOD_NAME.h"

    echo "
#include \"$MOD_NAME.h\"

// int $CLASS_NAME::add(int a, int b) {
//     return a + b;
// }

void $CLASS_NAME::_bind_methods() {
    // bind gdscript visible methods here
    // ClassDB::bind_method(D_METHOD("add", "a", "b"), &$CLASS_NAME::add);
}

$CLASS_NAME::$CLASS_NAME() {
    // Initialize class state when created
}
// $CLASS_NAME::~$CLASS_NAME() {
//     // De-initialize class state when destroyed
// }
" > "modules/$MOD_NAME/$MOD_NAME.cpp"
fi

echo "Module $MOD_NAME created!"