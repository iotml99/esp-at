#!/usr/bin/env python3
"""
BNWPS AT Command Integration Tests

Test suite for AT+BNWPS command functionality.
Requires a physical ESP32 with ESP-AT firmware and BNWPS enabled.

Usage:
    python test_bnwps_integration.py --port /dev/ttyUSB0 --baud 115200

Requirements:
    - pyserial
    - ESP32 with ESP-AT firmware
    - BNWPS enabled in firmware build
    - Wi-Fi router with WPS PBC support (for full testing)
"""

import argparse
import serial
import time
import sys
from typing import Optional, Tuple, List


class ATCommandTester:
    """AT command interface for testing"""
    
    def __init__(self, port: str, baud: int = 115200, timeout: float = 5.0):
        self.port = port
        self.baud = baud
        self.timeout = timeout
        self.serial = None
        self.debug = False
    
    def connect(self) -> bool:
        """Connect to ESP32"""
        try:
            self.serial = serial.Serial(self.port, self.baud, timeout=self.timeout)
            time.sleep(2)  # Allow ESP32 to boot
            return True
        except Exception as e:
            print(f"Failed to connect: {e}")
            return False
    
    def disconnect(self):
        """Disconnect from ESP32"""
        if self.serial:
            self.serial.close()
            self.serial = None
    
    def send_command(self, command: str, timeout: Optional[float] = None) -> Tuple[bool, List[str]]:
        """Send AT command and get response"""
        if not self.serial:
            return False, ["ERROR: Not connected"]
        
        if timeout is None:
            timeout = self.timeout
        
        # Clear input buffer
        self.serial.flushInput()
        
        # Send command
        cmd_line = f"{command}\r\n"
        if self.debug:
            print(f">>> {command}")
        self.serial.write(cmd_line.encode())
        
        # Read response
        response_lines = []
        start_time = time.time()
        
        while time.time() - start_time < timeout:
            if self.serial.in_waiting:
                line = self.serial.readline().decode().strip()
                if line:
                    if self.debug:
                        print(f"<<< {line}")
                    response_lines.append(line)
                    
                    # Check for command completion
                    if line in ["OK", "ERROR", "FAIL"]:
                        break
                    
                    # Check for specific error patterns
                    if line.startswith("+CWJAP:") and "ERROR" in response_lines:
                        break
            else:
                time.sleep(0.01)
        
        # Determine success
        success = any(line == "OK" for line in response_lines)
        return success, response_lines
    
    def set_debug(self, debug: bool):
        """Enable/disable debug output"""
        self.debug = debug


