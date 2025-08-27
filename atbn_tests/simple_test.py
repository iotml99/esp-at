#!/usr/bin/env python3
"""
Pytest-enabled test script for ESP32C6 AT commands.
This is a simplified version showing basic usage patterns with pytest integration.

Run with: pytest simple_test.py -v
Or run directly: python simple_test.py
"""

import serial
import time
import configparser
import os
import sys
import pytest
from typing import Optional, Tuple

def load_config():
    """Load configuration from config.ini file."""
    config = configparser.ConfigParser()
    config_file = os.path.join(os.path.dirname(__file__), 'config.ini')
    
    if not os.path.exists(config_file):
        print(f"Configuration file not found: {config_file}")
        print("Using default settings...")
        return None
    
    try:
        config.read(config_file)
        return config
    except Exception as e:
        print(f"Error reading configuration: {e}")
        print("Using default settings...")
        return None

class SimpleATTester:
    """Simple AT command tester for ESP32C6 with pytest integration."""
    
    def __init__(self, port='COM3', baudrate=115200, timeout=5):
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.conn = None
        
    def connect(self):
        """Connect to the ESP32C6."""
        try:
            self.conn = serial.Serial(self.port, self.baudrate, timeout=self.timeout)
            print(f"Connected to {self.port} at {self.baudrate} baud")
            return True
        except Exception as e:
            print(f"Connection failed: {e}")
            return False
    
    def send_at_command(self, command) -> Tuple[bool, str]:
        """
        Send an AT command and return success status and response.
        
        Returns:
            Tuple of (success, response_text)
        """
        if not self.conn:
            print("Not connected!")
            return False, "Not connected"
            
        print(f"\nSending: {command}")
        self.conn.write(f"{command}\r\n".encode())
        
        # Read response using configured timeout
        start_time = time.time()
        response_lines = []
        
        while time.time() - start_time < self.timeout:
            if self.conn.in_waiting > 0:
                line = self.conn.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    print(f"Response: {line}")
                    response_lines.append(line)
                    if line in ['OK', 'ERROR', 'SEND OK', 'SEND FAIL']:
                        success = line in ['OK', 'SEND OK']
                        return success, '\n'.join(response_lines)
            time.sleep(0.1)
        
        # Timeout occurred
        response_text = '\n'.join(response_lines) if response_lines else "TIMEOUT"
        return False, response_text
    
    def disconnect(self):
        """Disconnect from ESP32C6."""
        if self.conn:
            self.conn.close()
            print("Disconnected")

# Pytest fixtures
@pytest.fixture(scope="session")
def config():
    """Load configuration for tests."""
    return load_config()

@pytest.fixture(scope="session") 
def tester_config(config):
    """Get tester configuration from config file."""
    if config:
        try:
            port = config.get('serial', 'port', fallback='COM3')
            baudrate = config.getint('serial', 'baudrate', fallback=115200)
            timeout = config.getint('serial', 'timeout', fallback=5)
            return {'port': port, 'baudrate': baudrate, 'timeout': timeout}
        except Exception:
            pass
    
    # Default configuration
    return {'port': 'COM3', 'baudrate': 115200, 'timeout': 5}

@pytest.fixture(scope="session")
def wifi_config(config):
    """Get WiFi configuration from config file."""
    if config:
        try:
            ssid = config.get('wifi', 'ssid', fallback='')
            password = config.get('wifi', 'password', fallback='')
            return {'ssid': ssid, 'password': password}
        except Exception:
            pass
    
    return {'ssid': '', 'password': ''}

@pytest.fixture(scope="session")
def tester(tester_config):
    """Create and connect tester instance."""
    tester = SimpleATTester(**tester_config)
    
    if not tester.connect():
        pytest.skip("Could not connect to ESP32C6")
    
    yield tester
    
    tester.disconnect()

# Test functions
def test_basic_connectivity(tester):
    """Test basic AT command connectivity."""
    success, response = tester.send_at_command("AT")
    assert success, f"Basic AT command failed: {response}"
    assert "OK" in response, "Expected OK response not found"

def test_firmware_version(tester):
    """Test firmware version query."""
    success, response = tester.send_at_command("AT+GMR")
    assert success, f"Firmware version command failed: {response}"
    assert "AT version" in response, "AT version info not found in response"

def test_sd_card_help(tester):
    """Test SD card mount command help."""
    success, response = tester.send_at_command("AT+BNSD_MOUNT=?")
    assert success, f"SD card help command failed: {response}"

def test_sd_card_status(tester):
    """Test SD card mount status query."""
    success, response = tester.send_at_command("AT+BNSD_MOUNT?")
    assert success, f"SD card status command failed: {response}"

def test_sd_card_mount(tester):
    """Test SD card mount operation (may fail if no SD card)."""
    success, response = tester.send_at_command("AT+BNSD_MOUNT")
    # This test is allowed to fail if no SD card is present
    if not success:
        print(f"SD card mount failed (expected if no SD card): {response}")
        pytest.skip("SD card not available - skipping mount test")

def test_http_command_help(tester):
    """Test HTTP command help."""
    success, response = tester.send_at_command("AT+BNCURL=?")
    assert success, f"HTTP help command failed: {response}"
    assert "Usage:" in response, "Expected usage information not found"

