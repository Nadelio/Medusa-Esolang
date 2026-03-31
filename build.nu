echo "Attempting to build Medusa..."
clang main.c -O2 -DNDEBUG -MD -fuse-ld=lld -I"C:/Program Files (x86)/string_slice/include" -L"C:/Program Files (x86)/string_slice/lib" -lstring_slice -Wall -Wextra -Wpedantic -o build/medusa.exe
echo "Build Complete!"