import os
import sys
import subprocess

def run(build_type):
    # Common paths
    paths = [
        f"build/{build_type}/Debug/lithos.exe",
        f"build/{build_type}/Release/lithos.exe",
        f"build/{build_type}/lithos.exe",
        f"build/{build_type}/lithos"
    ]
    
    for path in paths:
        if os.path.exists(path):
            print(f"Running {path}...")
            try:
                # Use subprocess.call to pass through exit code
                sys.exit(subprocess.call([path] + sys.argv[2:]))
            except KeyboardInterrupt:
                sys.exit(0)
    
    print(f"Error: Could not find executable for {build_type} build.")
    print(f"Searched in: {paths}")
    sys.exit(1)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python run_game.py <debug|release>")
        sys.exit(1)
    run(sys.argv[1])
