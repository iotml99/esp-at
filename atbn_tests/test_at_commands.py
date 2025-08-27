#!/usr/bin/env python3
"""
Simple Python test script for ESP32C6 AT commands functionality.
Based on the ATBN_Readme.md guide.

This script tests the custom AT commands for SD card operations and HTTP requests
using the BNCURL functionality described in the readme.

Requirements:
- pyserial: pip install pyserial
- ESP32C6 connected via UART1 (GPIO 6/7)
- SD card properly wired and inserted
- WiFi credentials configured
"""

import serial
import time
import sys
import logging
from typing import Optional, Tuple, List

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

class ESP32C6Tester:
    """Test class for ESP32C6 AT commands functionality."""
    
    def __init__(self, port: str = 'COM3', baudrate: int = 115200, timeout: int = 10):
        """
        Initialize the ESP32C6 tester.
        
        Args:
            port: Serial port (e.g., 'COM3' on Windows, '/dev/ttyUSB0' on Linux)
            baudrate: Serial communication baud rate (default: 115200)
            timeout: Command timeout in seconds
        """
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.serial_conn: Optional[serial.Serial] = None
        
    def connect(self) -> bool:
        """
        Establish serial connection to ESP32C6.
        
        Returns:
            True if connection successful, False otherwise
        """
        try:
            self.serial_conn = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=self.timeout
            )
            logger.info(f"Connected to {self.port} at {self.baudrate} baud")
            return True
        except serial.SerialException as e:
            logger.error(f"Failed to connect to {self.port}: {e}")
            return False
    
    def disconnect(self):
        """Close serial connection."""
        if self.serial_conn and self.serial_conn.is_open:
            self.serial_conn.close()
            logger.info("Disconnected from serial port")
    
    def send_command(self, command: str) -> Tuple[bool, List[str]]:
        """
        Send AT command and return response.
        
        Args:
            command: AT command to send
            
        Returns:
            Tuple of (success, response_lines)
        """
        if not self.serial_conn or not self.serial_conn.is_open:
            logger.error("Serial connection not established")
            return False, []
        
        try:
            # Clear input buffer
            self.serial_conn.reset_input_buffer()
            
            # Send command with CRLF
            cmd_bytes = (command + '\r\n').encode('utf-8')
            self.serial_conn.write(cmd_bytes)
            logger.info(f"Sent: {command}")
            
            # Read response
            response_lines = []
            start_time = time.time()
            
            while time.time() - start_time < self.timeout:
                if self.serial_conn.in_waiting > 0:
                    line = self.serial_conn.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        response_lines.append(line)
                        logger.info(f"Received: {line}")
                        
                        # Check for command completion
                        if line in ['OK', 'ERROR', 'SEND OK', 'SEND FAIL']:
                            success = line in ['OK', 'SEND OK']
                            return success, response_lines
                
                time.sleep(0.1)
            
            logger.warning(f"Command timeout: {command}")
            return False, response_lines
            
        except Exception as e:
            logger.error(f"Error sending command '{command}': {e}")
            return False, []
    
    def test_basic_connectivity(self) -> bool:
        """Test basic AT command connectivity."""
        logger.info("Testing basic connectivity...")
        success, response = self.send_command("AT")
        return success and any("OK" in line for line in response)
    
    def test_firmware_version(self) -> bool:
        """Test firmware version query."""
        logger.info("Testing firmware version query...")
        success, response = self.send_command("AT+GMR")
        if success:
            # Check for expected version info
            has_version = any("AT version" in line for line in response)
            has_sdk = any("SDK version" in line for line in response)
            return has_version and has_sdk
        return False
    
    def test_wifi_setup(self, ssid: str, password: str) -> bool:
        """
        Test WiFi connection setup.
        
        Args:
            ssid: WiFi network name
            password: WiFi password
            
        Returns:
            True if WiFi connected successfully
        """
        logger.info("Testing WiFi setup...")
        
        # Set WiFi mode to station
        success, _ = self.send_command("AT+CWMODE=1")
        if not success:
            logger.error("Failed to set WiFi mode")
            return False
        
        # Connect to WiFi
        wifi_cmd = f'AT+CWJAP="{ssid}","{password}"'
        success, response = self.send_command(wifi_cmd)
        if not success:
            logger.error("Failed to connect to WiFi")
            return False
        
        # Check for successful connection indicators
        wifi_connected = any("WIFI CONNECTED" in line for line in response)
        wifi_got_ip = any("WIFI GOT IP" in line for line in response)
        
        if wifi_connected and wifi_got_ip:
            # Verify IP address
            success, ip_response = self.send_command("AT+CIFSR")
            if success and any("STAIP" in line for line in ip_response):
                logger.info("WiFi connection successful")
                return True
        
        logger.error("WiFi connection failed")
        return False
    
    def test_sd_card_operations(self) -> bool:
        """Test SD card mount/unmount operations."""
        logger.info("Testing SD card operations...")
        
        # Test command help
        success, _ = self.send_command("AT+BNSD_MOUNT=?")
        if not success:
            logger.error("SD mount help command failed")
            return False
        
        # Check current mount status
        success, status_response = self.send_command("AT+BNSD_MOUNT?")
        if not success:
            logger.error("SD mount status query failed")
            return False
        
        # Try to mount SD card
        success, mount_response = self.send_command("AT+BNSD_MOUNT")
        if success:
            # Check for successful mount message
            mount_success = any("mounted successfully" in line for line in mount_response)
            if mount_success:
                logger.info("SD card mounted successfully")
                
                # Test unmount
                success, unmount_response = self.send_command("AT+BNSD_UNMOUNT")
                if success and any("unmounted successfully" in line for line in unmount_response):
                    logger.info("SD card unmounted successfully")
                    return True
                else:
                    logger.error("SD card unmount failed")
            else:
                logger.warning("SD card mount failed - this may be expected if no SD card is present")
        
        return False
    
    def test_http_operations(self) -> bool:
        """Test HTTP operations using BNCURL."""
        logger.info("Testing HTTP operations...")
        
        # Test command help
        success, help_response = self.send_command("AT+BNCURL=?")
        if not success:
            logger.error("BNCURL help command failed")
            return False
        
        # Check help response contains expected usage info
        help_text = " ".join(help_response)
        if "Usage:" not in help_text or "GET" not in help_text:
            logger.error("BNCURL help response incomplete")
            return False
        
        # Test simple HTTP GET request
        logger.info("Testing HTTP GET request...")
        success, get_response = self.send_command('AT+BNCURL="GET","http://httpbin.org/get"')
        
        if success:
            # Check for expected response format
            has_len = any("+LEN:" in line for line in get_response)
            has_post = any("+POST:" in line for line in get_response)
            has_send_ok = any("SEND OK" in line for line in get_response)
            
            if has_len and has_post and has_send_ok:
                logger.info("HTTP GET request successful")
                return True
            else:
                logger.warning("HTTP GET request completed but response format unexpected")
        
        logger.error("HTTP GET request failed")
        return False
    
    def test_http_with_sd_card(self) -> bool:
        """Test HTTP operations with SD card file saving."""
        logger.info("Testing HTTP with SD card saving...")
        
        # First mount SD card
        success, _ = self.send_command("AT+BNSD_MOUNT")
        if not success:
            logger.warning("Cannot mount SD card - skipping SD card HTTP test")
            return False
        
        # Test HTTP GET with file saving
        cmd = 'AT+BNCURL="GET","http://httpbin.org/json","-dd","/sdcard/test_response.json"'
        success, response = self.send_command(cmd)
        
        # Unmount SD card
        self.send_command("AT+BNSD_UNMOUNT")
        
        if success:
            # Check for file save indicators
            saving_to_file = any("Saving to file:" in line for line in response)
            file_saved = any("File saved" in line for line in response)
            
            if saving_to_file and file_saved:
                logger.info("HTTP with SD card saving successful")
                return True
        
        logger.error("HTTP with SD card saving failed")
        return False
    
    def run_full_test_suite(self, wifi_ssid: str = "", wifi_password: str = "") -> bool:
        """
        Run the complete test suite.
        
        Args:
            wifi_ssid: WiFi network name (optional)
            wifi_password: WiFi password (optional)
            
        Returns:
            True if all tests pass, False otherwise
        """
        logger.info("Starting ESP32C6 AT Commands Test Suite")
        
        if not self.connect():
            return False
        
        test_results = {}
        
        try:
            # Basic connectivity test
            test_results['basic_connectivity'] = self.test_basic_connectivity()
            
            # Firmware version test
            test_results['firmware_version'] = self.test_firmware_version()
            
            # WiFi tests (if credentials provided)
            if wifi_ssid and wifi_password:
                test_results['wifi_setup'] = self.test_wifi_setup(wifi_ssid, wifi_password)
                
                # HTTP tests (require WiFi)
                if test_results['wifi_setup']:
                    test_results['http_operations'] = self.test_http_operations()
                    test_results['http_sd_card'] = self.test_http_with_sd_card()
                else:
                    logger.warning("Skipping HTTP tests due to WiFi connection failure")
            else:
                logger.warning("WiFi credentials not provided - skipping WiFi and HTTP tests")
            
            # SD card tests (independent of WiFi)
            test_results['sd_card_operations'] = self.test_sd_card_operations()
            
        finally:
            self.disconnect()
        
        # Print test results summary
        logger.info("\n" + "="*50)
        logger.info("TEST RESULTS SUMMARY")
        logger.info("="*50)
        
        all_passed = True
        for test_name, result in test_results.items():
            status = "PASS" if result else "FAIL"
            logger.info(f"{test_name}: {status}")
            if not result:
                all_passed = False
        
        logger.info("="*50)
        overall_status = "ALL TESTS PASSED" if all_passed else "SOME TESTS FAILED"
        logger.info(f"OVERALL: {overall_status}")
        
        return all_passed


def main():
    """Main function to run the test suite."""
    # Configuration
    SERIAL_PORT = "COM3"  # Change this to your actual serial port
    WIFI_SSID = ""        # Set your WiFi SSID here
    WIFI_PASSWORD = ""    # Set your WiFi password here
    
    print("ESP32C6 AT Commands Test Suite")
    print("=" * 40)
    print(f"Serial Port: {SERIAL_PORT}")
    print(f"WiFi SSID: {WIFI_SSID if WIFI_SSID else 'Not configured'}")
    print("=" * 40)
    
    # Initialize tester
    tester = ESP32C6Tester(port=SERIAL_PORT)
    
    # Run tests
    success = tester.run_full_test_suite(WIFI_SSID, WIFI_PASSWORD)
    
    # Exit with appropriate code
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
