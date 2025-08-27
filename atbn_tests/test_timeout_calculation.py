#!/usr/bin/env python3
"""
Test script to verify timeout calculation for different file sizes.
This simulates the C timeout calculation logic to verify it works correctly for files up to 1GB.
"""

def calculate_timeout_ms(content_length):
    """
    Python version of the C timeout calculation function for testing.
    """
    if content_length == 0:
        return 300000  # Default 5 minutes for unknown size
    
    # Conservative timeout calculation for files up to 1GB
    # Assume minimum speed of 50KB/s (400 Kbps) for very slow connections
    # Add safety margin for connection setup, TLS handshake, and network fluctuations
    min_speed_bytes_per_sec = 50 * 1024  # 50 KB/s - conservative
    base_timeout_ms = 60000  # 60 seconds base for connection setup
    max_timeout_ms = 3600000  # 60 minutes maximum for 1GB files
    min_timeout_ms = 300000  # 5 minutes minimum
    
    # Calculate timeout with 2x safety margin
    calculated_timeout = base_timeout_ms + (content_length * 2000 // min_speed_bytes_per_sec)
    
    # Clamp to reasonable bounds
    if calculated_timeout < min_timeout_ms:
        calculated_timeout = min_timeout_ms
    if calculated_timeout > max_timeout_ms:
        calculated_timeout = max_timeout_ms
    
    return calculated_timeout

def format_size(size_bytes):
    """Format size in human readable format."""
    for unit in ['B', 'KB', 'MB', 'GB']:
        if size_bytes < 1024.0:
            return f"{size_bytes:.1f}{unit}"
        size_bytes /= 1024.0
    return f"{size_bytes:.1f}TB"

def format_time(time_ms):
    """Format time in human readable format."""
    if time_ms < 60000:
        return f"{time_ms/1000:.1f}s"
    elif time_ms < 3600000:
        return f"{time_ms/60000:.1f}min"
    else:
        return f"{time_ms/3600000:.1f}h"

def test_timeout_calculations():
    """Test timeout calculations for various file sizes."""
    
    test_sizes = [
        ("1MB", 1 * 1024 * 1024),
        ("10MB", 10 * 1024 * 1024),
        ("50MB", 50 * 1024 * 1024),
        ("100MB", 100 * 1024 * 1024),
        ("500MB", 500 * 1024 * 1024),
        ("1GB", 1 * 1024 * 1024 * 1024),
        ("Unknown (0)", 0),
    ]
    
    print("Auto-Timeout Calculation Test")
    print("=" * 60)
    print(f"{'Size':<12} {'Bytes':<12} {'Timeout':<12} {'Minutes':<8}")
    print("-" * 60)
    
    for size_name, size_bytes in test_sizes:
        timeout_ms = calculate_timeout_ms(size_bytes)
        timeout_min = timeout_ms / 60000.0
        
        print(f"{size_name:<12} {format_size(size_bytes):<12} {format_time(timeout_ms):<12} {timeout_min:.1f}")
    
    print("\nTimeout Calculation Logic:")
    print("- Minimum speed assumption: 50 KB/s (very conservative)")
    print("- Base timeout: 60 seconds (connection setup)")
    print("- Safety margin: 2x calculated time")
    print("- Minimum timeout: 5 minutes")
    print("- Maximum timeout: 60 minutes")
    
    # Verify specific requirements
    print("\nVerification:")
    
    # 1GB should get maximum timeout
    gb_timeout = calculate_timeout_ms(1024 * 1024 * 1024)
    assert gb_timeout == 3600000, f"1GB timeout should be 60min, got {gb_timeout/60000:.1f}min"
    print("✓ 1GB files get maximum 60-minute timeout")
    
    # Small files should get minimum timeout
    small_timeout = calculate_timeout_ms(1024 * 1024)  # 1MB
    assert small_timeout == 300000, f"1MB timeout should be 5min, got {small_timeout/60000:.1f}min"
    print("✓ Small files get minimum 5-minute timeout")
    
    # Unknown size should get default
    unknown_timeout = calculate_timeout_ms(0)
    assert unknown_timeout == 300000, f"Unknown size should be 5min, got {unknown_timeout/60000:.1f}min"
    print("✓ Unknown size gets 5-minute default timeout")
    
    print("\n✅ All timeout calculations are working correctly!")

if __name__ == "__main__":
    test_timeout_calculations()
