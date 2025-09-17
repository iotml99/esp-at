#!/usr/bin/env python3
"""
ESP32-AT CA Certificate Generator

This script generates CA certificate files from the most important Certificate Authorities
for use with the ESP32-AT BNCERT system. It extracts individual certificates from the
system CA bundle and prepares them for copying to an SD card.

Usage:
    python generate_ca_certificates.py [options]

Requirements:
    - Python 3.6+
    - OpenSSL available in PATH (included with Git Bash on Windows)
    - System CA bundle file (usually from OpenSSL installation)

Author: ESP32-AT Team
Date: September 2025
"""

import sys
import subprocess
import argparse
from pathlib import Path
from typing import List, Dict, Optional
import re

# Most important Certificate Authorities used by common web services
IMPORTANT_CAS = [
    "DigiCert High Assurance EV Root CA",
    "DigiCert Global Root CA", 
    "DigiCert Global Root G2",
    "DigiCert Global Root G3",
    "Let's Encrypt Authority X3",
    "ISRG Root X1",  # Let's Encrypt root
    "GeoTrust Global CA",
    "GeoTrust Primary Certification Authority",
    "VeriSign Class 3 Public Primary Certification Authority - G5",
    "Sectigo RSA Domain Validation Secure Server CA",
    "Comodo CA Limited",
    "Baltimore CyberTrust Root", 
    "AddTrust External CA Root",
    "Amazon Root CA 1",
    "Amazon Root CA 2", 
    "Amazon Root CA 3",
    "Amazon Root CA 4",
    "GlobalSign Root CA",
    "GlobalSign Root CA - R3",
    "Entrust Root Certification Authority",
    "Entrust Root Certification Authority - G2",
    "QuoVadis Root CA 2",
    "Starfield Services Root Certificate Authority - G2",
    "Go Daddy Root Certificate Authority - G2",
    "Microsoft RSA Root Certificate Authority 2017",
    "Cloudflare Inc ECC CA-3",
    "Google Trust Services - GlobalSign Root CA-R2"
]



