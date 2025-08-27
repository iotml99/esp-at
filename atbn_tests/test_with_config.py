#!/usr/bin/env python3
"""
Configurable ESP32C6 AT Commands Test Script
Reads configuration from config.ini file for easy customization.
"""

import configparser
import os
import sys
from test_at_commands import ESP32C6Tester

def load_config():
    """Load configuration from config.ini file."""
    config = configparser.ConfigParser()
    config_file = os.path.join(os.path.dirname(__file__), 'config.ini')
    
    if not os.path.exists(config_file):
        print(f"Configuration file not found: {config_file}")
        print("Please create config.ini or use test_at_commands.py directly")
        return None
    
    config.read(config_file)
    return config

def main():
    """Main function using configuration file."""
    config = load_config()
    if not config:
        sys.exit(1)
    
    # Read configuration
    try:
        serial_port = config.get('serial', 'port')
        baudrate = config.getint('serial', 'baudrate')
        timeout = config.getint('serial', 'timeout')
        
        wifi_ssid = config.get('wifi', 'ssid', fallback='')
        wifi_password = config.get('wifi', 'password', fallback='')
        
    except (configparser.NoSectionError, configparser.NoOptionError) as e:
        print(f"Configuration error: {e}")
        print("Please check your config.ini file")
        sys.exit(1)
    
    print("ESP32C6 AT Commands Test Suite (Configurable)")
    print("=" * 50)
    print(f"Serial Port: {serial_port}")
    print(f"Baud Rate: {baudrate}")
    print(f"Timeout: {timeout}s")
    print(f"WiFi SSID: {wifi_ssid if wifi_ssid else 'Not configured'}")
    print("=" * 50)
    
    # Initialize and run tester
    tester = ESP32C6Tester(port=serial_port, baudrate=baudrate, timeout=timeout)
    success = tester.run_full_test_suite(wifi_ssid, wifi_password)
    
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
