import sys


def main():
    print(f"{sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}")
    print(sys.executable)


if __name__ == "__main__":
    main()
