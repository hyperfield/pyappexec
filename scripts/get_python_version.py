import sys

def get_version():
    print(f"{sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}")

if __name__ == "__main__":
    get_version()
