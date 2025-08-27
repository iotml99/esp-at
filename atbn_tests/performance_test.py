#!/usr/bin/env python3
"""
Performance test for ESP32C6 AT commands - 50MB download test.
This test downloads a large file (50MB) to the SD card and measures performance metrics.

Usage:
    python performance_test.py
    pytest performance_test.py -v
"""

import serial
import time
import configparser
import os
import sys
import pytest
from typing import Dict, Tuple
import re


def load_config():
    """Load configuration from config.ini file."""
    config = configparser.ConfigParser()
    config_file = os.path.join(os.path.dirname(__file__), 'config.ini')
    
    if not os.path.exists(config_file):
        print(f"Configuration file not found: {config_file}")
        return None
    
    try:
        config.read(config_file)
        return config
    except Exception as e:
        print(f"Error reading configuration: {e}")
        return None


class PerformanceTester:
    """Performance tester for ESP32C6 AT commands with timing measurements."""
    
    def __init__(self, port='COM3', baudrate=115200, timeout=30):
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.conn = None
        self.performance_data = {}
        
    def connect(self):
        """Connect to the ESP32C6."""
        try:
            self.conn = serial.Serial(self.port, self.baudrate, timeout=self.timeout)
            print(f"Connected to {self.port} at {self.baudrate} baud")
            return True
        except Exception as e:
            print(f"Connection failed: {e}")
            return False
    
    def send_at_command(self, command, extended_timeout=None) -> Tuple[bool, str, float]:
        """
        Send an AT command and return success status, response, and timing.
        
        Args:
            command: AT command to send
            extended_timeout: Optional extended timeout for long operations
            
        Returns:
            Tuple of (success, response_text, execution_time)
        """
        if not self.conn:
            return False, "Not connected", 0.0
            
        timeout = extended_timeout or self.timeout
        print(f"\nSending: {command}")
        
        start_time = time.time()
        self.conn.write(f"{command}\r\n".encode())
        
        response_lines = []
        command_start = time.time()
        
        while time.time() - command_start < timeout:
            if self.conn.in_waiting > 0:
                line = self.conn.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    print(f"Response: {line}")
                    response_lines.append(line)
                    if line in ['OK', 'ERROR', 'SEND OK', 'SEND FAIL']:
                        end_time = time.time()
                        execution_time = end_time - start_time
                        success = line in ['OK', 'SEND OK']
                        return success, '\n'.join(response_lines), execution_time
            time.sleep(0.1)
        
        # Timeout occurred
        end_time = time.time()
        execution_time = end_time - start_time
        response_text = '\n'.join(response_lines) if response_lines else "TIMEOUT"
        return False, response_text, execution_time
    
    def extract_download_stats(self, response: str) -> Dict[str, float]:
        """
        Extract download statistics from BNCURL response.
        
        Args:
            response: Raw response from BNCURL command
            
        Returns:
            Dictionary with download statistics
        """
        stats = {}
        
        # Extract total length
        len_match = re.search(r'\+LEN:(\d+),', response)
        if len_match:
            stats['total_bytes'] = int(len_match.group(1))
        
        # Extract file saved information
        saved_match = re.search(r'File saved \((\d+) bytes\)', response)
        if saved_match:
            stats['saved_bytes'] = int(saved_match.group(1))
        
        # Count POST chunks to estimate transfer efficiency
        post_chunks = response.count('+POST:')
        if post_chunks > 0:
            stats['chunks_received'] = post_chunks
        
        return stats
    
    def test_sd_card_mount(self) -> Tuple[bool, float]:
        """Test SD card mount operation and measure timing."""
        print("\n=== Testing SD Card Mount ===")
        success, response, exec_time = self.send_at_command("AT+BNSD_MOUNT")
        
        if success and "mounted successfully" in response:
            print(f"✓ SD card mounted successfully in {exec_time:.2f}s")
            return True, exec_time
        else:
            print(f"✗ SD card mount failed: {response}")
            return False, exec_time
    
    def test_wifi_connection(self, ssid: str, password: str) -> Tuple[bool, float]:
        """Test WiFi connection and measure timing."""
        print("\n=== Testing WiFi Connection ===")
        
        # Set station mode
        success, response, _ = self.send_at_command("AT+CWMODE=1")
        if not success:
            return False, 0.0
        
        # Connect to WiFi with extended timeout
        success, response, exec_time = self.send_at_command(
            f'AT+CWJAP="{ssid}","{password}"', 
            extended_timeout=30
        )
        
        if success and "WIFI GOT IP" in response:
            print(f"✓ WiFi connected in {exec_time:.2f}s")
            return True, exec_time
        else:
            print(f"✗ WiFi connection failed: {response}")
            return False, exec_time
    
    def test_large_download(self, url: str, filename: str) -> Dict[str, float]:
        """
        Test large file download to SD card and measure performance metrics.
        
        Args:
            url: URL to download from
            filename: Filename to save on SD card
            
        Returns:
            Dictionary with performance metrics
        """
        print(f"\n=== Testing Large Download: {url} ===")
        print(f"Saving to: {filename}")
        
        command = f'AT+BNCURL="GET","{url}","-dd","{filename}"'
        
        # Use extended timeout for large downloads (10 minutes)
        success, response, total_time = self.send_at_command(command, extended_timeout=600)
        
        # Extract download statistics
        stats = self.extract_download_stats(response)
        
        # Calculate performance metrics
        metrics = {
            'success': success,
            'total_time': total_time,
            'response': response
        }
        
        if stats.get('total_bytes'):
            total_mb = stats['total_bytes'] / (1024 * 1024)
            metrics['total_mb'] = total_mb
            metrics['speed_mbps'] = (total_mb * 8) / total_time if total_time > 0 else 0
            metrics['speed_mb_per_sec'] = total_mb / total_time if total_time > 0 else 0
            
            print("Download completed:")
            print(f"  Total size: {total_mb:.2f} MB")
            print(f"  Total time: {total_time:.2f} seconds")
            print(f"  Speed: {metrics['speed_mb_per_sec']:.2f} MB/s")
            print(f"  Speed: {metrics['speed_mbps']:.2f} Mbps")
            
            if stats.get('chunks_received'):
                metrics['chunks'] = stats['chunks_received']
                metrics['avg_chunk_time'] = total_time / stats['chunks_received']
                print(f"  Chunks received: {stats['chunks_received']}")
                print(f"  Avg chunk time: {metrics['avg_chunk_time']:.3f}s")
        
        metrics.update(stats)
        return metrics
    
    def disconnect(self):
        """Disconnect from ESP32C6."""
        if self.conn:
            self.conn.close()
            print("Disconnected")