class CertificateGenerator:
    def __init__(self, output_dir: str = "sd_card_certs", verbose: bool = False):
        self.output_dir = Path(output_dir)
        self.verbose = verbose
        self.ca_bundle_path: Optional[Path] = None
        self.extracted_certs: List[Dict] = []
        
    def log(self, message: str, force: bool = False):
        """Print message if verbose mode is enabled or force is True"""
        if self.verbose or force:
            print(f"[INFO] {message}")
            
    def error(self, message: str):
        """Print error message"""
        print(f"[ERROR] {message}", file=sys.stderr)
        
    def validate_ca_bundle(self, bundle_path: str) -> bool:
        """Validate the provided CA bundle file path"""
        path = Path(bundle_path)
        if not path.exists():
            self.error(f"CA bundle file not found: {bundle_path}")
            return False
        if not path.is_file():
            self.error(f"CA bundle path is not a file: {bundle_path}")
            return False
        
        self.ca_bundle_path = path
        self.log(f"Using CA bundle: {path}", force=True)
        return True
        
    def check_openssl(self) -> bool:
        """Check if OpenSSL is available"""
        try:
            subprocess.run(["openssl", "version"], 
                         capture_output=True, check=True)
            self.log("OpenSSL found and working")
            return True
        except (subprocess.CalledProcessError, FileNotFoundError):
            self.error("OpenSSL not found. Please install OpenSSL or Git Bash.")
            return False
            
    def extract_certificates(self) -> bool:
        """Extract individual certificates from CA bundle"""
        if not self.ca_bundle_path or not self.ca_bundle_path.exists():
            self.error("CA bundle path not found")
            return False
            
        self.log("Extracting individual certificates from bundle...")
        
        try:
            with open(self.ca_bundle_path, 'r', encoding='utf-8') as f:
                bundle_content = f.read()
        except Exception as e:
            self.error(f"Failed to read CA bundle: {e}")
            return False
            
        # Split certificates using regex
        cert_pattern = re.compile(
            r'(-----BEGIN CERTIFICATE-----.*?-----END CERTIFICATE-----)', 
            re.DOTALL
        )
        
        certificates = cert_pattern.findall(bundle_content)
        self.log(f"Found {len(certificates)} certificates in bundle")
        
        # Process each certificate
        for i, cert_content in enumerate(certificates, 1):
            cert_info = self._process_certificate(cert_content, i)
            if cert_info:
                self.extracted_certs.append(cert_info)
                
        self.log(f"Successfully processed {len(self.extracted_certs)} certificates")
        return len(self.extracted_certs) > 0
        
    def _process_certificate(self, cert_content: str, cert_num: int) -> Optional[Dict]:
        """Process individual certificate and extract metadata"""
        try:
            # Create temporary file for OpenSSL processing
            temp_cert = Path(f"temp_cert_{cert_num}.pem")
            with open(temp_cert, 'w') as f:
                f.write(cert_content)
                
            # Extract certificate information using OpenSSL
            try:
                # Get subject information
                result = subprocess.run([
                    "openssl", "x509", "-in", str(temp_cert), 
                    "-subject", "-noout"
                ], capture_output=True, text=True, check=True)
                subject = result.stdout.strip()
                
                # Get issuer information  
                result = subprocess.run([
                    "openssl", "x509", "-in", str(temp_cert),
                    "-issuer", "-noout"
                ], capture_output=True, text=True, check=True)
                issuer = result.stdout.strip()
                
                # Get validity dates
                result = subprocess.run([
                    "openssl", "x509", "-in", str(temp_cert),
                    "-dates", "-noout"
                ], capture_output=True, text=True, check=True)
                dates = result.stdout.strip()
                
                # Calculate certificate size
                cert_size = len(cert_content.encode('utf-8'))
                
                cert_info = {
                    'number': cert_num,
                    'content': cert_content,
                    'subject': subject,
                    'issuer': issuer, 
                    'dates': dates,
                    'size': cert_size,
                    'common_name': self._extract_cn(subject),
                    'is_important': False,
                    'filename': None
                }
                
                # Check if this is an important CA
                for important_ca in IMPORTANT_CAS:
                    if important_ca.lower() in subject.lower() or important_ca.lower() in issuer.lower():
                        cert_info['is_important'] = True
                        # Generate safe filename
                        safe_name = re.sub(r'[^a-zA-Z0-9\-_]', '_', important_ca)
                        safe_name = re.sub(r'_+', '_', safe_name).strip('_')
                        cert_info['filename'] = f"{safe_name}.pem"
                        break
                        
                return cert_info
                
            finally:
                # Clean up temp file
                if temp_cert.exists():
                    temp_cert.unlink()
                    
        except Exception as e:
            self.log(f"Failed to process certificate {cert_num}: {e}")
            return None
            
    def _extract_cn(self, subject: str) -> str:
        """Extract Common Name from certificate subject"""
        # Look for CN= in subject
        match = re.search(r'CN\s*=\s*([^,/]+)', subject)
        if match:
            return match.group(1).strip()
        return "Unknown"
        
    def generate_certificate_files(self, important_only: bool = True) -> bool:
        """Generate certificate files for SD card"""
        if not self.extracted_certs:
            self.error("No certificates extracted. Run extract_certificates() first.")
            return False
            
        # Create output directory
        self.output_dir.mkdir(parents=True, exist_ok=True)
        self.log(f"Creating certificate files in: {self.output_dir}")
        
        # Filter certificates if needed
        certs_to_save = []
        if important_only:
            certs_to_save = [cert for cert in self.extracted_certs if cert['is_important']]
            self.log(f"Saving {len(certs_to_save)} important certificates")
        else:
            certs_to_save = self.extracted_certs
            self.log(f"Saving all {len(certs_to_save)} certificates")
            
        # Check certificate sizes
        oversized_certs = [cert for cert in certs_to_save if cert['size'] > 4096]
        if oversized_certs:
            self.log(f"WARNING: {len(oversized_certs)} certificates exceed 4KB limit:")
            for cert in oversized_certs:
                self.log(f"  - {cert['common_name']}: {cert['size']} bytes")
                
        # Save certificates
        saved_count = 0
        flash_commands = []
        
        for i, cert in enumerate(certs_to_save):
            if cert['size'] > 4096:
                self.log(f"Skipping oversized certificate: {cert['common_name']} ({cert['size']} bytes)")
                continue
                
            # Determine filename
            if cert['filename']:
                filename = cert['filename']
            else:
                filename = f"ca_cert_{cert['number']:03d}.pem"
                
            file_path = self.output_dir / filename
            
            try:
                with open(file_path, 'w', encoding='utf-8') as f:
                    f.write(cert['content'])
                    
                saved_count += 1
                address = 0x380000 + (i * 0x1000)
                flash_commands.append(f"AT+BNFLASH_CERT=0x{address:X},@/certs/{filename}")
                
                if self.verbose:
                    self.log(f"Saved: {filename} ({cert['size']} bytes) - {cert['common_name']}")
                    
            except Exception as e:
                self.error(f"Failed to save {filename}: {e}")
                
        # Generate flash commands file
        commands_file = self.output_dir / "flash_commands.txt"
        try:
            with open(commands_file, 'w') as f:
                f.write("# ESP32-AT Certificate Flash Commands\n")
                f.write("# Copy these certificates to SD card /certs/ directory\n")
                f.write("# Then execute these AT commands:\n\n")
                for cmd in flash_commands:
                    f.write(f"{cmd}\n")
                    
            self.log(f"Flash commands saved to: {commands_file}", force=True)
            
        except Exception as e:
            self.error(f"Failed to save flash commands: {e}")
            
        # Generate summary
        self._generate_summary(saved_count, len(certs_to_save))
        
        return saved_count > 0
        
    def _generate_summary(self, saved_count: int, total_count: int):
        """Generate summary file with certificate information"""
        summary_file = self.output_dir / "certificate_summary.txt"
        
        try:
            with open(summary_file, 'w') as f:
                f.write("ESP32-AT Certificate Bundle Summary\n")
                f.write("=" * 40 + "\n\n")
                f.write(f"Generated: {saved_count}/{total_count} certificates\n")
                f.write(f"Source: {self.ca_bundle_path}\n")
                f.write(f"Output: {self.output_dir}\n\n")
                
                f.write("Important Certificate Authorities Included:\n")
                f.write("-" * 40 + "\n")
                
                important_saved = [cert for cert in self.extracted_certs 
                                 if cert['is_important'] and cert['size'] <= 4096]
                
                for cert in important_saved:
                    f.write(f"* {cert['common_name']} ({cert['size']} bytes)\n")
                    
                f.write(f"\nTotal partition usage: {len(important_saved)} / 64 sectors\n")
                f.write(f"Storage used: {len(important_saved) * 4} KB / 256 KB\n\n")
                
                f.write("Installation Instructions:\n")
                f.write("-" * 25 + "\n")
                f.write("1. Copy all .pem files to SD card /certs/ directory\n")
                f.write("2. Insert SD card into ESP32 device\n")
                f.write("3. Execute AT commands from flash_commands.txt\n")
                f.write("4. Verify with: AT+BNCERT_LIST?\n\n")
                
                f.write("Test your installation with:\n")
                f.write('AT+BNCURL="GET","https://httpbin.org/get"\n')
                
            self.log(f"Summary saved to: {summary_file}", force=True)
            
        except Exception as e:
            self.error(f"Failed to save summary: {e}")