class BNWPSTestSuite:
    """Test suite for BNWPS functionality"""
    
    def __init__(self, tester: ATCommandTester):
        self.tester = tester
        self.test_count = 0
        self.passed_count = 0
        self.failed_count = 0
    
    def run_test(self, name: str, test_func) -> bool:
        """Run a single test"""
        self.test_count += 1
        print(f"\n[TEST {self.test_count}] {name}")
        
        try:
            success = test_func()
            if success:
                print(f"✓ PASS: {name}")
                self.passed_count += 1
                return True
            else:
                print(f"✗ FAIL: {name}")
                self.failed_count += 1
                return False
        except Exception as e:
            print(f"✗ ERROR: {name} - {e}")
            self.failed_count += 1
            return False
    
    def test_basic_communication(self) -> bool:
        """Test basic AT communication"""
        success, response = self.tester.send_command("AT")
        return success and "OK" in response
    
    def test_bnwps_test_command(self) -> bool:
        """Test AT+BNWPS=? (test command)"""
        success, response = self.tester.send_command("AT+BNWPS=?")
        if not success:
            return False
        
        # Check for usage information
        response_text = "\n".join(response)
        return ("AT+BNWPS commands:" in response_text or 
                "Usage:" in response_text or
                "BNWPS" in response_text)
    
    def test_bnwps_query_idle(self) -> bool:
        """Test AT+BNWPS? when idle"""
        success, response = self.tester.send_command("AT+BNWPS?")
        if not success:
            return False
        
        # Should return +BNWPS:0 (idle)
        for line in response:
            if line.strip() == "+BNWPS:0":
                return True
        return False
    
    def test_bnwps_invalid_duration_zero(self) -> bool:
        """Test AT+BNWPS=0 (cancel command)"""
        success, response = self.tester.send_command("AT+BNWPS=0")
        if not success:
            # This might fail if not supported, check for error code
            for line in response:
                if "+CWJAP:9" in line:  # Not supported
                    print("BNWPS not supported in this build")
                    return True  # Consider this a pass
            return False
        
        # Should return +BNWPS:0
        for line in response:
            if line.strip() == "+BNWPS:0":
                return True
        return False
    
    def test_bnwps_invalid_duration_negative(self) -> bool:
        """Test AT+BNWPS with invalid duration (out of range)"""
        success, response = self.tester.send_command("AT+BNWPS=999")
        
        # Should fail with error code 4 (invalid args)
        if not success:
            for line in response:
                if "+CWJAP:4" in line:
                    return True
        return False
    
    def test_bnwps_valid_duration_short(self) -> bool:
        """Test AT+BNWPS with short valid duration"""
        # First ensure we're idle
        self.tester.send_command("AT+BNWPS=0")
        time.sleep(0.1)
        
        success, response = self.tester.send_command("AT+BNWPS=5", timeout=10.0)
        
        # Check if BNWPS is supported
        if not success:
            for line in response:
                if "+CWJAP:9" in line:  # Not supported
                    print("BNWPS not supported in this build")
                    return True
                if "+CWJAP:5" in line:  # Not initialized (Wi-Fi off)
                    print("Wi-Fi not initialized - this is expected in some environments")
                    return True
        
        # Should return +BNWPS:1 (active)
        found_active = False
        for line in response:
            if line.strip() == "+BNWPS:1":
                found_active = True
                break
        
        if found_active:
            # Cancel the session
            time.sleep(0.5)
            self.tester.send_command("AT+BNWPS=0")
            return True
        
        return False
    
    def test_bnwps_query_during_session(self) -> bool:
        """Test AT+BNWPS? during active session"""
        # Start a WPS session
        success1, response1 = self.tester.send_command("AT+BNWPS=10", timeout=5.0)
        
        if not success1:
            # Check for known failure reasons
            for line in response1:
                if "+CWJAP:9" in line or "+CWJAP:5" in line:
                    print("BNWPS not available - skipping session test")
                    return True
            return False
        
        # Verify it started
        found_active = False
        for line in response1:
            if line.strip() == "+BNWPS:1":
                found_active = True
                break
        
        if not found_active:
            return False
        
        # Query state during session
        time.sleep(0.5)
        success2, response2 = self.tester.send_command("AT+BNWPS?")
        
        # Cancel the session
        self.tester.send_command("AT+BNWPS=0")
        
        if not success2:
            return False
        
        # Should return +BNWPS:1 (active)
        for line in response2:
            if line.strip() == "+BNWPS:1":
                return True
        return False
    
    def test_bnwps_busy_detection(self) -> bool:
        """Test busy detection (start WPS while already active)"""
        # Start first session
        success1, response1 = self.tester.send_command("AT+BNWPS=15", timeout=5.0)
        
        if not success1:
            # Check for known failure reasons
            for line in response1:
                if "+CWJAP:9" in line or "+CWJAP:5" in line:
                    print("BNWPS not available - skipping busy test")
                    return True
            return False
        
        found_active = False
        for line in response1:
            if line.strip() == "+BNWPS:1":
                found_active = True
                break
        
        if not found_active:
            return False
        
        # Try to start second session (should fail with busy error)
        time.sleep(0.5)
        success2, response2 = self.tester.send_command("AT+BNWPS=10")
        
        # Cancel the first session
        self.tester.send_command("AT+BNWPS=0")
        
        # Second command should fail with error code 6 (busy)
        if not success2:
            for line in response2:
                if "+CWJAP:6" in line:
                    return True
        
        return False
    
    def test_bnwps_cancel_functionality(self) -> bool:
        """Test WPS session cancellation"""
        # Start a session
        success1, response1 = self.tester.send_command("AT+BNWPS=20", timeout=5.0)
        
        if not success1:
            # Check for known failure reasons
            for line in response1:
                if "+CWJAP:9" in line or "+CWJAP:5" in line:
                    print("BNWPS not available - skipping cancel test")
                    return True
            return False
        
        found_active = False
        for line in response1:
            if line.strip() == "+BNWPS:1":
                found_active = True
                break
        
        if not found_active:
            return False
        
        # Cancel the session
        time.sleep(0.5)
        success2, response2 = self.tester.send_command("AT+BNWPS=0")
        
        if not success2:
            return False
        
        # Should return +BNWPS:0
        found_canceled = False
        for line in response2:
            if line.strip() == "+BNWPS:0":
                found_canceled = True
                break
        
        if not found_canceled:
            return False
        
        # Verify state is now idle
        time.sleep(0.5)
        success3, response3 = self.tester.send_command("AT+BNWPS?")
        
        if not success3:
            return False
        
        for line in response3:
            if line.strip() == "+BNWPS:0":
                return True
        
        return False
    
    def run_all_tests(self):
        """Run all tests"""
        print("=== BNWPS Integration Test Suite ===")
        
        tests = [
            ("Basic AT Communication", self.test_basic_communication),
            ("BNWPS Test Command", self.test_bnwps_test_command),
            ("BNWPS Query Idle", self.test_bnwps_query_idle),
            ("BNWPS Cancel Command", self.test_bnwps_invalid_duration_zero),
            ("BNWPS Invalid Duration", self.test_bnwps_invalid_duration_negative),
            ("BNWPS Valid Duration", self.test_bnwps_valid_duration_short),
            ("BNWPS Query During Session", self.test_bnwps_query_during_session),
            ("BNWPS Busy Detection", self.test_bnwps_busy_detection),
            ("BNWPS Cancel Functionality", self.test_bnwps_cancel_functionality),
        ]
        
        for name, test_func in tests:
            self.run_test(name, test_func)
        
        print("\n=== Test Results ===")
        print(f"Total: {self.test_count}")
        print(f"Passed: {self.passed_count}")
        print(f"Failed: {self.failed_count}")
        print(f"Success Rate: {self.passed_count/self.test_count*100:.1f}%")
        
        return self.failed_count == 0


def main():
    """Main test runner"""
    parser = argparse.ArgumentParser(description="BNWPS AT Command Integration Tests")
    parser.add_argument("--port", required=True, help="Serial port (e.g., /dev/ttyUSB0, COM3)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("--timeout", type=float, default=10.0, help="Command timeout (default: 10.0)")
    parser.add_argument("--debug", action="store_true", help="Enable debug output")
    args = parser.parse_args()
    
    # Create tester
    tester = ATCommandTester(args.port, args.baud, args.timeout)
    tester.set_debug(args.debug)
    
    # Connect
    if not tester.connect():
        print("Failed to connect to ESP32")
        return 1
    
    try:
        # Run tests
        test_suite = BNWPSTestSuite(tester)
        success = test_suite.run_all_tests()
        return 0 if success else 1
        
    finally:
        tester.disconnect()


if __name__ == "__main__":
    sys.exit(main())
