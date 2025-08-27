"""
Pytest configuration and shared fixtures for ESP32C6 AT command tests.
"""

import pytest
import os

def pytest_configure(config):
    """Configure pytest with custom settings."""
    # Add custom markers
    config.addinivalue_line("markers", "hardware: mark test as requiring hardware")
    config.addinivalue_line("markers", "wifi: mark test as requiring WiFi")
    config.addinivalue_line("markers", "sd_card: mark test as requiring SD card")
    config.addinivalue_line("markers", "slow: mark test as slow running")
    config.addinivalue_line("markers", "performance: mark test as performance test")

def pytest_collection_modifyitems(config, items):
    """Modify collected test items."""
    # Add hardware marker to all tests since they require ESP32C6 connection
    hardware_marker = pytest.mark.hardware
    for item in items:
        item.add_marker(hardware_marker)
        
        # Add specific markers based on test names
        if "wifi" in item.name.lower():
            item.add_marker(pytest.mark.wifi)
        if "sd" in item.name.lower():
            item.add_marker(pytest.mark.sd_card)
        if "performance" in item.name.lower():
            item.add_marker(pytest.mark.performance)
            item.add_marker(pytest.mark.slow)

def pytest_sessionstart(session):
    """Called after the Session object has been created."""
    print("\n" + "="*60)
    print("ESP32C6 AT Commands Test Session Starting")
    print("="*60)
    print("Make sure:")
    print("1. ESP32C6 is connected via UART1 (GPIO 6/7)")
    print("2. Custom AT firmware is flashed")
    print("3. Configuration is set in config.ini")
    print("4. Hardware is powered on and ready")
    print("="*60)

def pytest_sessionfinish(session, exitstatus):
    """Called after whole test run finished."""
    print("\n" + "="*60)
    print("ESP32C6 AT Commands Test Session Finished")
    if exitstatus == 0:
        print("Status: ALL TESTS PASSED ✓")
    else:
        print("Status: SOME TESTS FAILED ✗")
    print("="*60)

def pytest_runtest_setup(item):
    """Called before each test function execution."""
    # Skip WiFi tests if no credentials provided
    if "wifi" in item.keywords:
        try:
            from simple_test import load_config
            config = load_config()
            if config:
                ssid = config.get('wifi', 'ssid', fallback='')
                if not ssid:
                    pytest.skip("WiFi credentials not configured in config.ini")
            else:
                pytest.skip("Configuration file not available")
        except Exception:
            pytest.skip("Could not check WiFi configuration")

@pytest.fixture(scope="session", autouse=True)
def check_dependencies():
    """Check that required dependencies are available."""
    try:
        import serial  # noqa: F401
    except ImportError:
        pytest.fail("pyserial not installed. Run: pip install pyserial")
    
    # Check if config file exists
    config_file = os.path.join(os.path.dirname(__file__), 'config.ini')
    if not os.path.exists(config_file):
        pytest.fail("config.ini not found. Please create configuration file.")

def pytest_terminal_summary(terminalreporter, exitstatus, config):
    """Add additional summary information."""
    if exitstatus != 0:
        terminalreporter.write_sep("=", "Troubleshooting Tips")
        terminalreporter.write_line("")
        terminalreporter.write_line("If tests failed, check:")
        terminalreporter.write_line("1. Serial port connection (check config.ini)")
        terminalreporter.write_line("2. ESP32C6 is powered on and responsive")
        terminalreporter.write_line("3. Custom AT firmware is properly flashed")
        terminalreporter.write_line("4. UART1 connections (GPIO 6/7)")
        terminalreporter.write_line("5. SD card is inserted and properly wired")
        terminalreporter.write_line("6. WiFi credentials in config.ini (for WiFi tests)")
        terminalreporter.write_line("")
