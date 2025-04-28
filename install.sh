#!/bin/bash

echo "📦 Starting installation for calculate_pi..."

# Detect OS
OS_TYPE="$(uname)"
IS_MAC=false
IS_DEBIAN=false

if [[ "$OS_TYPE" == "Darwin" ]]; then
    IS_MAC=true
elif [[ -f /etc/debian_version ]]; then
    IS_DEBIAN=true
fi

# Prompt user before installing extra dependencies
confirm_install() {
    echo -n "$1 (y/n)? "
    read -r response
    case "$response" in
        [Yy]*) return 0 ;;
        *) echo "❌ Aborted."; exit 1 ;;
    esac
}

# Debian Linux Setup
if $IS_DEBIAN; then
    echo "🔍 Detected Debian-based Linux"

    echo "Updating package list..."
    sudo apt update

    echo "Checking required packages..."
    for pkg in libgmp-dev libmpfr-dev lm-sensors; do
        if ! dpkg -s "$pkg" &> /dev/null; then
            confirm_install "Install missing package: $pkg"
            sudo apt install -y "$pkg"
        else
            echo "✅ $pkg already installed."
        fi
    done

#    echo
#    echo "⚡ Energy Sensor Setup"
#    echo "Would you like to apply world-readable permissions to energy sensors"
#    echo "so that running calculate_pi won't require sudo? (y/n)"
#    read -r reply
#    if [[ "$reply" =~ ^[Yy]$ ]]; then
#        echo "🔧 Applying temporary permissions (until next reboot)..."
#        sudo find -L /sys/class/powercap/ -name "energy_uj" -exec chmod o+r {} + 2>/dev/null
#        echo "✅ Temporary energy sensor permissions updated!"
#
#        echo
#        echo "🛡️ Would you like to make the permissions permanent across reboots with a udev rule (recommended)? (y/n)"
#        read -r udev_reply
#        if [[ "$udev_reply" =~ ^[Yy]$ ]]; then
#            echo "🔧 Installing udev rule..."
#            sudo bash -c 'cat > /etc/udev/rules.d/60-calculate-pi-energy.rules <<EOF
# Allow world-read access to energy_uj files under powercap subsystem
#SUBSYSTEM=="powercap", ATTR{energy_uj}=="*", MODE="0444"
#EOF'
#            echo "✅ Udev rule installed."
#
#            echo "🔄 Reloading udev rules..."
#            sudo udevadm control --reload-rules
#            sudo udevadm trigger
#            echo "✅ Permissions will now persist after reboot!"
#        else
#            echo "⚠️ Energy sensor permissions will need to be manually re-applied after each reboot."
#        fi
#    else
#        echo "⚠️ You can still run calculate_pi, but you must use sudo if you want power monitoring."
#    fi

# macOS Setup
elif $IS_MAC; then
    echo "🍏 Detected macOS (Intel or Apple Silicon)"

    if ! command -v brew &> /dev/null; then
        confirm_install "Homebrew not found. Install Homebrew"
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
        eval "$(/opt/homebrew/bin/brew shellenv || /usr/local/bin/brew shellenv)"
    else
        echo "✅ Homebrew is installed."
    fi

    echo "Checking required packages with Homebrew..."
    for pkg in gmp mpfr; do
        if ! brew list "$pkg" &> /dev/null; then
            confirm_install "Install missing package: $pkg"
            brew install "$pkg"
        else
            echo "✅ $pkg already installed."
        fi
    done

else
    echo "⚠️ Unsupported OS. This installer only supports:"
    echo "- Debian-based Linux"
    echo "- macOS (Intel or Apple Silicon)"
    exit 1
fi

# Build the program
echo "🔧 Building the project..."
make

# Install binary
echo "📂 Installing to /usr/local/bin (sudo required)..."
sudo make install

echo "✅ Installation complete!"

