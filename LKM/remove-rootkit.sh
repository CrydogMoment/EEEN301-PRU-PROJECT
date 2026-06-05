MODULE_NAME="rootkit"

# Check if the module name exists in the active modules list
if grep -q "^${MODULE_NAME} " /proc/modules; then
    echo "Module ${MODULE_NAME} found. Removing..."
    sudo rmmod "${MODULE_NAME}"
else
    echo "Module ${MODULE_NAME} is not loaded. Skipping."
fi