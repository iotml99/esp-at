#!/usr/bin/env python3
"""
Simple test runner for ATBN commands using environment configuration.
Run this script to execute specific test categories.
"""

import sys
from pathlib import Path

# Add current directory to path for imports
sys.path.insert(0, str(Path(__file__).parent))

from test_atbn_comprehensive import ATBNTester

def main():
    """Main test runner with menu options."""
    print("=" * 60)
    print("ESP32 ATBN Test Runner")
    print("=" * 60)
    
    # Check for .env file
    env_path = Path(__file__).parent / '.env'
    if not env_path.exists():
        print(f"⚠️  .env file not found at {env_path}")
        print("📋 Please copy .env.example to .env and configure your settings")
        print("🔧 Using system environment variables or defaults")
        print()
    
    tester = ATBNTester()
    
    print(f"📡 Serial Port: {tester.port}")
    print(f"📶 WiFi SSID: {tester.wifi_ssid if tester.wifi_ssid else '❌ Not configured'}")
    print(f"🔧 Test Config: {sum(tester.test_config.values())} categories enabled")
    print()
    
    # Menu options
    print("Select test category to run:")
    print("1. 🔌 Basic Connectivity Tests")
    print("2. 📡 WiFi Connection Tests") 
    print("3. 💾 SD Card Operation Tests")
    print("4. 🌐 HTTP GET Tests")
    print("5. 📤 HTTP POST Tests")
    print("6. 📋 HTTP HEAD Tests")
    print("7. 🏷️  HTTP Header Tests")
    print("8. 🍪 Cookie Management Tests")
    print("9. 🔍 Verbose Mode Tests")
    print("10. ❌ Error Handling Tests")
    print("11. 🔄 Integration Tests")
    print("12. 🚀 ALL TESTS (Full Suite)")
    print("0. ❌ Exit")
    print()
    
    try:
        choice = input("Enter your choice (0-12): ").strip()
        
        if choice == "0":
            print("👋 Goodbye!")
            return 0
        
        # Connect to device
        if not tester.connect():
            print("❌ Failed to connect to ESP32")
            return 1
        
        print(f"✅ Connected to {tester.port}")
        
        # Set up environment
        if not tester.setup_test_environment():
            print("❌ Failed to set up test environment")
            return 1
        
        print("✅ Test environment ready")
        print("🧪 Running tests...")
        print()
        
        results = {}
        
        try:
            if choice == "1":
                print("🔌 Running Basic Connectivity Tests...")
                results['basic_connectivity'] = tester.test_basic_connectivity()
                results['firmware_version'] = tester.test_firmware_version()
            
            elif choice == "2":
                print("📡 Running WiFi Connection Tests...")
                if tester.wifi_connected:
                    results['wifi_status'] = True
                    print("✅ WiFi already connected")
                else:
                    print("❌ WiFi not connected - check credentials in .env")
                    results['wifi_status'] = False
            
            elif choice == "3":
                print("💾 Running SD Card Operation Tests...")
                if tester.sd_mounted:
                    results['sd_mounted'] = True
                    print("✅ SD card mounted successfully")
                else:
                    print("❌ SD card not mounted - check hardware")
                    results['sd_mounted'] = False
            
            elif choice == "4":
                print("🌐 Running HTTP GET Tests...")
                if not tester.wifi_connected:
                    print("❌ WiFi not connected - skipping HTTP tests")
                    return 1
                results['get_https_uart'] = tester.test_get_https_to_uart()
                results['get_http_uart'] = tester.test_get_http_to_uart()
                results['get_404_error'] = tester.test_get_404_error()
                results['get_invalid_host'] = tester.test_get_invalid_host()
                if tester.sd_mounted:
                    results['get_https_sd'] = tester.test_get_https_to_sd()
                    results['get_redirect'] = tester.test_get_redirect_single()
            
            elif choice == "5":
                print("📤 Running HTTP POST Tests...")
                if not tester.wifi_connected:
                    print("❌ WiFi not connected - skipping HTTP tests")
                    return 1
                results['post_uart_uart'] = tester.test_post_uart_to_uart()
                results['post_empty'] = tester.test_post_empty_body()
                results['post_content_type'] = tester.test_post_with_content_type()
            
            elif choice == "6":
                print("📋 Running HTTP HEAD Tests...")
                if not tester.wifi_connected:
                    print("❌ WiFi not connected - skipping HTTP tests")
                    return 1
                results['head_basic'] = tester.test_head_basic()
                results['head_illegal_du'] = tester.test_head_with_illegal_du()
            
            elif choice == "7":
                print("🏷️  Running HTTP Header Tests...")
                if not tester.wifi_connected:
                    print("❌ WiFi not connected - skipping HTTP tests")
                    return 1
                results['multiple_headers'] = tester.test_multiple_headers()
                results['duplicate_header'] = tester.test_duplicate_header()
                results['nested_quotes'] = tester.test_nested_quotes_in_headers()
            
            elif choice == "8":
                print("🍪 Running Cookie Management Tests...")
                if not tester.wifi_connected or not tester.sd_mounted:
                    print("❌ WiFi or SD card not available - skipping cookie tests")
                    return 1
                results['save_cookie'] = tester.test_save_cookie()
                results['send_cookie'] = tester.test_send_cookie()
            
            elif choice == "9":
                print("🔍 Running Verbose Mode Tests...")
                if not tester.wifi_connected:
                    print("❌ WiFi not connected - skipping verbose tests")
                    return 1
                results['verbose_get'] = tester.test_verbose_get()
            
            elif choice == "10":
                print("❌ Running Error Handling Tests...")
                if not tester.wifi_connected:
                    print("❌ WiFi not connected - skipping error tests")
                    return 1
                results['unknown_option'] = tester.test_unknown_option()
                results['duplicate_option'] = tester.test_duplicate_option()
            
            elif choice == "11":
                print("🔄 Running Integration Tests...")
                if not tester.wifi_connected:
                    print("❌ WiFi not connected - skipping integration tests")
                    return 1
                results['weather_api'] = tester.test_weather_api_integration()
            
            elif choice == "12":
                print("🚀 Running ALL TESTS...")
                results = tester.run_comprehensive_test_suite()
                
            else:
                print("❌ Invalid choice")
                return 1
            
        finally:
            tester.cleanup_test_environment()
            tester.disconnect()
        
        # Print results
        print()
        tester.print_test_results(results)
        
        # Return exit code
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
