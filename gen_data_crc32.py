import os
import zlib

def crc32(filename, chunksize=65536):
    """Compute the CRC-32 checksum of the contents of the given filename"""
    with open(filename, "rb") as f:
        checksum = 0
        while (chunk := f.read(chunksize)) :
            checksum = zlib.crc32(chunk, checksum)
        return checksum

data_path = r'data'

for path in os.listdir(data_path):
    # check if current path is a file
    if os.path.isfile(os.path.join(data_path, path)):
        file = data_path + "\\" + path
        if not file.endswith(".crc"):
            print(f"Generating CRC for {file}")
            with open(file+".crc", "w") as f:
                f.write(f'"{crc32(file):08x}"')
