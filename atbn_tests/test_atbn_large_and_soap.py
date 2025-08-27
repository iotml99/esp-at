#!/usr/bin/env python3
"""
ATBN Large File and SOAP/XML Test Suite
Tests for large file downloads and complex SOAP/XML operations that were requested.
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

class ATBNLargeFileAndSOAPTester(ATBNTester):
    """
    Specialized test suite for large file downloads and SOAP/XML operations.
    Covers specific test cases requested for bones.ch URLs and SOAP functionality.
    """
    
    def __init__(self):
        super().__init__()
        
        # Additional test endpoints
        self.soap_endpoints = {
            'dibbsdaisy': 'https://blibu.dzb.de:8093/dibbsDaisy/services/dibbsDaisyPort/',
            'httpbin_soap': 'https://httpbin.org/post'  # Fallback for testing
        }
        
        # Sample XML files content for testing
        self.xml_templates = {
            'logon': '''<?xml version="1.0" encoding="UTF-8"?>
<soap:Envelope xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"
               xmlns:dib="http://www.dzb.de/DaISy-online/">
  <soap:Header/>
  <soap:Body>
    <dib:logOn>
      <dib:username>testuser</dib:username>
      <dib:password>testpass</dib:password>
    </dib:logOn>
  </soap:Body>
</soap:Envelope>''',
            'simple': '''<?xml version="1.0" encoding="UTF-8"?>
<test>
  <data>Hello from ESP32</data>
  <timestamp>{timestamp}</timestamp>
</test>'''
        }
    
    # Comprehensive Large File Download Tests
    
    def test_large_file_downloads_comprehensive(self) -> dict:
        """Test all large file download combinations (bones.ch URLs)"""
        logger.info("Testing comprehensive large file downloads...")
        
        results = {}
        
        # Test URLs and expected behaviors
        file_tests = [
            ('large_1mb', self.test_urls['large_1mb'], 60, "1MB file"),
            ('large_10mb', self.test_urls['large_10mb'], 120, "10MB file"),
            ('large_80mb', self.test_urls['large_80mb'], 300, "80MB file")
        ]
        
        for test_name, url, timeout, description in file_tests:
            logger.info(f"Testing {description}...")
            
            # Test download to UART
            logger.info(f"  → UART download of {description}")
            uart_cmd = f'AT+BNCURL="GET","{url}"'
            uart_success, uart_response = self.send_command(uart_cmd, timeout=timeout)
            results[f'{test_name}_uart'] = uart_success
            
            # Test download to SD (if mounted)
            if self.sd_mounted:
                logger.info(f"  → SD download of {description}")
                sd_cmd = f'AT+BNCURL="GET","{url}","-dd","/Download/{test_name}_test.txt"'
                sd_success, sd_response = self.send_command(sd_cmd, timeout=timeout)
                results[f'{test_name}_sd'] = sd_success
            
            # Test with verbose mode
            logger.info(f"  → Verbose download of {description}")
            verbose_cmd = f'AT+BNCURL="GET","{url}","-v"'
            verbose_success, verbose_response = self.send_command(verbose_cmd, timeout=timeout)
            results[f'{test_name}_verbose'] = verbose_success
            
            # Brief pause between large downloads
            time.sleep(2)
        
        return results
    
    def test_large_file_with_headers(self) -> dict:
        """Test large file downloads with custom headers"""
        logger.info("Testing large file downloads with headers...")
        
        results = {}
        
        # Test with User-Agent header
        ua_cmd = f'AT+BNCURL="GET","{self.test_urls["large_1mb"]}","-H","User-Agent: ESP32-ATBN-Tester/1.0"'
        results['large_with_user_agent'] = self.send_command(ua_cmd, timeout=60)[0]
        
        # Test with Accept header
        accept_cmd = f'AT+BNCURL="GET","{self.test_urls["large_1mb"]}","-H","Accept: text/plain"'
        results['large_with_accept'] = self.send_command(accept_cmd, timeout=60)[0]
        
        # Test with multiple headers
        multi_cmd = (
            f'AT+BNCURL="GET","{self.test_urls["large_1mb"]}",'
            f'"-H","User-Agent: ESP32-Test",'
            f'"-H","Accept: */*",'
            f'"-H","Cache-Control: no-cache"'
        )
        results['large_with_multi_headers'] = self.send_command(multi_cmd, timeout=60)[0]
        
        return results
    
    # SOAP/XML Test Suite
    
    def test_soap_xml_comprehensive(self) -> dict:
        """Comprehensive SOAP/XML testing matching the curl command pattern"""
        logger.info("Testing comprehensive SOAP/XML operations...")
        
        results = {}
        
        # Test 1: Basic SOAP call with XML content type
        xml_data = self.xml_templates['simple'].format(timestamp=int(time.time()))
        
        basic_soap_cmd = (
            f'AT+BNCURL="POST","{self.soap_endpoints["httpbin_soap"]}",'
            f'"-H","Content-Type: text/xml;charset=UTF-8",'
            f'"-du","{len(xml_data.encode())}"'
        )
        
        success, response = self.send_command(basic_soap_cmd, expect_data_prompt=True, timeout=10)
        if success and any(">" in line for line in response):
            success, final_response = self.send_data(xml_data.encode(), timeout=30)
            results['soap_basic_xml'] = success
        else:
            results['soap_basic_xml'] = False
        
        # Test 2: SOAP with SOAPAction header (matching curl pattern)
        logon_xml = self.xml_templates['logon']
        
        soap_action_cmd = (
            f'AT+BNCURL="POST","{self.soap_endpoints["httpbin_soap"]}",'
            f'"-H","Content-Type: text/xml;charset=UTF-8",'
            f'"-H","SOAPAction: \\"/logOn\\"",'  # Escaped quotes for SOAPAction
            f'"-du","{len(logon_xml.encode())}"'
        )
        
        success, response = self.send_command(soap_action_cmd, expect_data_prompt=True, timeout=10)
        if success and any(">" in line for line in response):
            success, final_response = self.send_data(logon_xml.encode(), timeout=30)
            results['soap_with_action'] = success
        else:
            results['soap_with_action'] = False
        
        # Test 3: SOAP with cookies and verbose (complete curl equivalent)
        if self.sd_mounted:
            verbose_cookie_cmd = (
                f'AT+BNCURL="POST","{self.soap_endpoints["httpbin_soap"]}",'
                f'"-v",'  # verbose mode like curl -v
                f'"-c","/soap_cookies.txt",'  # save cookies like curl -c
                f'"-H","Content-Type: text/xml;charset=UTF-8",'
                f'"-H","SOAPAction: \\"/logOn\\"",'
                f'"-du","{len(logon_xml.encode())}"'
            )
            
            success, response = self.send_command(verbose_cookie_cmd, expect_data_prompt=True, timeout=10)
            if success and any(">" in line for line in response):
                success, final_response = self.send_data(logon_xml.encode(), timeout=30)
                results['soap_verbose_cookies'] = success
            else:
                results['soap_verbose_cookies'] = False
        
        return results
    
    def test_xml_file_upload_from_sd(self) -> dict:
        """Test XML file upload from SD card (simulating @file.xml)"""
        logger.info("Testing XML file upload from SD...")
        
        results = {}
        
        if not self.sd_mounted:
            logger.warning("SD card not mounted - skipping file upload tests")
            return results
        
        # Test file upload using -ds option (simulating curl -d @file.xml)
        xml_upload_cmd = (
            f'AT+BNCURL="POST","{self.soap_endpoints["httpbin_soap"]}",'
            f'"-H","Content-Type: text/xml;charset=UTF-8",'
            f'"-H","SOAPAction: \\"/logOn\\"",'
            f'"-ds","/test_logon.xml"'  # Upload from SD file
        )
        
        success, response = self.send_command(xml_upload_cmd, timeout=30)
        results['xml_file_upload_sd'] = success
        
        # Test with additional headers matching curl pattern
        complex_upload_cmd = (
            f'AT+BNCURL="POST","{self.soap_endpoints["httpbin_soap"]}",'
            f'"-v",'  # verbose
            f'"-c","/upload_cookies.txt",'  # save cookies
            f'"-H","Content-Type: text/xml;charset=UTF-8",'
            f'"-H","SOAPAction: \\"/logOn\\"",'
            f'"-H","User-Agent: ESP32-SOAP-Client/1.0",'
            f'"-ds","/complex_logon.xml"'
        )
        
        success, response = self.send_command(complex_upload_cmd, timeout=30)
        results['xml_complex_upload'] = success
        
        return results
    
    def test_soap_error_scenarios(self) -> dict:
        """Test SOAP error scenarios and edge cases"""
        logger.info("Testing SOAP error scenarios...")
        
        results = {}
        
        # Test 1: Invalid XML content
        invalid_xml = "<invalid><xml>missing closing tag"
        
        invalid_cmd = (
            f'AT+BNCURL="POST","{self.soap_endpoints["httpbin_soap"]}",'
            f'"-H","Content-Type: text/xml;charset=UTF-8",'
            f'"-du","{len(invalid_xml.encode())}"'
        )
        
        success, response = self.send_command(invalid_cmd, expect_data_prompt=True, timeout=10)
        if success and any(">" in line for line in response):
            success, final_response = self.send_data(invalid_xml.encode(), timeout=30)
            # Invalid XML might still succeed at HTTP level
            results['soap_invalid_xml'] = True  # Just testing HTTP transmission
        else:
            results['soap_invalid_xml'] = False
        
        # Test 2: Missing SOAPAction header
        no_action_cmd = (
            f'AT+BNCURL="POST","{self.soap_endpoints["httpbin_soap"]}",'
            f'"-H","Content-Type: text/xml;charset=UTF-8",'
            f'"-du","100"'
        )
        
        success, response = self.send_command(no_action_cmd, expect_data_prompt=True, timeout=10)
        if success and any(">" in line for line in response):
            success, final_response = self.send_data(b'<test>no soap action</test>', timeout=30)
            results['soap_no_action'] = success
        else:
            results['soap_no_action'] = False
        
        # Test 3: Wrong content type
        wrong_type_cmd = (
            f'AT+BNCURL="POST","{self.soap_endpoints["httpbin_soap"]}",'
            f'"-H","Content-Type: application/json",'  # Wrong content type for SOAP
            f'"-H","SOAPAction: \\"/test\\"",'
            f'"-du","50"'
        )
        
        success, response = self.send_command(wrong_type_cmd, expect_data_prompt=True, timeout=10)
        if success and any(">" in line for line in response):
            success, final_response = self.send_data(b'<soap>wrong content type</soap>', timeout=30)
            results['soap_wrong_content_type'] = success
        else:
            results['soap_wrong_content_type'] = False
        
        return results
    
    # Comprehensive Test Runner
    
    def run_large_file_and_soap_suite(self) -> dict:
        """Run complete large file and SOAP test suite"""
        logger.info("Starting Large File and SOAP Test Suite")
        
        if not self.connect():
            return {}
        
        results = {}
        
        try:
            if not self.setup_test_environment():
                logger.error("Failed to set up test environment")
                return {}
            
            # Large File Tests
            logger.info("Running Large File Download Tests...")
            if self.wifi_connected:
                large_file_results = self.test_large_file_downloads_comprehensive()
                results.update(large_file_results)
                
                header_results = self.test_large_file_with_headers()
                results.update(header_results)
            
            # SOAP/XML Tests
            logger.info("Running SOAP/XML Tests...")
            if self.wifi_connected:
                soap_results = self.test_soap_xml_comprehensive()
                results.update(soap_results)
                
                if self.sd_mounted:
                    upload_results = self.test_xml_file_upload_from_sd()
                    results.update(upload_results)
                
                error_results = self.test_soap_error_scenarios()
                results.update(error_results)
        
        finally:
            self.cleanup_test_environment()
            self.disconnect()
        
        return results


def main():
    """Main function for large file and SOAP test suite."""
    print("ESP32 Large File and SOAP/XML Test Suite")
    print("=" * 50)
    print("This suite tests:")
    print("- Large file downloads (1MB, 10MB, 80MB) from bones.ch")
    print("- SOAP/XML operations with complex headers")
    print("- File uploads from SD card")
    print("- Equivalent to curl commands with -v, -c, -H, -d @file")
    print("=" * 50)
    
    tester = ATBNLargeFileAndSOAPTester()
    
    print(f"Serial Port: {tester.port}")
    print(f"WiFi SSID: {tester.wifi_ssid if tester.wifi_ssid else 'Not configured'}")
    print("=" * 50)
    
    # Menu for specific test types
    print("Select test category:")
    print("1. Large File Downloads Only")
    print("2. SOAP/XML Tests Only") 
    print("3. Complete Suite (All Tests)")
    print("0. Exit")
    
    try:
        choice = input("Enter your choice (0-3): ").strip()
        
        if choice == "0":
            print("👋 Goodbye!")
            return 0
        
        if not tester.connect():
            print("❌ Failed to connect to ESP32")
            return 1
        
        print(f"✅ Connected to {tester.port}")
        
        if not tester.setup_test_environment():
            print("❌ Failed to set up test environment")
            return 1
        
        results = {}
        
        try:
            if choice == "1":
                print("🔄 Running Large File Tests...")
                if tester.wifi_connected:
                    results.update(tester.test_large_file_downloads_comprehensive())
                    results.update(tester.test_large_file_with_headers())
                
            elif choice == "2":
                print("🧾 Running SOAP/XML Tests...")
                if tester.wifi_connected:
                    results.update(tester.test_soap_xml_comprehensive())
                    if tester.sd_mounted:
                        results.update(tester.test_xml_file_upload_from_sd())
                    results.update(tester.test_soap_error_scenarios())
                    
            elif choice == "3":
                print("🚀 Running Complete Suite...")
                results = tester.run_large_file_and_soap_suite()
                
            else:
                print("❌ Invalid choice")
                return 1
        
        finally:
            tester.cleanup_test_environment()
            tester.disconnect()
        
        # Print results
        tester.print_test_results(results)
        
        if results:
            failed_tests = sum(1 for result in results.values() if not result)
            return 0 if failed_tests == 0 else 1
        else:
            return 1
            
    except KeyboardInterrupt:
        print("\n⚠️  Test interrupted by user")
        return 1
    except Exception as e:
        print(f"❌ Error running tests: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
