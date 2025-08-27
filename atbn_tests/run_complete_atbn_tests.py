#!/usr/bin/env python3
"""
Complete test runner for ALL ATBN commands inclu    print("📁 LARGE FILE & SOAP TESTS (New additions)")
    print("23. 📥 Large File Downloads (1MB, 10MB, 80MB)")
    print("24. 🧾 SOAP/XML Operations with Headers")
    print("25. 📤 File Upload from SD (XML/SOAP)")
    print()
    print("📋 SPECIFICATION COMPLI                results.update(current_tester.test_xml_file_upload_from_sd())
            
            elif choice == "26":
                print("🔍 Running Protocol Response Format Tests...")
                if not current_tester.wifi_connected:
                    print("❌ WiFi not connected - skipping protocol tests")
                    return 1
                results['len_post_format'] = current_tester.test_response_format_len_post_chunks()
                results['send_ok_confirmation'] = current_tester.test_send_ok_confirmation()
                results['verbose_ip_dialog'] = current_tester.test_verbose_ip_dialog()
            
            elif choice == "27":
                print("🌐 Running Port Addressing & Redirects Tests...")
                if not current_tester.wifi_connected:
                    print("❌ WiFi not connected - skipping redirect tests")
                    return 1
                results['port_http'] = current_tester.test_port_addressing_http()
                results['port_https'] = current_tester.test_port_addressing_https()
                results['redirect_301'] = current_tester.test_redirect_301_handling()
                results['redirect_302'] = current_tester.test_redirect_302_handling()
                results['redirect_303'] = current_tester.test_redirect_303_handling()
                results['redirect_chain'] = current_tester.test_multiple_redirects_chain()
            
            elif choice == "28":
                print("🔒 Running TLS Negotiation & Flow Control Tests...")
                if not current_tester.wifi_connected:
                    print("❌ WiFi not connected - skipping TLS tests")
                    return 1
                results['tls_negotiation'] = current_tester.test_tls_version_negotiation()
                results['flow_control_upload'] = current_tester.test_flow_control_large_upload()
                if current_tester.sd_mounted:
                    results['range_with_redirect'] = current_tester.test_range_download_with_redirect()
                    results['cookie_uart_forward'] = current_tester.test_cookie_immediate_uart_forward()
            
            elif choice == "29":
                print("📡 Running DZB Endpoint & UART High-Speed Tests...")
                results['dzb_soap_structure'] = current_tester.test_dzb_soap_endpoint_structure()
                results['uart_high_speed'] = current_tester.test_uart_high_speed_capability()
                if current_tester.wifi_connected and current_tester.sd_mounted:
                    results['spec_range_first'] = current_tester.test_spec_range_example_first_chunk()
                    results['spec_range_second'] = current_tester.test_spec_range_example_second_chunk()
            
            elif choice == "30":
                print("🚀 Running ALL BASIC TESTS...")
                results = current_tester.run_comprehensive_test_suite()
                
            elif choice == "31":
                print("🔥 Running ALL ADVANCED TESTS...")
                results = current_tester.run_advanced_test_suite()
                
            elif choice == "32":
                print("⭐ Running ALL MISSING COMMANDS...")
                results = current_tester.run_missing_commands_test_suite()
                
            elif choice == "33":
                print("📁 Running ALL LARGE FILE & SOAP TESTS...")
                results = current_tester.run_large_file_and_soap_suite()
                
            elif choice == "34":
                print("📋 Running ALL SPEC COMPLIANCE TESTS...")
                results = current_tester.run_spec_compliance_test_suite()
                
            elif choice == "35":rint("26. 🔍 Protocol Response Format (+LEN:, +POST:)")
    print("27. 🌐 Port Addressing & Redirects (301/302/303)")
    print("28. 🔒 TLS Negotiation & Flow Control")
    print("29. 📡 DZB Endpoint & UART High-Speed")
    print()
    print("💯 COMPLETE TEST SUITES")
    print("30. 🚀 ALL BASIC TESTS (1-11)")
    print("31. 🔥 ALL ADVANCED TESTS (12-15)")
    print("32. ⭐ ALL MISSING COMMANDS (16-22)")
    print("33. 📁 ALL LARGE FILE & SOAP (23-25)")
    print("34. 📋 ALL SPEC COMPLIANCE (26-29)")
    print("35. 🌟 EVERYTHING (Complete test coverage)")g ones from test.md.
This runner provides access to all test categories including WPS, Certificate Flashing,
SD Management, Progress Monitoring, Transfer Cancellation, and Enhanced Web Radio.
"""

