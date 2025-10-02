#!/usr/bin/env python3
import sys

if len(sys.argv) != 4:
    print("Usage: generate_cpp.py <input.class> <java_source.java> <output.cpp>")
    sys.exit(1)

class_file = sys.argv[1]
java_file = sys.argv[2]
cpp_file = sys.argv[3]

with open(java_file, "r") as f:
    java_source = f.read()

package_name = java_source.split("package ")[1].split(";")[0]

simple_class_name = java_source.split("class ")[1].split(" ")[0]

full_class_name = (
    f"{package_name}.{simple_class_name}" if package_name else simple_class_name
)

class_identifier = full_class_name.replace(".", "_")

with open(class_file, "rb") as f:
    data = f.read()

hex_bytes = ", ".join(f"0x{b:02x}" for b in data)

with open(cpp_file, "w") as f:
    _ = f.write(f"""// Auto-generated from {class_file}
// Class: {full_class_name}

namespace embedded {{
    unsigned char {class_identifier}[] = {{ {hex_bytes} }};
    unsigned int {class_identifier}_size = {len(data)};
    const char* {class_identifier}_name = "{full_class_name.replace(".", "/")}";
}}
""")

print(f"Generated {cpp_file} ({len(data)} bytes) for class {full_class_name}")
