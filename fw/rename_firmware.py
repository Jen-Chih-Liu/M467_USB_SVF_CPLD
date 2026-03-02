# -*- coding: utf-8 -*-
import os
import shutil
from datetime import datetime
import hashlib

def calculate_hashes_for_file(filepath, chunk_size=4096):
    """
    Calculates the checksum (simple byte sum) and MD5 hash for a file
    in a memory-efficient way by reading it in chunks.
    """
    checksum = 0
    md5 = hashlib.md5()
    try:
        with open(filepath, 'rb') as f:
            while chunk := f.read(chunk_size):
                checksum += sum(chunk)
                md5.update(chunk)
        return checksum, md5.hexdigest()
    except FileNotFoundError:
        print(f"Error: Could not open {filepath} to calculate hashes.")
        return None, None
    except Exception as e:
        print(f"An error occurred during hash calculation: {e}")
        return None, None

def find_and_copy_firmware(target_filename, name_prefix, date_str, version_str):
    """
    Finds a firmware file, renames it with date and version, and copies it.
    """
    print(f"--- Processing {target_filename} ---")
    src_file_path = None

    # 1. Find the source file
    print(f"Searching for '{target_filename}'...")
    for root, dirs, files in os.walk("."):
        if target_filename in files:
            src_file_path = os.path.join(root, target_filename)
            print(f"File found: {src_file_path}")
            break

    # 2. Check if the source file was found
    if src_file_path is None:
        print(f"Error: Source file '{target_filename}' not found in any subdirectories.")
        print("Please ensure the project has been compiled successfully.\n")
        return

    # Get and print the file size
    file_size = os.path.getsize(src_file_path)
    print(f"File size: {file_size} bytes")

    # 3. Construct the new filename
    new_filename = f"{name_prefix}_{date_str}_{version_str}.bin"

    # 4. Copy and rename
    print(f"Preparing to copy...")
    print(f"Destination: {new_filename}")
    shutil.copy(src_file_path, new_filename)
    print("File copied successfully!")

    # 5. Calculate and print checksum and MD5 for the new file
    checksum, md5_hash = calculate_hashes_for_file(new_filename)
    if checksum is not None and md5_hash is not None:
        print(f"Checksum: {checksum}")
        print(f"MD5: {md5_hash}\n")
    

def main():
    """
    Main function to process all required firmware files.
    """
    today_str = datetime.now().strftime("%y_%m_%d")

    # --- Settings ---
    # Define the firmware files to process.
    # Key: source filename
    # Value: a tuple containing (output_name_prefix, firmware_version)
    firmware_map = {
        #                      (prefix,         version)
        "APROM_SUM.bin": ("msi_gpu_cp", "00_00_07"), # APROM version
        "LDROM.bin":     ("msi_gpu_ld", "00_00_01"), # LDROM version (please change)
    }

    for filename, (prefix, version) in firmware_map.items():
        find_and_copy_firmware(filename, prefix, today_str, version)

if __name__ == "__main__":
    main()
    os.system("pause")