import os
import zipfile
import io

BUNDLE_DIR = 'gemini-cli/bundle'
OUTPUT_HEADER = 'include/EmbeddedCli.h'

def zip_bundle():
    zip_buffer = io.BytesIO()
    # Use ZIP_DEFLATED for compression
    with zipfile.ZipFile(zip_buffer, 'w', zipfile.ZIP_DEFLATED) as zip_file:
        for root, dirs, files in os.walk(BUNDLE_DIR):
            for file in files:
                file_path = os.path.join(root, file)
                # Compute relative path for the zip archive
                arcname = os.path.relpath(file_path, BUNDLE_DIR)
                print(f"Adding {arcname}")
                zip_file.write(file_path, arcname)
    return zip_buffer.getvalue()

def write_header(data):
    with open(OUTPUT_HEADER, 'w') as f:
        f.write('#ifndef EMBEDDED_CLI_H\n')
        f.write('#define EMBEDDED_CLI_H\n\n')
        f.write('#include <vector>\n')
        f.write('#include <cstdint>\n\n')
        f.write(f'// Generated from {BUNDLE_DIR}\n')
        f.write(f'const size_t EMBEDDED_CLI_SIZE = {len(data)};\n')
        f.write('const unsigned char EMBEDDED_CLI_DATA[] = {\n')
        
        # Write hex data
        for i, byte in enumerate(data):
            if i % 12 == 0:
                f.write('    ')
            f.write(f'0x{byte:02x}, ')
            if i % 12 == 11:
                f.write('\n')
        
        f.write('\n};\n\n')
        f.write('#endif // EMBEDDED_CLI_H\n')

if __name__ == '__main__':
    if not os.path.exists(BUNDLE_DIR):
        print(f"Error: {BUNDLE_DIR} not found.")
        exit(1)
        
    print(f"Compressing {BUNDLE_DIR}...")
    data = zip_bundle()
    print(f"Compressed size: {len(data)} bytes")
    print(f"Writing to {OUTPUT_HEADER}...")
    write_header(data)
    print("Done.")