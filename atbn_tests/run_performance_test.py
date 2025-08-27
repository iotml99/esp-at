#!/usr/bin/env python3
"""
Quick performance test runner for ESP32C6 AT commands.
Runs a simplified version of the 50MB download performance test.
"""

import sys
import os

# Add the current directory to the Python path
sys.path.insert(0, os.path.dirname(__file__))

from performance_test import run_performance_tests

if __name__ == "__main__":
    print("ESP32C6 Quick Performance Test")
    print("="*40)
    print("This test will:")
    print("1. Mount SD card and measure timing")
    print("2. Connect to WiFi and measure timing") 
    print("3. Download test files (1MB, 10MB, 80MB) to SD card")
    print("4. Test httpbin.org API response times")
    print("5. Measure download speed and performance")
    print()
    print("Test Sources:")
    print("- bones.ch/media/qr/ - Reliable test files")
    print("- httpbin.org - API testing and controlled data")
    print()
    print("Requirements:")
    print("- ESP32C6 connected via UART1 (GPIO 6/7)")
    print("- SD card inserted and properly wired")
    print("- WiFi credentials in config.ini")
    print("- Stable internet connection")
    print()
    
    response = input("Continue with performance test? (y/N): ")
    if response.lower() != 'y':
        print("Performance test cancelled.")
        sys.exit(0)
    
    print("\nStarting performance test...")
    print("This may take several minutes for larger files...")
    print("Tests: 1MB → 10MB → 80MB downloads")
    print("-" * 40)
    
    success = run_performance_tests()
    
    if success:
        print("\n✓ Performance test completed successfully!")
        sys.exit(0)
    else:
        print("\n✗ Performance test failed!")
        sys.exit(1)