# Test URLs for performance testing
TEST_URLS = {
    # bones.ch test files - reliable and fast
    '1mb_bones': 'https://bones.ch/media/qr/1M.txt',
    '10mb_bones': 'https://bones.ch/media/qr/10M.txt',
    '80mb_bones': 'https://bones.ch/media/qr/80M.txt',
    # httpbin.org for performance data and testing
    'httpbin_get': 'https://httpbin.org/get',
    'httpbin_json': 'https://httpbin.org/json',
    'httpbin_bytes_1mb': 'https://httpbin.org/bytes/1048576',  # 1MB
    'httpbin_bytes_10mb': 'https://httpbin.org/bytes/10485760',  # 10MB
}


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
            timeout = config.getint('serial', 'timeout', fallback=30)
            return {'port': port, 'baudrate': baudrate, 'timeout': timeout}
        except Exception:
            pass
    
    return {'port': 'COM3', 'baudrate': 115200, 'timeout': 30}


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
def perf_tester(tester_config):
    """Create and connect performance tester instance."""
    tester = PerformanceTester(**tester_config)
    
    if not tester.connect():
        pytest.skip("Could not connect to ESP32C6")
    
    yield tester
    
    tester.disconnect()


# Performance test functions
@pytest.mark.slow
@pytest.mark.sd_card
def test_sd_card_mount_performance(perf_tester):
    """Test SD card mount performance."""
    success, mount_time = perf_tester.test_sd_card_mount()
    assert success, "SD card mount failed"
    assert mount_time < 5.0, f"SD card mount too slow: {mount_time:.2f}s"
    
    # Store performance data
    perf_tester.performance_data['sd_mount_time'] = mount_time