def main():
    parser = argparse.ArgumentParser(
        description="Generate CA certificates for ESP32-AT BNCERT system",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python generate_ca_certificates.py --bundle /usr/ssl/cert.pem
  python generate_ca_certificates.py --bundle C:\\Users\\user\\cert.pem --output my_certs --verbose
  python generate_ca_certificates.py --bundle /etc/ssl/cert.pem --all
  
Common CA bundle locations:
  Windows (Git Bash): /usr/ssl/cert.pem or /mingw64/ssl/cert.pem
  Windows (User):     C:\\Users\\username\\system_ca_bundle.pem  
  Linux:              /etc/ssl/cert.pem or /etc/ssl/certs/ca-bundle.crt
  macOS:              /System/Library/OpenSSL/cert.pem
        """
    )
    
    parser.add_argument(
        "--output", "-o",
        default="sd_card_certs",
        help="Output directory for certificate files (default: sd_card_certs)"
    )
    
    parser.add_argument(
        "--bundle", "-b",
        required=True,
        help="Path to CA bundle file (required)"
    )
    
    parser.add_argument(
        "--all", "-a",
        action="store_true",
        help="Generate all certificates, not just important ones"
    )
    
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Enable verbose output"
    )
    
    parser.add_argument(
        "--list-cas",
        action="store_true", 
        help="List important CAs and exit"
    )
    
    args = parser.parse_args()
    
    if args.list_cas:
        print("Important Certificate Authorities:")
        print("=" * 35)
        for i, ca in enumerate(IMPORTANT_CAS, 1):
            print(f"{i:2d}. {ca}")
        return 0
        
    # Initialize generator
    generator = CertificateGenerator(args.output, args.verbose)
    
    # Check OpenSSL availability
    if not generator.check_openssl():
        return 1
        
    # Validate specified CA bundle
    if not generator.validate_ca_bundle(args.bundle):
        return 1
            
    # Extract certificates
    if not generator.extract_certificates():
        return 1
        
    # Generate certificate files
    if not generator.generate_certificate_files(important_only=not args.all):
        return 1
        
    print("\nCertificate generation complete!")
    print(f"Files saved to: {generator.output_dir}")
    print("See flash_commands.txt for AT commands")
    print("See certificate_summary.txt for details")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())