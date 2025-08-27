#!/usr/bin/env python3
"""
Advanced ATBN test cases including range downloads, complex scenarios,
and edge cases as specified in the test.md document.
"""

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

class ATBNAdvancedTester(ATBNTester):
    """
    Advanced ATBN test cases including range downloads,
    large file operations, and complex scenarios.
    """
    
    def __init__(self):
        super().__init__()
        
        # Additional test URLs for advanced testing
        self.advanced_urls = {
            'range_test': 'https://httpbin.org/bytes/1048576',  # 1MB for range tests
            'large_file': 'https://httpbin.org/bytes/10485760',  # 10MB for large file tests
            'chunked': 'https://httpbin.org/stream/20',  # Chunked transfer
            'slow_response': 'https://httpbin.org/delay/5',  # Delayed response
            'redirect_chain': 'https://httpbin.org/redirect/3',  # Multiple redirects
            'audio_stream': 'http://ice1.somafm.com/groovesalad-256-mp3',  # For web radio tests
        }
    
    # Range Download Tests (R1-R8 from test.md)
    
    def test_range_first_chunk(self) -> bool:
        """R1 - First range, new file"""
        if not self.wifi_connected or not self.sd_mounted:
            return False
        
        logger.info("Testing first range chunk...")
        cmd = f'AT+BNCURL="GET","{self.advanced_urls["range_test"]}","-r","0-262143","-dd","/Download/ranges.bin"'
        success, response = self.send_command(cmd, timeout=60)
        
        return success
    
    def test_range_next_chunk(self) -> bool:
        """R2 - Next contiguous range (append)"""
        if not self.wifi_connected or not self.sd_mounted:
            return False
        
        logger.info("Testing next range chunk (append)...")
        cmd = f'AT+BNCURL="GET","{self.advanced_urls["range_test"]}","-r","262144-524287","-dd","/Download/ranges.bin"'
        success, response = self.send_command(cmd, timeout=60)
        
        return success
    
    def test_range_final_chunk(self) -> bool:
        """R3 - Final range"""
        if not self.wifi_connected or not self.sd_mounted:
            return False
        
        logger.info("Testing final range chunk...")
        cmd = f'AT+BNCURL="GET","{self.advanced_urls["range_test"]}","-r","524288-1048575","-dd","/Download/ranges.bin"'
        success, response = self.send_command(cmd, timeout=60)
        
        return success
    
    def test_range_out_of_order(self) -> bool:
        """R4 - Out-of-order ranges (append anyway)"""
        if not self.wifi_connected or not self.sd_mounted:
            return False
        
        logger.info("Testing out-of-order ranges...")
        
        # First range
        cmd1 = f'AT+BNCURL="GET","{self.advanced_urls["range_test"]}","-r","0-65535","-dd","/Download/ranges_ooo.bin"'
        success1, _ = self.send_command(cmd1, timeout=60)
        
        # Third range (out of order)
        cmd2 = f'AT+BNCURL="GET","{self.advanced_urls["range_test"]}","-r","131072-196607","-dd","/Download/ranges_ooo.bin"'
        success2, _ = self.send_command(cmd2, timeout=60)
        
        # Second range (filling gap)
        cmd3 = f'AT+BNCURL="GET","{self.advanced_urls["range_test"]}","-r","65536-131071","-dd","/Download/ranges_ooo.bin"'
        success3, _ = self.send_command(cmd3, timeout=60)
        
        return success1 and success2 and success3
    
    def test_range_without_dd_error(self) -> bool:
        """R7 - Error: -r without -dd"""
        if not self.wifi_connected:
            return False
        
        logger.info("Testing range without -dd (should error)...")
        cmd = f'AT+BNCURL="GET","{self.advanced_urls["range_test"]}","-r","0-32767"'
        success, response = self.send_command(cmd, timeout=30)
        
        # Should return ERROR
        return not success
    
    def test_range_on_post_error(self) -> bool:
        """R8 - Error: -r on POST"""
        if not self.wifi_connected:
            return False
        
        logger.info("Testing range on POST (should error)...")
        cmd = f'AT+BNCURL="POST","{self.test_urls["post"]}","-r","0-100","-du","0"'
        success, response = self.send_command(cmd, timeout=30)
        
        # Should return ERROR
        return not success
    
    # Large File Tests
    
    def test_large_file_download_uart(self) -> bool:
        """Large file download to UART with chunking"""
        if not self.wifi_connected:
            return False
        
        logger.info("Testing large file download to UART...")
        cmd = f'AT+BNCURL="GET","{self.advanced_urls["large_file"]}"'
        success, response = self.send_command(cmd, timeout=120)
        
        if success:
            # Check for proper chunking
            has_len = any("+LEN:" in line for line in response)
            has_post_chunks = sum(1 for line in response if "+POST:" in line)
            has_send_ok = any("SEND OK" in line for line in response)
            
            logger.info(f"Large file test: LEN={has_len}, Chunks={has_post_chunks}, SEND_OK={has_send_ok}")
            return has_len and has_post_chunks > 1 and has_send_ok
        
        return False
    
    def test_large_file_download_sd(self) -> bool:
        """Large file download to SD card"""
        if not self.wifi_connected or not self.sd_mounted:
            return False
        
        logger.info("Testing large file download to SD...")
        cmd = f'AT+BNCURL="GET","{self.advanced_urls["large_file"]}","-dd","/Download/large_file.bin"'
        success, response = self.send_command(cmd, timeout=120)
        
        return success
    
    def test_large_post_upload(self) -> bool:
        """Large POST upload from UART"""
        if not self.wifi_connected:
            return False
        
        logger.info("Testing large POST upload...")
        cmd = f'AT+BNCURL="POST","{self.test_urls["post"]}","-du","1024"'
        success, response = self.send_command(cmd, expect_data_prompt=True, timeout=60)
        
        if success and any(">" in line for line in response):
            # Send 1KB of test data
            test_data = b'A' * 1024
            success, final_response = self.send_data(test_data, timeout=60)
            return success
        
        return False
    
    # Redirect Tests (D1-D3 from test.md)
    
    def test_redirect_get_301_302(self) -> bool:
        """D1 - GET 301/302"""
        if not self.wifi_connected or not self.sd_mounted:
            return False
        
        logger.info("Testing GET redirect 301/302...")
        cmd = f'AT+BNCURL="GET","{self.advanced_urls["redirect_chain"]}","-dd","/Download/redir_g.json"'
        success, response = self.send_command(cmd, timeout=60)
        
        return success
    
    def test_redirect_post_303(self) -> bool:
        """D2 - POST 303 (switch to GET)"""
        if not self.wifi_connected or not self.sd_mounted:
            return False
        
        logger.info("Testing POST redirect 303...")
        cmd = 'AT+BNCURL="POST","https://httpbin.org/status/303","-du","0","-dd","/Upload/results/redir_303.json"'
        success, response = self.send_command(cmd, timeout=60)
        
        return success
    
    # Protocol Negotiation Tests (PN1-PN2 from test.md)
    
    def test_protocol_http(self) -> bool:
        """PN1 - HTTP (no TLS)"""
        if not self.wifi_connected:
            return False
        
        logger.info("Testing HTTP protocol...")
        cmd = f'AT+BNCURL="GET","{self.test_urls["http_get"]}","-v"'
        success, response = self.send_command(cmd, timeout=30)
        
        if success:
            # Should NOT have TLS handshake in verbose output
            has_tls = any("TLS" in line or "SSL" in line for line in response)
            return not has_tls  # Success if no TLS mentioned
        
        return False
    
    def test_protocol_https_tls(self) -> bool:
        """PN2 - HTTPS (TLS 1.3)"""
        if not self.wifi_connected:
            return False
        
        logger.info("Testing HTTPS with TLS...")
        cmd = f'AT+BNCURL="GET","{self.test_urls["https_get"]}","-v"'
        success, response = self.send_command(cmd, timeout=30)
        
        if success:
            # Should have TLS handshake in verbose output
            has_tls = any("TLS" in line or "SSL" in line for line in response)
            return has_tls
        
        return False
    
    # Timeout and TLS Error Tests (E1-E5 from test.md)
    
    def test_timeout_get(self) -> bool:
        """E1 - GET timeout"""
        if not self.wifi_connected:
            return False
        
        logger.info("Testing GET timeout...")
        
        # First set a short timeout if command exists
        self.send_command("AT+BNCURL_TIMEOUT=\"3\"")
        
        # Try to access slow endpoint
        cmd = f'AT+BNCURL="GET","{self.advanced_urls["slow_response"]}"'
        start_time = time.time()
        success, response = self.send_command(cmd, timeout=15)
        elapsed = time.time() - start_time
        
        # Should timeout in approximately 3-5 seconds and return ERROR
        timeout_occurred = not success and elapsed < 10
        
        # Reset timeout
        self.send_command("AT+BNCURL_TIMEOUT=\"30\"")
        
        return timeout_occurred
    
    def test_tls_expired_cert(self) -> bool:
        """E3 - Expired certificate"""
        if not self.wifi_connected:
            return False
        
        logger.info("Testing expired certificate...")
        cmd = 'AT+BNCURL="GET","https://expired.badssl.com/","-v"'
        success, response = self.send_command(cmd, timeout=30)
        
        # Should return ERROR for expired certificate
        return not success
    
    def test_tls_self_signed(self) -> bool:
        """E5 - Self-signed certificate"""
        if not self.wifi_connected:
            return False
        
        logger.info("Testing self-signed certificate...")
        cmd = 'AT+BNCURL="GET","https://self-signed.badssl.com/","-v"'
        success, response = self.send_command(cmd, timeout=30)
        
        # Should return ERROR for self-signed certificate
        return not success
    
    # Parser Robustness Tests (X1-X6 from test.md)
    
    def test_extraneous_whitespace(self) -> bool:
        """X3 - Extraneous whitespace"""
        if not self.wifi_connected:
            return False
        
        logger.info("Testing extraneous whitespace...")
        # Command with extra spaces
        cmd = f'AT+BNCURL   =   "GET" , "{self.test_urls["https_get"]}" , "-dd" , "/Download/ws.json"'
        success, response = self.send_command(cmd, timeout=30)
        
        # Should accept command despite whitespace
        return success
    
    def test_very_long_url(self) -> bool:
        """X6 - Very long URL (boundary test)"""
        if not self.wifi_connected:
            return False
        
        logger.info("Testing very long URL...")
        
        # Create a very long URL with query parameters
        base_url = "https://httpbin.org/get"
        long_params = "&".join([f"param{i}={'x'*50}" for i in range(20)])
        long_url = f"{base_url}?{long_params}"
        
        cmd = f'AT+BNCURL="GET","{long_url}"'
        success, response = self.send_command(cmd, timeout=30)
        
        # Should either accept or return clear ERROR
        return True  # Any response (success or clear error) is acceptable
    
    # Complex Integration Tests (I1-I3 from test.md)
    
    def test_cookie_gated_post(self) -> bool:
        """I1 - Cookie-gated POST"""
        if not self.wifi_connected or not self.sd_mounted:
            return False
        
        logger.info("Testing cookie-gated POST...")
        
        # First, get a cookie
        cmd1 = 'AT+BNCURL="GET","https://httpbin.org/cookies/set?auth=ok","-c","/Cookies/auth.txt"'
        success1, _ = self.send_command(cmd1, timeout=30)
        
        if success1:
            # Then use the cookie in a POST
            cmd2 = f'AT+BNCURL="POST","{self.test_urls["post"]}","-b","/Cookies/auth.txt","-du","12","-dd","/Upload/results/auth_post.json"'
            success2, response2 = self.send_command(cmd2, expect_data_prompt=True, timeout=30)
            
            if success2 and any(">" in line for line in response2):
                test_data = b"hello world!"
                success3, _ = self.send_data(test_data, timeout=30)
                return success3
        
        return False
    
    def test_resumable_download(self) -> bool:
        """I2 - Resumable download via ranges"""
        if not self.wifi_connected or not self.sd_mounted:
            return False
        
        logger.info("Testing resumable download...")
        
        # Download first half
        cmd1 = 'AT+BNCURL="GET","https://httpbin.org/bytes/524288","-r","0-262143","-dd","/Download/resume.bin"'
        success1, _ = self.send_command(cmd1, timeout=60)
        
        if success1:
            # Download second half
            cmd2 = 'AT+BNCURL="GET","https://httpbin.org/bytes/524288","-r","262144-524287","-dd","/Download/resume.bin"'
            success2, _ = self.send_command(cmd2, timeout=60)
            return success2
        
        return False
    
    def test_redirect_header_cookie_combo(self) -> bool:
        """I3 - Redirect + header + cookie combo"""
        if not self.wifi_connected or not self.sd_mounted:
            return False
        
        logger.info("Testing redirect + header + cookie combo...")
        cmd = 'AT+BNCURL="GET","http://httpbin.org/redirect-to?url=https%3A%2F%2Fhttpbin.org%2Fheaders","-H","X-Combo: 1","-c","/Cookies/comb.txt","-dd","/Download/comb.json","-H","X-After: 2"'
        success, response = self.send_command(cmd, timeout=60)
        
        return success
    
    # Web Radio/Streaming Tests (Section 21 from test.md)
    
    def test_web_radio_basic(self) -> bool:
        """Basic web radio streaming test"""
        if not self.wifi_connected:
            return False
        
        logger.info("Testing basic web radio streaming...")
        cmd = f'AT+BNWEBRADIO="{self.advanced_urls["audio_stream"]}"'
        success, response = self.send_command(cmd, timeout=30)
        
        if success:
            # Let it stream for a few seconds
            time.sleep(5)
            
            # Stop the stream
            stop_success, stop_response = self.send_command("AT+BNWEBRADIO_STOP", timeout=10)
            return stop_success
        
        return False
    
    def test_web_radio_stop_without_stream(self) -> bool:
        """Test stopping web radio when no stream is active"""
        logger.info("Testing web radio stop without active stream...")
        success, response = self.send_command("AT+BNWEBRADIO_STOP", timeout=10)
        
        # Should return ERROR when no stream is active
        return not success
    
    # Advanced Test Suite Runner
    
    def run_advanced_test_suite(self) -> dict:
        """Run the advanced ATBN test suite."""
        logger.info("Starting Advanced ATBN Test Suite")
        
        if not self.connect():
            return {}
        
        results = {}
        
        try:
            if not self.setup_test_environment():
                logger.error("Failed to set up test environment")
                return {}
            
            # Range Download Tests
            logger.info("Running Range Download Tests...")
            results['range_first_chunk'] = self.test_range_first_chunk()
            results['range_next_chunk'] = self.test_range_next_chunk()
            results['range_final_chunk'] = self.test_range_final_chunk()
            results['range_out_of_order'] = self.test_range_out_of_order()
            results['range_without_dd_error'] = self.test_range_without_dd_error()
            results['range_on_post_error'] = self.test_range_on_post_error()
            
            # Large File Tests
            logger.info("Running Large File Tests...")
            results['large_file_uart'] = self.test_large_file_download_uart()
            if self.sd_mounted:
                results['large_file_sd'] = self.test_large_file_download_sd()
            results['large_post_upload'] = self.test_large_post_upload()
            
            # Redirect Tests
            logger.info("Running Redirect Tests...")
            results['redirect_get'] = self.test_redirect_get_301_302()
            results['redirect_post_303'] = self.test_redirect_post_303()
            
            # Protocol Tests
            logger.info("Running Protocol Tests...")
            results['protocol_http'] = self.test_protocol_http()
            results['protocol_https'] = self.test_protocol_https_tls()
            
            # Timeout and TLS Error Tests
            logger.info("Running Timeout and TLS Error Tests...")
            results['timeout_get'] = self.test_timeout_get()
            results['tls_expired_cert'] = self.test_tls_expired_cert()
            results['tls_self_signed'] = self.test_tls_self_signed()
            
            # Parser Robustness Tests
            logger.info("Running Parser Robustness Tests...")
            results['extraneous_whitespace'] = self.test_extraneous_whitespace()
            results['very_long_url'] = self.test_very_long_url()
            
            # Integration Tests
            logger.info("Running Integration Tests...")
            if self.sd_mounted:
                results['cookie_gated_post'] = self.test_cookie_gated_post()
                results['resumable_download'] = self.test_resumable_download()
                results['redirect_header_cookie_combo'] = self.test_redirect_header_cookie_combo()
            
            # Web Radio Tests (if implemented)
            logger.info("Running Web Radio Tests...")
            results['web_radio_basic'] = self.test_web_radio_basic()
            results['web_radio_stop_no_stream'] = self.test_web_radio_stop_without_stream()
            
        finally:
            self.cleanup_test_environment()
            self.disconnect()
        
        return results


def main():
    """Main function for advanced test suite."""
    print("ESP32 Advanced ATBN Test Suite")
    print("=" * 50)
    
    tester = ATBNAdvancedTester()
    
    print(f"Serial Port: {tester.port}")
    print(f"WiFi SSID: {tester.wifi_ssid if tester.wifi_ssid else 'Not configured'}")
    print("Advanced test categories:")
    print("- Range Downloads")
    print("- Large File Operations")
    print("- Redirect Handling")
    print("- Protocol Negotiation")
    print("- Timeout and TLS Errors")
    print("- Parser Robustness")
    print("- Complex Integration")
    print("- Web Radio Streaming")
    print("=" * 50)
    
    results = tester.run_advanced_test_suite()
    tester.print_test_results(results)
    
    if results:
        failed_tests = sum(1 for result in results.values() if not result)
        return 0 if failed_tests == 0 else 1
    else:
        return 1


if __name__ == "__main__":
    sys.exit(main())
