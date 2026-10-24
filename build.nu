echo "Building Medusa without external dependencies..."
clang src/main.c src/debug.c src/parser.c src/bytecode.c src/interpreter.c src/structs.c -O2 -DNDEBUG -MD -fuse-ld=lld -Wall -Wextra -Wpedantic -o build/medusa.exe
echo "Build Complete!"