import sys
from pathlib import Path

# Add current directory to path for imports
sys.path.insert(0, str(Path(__file__).parent))

def main():
    """Main test runner with comprehensive menu options."""
    print("=" * 70)
    print("ESP32 COMPLETE ATBN Test Runner")
    print("=" * 70)
    
    # Check for .env file
    env_path = Path(__file__).parent / '.env'
    if not env_path.exists():
        print(f"⚠️  .env file not found at {env_path}")
        print("📋 Please copy .env.example to .env and configure your settings")
        print("🔧 Using system environment variables or defaults")
        print()
    
    # Import test modules
    try:
        from test_atbn_comprehensive import ATBNTester
        from test_atbn_advanced import ATBNAdvancedTester  
        from test_atbn_missing_commands import ATBNMissingCommandsTester
        from test_atbn_large_and_soap import ATBNLargeFileAndSOAPTester
        from test_atbn_spec_compliance import ATBNSpecComplianceTester
    except ImportError as e:
        print(f"❌ Error importing test modules: {e}")
        print("🔧 Make sure all test files are present in the atbn_tests directory")
        return 1
    
    # Show configuration
    tester = ATBNTester()
    print(f"📡 Serial Port: {tester.port}")
    print(f"📶 WiFi SSID: {tester.wifi_ssid if tester.wifi_ssid else '❌ Not configured'}")
    print(f"🔧 Test Config: {sum(tester.test_config.values())} categories enabled")
    print()
    
    # Menu options
    print("Select test category to run:")
    print()
    print("📚 BASIC ATBN TESTS (Standard BNCURL commands)")
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
    print()
    print("🚀 ADVANCED ATBN TESTS (Extended functionality)")
    print("12. 📥 Range Download Tests")
    print("13. 📁 Large File Tests")
    print("14. 🔒 TLS Error Tests")
    print("15. 📻 Basic Web Radio Tests")
    print()
    print("🔥 MISSING COMMANDS (Previously unimplemented)")
    print("16. 📶 WPS (WiFi Protected Setup) Tests")
    print("17. 🔐 Certificate Flashing Tests")
    print("18. 🗃️  SD Card Management Tests")
    print("19. 📊 Progress Monitoring Tests")
    print("20. ⏹️  Transfer Cancellation Tests")
    print("21. ⏱️  Timeout Management Tests")
    print("22. 📻 Enhanced Web Radio Tests")
    print()
    print("� LARGE FILE & SOAP TESTS (New additions)")
    print("23. 📥 Large File Downloads (1MB, 10MB, 80MB)")
    print("24. 🧾 SOAP/XML Operations with Headers")
    print("25. 📤 File Upload from SD (XML/SOAP)")
    print()
    print("�💯 COMPLETE TEST SUITES")
    print("26. 🚀 ALL BASIC TESTS (1-11)")
    print("27. 🔥 ALL ADVANCED TESTS (12-15)")
    print("28. ⭐ ALL MISSING COMMANDS (16-22)")
    print("29. 📁 ALL LARGE FILE & SOAP (23-25)")
    print("30. 🌟 EVERYTHING (Complete test coverage)")
    print()
    print("0. ❌ Exit")
    print()
    
    try:
        choice = input("Enter your choice (0-35): ").strip()
        
        if choice == "0":
            print("👋 Goodbye!")
            return 0
        
        # Initialize appropriate tester based on choice
        if choice in ["1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "30"]:
            current_tester = ATBNTester()
        elif choice in ["12", "13", "14", "15", "31"]:
            current_tester = ATBNAdvancedTester()
        elif choice in ["16", "17", "18", "19", "20", "21", "22", "32"]:
            current_tester = ATBNMissingCommandsTester()
        elif choice in ["23", "24", "25", "33"]:
            current_tester = ATBNLargeFileAndSOAPTester()
        elif choice in ["26", "27", "28", "29", "34"]:
            current_tester = ATBNSpecComplianceTester()
        else:
            # For complete suites, we'll use the comprehensive tester
            current_tester = ATBNTester()
        
        # Connect to device
        print(f"🔌 Connecting to {current_tester.port}...")
        if not current_tester.connect():
            print("❌ Failed to connect to ESP32")
            return 1
        
        print(f"✅ Connected to {current_tester.port}")
        
        # Set up environment
        if not current_tester.setup_test_environment():
            print("❌ Failed to set up test environment")
            return 1
        
        print("✅ Test environment ready")
        print("🧪 Running tests...")
        print()
        
        results = {}
        
        try:
            if choice == "1":
                print("🔌 Running Basic Connectivity Tests...")
                results['basic_connectivity'] = current_tester.test_basic_connectivity()
                results['firmware_version'] = current_tester.test_firmware_version()
            
            elif choice == "2":
                print("📡 Running WiFi Connection Tests...")
                if current_tester.wifi_connected:
                    results['wifi_status'] = True
                    print("✅ WiFi already connected")
                else:
                    print("❌ WiFi not connected - check credentials in .env")
                    results['wifi_status'] = False
            
            elif choice == "3":
                print("💾 Running SD Card Operation Tests...")
                if current_tester.sd_mounted:
                    results['sd_mounted'] = True
                    print("✅ SD card mounted successfully")
                else:
                    print("❌ SD card not mounted - check hardware")
                    results['sd_mounted'] = False
            
            elif choice == "4":
                print("🌐 Running HTTP GET Tests...")
                if not current_tester.wifi_connected:
                    print("❌ WiFi not connected - skipping HTTP tests")
                    return 1
                results['get_https_uart'] = current_tester.test_get_https_to_uart()
                results['get_http_uart'] = current_tester.test_get_http_to_uart()
                results['get_404_error'] = current_tester.test_get_404_error()
                results['get_invalid_host'] = current_tester.test_get_invalid_host()
                if current_tester.sd_mounted:
                    results['get_https_sd'] = current_tester.test_get_https_to_sd()
                    results['get_redirect'] = current_tester.test_get_redirect_single()
            
            elif choice == "5":
                print("📤 Running HTTP POST Tests...")
                if not current_tester.wifi_connected:
                    print("❌ WiFi not connected - skipping HTTP tests")
                    return 1
                results['post_uart_uart'] = current_tester.test_post_uart_to_uart()
                results['post_empty'] = current_tester.test_post_empty_body()
                results['post_content_type'] = current_tester.test_post_with_content_type()
            
            elif choice == "6":
                print("📋 Running HTTP HEAD Tests...")
                if not current_tester.wifi_connected:
                    print("❌ WiFi not connected - skipping HTTP tests")
                    return 1
                results['head_basic'] = current_tester.test_head_basic()
                results['head_illegal_du'] = current_tester.test_head_with_illegal_du()
            
            elif choice == "7":
                print("🏷️  Running HTTP Header Tests...")
                if not current_tester.wifi_connected:
                    print("❌ WiFi not connected - skipping HTTP tests")
                    return 1
                results['multiple_headers'] = current_tester.test_multiple_headers()
                results['duplicate_header'] = current_tester.test_duplicate_header()
                results['nested_quotes'] = current_tester.test_nested_quotes_in_headers()
            
            elif choice == "8":
                print("🍪 Running Cookie Management Tests...")
                if not current_tester.wifi_connected or not current_tester.sd_mounted:
                    print("❌ WiFi or SD card not available - skipping cookie tests")
                    return 1
                results['save_cookie'] = current_tester.test_save_cookie()
                results['send_cookie'] = current_tester.test_send_cookie()
            
            elif choice == "9":
                print("🔍 Running Verbose Mode Tests...")
                if not current_tester.wifi_connected:
                    print("❌ WiFi not connected - skipping verbose tests")
                    return 1
                results['verbose_get'] = current_tester.test_verbose_get()
            
            elif choice == "10":
                print("❌ Running Error Handling Tests...")
                if not current_tester.wifi_connected:
                    print("❌ WiFi not connected - skipping error tests")
                    return 1
                results['unknown_option'] = current_tester.test_unknown_option()
                results['duplicate_option'] = current_tester.test_duplicate_option()
            
            elif choice == "11":
                print("🔄 Running Integration Tests...")
                if not current_tester.wifi_connected:
                    print("❌ WiFi not connected - skipping integration tests")
                    return 1
                results['weather_api'] = current_tester.test_weather_api_integration()
            
            elif choice == "12":
                print("📥 Running Range Download Tests...")
                if not current_tester.wifi_connected:
                    print("❌ WiFi not connected - skipping range tests")
                    return 1
                results.update(current_tester.test_range_downloads())
            
            elif choice == "13":
                print("📁 Running Large File Tests...")
                if not current_tester.wifi_connected:
                    print("❌ WiFi not connected - skipping large file tests")
                    return 1
                results.update(current_tester.test_large_file_operations())
            
            elif choice == "14":
                print("🔒 Running TLS Error Tests...")
                if not current_tester.wifi_connected:
                    print("❌ WiFi not connected - skipping TLS tests")
                    return 1
                results.update(current_tester.test_tls_error_scenarios())
            
            elif choice == "15":
                print("📻 Running Basic Web Radio Tests...")
                if not current_tester.wifi_connected:
                    print("❌ WiFi not connected - skipping web radio tests")
                    return 1
                results.update(current_tester.test_web_radio_basic())
            
            elif choice == "16":
                print("📶 Running WPS Tests...")
                results['wps_30_seconds'] = current_tester.test_wps_connection_30_seconds()
                results['wps_status_query'] = current_tester.test_wps_status_query()
                results['wps_cancellation'] = current_tester.test_wps_cancellation()
                results['wps_error_cases'] = current_tester.test_wps_error_cases()
            
            elif choice == "17":
                print("🔐 Running Certificate Flashing Tests...")
                results['cert_flash_uart'] = current_tester.test_flash_cert_via_uart()
                results['cert_flash_sizes'] = current_tester.test_flash_cert_sizes()
                results['cert_flash_errors'] = current_tester.test_flash_cert_error_cases()
                if current_tester.sd_mounted:
                    results['cert_flash_sd'] = current_tester.test_flash_cert_from_sd()
            
            elif choice == "18":
                print("🗃️  Running SD Card Management Tests...")
                if current_tester.sd_mounted:
                    results['sd_format'] = current_tester.test_sd_format()
                    results['sd_space_query'] = current_tester.test_sd_space_query()
                results['sd_format_errors'] = current_tester.test_sd_format_error_cases()
                results['sd_space_errors'] = current_tester.test_sd_space_error_cases()
            
            elif choice == "19":
                print("📊 Running Progress Monitoring Tests...")
                if not current_tester.wifi_connected:
                    print("❌ WiFi not connected - skipping progress tests")
                    return 1
                results['progress_download'] = current_tester.test_progress_monitoring_download()
                results['progress_upload'] = current_tester.test_progress_monitoring_upload()
                results['progress_errors'] = current_tester.test_progress_monitoring_error_cases()
            
            elif choice == "20":
                print("⏹️  Running Transfer Cancellation Tests...")
                if not current_tester.wifi_connected:
                    print("❌ WiFi not connected - skipping cancellation tests")
                    return 1
                results['cancel_download'] = current_tester.test_transfer_cancellation_download()
                results['cancel_upload'] = current_tester.test_transfer_cancellation_upload()
                results['cancel_errors'] = current_tester.test_transfer_cancellation_error_cases()
            
            elif choice == "21":
                print("⏱️  Running Timeout Management Tests...")
                results['timeout_settings'] = current_tester.test_timeout_settings()
                results['timeout_query'] = current_tester.test_timeout_query()
                results['timeout_invalid'] = current_tester.test_timeout_invalid_values()
            
            elif choice == "22":
                print("📻 Running Enhanced Web Radio Tests...")
                if not current_tester.wifi_connected:
                    print("❌ WiFi not connected - skipping web radio tests")
                    return 1
                results['webradio_comprehensive'] = current_tester.test_webradio_comprehensive()
                results['webradio_errors'] = current_tester.test_webradio_error_cases()
            
            elif choice == "23":
                print("� Running Large File Download Tests...")
                if not current_tester.wifi_connected:
                    print("❌ WiFi not connected - skipping large file tests")
                    return 1
                results.update(current_tester.test_large_file_downloads_comprehensive())
                results.update(current_tester.test_large_file_with_headers())
            
            elif choice == "24":
                print("🧾 Running SOAP/XML Tests...")
                if not current_tester.wifi_connected:
                    print("❌ WiFi not connected - skipping SOAP tests")
                    return 1
                results.update(current_tester.test_soap_xml_comprehensive())
                results.update(current_tester.test_soap_error_scenarios())
            
            elif choice == "25":
                print("📤 Running File Upload from SD Tests...")
                if not current_tester.wifi_connected or not current_tester.sd_mounted:
                    print("❌ WiFi or SD card not available - skipping file upload tests")
                    return 1
                results.update(current_tester.test_xml_file_upload_from_sd())
            
            elif choice == "26":
                print("�🚀 Running ALL BASIC TESTS...")
                results = current_tester.run_comprehensive_test_suite()
                
            elif choice == "27":
                print("🔥 Running ALL ADVANCED TESTS...")
                results = current_tester.run_advanced_test_suite()
                
            elif choice == "28":
                print("⭐ Running ALL MISSING COMMANDS...")
                results = current_tester.run_missing_commands_test_suite()
                
            elif choice == "29":
                print("📁 Running ALL LARGE FILE & SOAP TESTS...")
                results = current_tester.run_large_file_and_soap_suite()
                
            elif choice == "35":
                print("🌟 Running EVERYTHING (Complete Coverage)...")
                print("This will run all test suites sequentially...")
                
                # Run basic tests
                basic_tester = ATBNTester()
                if basic_tester.connect():
                    basic_tester.setup_test_environment()
                    basic_results = basic_tester.run_comprehensive_test_suite()
                    basic_tester.cleanup_test_environment()
                    basic_tester.disconnect()
                    results.update({f"basic_{k}": v for k, v in basic_results.items()})
                
                # Run advanced tests
                advanced_tester = ATBNAdvancedTester()
                if advanced_tester.connect():
                    advanced_tester.setup_test_environment()
                    advanced_results = advanced_tester.run_advanced_test_suite()
                    advanced_tester.cleanup_test_environment()
                    advanced_tester.disconnect()
                    results.update({f"advanced_{k}": v for k, v in advanced_results.items()})
                
                # Run missing commands tests
                missing_tester = ATBNMissingCommandsTester()
                if missing_tester.connect():
                    missing_tester.setup_test_environment()
                    missing_results = missing_tester.run_missing_commands_test_suite()
                    missing_tester.cleanup_test_environment()
                    missing_tester.disconnect()
                    results.update({f"missing_{k}": v for k, v in missing_results.items()})
                
                # Run large file and SOAP tests
                soap_tester = ATBNLargeFileAndSOAPTester()
                if soap_tester.connect():
                    soap_tester.setup_test_environment()
                    soap_results = soap_tester.run_large_file_and_soap_suite()
                    soap_tester.cleanup_test_environment()
                    soap_tester.disconnect()
                    results.update({f"soap_{k}": v for k, v in soap_results.items()})
                
                # Run specification compliance tests
                spec_tester = ATBNSpecComplianceTester()
                if spec_tester.connect():
                    spec_tester.setup_test_environment()
                    spec_results = spec_tester.run_spec_compliance_test_suite()
                    spec_tester.cleanup_test_environment()
                    spec_tester.disconnect()
                    results.update({f"spec_{k}": v for k, v in spec_results.items()})
                
                # Run large file and SOAP tests
                large_soap_tester = ATBNLargeFileAndSOAPTester()
                if large_soap_tester.connect():
                    large_soap_tester.setup_test_environment()
                    large_soap_results = large_soap_tester.run_large_file_and_soap_suite()
                    large_soap_tester.cleanup_test_environment()
                    large_soap_tester.disconnect()
                    results.update({f"large_soap_{k}": v for k, v in large_soap_results.items()})
            
            else:
                print("❌ Invalid choice")
                return 1
            
        finally:
            if choice != "35":  # Complete suite handles its own cleanup
                current_tester.cleanup_test_environment()
                current_tester.disconnect()
        
        # Print results
        print()
        current_tester.print_test_results(results)
        
        # Return exit code
        if results:
            failed_tests = sum(1 for result in results.values() if not result)
            total_tests = len(results)
            print(f"\n📊 Test Summary: {total_tests - failed_tests}/{total_tests} passed")
            return 0 if failed_tests == 0 else 1
        else:
            return 1
            
    except KeyboardInterrupt:
        print("\n⚠️  Test interrupted by user")
        return 1
    except Exception as e:
        print(f"❌ Error running tests: {e}")
        import traceback
        traceback.print_exc()
        return 1

if __name__ == "__main__":
    sys.exit(main())
