#!/usr/bin/env python3
import os
import struct
import subprocess
import sys
from typing import override


# java flags
ACC_PUBLIC = 0x0001
ACC_PRIVATE = 0x0002
ACC_PROTECTED = 0x0004
ACC_NATIVE = 0x0100


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


class method_info:
    def __init__(self, name: str, access: int, desc: str) -> None:
        self.access: int = access
        self.name: str = name
        self.desc: str = desc

    @override
    def __str__(self) -> str:
        return self.name + self.desc


class class_info:
    def __init__(self, name: str, methods: list[method_info]) -> None:
        self.methods: list[method_info] = methods
        self.name: str = name

    @override
    def __str__(self) -> str:
        return self.name


class buffer:
    def __init__(self, data: bytes):
        self.data: bytes = data
        self.pos: int = 0
        pass

    def read_u1(self):
        val = self.data[self.pos]
        self.pos += 1
        return val

    def read_u2(self) -> int:
        val: int = struct.unpack(">H", self.data[self.pos : self.pos + 2])[0]  # pyright: ignore[reportAny]
        self.pos += 2
        return val

    def read_u4(self) -> int:
        val: int = struct.unpack(">I", self.data[self.pos : self.pos + 4])[0]  # pyright: ignore[reportAny]
        self.pos += 4
        return val

    def read(self, size: int) -> bytes:
        bytes = self.data[self.pos : self.pos + size]
        self.pos += size
        return bytes

    def skip(self, count: int) -> None:
        self.pos += count


def parse_constant_pool(buf: buffer) -> list:  # pyright: ignore[reportMissingTypeArgument, reportUnknownParameterType]
    constant_pool_count = buf.read_u2()

    constant_pool: list = [None]  # pyright: ignore[reportMissingTypeArgument]
    i = 1
    while i < constant_pool_count:
        tag = buf.read_u1()
        if tag == 1:
            length = buf.read_u2()
            utf8_bytes = buf.read(length)
            constant_pool.append(utf8_bytes.decode("utf-8"))  # pyright: ignore[reportArgumentType]
        elif tag in (7, 8):
            index = buf.read_u2()
            constant_pool.append(index)
        elif tag in (3, 4):
            buf.skip(4)
            constant_pool.append(None)
        elif tag in (5, 6):
            buf.skip(8)
            constant_pool.append(None)
            i += 1
        elif tag in (9, 10, 11, 12, 15, 16, 18):
            buf.skip(4 if tag in (9, 10, 11, 12, 18) else 3)
            constant_pool.append(None)
        else:
            raise ValueError(f"Unknown constant pool tag {tag}")
        i += 1

    return constant_pool


def parse_methods(buf: buffer, constant_pool: list) -> list[method_info]:  # pyright: ignore[reportMissingTypeArgument]
    parsed: list[method_info] = []
    methods_count = buf.read_u2()

    for i in range(methods_count):
        access = buf.read_u2()
        name_index = buf.read_u2()
        descriptor_index = buf.read_u2()
        attributes_count = buf.read_u2()

        name = constant_pool[name_index]
        desc = constant_pool[descriptor_index]

        for _ in range(attributes_count):
            _ = buf.read_u2()  # attribute_name_index
            length = buf.read_u4()
            buf.skip(length)

        parsed.append(method_info(name, access, desc))

    return parsed


def extract_class_information(class_file: str) -> class_info:
    with open(class_file, "rb") as f:
        data = f.read()

    buf = buffer(data)

    magic = buf.read_u4()
    if magic != 0xCAFEBABE:
        raise ValueError(f"Not a valid class file: {class_file}")

    _ = buf.read_u2()  # major version
    _ = buf.read_u2()  # minor version

    constant_pool = parse_constant_pool(buf)

    _ = buf.read_u2()  # access
    this_class_index = buf.read_u2()
    class_name_index = constant_pool[this_class_index]
    class_name = constant_pool[class_name_index]

    _ = buf.read_u2()  # super_class

    interfaces_count = buf.read_u2()
    buf.skip(interfaces_count * 2)  # u2 = 2 * 1 byte

    fields_count = buf.read_u2()
    for _ in range(fields_count):
        buf.skip(6)  # access_flags + name_index + descriptor_index (3 * u2)
        attributes_count = buf.read_u2()
        for _ in range(attributes_count):
            _ = buf.read_u2()  # attribute_name_index
            length = buf.read_u4()
            buf.skip(length)

    methods = parse_methods(buf, constant_pool)
    for m in methods:
        print(m)

    return class_info(class_name.replace("/", "."), methods)


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
        class_files: list[str] = []
        for dirpath, _, filenames in os.walk(output_dir):
            for f in filenames:
                if f.endswith(".class"):
                    class_files.append(os.path.join(dirpath, f))

        lines = [
            "// Auto-generated C++ embedded Java classes\n",
            "#include <cstdint> \n",
            "#include <vector>\n",
            "\n",
            "namespace embedded {\n",
            "\n",
            "struct method_info {\n",
            "    const char* name;\n",
            "    const char* desc;\n",
            "    uint16_t access;\n",
            "};\n",
            "\n",
            "struct class_info {\n",
            "    const char* name;\n",
            "    std::vector<unsigned char> bytes;\n",
            "    std::vector<method_info> methods;\n",
            "};\n",
            "\n",
            "std::vector<class_info> classes = {\n",
        ]
        class_names: list[str] = []

        for cf in class_files:
            info = extract_class_information(cf)
            var_name = info.name.replace(".", "_")
            class_names.append(var_name)

            with open(cf, "rb") as f:
                bytes_data = f.read()
            byte_str = ", ".join(f"0x{b:02x}" for b in bytes_data)

            lines.append("    class_info {\n")
            lines.append(f"        // Auto-generated from {cf}\n")
            lines.append(f"        // Class: {info.name}\n")
            lines.append(f'        .name = "{info.name.replace(".", "/")}",\n')
            lines.append(f"        .bytes = {{ {byte_str} }},\n")
            lines.append("        .methods = {\n")

            for method in info.methods:
                lines.append("            method_info {\n")
                lines.append(f'                .name = "{method.name}",\n')
                lines.append(f'                .desc = "{method.desc}",\n')
                lines.append(f"                .access = {hex(method.access)},\n")
                lines.append("            },\n")

            lines.append("        }\n")
            lines.append("    },\n")

        lines.append("};\n\n")
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
