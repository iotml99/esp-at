#!/usr/bin/env python3
"""
Setup script for ATBN test environment.
Installs dependencies and sets up configuration.
"""

import sys
import subprocess
import importlib.util
from pathlib import Path

def run_command(cmd):
    """Run a command and return success status."""
    try:
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
        if result.returncode == 0:
            print(f"✅ {cmd}")
            return True
        else:
            print(f"❌ {cmd}")
            print(f"Error: {result.stderr}")
            return False
    except Exception as e:
        print(f"❌ {cmd}")
        print(f"Exception: {e}")
        return False

def check_python_version():
    """Check Python version compatibility."""
    if sys.version_info < (3, 7):
        print("❌ Python 3.7 or higher is required")
        return False
    print(f"✅ Python {sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}")
    return True

def install_dependencies():
    """Install required Python packages."""
    print("📦 Installing dependencies...")
    
    packages = [
        "pyserial>=3.5",
        "python-dotenv>=1.0.0", 
        "pytest>=7.0.0",
        "pytest-html>=3.1.0",
        "pytest-timeout>=2.1.0",
        "configparser>=5.3.0"
    ]
    
    success = True
    for package in packages:
        if not run_command(f"pip install {package}"):
            success = False
    
    return success

def setup_config_file():
    """Set up .env configuration file."""
    env_example = Path(__file__).parent / '.env.example'
    env_file = Path(__file__).parent / '.env'
    
    if env_file.exists():
        print("✅ .env file already exists")
        return True
    
    if env_example.exists():
        try:
            import shutil
            shutil.copy(env_example, env_file)
            print("✅ Created .env from .env.example")
            print("📝 Please edit .env with your configuration:")
            print("   - WIFI_SSID and WIFI_PASSWORD")
            print("   - SERIAL_PORT (e.g., COM5 on Windows)")
            print("   - Other test settings as needed")
            return True
        except Exception as e:
            print(f"❌ Failed to copy .env.example: {e}")
            return False
    else:
        print("❌ .env.example not found")
        return False

def create_reports_directory():
    """Create reports directory for test output."""
    reports_dir = Path(__file__).parent / 'reports'
    try:
        reports_dir.mkdir(exist_ok=True)
        print("✅ Created reports directory")
        return True
    except Exception as e:
        print(f"❌ Failed to create reports directory: {e}")
        return False

def verify_installation():
    """Verify the installation by checking if key modules can be found."""
    print("🔍 Verifying installation...")
    
    modules = [
        ("serial", "pyserial"),
        ("dotenv", "python-dotenv"), 
        ("pytest", "pytest")
    ]
    
    for module_name, package_name in modules:
        spec = importlib.util.find_spec(module_name)
        if spec is not None:
            print(f"✅ {package_name} available")
        else:
            print(f"❌ {package_name} not available")
            return False
    
    return True

def main():
    """Main setup function."""
    print("🚀 ESP32 ATBN Test Suite Setup")
    print("=" * 40)
    
    # Check Python version
    if not check_python_version():
        return 1
    
    # Install dependencies
    if not install_dependencies():
        print("❌ Failed to install some dependencies")
        return 1
    
    # Set up configuration
    if not setup_config_file():
        print("❌ Failed to set up configuration")
        return 1
    
    # Create reports directory
    if not create_reports_directory():
        print("❌ Failed to create reports directory") 
        return 1
    
    # Verify installation
    if not verify_installation():
        print("❌ Installation verification failed")
        return 1
    
    print("\n🎉 Setup completed successfully!")
    print("\n📋 Next steps:")
    print("1. Edit .env with your WiFi credentials and serial port")
    print("2. Connect your ESP32 device")
    print("3. Run tests with: python run_atbn_tests.py")
    print("4. Or use pytest: pytest -v")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