@pytest.mark.slow
@pytest.mark.wifi
def test_wifi_connection_performance(perf_tester, wifi_config):
    """Test WiFi connection performance."""
    ssid = wifi_config['ssid']
    password = wifi_config['password']
    
    if not ssid or not password:
        pytest.skip("WiFi credentials not provided in config")
    
    success, connect_time = perf_tester.test_wifi_connection(ssid, password)
    assert success, "WiFi connection failed"
    assert connect_time < 30.0, f"WiFi connection too slow: {connect_time:.2f}s"
    
    # Store performance data
    perf_tester.performance_data['wifi_connect_time'] = connect_time


@pytest.mark.slow
@pytest.mark.sd_card
@pytest.mark.wifi
@pytest.mark.parametrize("url_name,url", [
    ('1mb_bones', TEST_URLS['1mb_bones']),
    ('10mb_bones', TEST_URLS['10mb_bones']),
    ('httpbin_1mb', TEST_URLS['httpbin_bytes_1mb']),
])
def test_performance_download_small(perf_tester, wifi_config, url_name, url):
    """Test performance with smaller files (1MB, 10MB) for quick testing."""
    # Skip if WiFi not configured
    if not wifi_config['ssid']:
        pytest.skip("WiFi credentials not provided")
    
    # Ensure SD card is mounted
    perf_tester.send_at_command("AT+BNSD_MOUNT")
    
    # Define filename
    filename = f"/sdcard/perf_test_{url_name}_{int(time.time())}.bin"
    
    # Perform download test
    metrics = perf_tester.test_large_download(url, filename)
    
    # Assertions for performance
    assert metrics['success'], f"Download failed: {metrics.get('response', 'Unknown error')}"
    
    if 'total_mb' in metrics:
        # Verify file size is reasonable
        expected_size = 1 if '1mb' in url_name.lower() else 10
        assert (expected_size * 0.9) <= metrics['total_mb'] <= (expected_size * 1.1), \
            f"Unexpected file size: {metrics['total_mb']:.2f} MB (expected ~{expected_size}MB)"
        
        # Performance benchmarks (adjust based on expected performance)
        assert metrics['speed_mb_per_sec'] > 0.05, f"Download too slow: {metrics['speed_mb_per_sec']:.3f} MB/s"
        assert metrics['total_time'] < 300, f"Download took too long: {metrics['total_time']:.2f}s"
        
        # Store performance data
        test_key = f"download_{url_name}"
        perf_tester.performance_data[test_key] = {
            'size_mb': metrics['total_mb'],
            'time_seconds': metrics['total_time'],
            'speed_mbps': metrics['speed_mbps'],
            'speed_mb_per_sec': metrics['speed_mb_per_sec']
        }
        
        print(f"\n✓ Performance Test Results for {url_name}:")
        print(f"  File size: {metrics['total_mb']:.2f} MB")
        print(f"  Download time: {metrics['total_time']:.2f} seconds")
        print(f"  Speed: {metrics['speed_mb_per_sec']:.2f} MB/s ({metrics['speed_mbps']:.2f} Mbps)")