@pytest.mark.parametrize("command,expected", [
    ("AT", "OK"),
    ("AT+GMR", "AT version"),
    ("AT+BNSD_MOUNT=?", "OK"),
    ("AT+BNCURL=?", "Usage:")
])
def test_command_responses(tester, command, expected):
    """Parametrized test for various AT commands."""
    success, response = tester.send_at_command(command)
    assert success, f"Command {command} failed: {response}"
    assert expected in response, f"Expected '{expected}' not found in response"

def test_wifi_connection(tester, wifi_config):
    """Test WiFi connection if credentials are provided."""
    ssid = wifi_config['ssid']
    password = wifi_config['password']
    
    if not ssid or not password:
        pytest.skip("WiFi credentials not provided in config")
    
    # Set station mode
    success, response = tester.send_at_command("AT+CWMODE=1")
    assert success, f"Failed to set WiFi mode: {response}"
    
    # Connect to WiFi
    success, response = tester.send_at_command(f'AT+CWJAP="{ssid}","{password}"')
    assert success, f"Failed to connect to WiFi: {response}"
    assert "WIFI CONNECTED" in response, "WiFi connection not confirmed"
    
    # Wait a bit for IP assignment
    time.sleep(3)
    
    # Get IP address
    success, response = tester.send_at_command("AT+CIFSR")
    assert success, f"Failed to get IP address: {response}"
    assert "STAIP" in response, "Station IP not found in response"

def run_basic_tests():
    """Run basic AT command tests using configuration (non-pytest mode)."""
    # Load configuration
    config = load_config()
    
    # Get settings from config or use defaults
    if config:
        try:
            port = config.get('serial', 'port', fallback='COM3')
            baudrate = config.getint('serial', 'baudrate', fallback=115200)
            timeout = config.getint('serial', 'timeout', fallback=5)
            
            # Get WiFi settings
            wifi_ssid = config.get('wifi', 'ssid', fallback='')
            wifi_password = config.get('wifi', 'password', fallback='')
            
        except Exception as e:
            print(f"Error reading configuration: {e}")
            print("Using default settings...")
            port, baudrate, timeout = 'COM3', 115200, 5
            wifi_ssid = wifi_password = ''
    else:
        # Default settings if no config file
        port, baudrate, timeout = 'COM3', 115200, 5
        wifi_ssid = wifi_password = ''
    
    print(f"Using serial port: {port} at {baudrate} baud (timeout: {timeout}s)")
    if wifi_ssid:
        print(f"WiFi configured: {wifi_ssid}")
    
    tester = SimpleATTester(port=port, baudrate=baudrate, timeout=timeout)
    
    if not tester.connect():
        print("Failed to connect to ESP32C6")
        return False
    
    test_results = []
    
    try:
        # Test basic connectivity
        print("\n=== Testing Basic Connectivity ===")
        success, response = tester.send_at_command("AT")
        test_results.append(("Basic Connectivity", success))
        
        # Get firmware version
        print("\n=== Getting Firmware Version ===")
        success, response = tester.send_at_command("AT+GMR")
        test_results.append(("Firmware Version", success))
        
        # Test SD card commands
        print("\n=== Testing SD Card Commands ===")
        success, response = tester.send_at_command("AT+BNSD_MOUNT=?")  # Help
        test_results.append(("SD Card Help", success))
        
        success, response = tester.send_at_command("AT+BNSD_MOUNT?")   # Status
        test_results.append(("SD Card Status", success))
        
        success, response = tester.send_at_command("AT+BNSD_MOUNT")    # Mount
        test_results.append(("SD Card Mount", success))
        
        # Test HTTP command help
        print("\n=== Testing HTTP Commands ===")
        success, response = tester.send_at_command("AT+BNCURL=?")      # Help
        test_results.append(("HTTP Help", success))
        
        # Optional WiFi test if credentials are provided
        if wifi_ssid and wifi_password:
            print("\n=== Testing WiFi Connection ===")
            success, response = tester.send_at_command("AT+CWMODE=1")  # Station mode
            test_results.append(("WiFi Mode", success))
            
            success, response = tester.send_at_command(f'AT+CWJAP="{wifi_ssid}","{wifi_password}"')
            test_results.append(("WiFi Connect", success))
            
            if success:
                time.sleep(2)  # Wait for connection
                success, response = tester.send_at_command("AT+CIFSR")  # Get IP
                test_results.append(("Get IP", success))
        
    finally:
        tester.disconnect()
    
    # Print test summary
    print("\n" + "="*50)
    print("TEST SUMMARY")
    print("="*50)
    passed = 0
    total = len(test_results)
    
    for test_name, result in test_results:
        status = "PASS" if result else "FAIL"
        print(f"{test_name}: {status}")
        if result:
            passed += 1
    
    print("="*50)
    print(f"PASSED: {passed}/{total}")
    
    return passed == total

if __name__ == "__main__":
    print("ESP32C6 Simple AT Command Tester (Pytest-Enabled)")
    print("Make sure ESP32C6 is connected via UART1 (GPIO 6/7)")
    print("Configuration will be read from config.ini if available")
    print("")
    print("Run modes:")
    print("  python simple_test.py      - Direct execution")
    print("  pytest simple_test.py -v   - Pytest execution")
    print("-" * 60)
    
    success = run_basic_tests()
    sys.exit(0 if success else 1)
