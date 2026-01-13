#!/bin/bash
# Quick start script for testing libccp with Portus

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=== libccp + Portus Quick Start ==="
echo

# Check if libccp is built
if [ ! -f "libccp.a" ]; then
    echo "Building libccp..."
    make clean
    make -j$(nproc 2>/dev/null || echo 2)
    echo "✓ libccp built"
else
    echo "✓ libccp already built"
fi

# Check if testbenches are built
if [ ! -f "testbench_with_portus" ]; then
    echo "Building testbenches..."
    make testbench_with_portus
    echo "✓ Testbenches built"
else
    echo "✓ Testbenches already built"
fi

echo
echo "=== Choose an option ==="
echo "1) Run standalone testbench (no Portus - will show warnings)"
echo "2) Run testbench with Portus (requires Portus + algorithm installed)"
echo "3) Install Portus and generic-cong-avoid (Reno/Cubic)"
echo "4) Show Portus setup instructions"
echo
read -p "Enter choice [1-4]: " choice

case $choice in
    1)
        echo
        echo "Running standalone testbench..."
        echo "(You'll see 'no datapath program set' warnings - this is expected)"
        echo
        ./testbench 50 20
        ;;
    2)
        echo
        echo "=== Running with Portus ==="
        echo
        echo "Step 1: Start Portus in a separate terminal:"
        echo "  cd /path/to/generic-cong-avoid"
        echo "  ./target/release/reno --ipc=unix"
        echo
        echo "Step 2: Press Enter once Portus is running..."
        read
        echo "Starting testbench with Portus IPC..."
        ./testbench_with_portus 50 50
        ;;
    3)
        echo
        echo "=== Installing Portus and Algorithms ==="
        echo
        
        # Check if Rust is installed
        if ! command -v cargo &> /dev/null; then
            echo "Rust not found. Installing..."
            curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
            source "$HOME/.cargo/env"
        else
            echo "✓ Rust already installed"
        fi
        
        # Clone and build generic-cong-avoid if not present
        PARENT_DIR="$(dirname "$SCRIPT_DIR")"
        GCA_DIR="$PARENT_DIR/generic-cong-avoid"
        
        if [ ! -d "$GCA_DIR" ]; then
            echo "Cloning generic-cong-avoid..."
            cd "$PARENT_DIR"
            git clone https://github.com/ccp-project/generic-cong-avoid.git
            cd "$GCA_DIR"
        else
            echo "✓ generic-cong-avoid already cloned"
            cd "$GCA_DIR"
        fi
        
        echo "Building Reno and Cubic algorithms..."
        cargo build --release --features=bin --bin reno
        cargo build --release --features=bin --bin cubic
        
        echo
        echo "✓ Installation complete!"
        echo
        echo "Binaries are in: $GCA_DIR/target/release/"
        echo "  - reno"
        echo "  - cubic"
        echo
        echo "To run, open two terminals:"
        echo "Terminal 1: cd $GCA_DIR && ./target/release/reno --ipc=unix"
        echo "Terminal 2: cd $SCRIPT_DIR && ./testbench_with_portus"
        ;;
    4)
        cat << 'EOF'

=== Portus Setup Instructions ===

1. Install Rust (if not installed):
   curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh

2. Clone and build generic-cong-avoid:
   cd /path/to/parent/directory
   git clone https://github.com/ccp-project/generic-cong-avoid.git
   cd generic-cong-avoid
   cargo build --release --bin reno
   cargo build --release --bin cubic

3. Run in two terminals:
   
   Terminal 1 (Portus):
   cd /path/to/generic-cong-avoid
   ./target/release/reno --ipc=unix
   
   Terminal 2 (Testbench):
   cd /path/to/libccp
   ./testbench_with_portus [iterations] [sleep_ms]

4. You should see:
   - Portus installing datapath programs
   - Real congestion control decisions
   - Measurements flowing between datapath and Portus

See PORTUS_SETUP.md for detailed documentation.

EOF
        ;;
    *)
        echo "Invalid choice"
        exit 1
        ;;
esac

echo
echo "Done!"