@pytest.mark.slow
@pytest.mark.sd_card
@pytest.mark.wifi
@pytest.mark.parametrize("url_name,url", [
    ('80mb_bones', TEST_URLS['80mb_bones']),
])
def test_large_download_performance(perf_tester, wifi_config, url_name, url):
    """Test performance with large file (80MB) - the main performance test."""
    # Skip if WiFi not configured
    if not wifi_config['ssid']:
        pytest.skip("WiFi credentials not provided")
    
    # Ensure SD card is mounted
    perf_tester.send_at_command("AT+BNSD_MOUNT")
    
    # Define filename
    filename = f"/sdcard/perf_test_{url_name}_{int(time.time())}.bin"
    
    # Perform download test
    metrics = perf_tester.test_large_download(url, filename)
    
    # Assertions for performance
    assert metrics['success'], f"Download failed: {metrics.get('response', 'Unknown error')}"
    
    if 'total_mb' in metrics:
        # Verify file size is approximately 80MB (allow 10% variance)
        assert 70 <= metrics['total_mb'] <= 90, f"Unexpected file size: {metrics['total_mb']:.2f} MB"
        
        # Performance benchmarks for large file
        assert metrics['speed_mb_per_sec'] > 0.1, f"Download too slow: {metrics['speed_mb_per_sec']:.3f} MB/s"
        assert metrics['total_time'] < 800, f"Download took too long: {metrics['total_time']:.2f}s"
        
        # Store performance data
        test_key = f"download_{url_name}"
        perf_tester.performance_data[test_key] = {
            'size_mb': metrics['total_mb'],
            'time_seconds': metrics['total_time'],
            'speed_mbps': metrics['speed_mbps'],
            'speed_mb_per_sec': metrics['speed_mb_per_sec']
        }
        
        print(f"\n✓ Large File Performance Test Results for {url_name}:")
        print(f"  File size: {metrics['total_mb']:.2f} MB")
        print(f"  Download time: {metrics['total_time']:.2f} seconds")
        print(f"  Speed: {metrics['speed_mb_per_sec']:.2f} MB/s ({metrics['speed_mbps']:.2f} Mbps)")


@pytest.mark.wifi
def test_httpbin_api_performance(perf_tester, wifi_config):
    """Test httpbin.org API endpoints for quick performance validation."""
    # Skip if WiFi not configured
    if not wifi_config['ssid']:
        pytest.skip("WiFi credentials not provided")
    
    # Test basic httpbin endpoints
    test_endpoints = [
        ('httpbin_get', TEST_URLS['httpbin_get']),
        ('httpbin_json', TEST_URLS['httpbin_json']),
    ]
    
    for name, url in test_endpoints:
        print(f"\n=== Testing {name}: {url} ===")
        
        # Test without saving to SD card (stream to UART)
        command = f'AT+BNCURL="GET","{url}"'
        success, response, exec_time = perf_tester.send_at_command(command, extended_timeout=30)
        
        assert success, f"HTTPBin API test failed for {name}: {response}"
        assert exec_time < 30, f"HTTPBin API test too slow for {name}: {exec_time:.2f}s"
        
        print(f"✓ {name} completed in {exec_time:.2f}s")
        
        # Store performance data
        perf_tester.performance_data[f"api_{name}"] = {
            'response_time': exec_time,
            'success': success
        }


@pytest.mark.slow
def test_performance_summary(perf_tester):
    """Print comprehensive performance summary."""
    print("\n" + "="*60)
    print("PERFORMANCE TEST SUMMARY")
    print("="*60)
    
    data = perf_tester.performance_data
    
    if 'sd_mount_time' in data:
        print(f"SD Card Mount Time: {data['sd_mount_time']:.2f}s")
    
    if 'wifi_connect_time' in data:
        print(f"WiFi Connection Time: {data['wifi_connect_time']:.2f}s")
    
    # Summary of download tests
    download_tests = {k: v for k, v in data.items() if k.startswith('download_')}
    if download_tests:
        print("\nDownload Performance:")
        print("-" * 40)
        
        total_data = 0
        total_time = 0
        
        for test_name, metrics in download_tests.items():
            url_name = test_name.replace('download_', '')
            print(f"{url_name}:")
            print(f"  Size: {metrics['size_mb']:.2f} MB")
            print(f"  Time: {metrics['time_seconds']:.2f}s")
            print(f"  Speed: {metrics['speed_mb_per_sec']:.2f} MB/s")
            
            total_data += metrics['size_mb']
            total_time += metrics['time_seconds']
        
        if len(download_tests) > 1:
            avg_speed = total_data / total_time if total_time > 0 else 0
            print(f"\nOverall Average Speed: {avg_speed:.2f} MB/s")
    
    print("="*60)


