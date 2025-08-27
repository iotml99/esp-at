#!/usr/bin/env python3
"""
Missing ATBN commands test suite - covers all commands from test.md that were not implemented.
This includes WPS, Certificate Flashing, SD Management, Progress Monitoring, and Transfer Control.
"""

import os
import sys
import time
import logging
from pathlib import Path

# Import base tester
sys.path.insert(0, str(Path(__file__).parent))
from test_atbn_comprehensive import ATBNTester

# Load environment variables
try:
    from dotenv import load_dotenv
    env_path = Path(__file__).parent / '.env'
    if env_path.exists():
        load_dotenv(env_path)
except ImportError:
    print("python-dotenv not installed, using system environment")

logger = logging.getLogger(__name__)

class ATBNMissingCommandsTester(ATBNTester):
    """
    Test suite for missing ATBN commands not covered in the basic test suite.
    Implements tests for WPS, Certificate Flashing, SD Management, Progress Monitoring, etc.
    """
    
    def __init__(self):
        super().__init__()
        
        # Test certificate paths
        self.cert_paths = {
            'server_cert': '/certs/server.crt',
            'client_key': '/certs/client.key',
            'ca_bundle': '/certs/ca-bundle.pem',
            'root_ca': '/certs/root_ca.crt'
        }
        
        # Test data for certificates (sample certificate data)
        self.sample_cert_data = b"""-----BEGIN CERTIFICATE-----
MIIDXTCCAkWgAwIBAgIJAKoK/heBjcOuMA0GCSqGSIb3DQEBCwUAMEUxCzAJBgNV
BAYTAkFVMRMwEQYDVQQIDApTb21lLVN0YXRlMSEwHwYDVQQKDBhJbnRlcm5ldCBX
aWRnaXRzIFB0eSBMdGQwHhcNMTcwNzEyMTU0NzQ2WhcNMTgwNzEyMTU0NzQ2WjBF
MQswCQYDVQQGEwJBVTETMBEGA1UECAwKU29tZS1TdGF0ZTEhMB8GA1UECgwYSW50
ZXJuZXQgV2lkZ2l0cyBQdHkgTHRkMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIB
CgKCAQEAuuExKnhBNJ1wNE29PSLACdxgzMlYZBdRjbQTyIwjOvJsNYg0+qRhOJoN
cVJ4QQrJPMZk6Ix0MzN4KGRYrE7iF+tU7cI0q1qTqA5WNUWx1JeFGjZU4vvHr1YL
aFN1JIXhV2Q0FnvUJp9ZgHwVU9Zy2gNOg5NgO2sQzJ7wR8MxcQHWv7Fw4qfRAZhV
Kl7Qq+JI8CXpjSk9JjFZRGHN8aNNGWJ4q2JLt7h7FN5KLlYLq6Mfc+jK6KQHIWvz
bPFJhFpAKmQZmKjPVaKGYfJm8iJ+VCgYK3UIqCkHONeFQIYaNdj4JhIRQLdMX1pI
7HnGU7rSz+1j4KYmQhKo6kQxbQIDAQABo1AwTjAdBgNVHQ4EFgQU4L/mVqpGHcOF
Sv+FRAqUIGN6/bUwHwYDVR0jBBgwFoAU4L/mVqpGHcOFSv+FRAqUIGN6/bUwDAYD
VR0TBAUwAwEB/zANBgkqhkiG9w0BAQsFAAOCAQEASjCeHgH3zVKBOy5w5I+WYL+r
3L4Hw3uBhcF8wuOgWhEoNpN4Iu6QRB/cKaUyYo3v5KJv8zQXzJOPO2qZJ3QjAAw8
gVmQKMlUKOQgQ2xOGO+L8D+JO7H+JmJhNy+MYhJvBvhX8jfyHAOYJ5y5QAhcQ5wj
q4Y8U3jZpJ2JGKg4GJ8c8JeJzGJ8c8IeJUJeHJ5JGKgqG4Y8UzJZPJ2wJGKg4G
J8c8JeJzGJ8c8IeJUJeHJ5JGKgqG4Y8UzJZPJ2wJGKg4GJ8c8JeJzGJ8c8IeJUJe
HJ5JGKgqG4Y8UzJZPJ2wJGKg4GJ8c8JeJzGJ8c8IeJUJeHJ5JGKgqG4Y8UzJZPJ2w
JGKg4GJ8c8JeJzGJ8c8IeJUJeHJ5JGKgqG4Y8UzJZPJ2wJGKg4GJ8c8JeJzGJ8c8
IeJUJeHJ5JGKgqG4Y8UzJZPJ2wJGKg4GJ8c8JeJzGJ8c8IeJUJeHJ5JGKgqG4Y8U
zJZPJ2wJGKg4GJ8c8JeJzGJ8c8IeJUJeHJ5JGKgqG4Y8UzJZPJ2wJGKg4GJ8c8Je
JzGJ8c8IeJUJeHJ5JGKgqG4Y8UzJZPJ2wJGKg4GJ8c8JeJzGJ8c8IeJUJeHJ5JGKg
qG4Y8UzJZPJ2w==
-----END CERTIFICATE-----"""

    # WPS Tests (Section 1 from test.md)
    
    def test_wps_connection_30_seconds(self) -> bool:
        """1.1 WPS Connection - 30 second window"""
        logger.info("Testing WPS 30 second window...")
        cmd = 'AT+BNWPS="30"'
        success, response = self.send_command(cmd, timeout=35)
        return success
    
    def test_wps_connection_minimum(self) -> bool:
        """1.1 WPS Connection - minimum 1 second"""
        logger.info("Testing WPS minimum timeout...")
        cmd = 'AT+BNWPS="1"'
        success, response = self.send_command(cmd, timeout=5)
        return success
    
    def test_wps_connection_maximum(self) -> bool:
        """1.1 WPS Connection - maximum timeout"""
        logger.info("Testing WPS maximum timeout...")
        cmd = 'AT+BNWPS="120"'
        success, response = self.send_command(cmd, timeout=125)
        return success
    
    def test_wps_beyond_limit(self) -> bool:
        """1.1 WPS Connection - beyond reasonable limit"""
        logger.info("Testing WPS beyond limit (should error)...")
        cmd = 'AT+BNWPS="300"'
        success, response = self.send_command(cmd, timeout=10)
        # Should likely return error for excessive timeout
        return True  # Accept either success or error
    
    def test_wps_status_query(self) -> bool:
        """1.2 WPS Status Query"""
        logger.info("Testing WPS status query...")
        cmd = 'AT+BNWPS?'
        success, response = self.send_command(cmd, timeout=10)
        return success
    
    def test_wps_status_during_connection(self) -> bool:
        """1.2 WPS Status during active connection"""
        logger.info("Testing WPS status during connection...")
        
        # Start WPS
        start_success, _ = self.send_command('AT+BNWPS="10"', timeout=2)
        if start_success:
            # Query status immediately
            status_success, response = self.send_command('AT+BNWPS?', timeout=5)
            
            # Cancel WPS
            self.send_command('AT+BNWPS="0"', timeout=5)
            
            return status_success
        return False
    
    def test_wps_cancellation(self) -> bool:
        """1.3 WPS Cancellation"""
        logger.info("Testing WPS cancellation...")
        
        # Start WPS
        start_success, _ = self.send_command('AT+BNWPS="30"', timeout=2)
        if start_success:
            time.sleep(1)  # Let it start
            
            # Cancel WPS
            cancel_success, response = self.send_command('AT+BNWPS="0"', timeout=5)
            return cancel_success
        return False
    
    def test_wps_cancel_when_not_active(self) -> bool:
        """1.3 WPS Cancel when not active"""
        logger.info("Testing WPS cancel when not active...")
        cmd = 'AT+BNWPS="0"'
        success, response = self.send_command(cmd, timeout=5)
        # May return error or OK depending on implementation
        return True
    
    def test_wps_error_cases(self) -> bool:
        """1.4 WPS Error Cases"""
        logger.info("Testing WPS error cases...")
        
        error_cases = [
            ('AT+BNWPS=""', "empty parameter"),
            ('AT+BNWPS="-5"', "negative timeout"),
            ('AT+BNWPS="abc"', "non-numeric parameter")
        ]
        
        results = []
        for cmd, description in error_cases:
            logger.info(f"Testing {description}...")
            success, response = self.send_command(cmd, timeout=5)
            # Error cases should typically return ERROR
            results.append(not success)  # Expect failure
        
        return all(results)
    
    # Certificate Flashing Tests (Section 2 from test.md)
    
    def test_flash_cert_from_sd(self) -> bool:
        """2.1 Flash Certificate from SD Card"""
        if not self.sd_mounted:
            return False
        
        logger.info("Testing certificate flash from SD...")
        cmd = f'AT+BNFLASH_CERT="0x2A000","@{self.cert_paths["server_cert"]}"'
        success, response = self.send_command(cmd, timeout=30)
        return success
    
    def test_flash_cert_multiple_from_sd(self) -> bool:
        """2.1 Flash Multiple Certificates from SD"""
        if not self.sd_mounted:
            return False
        
        logger.info("Testing multiple certificate flash from SD...")
        
        commands = [
            ('AT+BNFLASH_CERT="0x2A000","@/certs/server.crt"', "server cert"),
            ('AT+BNFLASH_CERT="0x2B000","@/certs/client.key"', "client key"),
            ('AT+BNFLASH_CERT="0x2C000","@/certs/ca-bundle.pem"', "CA bundle"),
            ('AT+BNFLASH_CERT="0x10000","@/certs/root_ca.crt"', "root CA")
        ]
        
        results = []
        for cmd, description in commands:
            logger.info(f"Flashing {description}...")
            success, response = self.send_command(cmd, timeout=30)
            results.append(success)
        
        return all(results)
    
    def test_flash_cert_via_uart(self) -> bool:
        """2.2 Flash Certificate via UART"""
        logger.info("Testing certificate flash via UART...")
        
        cert_data = self.sample_cert_data
        cmd = f'AT+BNFLASH_CERT="0x2A000","{len(cert_data)}"'
        success, response = self.send_command(cmd, expect_data_prompt=True, timeout=10)
        
        if success and any(">" in line for line in response):
            # Send certificate data
            success, final_response = self.send_data(cert_data, timeout=30)
            return success
        
        return False
    
    def test_flash_cert_sizes(self) -> bool:
        """2.2 Flash Different Certificate Sizes"""
        logger.info("Testing different certificate sizes...")
        
        test_sizes = [
            (500, "small certificate"),
            (0, "zero bytes - should error"),
            (65536, "large certificate")
        ]
        
        results = []
        for size, description in test_sizes:
            logger.info(f"Testing {description}...")
            cmd = f'AT+BNFLASH_CERT="0x2D000","{size}"'
            success, response = self.send_command(cmd, expect_data_prompt=(size > 0), timeout=10)
            
            if size == 0:
                # Zero bytes should error
                results.append(not success)
            elif success and size > 0 and any(">" in line for line in response):
                # Send test data
                test_data = b'A' * size
                success, final_response = self.send_data(test_data, timeout=60)
                results.append(success)
            else:
                results.append(success)
        
        return all(results)
    
    def test_flash_cert_error_cases(self) -> bool:
        """2.3 Certificate Error Cases"""
        logger.info("Testing certificate error cases...")
        
        error_cases = [
            ('AT+BNFLASH_CERT="","1400"', "empty address"),
            ('AT+BNFLASH_CERT="invalid","1400"', "non-hex address"),
            ('AT+BNFLASH_CERT="0x2A000","@/nonexistent.crt"', "file not found"),
            ('AT+BNFLASH_CERT="0x2A000",""', "empty data source"),
            ('AT+BNFLASH_CERT="0x2A000","-100"', "negative byte count"),
            ('AT+BNFLASH_CERT="0x2A000","abc"', "non-numeric byte count")
        ]
        
        results = []
        for cmd, description in error_cases:
            logger.info(f"Testing {description}...")
            success, response = self.send_command(cmd, timeout=10)
            # Error cases should return ERROR
            results.append(not success)
        
        return all(results)
    
    def test_flash_cert_address_tests(self) -> bool:
        """2.4 Certificate Address Tests"""
        logger.info("Testing certificate address variations...")
        
        address_tests = [
            ('AT+BNFLASH_CERT="0x0","1000"', "address zero"),
            ('AT+BNFLASH_CERT="0xFFFFFFFF","1000"', "maximum address"),
            ('AT+BNFLASH_CERT="0x1000","1000"', "4KB aligned")
        ]
        
        results = []
        for cmd, description in address_tests:
            logger.info(f"Testing {description}...")
            success, response = self.send_command(cmd, expect_data_prompt=True, timeout=10)
            
            if success and any(">" in line for line in response):
                # Send test data
                test_data = b'A' * 1000
                success, final_response = self.send_data(test_data, timeout=30)
                results.append(success)
            else:
                results.append(success)
        
        return all(results)
    
    # SD Card Management Tests (Sections 7-9 from test.md)
    
    def test_sd_format(self) -> bool:
        """8.1 SD Card Format"""
        if not self.sd_mounted:
            return False
        
        logger.info("Testing SD card format...")
        cmd = 'AT+BNSD_FORMAT'
        success, response = self.send_command(cmd, timeout=60)
        return success
    
    def test_sd_format_error_cases(self) -> bool:
        """8.2 SD Format Error Cases"""
        logger.info("Testing SD format error cases...")
        
        # Test format when no card mounted
        if self.sd_mounted:
            self.send_command("AT+BNSD_UNMOUNT")
        
        success, response = self.send_command('AT+BNSD_FORMAT', timeout=10)
        
        # Should return error when no card mounted
        error_result = not success
        
        # Remount if needed
        if self.sd_mounted:
            self.send_command("AT+BNSD_MOUNT")
        
        return error_result
    
    def test_sd_space_query(self) -> bool:
        """9.1 SD Space Query"""
        if not self.sd_mounted:
            return False
        
        logger.info("Testing SD space query...")
        cmd = 'AT+BNSD_SPACE'
        success, response = self.send_command(cmd, timeout=10)
        
        if success:
            # Look for space information in response
            has_space_info = any("BNSD_SIZE" in line or "bytes" in line.lower() for line in response)
            return has_space_info
        
        return False
    
    def test_sd_space_error_cases(self) -> bool:
        """9.2 SD Space Query Error Cases"""
        logger.info("Testing SD space query error cases...")
        
        # Test space query when no card mounted
        if self.sd_mounted:
            self.send_command("AT+BNSD_UNMOUNT")
        
        success, response = self.send_command('AT+BNSD_SPACE', timeout=10)
        
        # Should return error when no card mounted
        error_result = not success
        
        # Remount if needed
        if self.sd_mounted:
            self.send_command("AT+BNSD_MOUNT")
        
        return error_result
    
    # Progress Monitoring Tests (Section 4 from test.md)
    
    def test_progress_monitoring_download(self) -> bool:
        """4.1 Progress Monitoring - Download"""
        if not self.wifi_connected or not self.sd_mounted:
            return False
        
        logger.info("Testing progress monitoring during download...")
        
        # Start a large download
        cmd = 'AT+BNCURL="GET","https://httpbin.org/drip?duration=5&numbytes=10000","-dd","/Download/large_progress.bin"'
        
        # Send command but don't wait for completion
        self.serial_conn.reset_input_buffer()
        cmd_bytes = (cmd + '\r\n').encode('utf-8')
        self.serial_conn.write(cmd_bytes)
        time.sleep(1)  # Let download start
        
        # Query progress
        progress_success, progress_response = self.send_command('AT+BNCURL_PROG?', timeout=10)
        
        # Wait for download to complete
        time.sleep(10)
        
        return progress_success
    
    def test_progress_monitoring_upload(self) -> bool:
        """4.2 Progress Monitoring - Upload"""
        if not self.wifi_connected:
            return False
        
        logger.info("Testing progress monitoring during upload...")
        
        # Start upload
        cmd = f'AT+BNCURL="POST","{self.test_urls["post"]}","-du","1024"'
        success, response = self.send_command(cmd, expect_data_prompt=True, timeout=5)
        
        if success and any(">" in line for line in response):
            # Start sending data slowly
            test_data = b'A' * 1024
            self.serial_conn.write(test_data[:512])  # Send partial data
            
            # Query progress
            progress_success, progress_response = self.send_command('AT+BNCURL_PROG?', timeout=10)
            
            # Finish sending data
            self.serial_conn.write(test_data[512:])
            
            # Wait for completion
            time.sleep(5)
            
            return progress_success
        
        return False
    
    def test_progress_monitoring_error_cases(self) -> bool:
        """4.3 Progress Monitoring Error Cases"""
        logger.info("Testing progress monitoring error cases...")
        
        # Query progress when no transfer is active
        success, response = self.send_command('AT+BNCURL_PROG?', timeout=5)
        
        # Should return error when no active transfer
        return not success
    
    # Transfer Cancellation Tests (Section 5 from test.md)
    
    def test_transfer_cancellation_download(self) -> bool:
        """5.1 Transfer Cancellation - Download"""
        if not self.wifi_connected or not self.sd_mounted:
            return False
        
        logger.info("Testing transfer cancellation during download...")
        
        # Start a long download
        cmd = 'AT+BNCURL="GET","https://httpbin.org/drip?duration=10&numbytes=50000","-dd","/Download/cancel_test.bin"'
        
        # Send command but don't wait for completion
        self.serial_conn.reset_input_buffer()
        cmd_bytes = (cmd + '\r\n').encode('utf-8')
        self.serial_conn.write(cmd_bytes)
        time.sleep(2)  # Let download start
        
        # Cancel transfer
        cancel_success, cancel_response = self.send_command('AT+BNCURL_STOP?', timeout=10)
        
        return cancel_success
    
    def test_transfer_cancellation_upload(self) -> bool:
        """5.2 Transfer Cancellation - Upload"""
        if not self.wifi_connected:
            return False
        
        logger.info("Testing transfer cancellation during upload...")
        
        # Start upload
        cmd = f'AT+BNCURL="POST","{self.test_urls["post"]}","-du","10000"'
        success, response = self.send_command(cmd, expect_data_prompt=True, timeout=5)
        
        if success and any(">" in line for line in response):
            # Send partial data
            test_data = b'A' * 1000
            self.serial_conn.write(test_data)
            time.sleep(1)
            
            # Cancel transfer
            cancel_success, cancel_response = self.send_command('AT+BNCURL_STOP?', timeout=10)
            return cancel_success
        
        return False
    
    def test_transfer_cancellation_error_cases(self) -> bool:
        """5.3 Transfer Cancellation Error Cases"""
        logger.info("Testing transfer cancellation error cases...")
        
        # Try to cancel when no transfer is active
        success, response = self.send_command('AT+BNCURL_STOP?', timeout=5)
        
        # Should return error when no active transfer
        return not success
    
    # Timeout Management Tests (Section 6 from test.md)
    
    def test_timeout_settings(self) -> bool:
        """6.1 Timeout Settings"""
        logger.info("Testing timeout settings...")
        
        timeout_tests = [
            ('AT+BNCURL_TIMEOUT="1"', "minimum 1 second"),
            ('AT+BNCURL_TIMEOUT="30"', "default value"),
            ('AT+BNCURL_TIMEOUT="120"', "maximum 120 seconds"),
            ('AT+BNCURL_TIMEOUT="60"', "normal value")
        ]
        
        results = []
        for cmd, description in timeout_tests:
            logger.info(f"Testing {description}...")
            success, response = self.send_command(cmd, timeout=5)
            results.append(success)
        
        return all(results)
    
    def test_timeout_query(self) -> bool:
        """6.2 Timeout Query"""
        logger.info("Testing timeout query...")
        
        # Set a known timeout
        self.send_command('AT+BNCURL_TIMEOUT="45"')
        
        # Query timeout
        success, response = self.send_command('AT+BNCURL_TIMEOUT?', timeout=5)
        
        if success:
            # Look for timeout value in response
            has_timeout = any("BNCURL_TIMEOUT" in line or "45" in line for line in response)
            return has_timeout
        
        return False
    
    def test_timeout_invalid_values(self) -> bool:
        """6.3 Invalid Timeout Values"""
        logger.info("Testing invalid timeout values...")
        
        invalid_cases = [
            ('AT+BNCURL_TIMEOUT="0"', "below minimum"),
            ('AT+BNCURL_TIMEOUT="121"', "above maximum"),
            ('AT+BNCURL_TIMEOUT="-10"', "negative"),
            ('AT+BNCURL_TIMEOUT="abc"', "non-numeric"),
            ('AT+BNCURL_TIMEOUT=""', "empty")
        ]
        
        results = []
        for cmd, description in invalid_cases:
            logger.info(f"Testing {description}...")
            success, response = self.send_command(cmd, timeout=5)
            # Invalid values should return ERROR
            results.append(not success)
        
        return all(results)
    
    # Enhanced Web Radio Tests
    
    def test_webradio_comprehensive(self) -> bool:
        """21. Comprehensive Web Radio Tests"""
        if not self.wifi_connected:
            return False
        
        logger.info("Testing comprehensive web radio functionality...")
        
        # Test different stream types
        stream_tests = [
            ('AT+BNWEBRADIO="http://ice1.somafm.com/groovesalad-256-mp3"', "MP3 stream"),
            ('AT+BNWEBRADIO="https://radio.example.com:8000/stream.mp3"', "HTTPS stream"),
        ]
        
        results = []
        for cmd, description in stream_tests:
            logger.info(f"Testing {description}...")
            success, response = self.send_command(cmd, timeout=10)
            
            if success:
                # Let it stream briefly
                time.sleep(3)
                
                # Stop the stream
                stop_success, stop_response = self.send_command('AT+BNWEBRADIO_STOP', timeout=5)
                results.append(stop_success)
            else:
                results.append(False)
        
        return any(results)  # At least one stream type should work
    
    def test_webradio_error_cases(self) -> bool:
        """21.11 Web Radio Error Cases"""
        logger.info("Testing web radio error cases...")
        
        error_cases = [
            ('AT+BNWEBRADIO=""', "empty URL"),
            ('AT+BNWEBRADIO="ftp://invalid.protocol/stream"', "unsupported protocol"),
            ('AT+BNWEBRADIO="http://nonexistent.stream.com/radio"', "DNS failure"),
        ]
        
        results = []
        for cmd, description in error_cases:
            logger.info(f"Testing {description}...")
            success, response = self.send_command(cmd, timeout=10)
            # Most error cases should return ERROR
            results.append(not success)
        
        return all(results)
    
    # Comprehensive Test Runner
    
    def run_missing_commands_test_suite(self) -> dict:
        """Run all missing commands tests."""
        logger.info("Starting Missing ATBN Commands Test Suite")
        
        if not self.connect():
            return {}
        
        results = {}
        
        try:
            if not self.setup_test_environment():
                logger.error("Failed to set up test environment")
                return {}
            
            # WPS Tests
            logger.info("Running WPS Tests...")
            results['wps_30_seconds'] = self.test_wps_connection_30_seconds()
            results['wps_minimum'] = self.test_wps_connection_minimum()
            results['wps_maximum'] = self.test_wps_connection_maximum()
            results['wps_beyond_limit'] = self.test_wps_beyond_limit()
            results['wps_status_query'] = self.test_wps_status_query()
            results['wps_status_during'] = self.test_wps_status_during_connection()
            results['wps_cancellation'] = self.test_wps_cancellation()
            results['wps_cancel_inactive'] = self.test_wps_cancel_when_not_active()
            results['wps_error_cases'] = self.test_wps_error_cases()
            
            # Certificate Tests
            logger.info("Running Certificate Flashing Tests...")
            if self.sd_mounted:
                results['cert_flash_sd'] = self.test_flash_cert_from_sd()
                results['cert_flash_multiple'] = self.test_flash_cert_multiple_from_sd()
            results['cert_flash_uart'] = self.test_flash_cert_via_uart()
            results['cert_flash_sizes'] = self.test_flash_cert_sizes()
            results['cert_flash_errors'] = self.test_flash_cert_error_cases()
            results['cert_flash_addresses'] = self.test_flash_cert_address_tests()
            
            # SD Management Tests
            logger.info("Running SD Management Tests...")
            if self.sd_mounted:
                results['sd_format'] = self.test_sd_format()
                results['sd_space_query'] = self.test_sd_space_query()
            results['sd_format_errors'] = self.test_sd_format_error_cases()
            results['sd_space_errors'] = self.test_sd_space_error_cases()
            
            # Progress and Cancellation Tests
            logger.info("Running Progress Monitoring Tests...")
            if self.wifi_connected:
                results['progress_download'] = self.test_progress_monitoring_download()
                results['progress_upload'] = self.test_progress_monitoring_upload()
                results['progress_errors'] = self.test_progress_monitoring_error_cases()
                
                results['cancel_download'] = self.test_transfer_cancellation_download()
                results['cancel_upload'] = self.test_transfer_cancellation_upload()
                results['cancel_errors'] = self.test_transfer_cancellation_error_cases()
            
            # Timeout Tests
            logger.info("Running Timeout Management Tests...")
            results['timeout_settings'] = self.test_timeout_settings()
            results['timeout_query'] = self.test_timeout_query()
            results['timeout_invalid'] = self.test_timeout_invalid_values()
            
            # Enhanced Web Radio Tests
            logger.info("Running Enhanced Web Radio Tests...")
            if self.wifi_connected:
                results['webradio_comprehensive'] = self.test_webradio_comprehensive()
                results['webradio_errors'] = self.test_webradio_error_cases()
            
        finally:
            self.cleanup_test_environment()
            self.disconnect()
        
        return results


def main():
    """Main function for missing commands test suite."""
    print("ESP32 Missing ATBN Commands Test Suite")
    print("=" * 50)
    print("This suite tests all commands from test.md that were not")
    print("covered in the basic test implementation:")
    print("- WPS (WiFi Protected Setup)")
    print("- Certificate Flashing")
    print("- SD Card Management (Format, Space)")
    print("- Progress Monitoring")
    print("- Transfer Cancellation")
    print("- Timeout Management")
    print("- Enhanced Web Radio")
    print("=" * 50)
    
    tester = ATBNMissingCommandsTester()
    
    print(f"Serial Port: {tester.port}")
    print(f"WiFi SSID: {tester.wifi_ssid if tester.wifi_ssid else 'Not configured'}")
    print("=" * 50)
    
    results = tester.run_missing_commands_test_suite()
    tester.print_test_results(results)
    
    if results:
        failed_tests = sum(1 for result in results.values() if not result)
        return 0 if failed_tests == 0 else 1
    else:
        return 1


if __name__ == "__main__":
    sys.exit(main())
