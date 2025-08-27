#!/usr/bin/env python3
"""
Quick status check and summary for ATBN test suite.
Shows environment configuration and readiness.
"""

import sys
from pathlib import Path

# Try to import requirements
def check_requirements():
    """Check if required packages are available."""
    requirements = {
        'pyserial': 'Serial communication with ESP32',
        'dotenv': 'Environment variable management',
        'pytest': 'Test framework'
    }
    
    missing = []
    available = []
    
    for package, description in requirements.items():
        try:
            if package == 'dotenv':
                __import__('dotenv')
            elif package == 'pyserial':
                __import__('serial')
            elif package == 'pytest':
                __import__('pytest')
            available.append((package, description))
        except ImportError:
            missing.append((package, description))
    
    return available, missing

def check_config():
    """Check configuration status."""
    env_file = Path(__file__).parent / '.env'
    env_example = Path(__file__).parent / '.env.example'
    
    config_status = {
        'env_file_exists': env_file.exists(),
        'env_example_exists': env_example.exists(),
    }
    
    # Try to load .env and check key settings
    if config_status['env_file_exists']:
        try:
            from dotenv import load_dotenv
            load_dotenv(env_file)
            import os
            
            config_status['wifi_configured'] = bool(os.getenv('WIFI_SSID'))
            config_status['serial_configured'] = bool(os.getenv('SERIAL_PORT'))
            config_status['wifi_ssid'] = os.getenv('WIFI_SSID', 'Not set')
            config_status['serial_port'] = os.getenv('SERIAL_PORT', 'Not set')
        except ImportError:
            config_status['wifi_configured'] = False
            config_status['serial_configured'] = False
            config_status['wifi_ssid'] = 'dotenv not available'
            config_status['serial_port'] = 'dotenv not available'
    else:
        config_status['wifi_configured'] = False
        config_status['serial_configured'] = False
        config_status['wifi_ssid'] = 'No .env file'
        config_status['serial_port'] = 'No .env file'
    
    return config_status

def check_test_files():
    """Check if test files are present."""
    test_files = [
        'test_atbn_comprehensive.py',
        'test_atbn_advanced.py',
        'test_atbn_missing_commands.py',
        'test_atbn_large_and_soap.py',
        'test_atbn_spec_compliance.py',
        'run_atbn_tests.py',
        'run_complete_atbn_tests.py',
        'setup_atbn_tests.py'
    ]
    
    files_status = {}
    for test_file in test_files:
        file_path = Path(__file__).parent / test_file
        files_status[test_file] = file_path.exists()
    
    return files_status

def main():
    """Main status check function."""
    print("=" * 60)
    print("ESP32 ATBN Test Suite - Status Check")
    print("=" * 60)
    
    # Check Python version
    print(f"🐍 Python Version: {sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}")
    if sys.version_info < (3, 7):
        print("⚠️  Warning: Python 3.7+ recommended")
    else:
        print("✅ Python version OK")
    
    print()
    
    # Check requirements
    print("📦 Package Dependencies:")
    available, missing = check_requirements()
    
    for package, description in available:
        print(f"✅ {package:<12} - {description}")
    
    for package, description in missing:
        print(f"❌ {package:<12} - {description}")
    
    if missing:
        print(f"\n💡 Install missing packages with: pip install {' '.join(pkg for pkg, _ in missing)}")
    
    print()
    
    # Check configuration
    print("⚙️  Configuration:")
    config = check_config()
    
    if config['env_file_exists']:
        print("✅ .env file exists")
        
        if config['wifi_configured']:
            print(f"✅ WiFi configured: {config['wifi_ssid']}")
        else:
            print("❌ WiFi not configured")
        
        if config['serial_configured']:
            print(f"✅ Serial port configured: {config['serial_port']}")
        else:
            print("❌ Serial port not configured")
    else:
        print("❌ .env file missing")
        if config['env_example_exists']:
            print("💡 Copy .env.example to .env and configure")
        else:
            print("❌ .env.example also missing")
    
    print()
    
    # Check test files
    print("📋 Test Files:")
    files = check_test_files()
    
    for filename, exists in files.items():
        status = "✅" if exists else "❌"
        print(f"{status} {filename}")
    
    print()
    
    # Overall readiness
    print("🚦 Readiness Assessment:")
    
    ready_for_basic = (
        len(missing) == 0 and 
        config['env_file_exists'] and 
        config['serial_configured']
    )
    
    ready_for_wifi = ready_for_basic and config['wifi_configured']
    
    if ready_for_wifi:
        print("🟢 READY - All tests can be run")
        print("   Run: python run_atbn_tests.py")
    elif ready_for_basic:
        print("🟡 PARTIALLY READY - Basic tests can be run")
        print("   WiFi tests will be skipped")
        print("   Run: python run_atbn_tests.py")
    else:
        print("🔴 NOT READY - Setup required")
        if missing:
            print("   1. Install missing packages")
        if not config['env_file_exists']:
            print("   2. Create .env file from .env.example")
        if not config['serial_configured']:
            print("   3. Configure SERIAL_PORT in .env")
        if not config['wifi_configured']:
            print("   4. Configure WIFI_SSID and WIFI_PASSWORD in .env")
        print("   Run: python setup_atbn_tests.py")
    
    print()
    print("📚 Available Commands:")
    print("   python setup_atbn_tests.py           - Initial setup")
    print("   python run_atbn_tests.py             - Interactive test runner")
    print("   python run_complete_atbn_tests.py    - Complete test suite runner")
    print("   python test_atbn_comprehensive.py    - Basic test suite")
    print("   python test_atbn_advanced.py         - Advanced test suite")
    print("   python test_atbn_missing_commands.py - Missing commands suite")
    print("   python test_atbn_large_and_soap.py   - Large files & SOAP tests")
    print("   python test_atbn_spec_compliance.py  - Specification compliance")
    print("   pytest -v                            - Run all tests with pytest")
    print("   pytest -m wifi                       - Run only WiFi-dependent tests")
    print("   pytest -m sd_card                    - Run only SD card tests")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