def run_performance_tests():
    """Run performance tests in standalone mode."""
    config = load_config()
    
    if config:
        try:
            port = config.get('serial', 'port', fallback='COM3')
            baudrate = config.getint('serial', 'baudrate', fallback=115200)
            timeout = config.getint('serial', 'timeout', fallback=30)
            wifi_ssid = config.get('wifi', 'ssid', fallback='')
            wifi_password = config.get('wifi', 'password', fallback='')
        except Exception as e:
            print(f"Error reading configuration: {e}")
            return False
    else:
        port, baudrate, timeout = 'COM3', 115200, 30
        wifi_ssid = wifi_password = ''
    
    if not wifi_ssid or not wifi_password:
        print("Error: WiFi credentials required for performance testing")
        return False
    
    print("ESP32C6 Performance Test - 50MB Download")
    print("="*50)
    print(f"Serial Port: {port}")
    print(f"WiFi Network: {wifi_ssid}")
    print("="*50)
    
    tester = PerformanceTester(port=port, baudrate=baudrate, timeout=timeout)
    
    if not tester.connect():
        return False
    
    try:
        # Test SD card mount
        sd_success, sd_time = tester.test_sd_card_mount()
        if not sd_success:
            print("SD card mount failed - aborting performance test")
            return False
        
        # Test WiFi connection
        wifi_success, wifi_time = tester.test_wifi_connection(wifi_ssid, wifi_password)
        if not wifi_success:
            print("WiFi connection failed - aborting performance test")
            return False
        
        # Test download performance with multiple URLs
        results = {}
        
        # Start with smaller files for quicker testing
        test_sequence = [
            ('1mb_bones', TEST_URLS['1mb_bones']),
            ('httpbin_1mb', TEST_URLS['httpbin_bytes_1mb']),
            ('10mb_bones', TEST_URLS['10mb_bones']),
            ('80mb_bones', TEST_URLS['80mb_bones']),  # Main performance test
        ]
        
        for name, url in test_sequence:
            print(f"\nTesting with {name}...")
            filename = f"/sdcard/perf_test_{name}_{int(time.time())}.bin"
            
            try:
                metrics = tester.test_large_download(url, filename)
                if metrics['success']:
                    results[name] = metrics
                    print(f"✓ {name} download successful")
                    
                    # If this is the 80MB test, we have our main result
                    if '80mb' in name:
                        break
                else:
                    print(f"Failed with {name}: {metrics.get('response', 'Unknown error')}")
            except Exception as e:
                print(f"Error with {name}: {e}")
                continue
        
        # Print final results
        if results:
            print("\n" + "="*50)
            print("PERFORMANCE TEST RESULTS")
            print("="*50)
            print(f"SD Mount Time: {sd_time:.2f}s")
            print(f"WiFi Connect Time: {wifi_time:.2f}s")
            
            for name, metrics in results.items():
                if 'total_mb' in metrics:
                    print(f"\n{name} Download:")
                    print(f"  Size: {metrics['total_mb']:.2f} MB")
                    print(f"  Time: {metrics['total_time']:.2f}s")
                    print(f"  Speed: {metrics['speed_mb_per_sec']:.2f} MB/s")
                    print(f"  Speed: {metrics['speed_mbps']:.2f} Mbps")
            
            return True
        else:
            print("All download tests failed")
            return False
        
    finally:
        tester.disconnect()


if __name__ == "__main__":
    print("ESP32C6 Performance Tester (50MB Download)")
    print("Make sure ESP32C6 is connected via UART1 (GPIO 6/7)")
    print("SD card must be inserted and WiFi credentials configured")
    print("")
    print("Run modes:")
    print("  python performance_test.py       - Direct execution")
    print("  pytest performance_test.py -v    - Pytest execution")
    print("-" * 60)
    
    success = run_performance_tests()
    sys.exit(0 if success else 1)
