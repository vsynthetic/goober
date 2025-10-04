#!/usr/bin/env python3
import os
import struct
import subprocess
import sys


class arg_flag:
    def __init__(self, names: tuple[str]) -> None:
        self.names: tuple[str] = names
        self.value: str | None = None
        pass

    def expect(self, message: str) -> str:
        if self.value is None:
            raise Exception(message)

        return self.value


class arg_parser:
    def __init__(self) -> None:
        self.__flag_map: dict[str, arg_flag] = {}
        self.__flags: list[arg_flag] = []

    def add(self, *vals: str) -> arg_flag:
        flag = arg_flag(tuple[str](vals))

        for s in vals:
            self.__flag_map[s] = flag

        self.__flags.append(flag)

        return flag

    def __find_flag(self, name: str) -> arg_flag | None:
        if name in self.__flag_map.keys():
            return self.__flag_map[name]
        return None

    def parse(self, argv: list[str]) -> None:
        index = 0
        while index < len(argv) - 1:
            current_flag = self.__find_flag(argv[index])
            index += 1
            if current_flag is None:
                continue

            current_flag.value = argv[index]
            index += 1


class cmdbuf:
    def __init__(self) -> None:
        self.args: list[str] = []

    def append(self, *vals: str) -> None:
        self.args.extend(vals)

    def exec(self) -> int:
        print("[INFO] Running:", " ".join(self.args))
        return subprocess.run(self.args, check=False).returncode


def parse_class_information(file: str) -> tuple[str, str]:
    with open(file, "r") as f:
        java_source = f.read()

    package_name = java_source.split("package ")[1].split(";")[0]

    simple_class_name = java_source.split("class ")[1].split(" ")[0]

    return package_name, simple_class_name


def extract_class_name(class_file: str) -> str:
    with open(class_file, "rb") as f:
        data = f.read()

    pos = 0

    def read_u1():
        nonlocal pos
        val = data[pos]
        pos += 1
        return val

    def read_u2():
        nonlocal pos
        val = struct.unpack(">H", data[pos : pos + 2])[0]
        pos += 2
        return val

    def read_u4():
        nonlocal pos
        val = struct.unpack(">I", data[pos : pos + 4])[0]
        pos += 4
        return val

    magic = read_u4()
    if magic != 0xCAFEBABE:
        raise ValueError(f"Not a valid class file: {class_file}")

    read_u2()
    read_u2()
    constant_pool_count = read_u2()

    constant_pool = [None]
    i = 1
    while i < constant_pool_count:
        tag = read_u1()
        if tag == 1:
            length = read_u2()
            utf8_bytes = data[pos : pos + length]
            constant_pool.append(utf8_bytes.decode("utf-8"))
            pos += length
        elif tag in (7, 8):
            index = read_u2()
            constant_pool.append(index)
        elif tag in (3, 4):
            pos += 4
            constant_pool.append(None)
        elif tag in (5, 6):
            pos += 8
            constant_pool.append(None)
            i += 1
        elif tag in (9, 10, 11, 12, 15, 16, 18):
            pos += 4 if tag in (9, 10, 11, 12, 18) else 3
            constant_pool.append(None)
        else:
            raise ValueError(f"Unknown constant pool tag {tag}")
        i += 1

    read_u2()
    this_class_index = read_u2()
    class_name_index = constant_pool[this_class_index]
    class_name = constant_pool[class_name_index]

    return class_name.replace("/", ".")


def compile_java(
    source: str,
    release: str,
    output_dir: str,
    jarpath: str | None,
    cpp_output: str | None,
) -> int:
    java_files: list[str] = []

    if os.path.isdir(source):
        for dirpath, _, filenames in os.walk(source):
            for f in filenames:
                if f.endswith(".java"):
                    java_files.append(os.path.join(dirpath, f))
    else:
        if source.endswith(".java"):
            java_files.append(source)

    if not java_files:
        print(f"[WARN] No Java sources found in {source}")
        return 1

    javac = cmdbuf()
    javac.append("javac", "--release", release, *java_files, "-d", output_dir)
    ret = javac.exec()
    if ret != 0:
        return ret

    if jarpath is not None:
        jar = cmdbuf()
        jar.append("jar", "-c", "-f", jarpath, "-C", output_dir, ".")
        ret = jar.exec()
        if ret != 0:
            return ret

    if cpp_output is not None:
        class_files = []
        for dirpath, _, filenames in os.walk(output_dir):
            for f in filenames:
                if f.endswith(".class"):
                    class_files.append(os.path.join(dirpath, f))

        lines = [
            "// Auto-generated C++ embedded Java classes\n",
            "namespace embedded {\n",
        ]
        class_names = []
        class_real_names = []

        for cf in class_files:
            class_name = extract_class_name(cf)
            var_name = class_name.replace(".", "_")
            class_names.append(var_name)

            with open(cf, "rb") as f:
                bytes_data = f.read()
            byte_str = ", ".join(f"0x{b:02x}" for b in bytes_data)

            lines.append(f"// Auto-generated from {cf}\n")
            lines.append(f"// Class: {class_name}\n\n")
            lines.append(f"unsigned char {var_name}[] = {{ {byte_str} }};\n")
            lines.append(f"unsigned int {var_name}_size = {len(bytes_data)};\n")
            lines.append(
                f'const char* {var_name}_name = "{class_name.replace(".", "/")}";\n\n'
            )

        lines.append("unsigned char *classes[] = {\n")
        for cn in class_names:
            lines.append(f"    {cn},\n")
        lines.append("};\n\n")

        lines.append("const char* class_names[] = {\n")
        for cn in class_names:
            lines.append(f"    {cn}_name,\n")
        lines.append("};\n\n")

        lines.append(f"unsigned int classes_count = {len(class_names)};\n")
        lines.append("}\n")

        with open(cpp_output, "w") as f:
            f.writelines(lines)

    return 0


def main() -> int:
    parser = arg_parser()

    source = parser.add("--source", "-src")
    release = parser.add("--release")
    output = parser.add("--output", "-o")
    jarpath = parser.add("--jarpath")

    parser.parse(sys.argv)

    return compile_java(
        source.expect("no source provided."),
        release.value or "8",
        "build/java",
        jarpath.value,
        output.value,
    )


if __name__ == "__main__":
    sys.exit(main())
