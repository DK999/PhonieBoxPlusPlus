#!/usr/bin/env python3
"""Generate ipc_commands.hpp from ipc_command_schema.json.

This generator reads a JSON schema and produces a C++ header with:
  - Command and status type enums
  - Typed command/struct structs with toJson() and fromJson()
  - std::variant groups per system domain
  - IncomingCommand::parse() for deserializing JSON into typed structs
  - Lookup tables and socket address helpers

---------------------------------------------------------------------------
HOW TO ADD A NEW COMMAND OR STATUS
---------------------------------------------------------------------------
There is NO need to modify this Python generator. Just edit the JSON schema:

1. Add a new command under its system key in "types":

   "types": {
       "MEDIA": {
           "PLAY_URL": {
               "parameters": {
                   "url":    { "type": "string", "required": true },
                   "volume": { "type": "number", "default": 0.5 },
                   "loop":   { "type": "boolean", "default": false }
               }
           }
       }
   }

   This auto-generates:
     - struct PLAY_URLCommand  (with url, volume, loop, tag_id fields)
     - PLAY_URL in the CommandType enum
     - PLAY_URLCommand::fromJson() / toJson() / isValid()
     - dispatch in IncomingCommand::parse()

2. Add a new status under its system key in "statuses":

   "statuses": {
       "MEDIA": {
           "URL_PLAYBACK_STARTED": {
               "parameters": {
                   "url": { "type": "string", "required": true }
               }
           }
       }
   }

3. Run the generator:

   python3 tools/generate_command_types.py \
     config/ipc_command_schema.json \
     build/Debug/generated/ipc_commands.hpp

4. Commit the regenerated ipc_commands.hpp.

IMPORTANT: The command/status name must be unique across all types and statuses.
"""

import json
import sys
from pathlib import Path


# ---------------------------------------------------------------------------
# Type helpers
# ---------------------------------------------------------------------------

def c_type(json_type: str) -> str:
    """Map a JSON schema type to a C++ type."""
    return {
        "string": "std::string",
        "integer": "int",
        "number": "float",
        "boolean": "bool",
    }.get(json_type, "std::string")


def cpp_default(param_def: dict) -> str:
    """Return the C++ default expression for a parameter definition."""
    t = param_def.get("type", "string")
    v = param_def.get("default")
    if v is None:
        if t == "string":
            return '""'
        if t == "boolean":
            return "false"
        return "0"
    if t == "boolean":
        return "true" if v else "false"
    if t == "string":
        return f'"{v}"' if v else '""'
    return str(v)


def _flatten_types(types_data: dict) -> list[dict]:
    """Flatten nested {system: {command_name: params}} into flat list."""
    result: list[dict] = []
    for system, commands in types_data.items():
        for cmd_name, cmd_def in commands.items():
            result.append({
                "name": cmd_name,
                "system": system,
                "parameters": cmd_def.get("parameters", {}),
            })
    return result


def _flatten_statuses(statuses_data: dict) -> list[dict]:
    """Flatten nested {system: {status_name: params}} into flat list."""
    result: list[dict] = []
    for system, statuses in statuses_data.items():
        for status_name, status_def in statuses.items():
            result.append({
                "name": status_name,
                "system": system,
                "parameters": status_def.get("parameters", {}),
            })
    return result


# ---------------------------------------------------------------------------
# Struct generators
# ---------------------------------------------------------------------------

def generate_command_struct(name: str, params: dict) -> str:
    """Generate a C++ command struct with toJson()/fromJson()/isValid()."""
    lines: list[str] = [
        f"struct {name}Command {{",
        "    System sock = System::UNKNOWN;",
    ]

    for key, pdef in params.items():
        ct = c_type(pdef.get("type", "string"))
        default = cpp_default(pdef)
        if "default" in pdef:
            lines.append(f"    {ct} {key} = {default};")
        else:
            lines.append(f"    {ct} {key};")

    lines += [
        "    std::string tag_id;",
        "",
        "    json toJson() const {",
        "        json j;",
        f'        j["type"] = "{name}";',
    ]
    for key in params:
        lines.append(f'        j["{key}"] = {key};')
    lines += [
        '        j["tag_id"] = tag_id;',
        "        return j;",
        "    }",
        "",
        f"    static {name}Command fromJson(const json& j) {{",
        f"        {name}Command cmd;",
    ]

    for key, pdef in params.items():
        ct = c_type(pdef.get("type", "string"))
        default = cpp_default(pdef)
        lines.append(f'        cmd.{key} = j.value("{key}", {default});')

    lines += [
        '        cmd.tag_id = j.value("tag_id", "");',
        "        return cmd;",
        "    }",
        "",
        "    bool isValid() const {",
    ]

    for key, pdef in params.items():
        if pdef.get("required"):
            if pdef.get("type") == "string":
                lines.append(f"        if ({key}.empty()) return false;")
            else:
                lines.append(f"        (void){key};")

    lines += ["        return true;", "    }", "};"]
    return "\n".join(lines)


def generate_status_struct(name: str, params: dict) -> str:
    """Generate a C++ status struct with toJson()/fromJson()."""
    lines: list[str] = [
        f"struct {name}Status {{",
        "    System sock = System::UNKNOWN;",
    ]

    for key, pdef in params.items():
        ct = c_type(pdef.get("type", "string"))
        default = cpp_default(pdef)
        if "default" in pdef:
            lines.append(f"    {ct} {key} = {default};")
        else:
            lines.append(f"    {ct} {key};")

    lines += [
        "",
        "    json toJson() const {",
        "        json j;",
        f'        j["topic"] = "STATUS." + std::string(to_string(sock)) + "." + std::string(to_string(StatusType::{name}));',
        f'        j["type"] = "{name}";',
    ]
    for key in params:
        lines.append(f'        j["{key}"] = {key};')
    lines += [
        '        j["sock"] = std::string(to_string(sock));',
        "        return j;",
        "    }",
        "",
        f"    static {name}Status fromJson(const json& j) {{",
        f"        {name}Status status;",
    ]

    for key, pdef in params.items():
        ct = c_type(pdef.get("type", "string"))
        default = cpp_default(pdef)
        lines.append(f'        status.{key} = j.value("{key}", {default});')

    lines += [
        '        status.sock = system_from_string(j.value("sock", "UNKNOWN"));',
        "        return status;",
        "    }",
        "};",
    ]
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Lookup table generators
# ---------------------------------------------------------------------------

def generate_to_string_cmd(types: list[dict]) -> str:
    """Generate the to_string(CommandType) switch function."""
    lines = ["inline std::string_view to_string(CommandType v) {", "    switch (v) {"]
    for t in types:
        lines.append(f'        case CommandType::{t["name"]}: return "{t["name"]}";')
    lines += ['        default: return "UNKNOWN";', "    }", "}", ""]
    return "\n".join(lines)


def generate_from_string_cmd(types: list[dict]) -> str:
    """Generate the command_type_from_string() lookup function."""
    lines = [
        "inline CommandType command_type_from_string(std::string_view s) {",
        "    for (const auto& [name, v] : kCommandTypeTable) {",
        '        if (s == name) return v;',
        "    }",
        "    return CommandType::UNKNOWN;",
        "}", "",
    ]
    return "\n".join(lines)


def generate_to_string_status(statuses: list[dict]) -> str:
    """Generate the to_string(StatusType) switch function."""
    lines = ["inline std::string_view to_string(StatusType v) {", "    switch (v) {"]
    for s in statuses:
        lines.append(f'        case StatusType::{s["name"]}: return "{s["name"]}";')
    lines += ['        default: return "UNKNOWN";', "    }", "}", ""]
    return "\n".join(lines)


def generate_to_string_system(systems: list[str]) -> str:
    """Generate the to_string(System) switch function."""
    lines = ["inline std::string_view to_string(System v) {", "    switch (v) {"]
    for s in systems:
        lines.append(f'        case System::{s}: return "{s}";')
    lines += ['        default: return "UNKNOWN";', "    }", "}", ""]
    return "\n".join(lines)


def generate_from_string_system(systems: list[str]) -> str:
    """Generate the system_from_string() lookup function."""
    lines = [
        "inline System system_from_string(std::string_view s) {",
        "    for (const auto& [name, v] : kSystemTable) {",
        '        if (s == name) return v;',
        "    }",
        "    return System::UNKNOWN;",
        "}", "",
    ]
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Variant and IncomingCommand generators
# ---------------------------------------------------------------------------

def generate_variants(types: list[dict], statuses: list[dict]) -> str:
    """Generate std::variant typedefs grouped by system domain."""
    lines: list[str] = []

    by_system: dict[str, list[str]] = {}
    for t in types:
        by_system.setdefault(t.get("system", "UNKNOWN"), []).append(t["name"])
    for system in sorted(by_system):
        cmd_names = ", ".join(f"{c}Command" for c in by_system[system])
        lines.append(f"using {system}Command = std::variant<{cmd_names}>;")

    by_system = {}
    for s in statuses:
        by_system.setdefault(s.get("system", "UNKNOWN"), []).append(s["name"])
    for system in sorted(by_system):
        status_names = ", ".join(f"{c}Status" for c in by_system[system])
        lines.append(f"using {system}Status = std::variant<{status_names}>;")

    return "\n".join(lines)


def generate_incoming_command(types: list[dict]) -> str:
    """Generate the IncomingCommand struct with parse() and toJson()."""
    lines: list[str] = [
        "struct IncomingCommand {",
        "    System domain = System::UNKNOWN;",
        "    std::string tag_id;",
        "    std::variant<MEDIACommand, SYSTEMCommand, VOLUMECommand> payload;",
        "",
        "    json toJson() const {",
        "        json j;",
        '        j["tag_id"] = tag_id;',
        '        j["command"] = std::visit(overloaded{',
        "            [](const auto& v) {",
        "                return std::visit([](const auto& c) {",
        "                    return c.toJson();",
        "                }, v);",
        "            },",
        "        }, payload);",
        '        return j;',
        "    }",
        "",
        "    static IncomingCommand parse(const json& j) {",
        "        IncomingCommand cmd;",
        "",
        '        if (j.contains("command") && j["command"].contains("type")) {',
        '            auto& c = j["command"];',
        '            std::string type_str = c.value("type", "UNKNOWN");',
        '            cmd.tag_id = j.value("tag_id", "");',
    ]

    by_system: dict[str, list[dict]] = {}
    for t in types:
        by_system.setdefault(t.get("system", "UNKNOWN"), []).append(t)

    for system in sorted(by_system):
        for t in by_system[system]:
            tname = t["name"]
            lines.append(f'        if (type_str == "{tname}") {{')
            lines.append(f"            cmd.domain = System::{system};")
            lines.append(f"            cmd.payload = {tname}Command::fromJson(c);")
            lines.append("            return cmd;")
            lines.append("        }")

    lines += [
        "        cmd.domain = System::UNKNOWN;",
        "    }",
        "",
        '        if (j.contains("type")) {',
        '            cmd.tag_id = j.value("tag_id", "");',
        '            std::string type_str = j.value("type", "UNKNOWN");',
        '            auto p = j.value("parameters", json::object());',
    ]

    for system in sorted(by_system):
        for t in by_system[system]:
            tname = t["name"]
            lines.append(f'        if (type_str == "{tname}") {{')
            lines.append(f"            cmd.domain = System::{system};")
            lines.append(f"            cmd.payload = {tname}Command::fromJson(p);")
            lines.append("            return cmd;")
            lines.append("        }")

    lines += [
        "        cmd.domain = System::UNKNOWN;",
        "    }",
        "    return cmd;",
        "}",
        "};",
    ]
    return "\n".join(lines)


def generate_enum_table(enum_class: str, items: list[str]) -> str:
    """Generate an inline constexpr array for an enum lookup table."""
    lines = [
        f"inline constexpr std::array<std::pair<std::string_view, {enum_class}>, {len(items)}> k{enum_class}Table" + "{",
    ]
    for item in items:
        line = "    " + "std::pair<std::string_view, " + enum_class + ">{std::string_view{\"" + item + "\"}, " + enum_class + "::" + item + "},"
        lines.append(line)
    lines.append("};")
    return "\n".join(lines)


def generate_socket_table(sockets: dict, role: str) -> str:
    """Generate the command or status socket table."""
    addresses = {s: d[role] for s, d in sockets.items()}
    table_name = "k" + role.title() + "SocketTable"
    lines = [
        "inline constexpr std::array<std::pair<System, std::string_view>, " + str(len(addresses)) + "> " + table_name + "{",
    ]
    for system, addr in addresses.items():
        line = "    std::pair<System, std::string_view>{System::" + system + ", std::string_view{\"" + addr + "\"}},"
        lines.append(line)
    lines.append("};")
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Header assembly
# ---------------------------------------------------------------------------

def emit_header(source_file: str, types: list[dict], statuses: list[dict],
                systems: list[str], sockets: dict) -> str:
    """Assemble the complete C++ header from schema data."""

    cmd_names = [t["name"] for t in types]
    status_names = [s["name"] for s in statuses]

    cmd_enum_lines = [f"    {name}," for name in cmd_names]
    status_enum_lines = [f"    {name}," for name in status_names]
    system_enum_lines = [f"    {name}," for name in systems]

    command_structs = "\n\n".join(
        generate_command_struct(t["name"], t["parameters"]) for t in types
    )

    status_structs = "\n\n".join(
        generate_status_struct(s["name"], s["parameters"]) for s in statuses
    )

    variants = generate_variants(types, statuses)

    header_lines: list[str] = [
        "// AUTO-GENERATED FILE - DO NOT EDIT MANUALLY",
        f"// Generated from {source_file}",
        "#pragma once", "",
        "#include <array>", "#include <functional>", "#include <iostream>",
        "#include <optional>", "#include <string>", "#include <string_view>",
        "#include <variant>", "#include <nlohmann/json.hpp>", "",
        "namespace ipc {", "using json = nlohmann::json;", "",
        "enum class CommandType {",
        *cmd_enum_lines,
        "    UNKNOWN",
        "};", "",
        "enum class StatusType {",
        *status_enum_lines,
        "    UNKNOWN",
        "};", "",
        "enum class System {",
        *system_enum_lines,
        "    UNKNOWN",
        "};", "",
        "enum class SocketRole {",
        "    COMMAND,",
        "    STATUS",
        "};", "",
        command_structs, "",
        generate_to_string_cmd(types), "",
        generate_to_string_status(statuses), "",
        generate_to_string_system(systems), "",
        generate_enum_table("CommandType", cmd_names), "",
        generate_enum_table("StatusType", status_names), "",
        generate_enum_table("System", systems), "",
        generate_socket_table(sockets, "command"), "",
        generate_socket_table(sockets, "status"), "",
        generate_from_string_system(systems), "",
        status_structs, "",
        variants, "",
        "template<class... Ts>",
        "struct overloaded : Ts... {",
        "    using Ts::operator()...;",
        "};",
        "template<class... Ts>",
        "overloaded(Ts...) -> overloaded<Ts...>;", "",
        generate_incoming_command(types), "",
        generate_from_string_cmd(types), "",
        "constexpr std::optional<std::string_view> command_socket_address(System s) {",
        "    for (const auto& [system, address] : kCommandSocketTable) {",
        "        if (system == s) return address;",
        "    }",
        "    return std::nullopt;",
        "}", "",
        "constexpr std::optional<std::string_view> status_socket_address(System s) {",
        "    for (const auto& [system, address] : kStatusSocketTable) {",
        "        if (system == s) return address;",
        "    }",
        "    return std::nullopt;",
        "}", "",
        "}  // namespace ipc",
    ]

    return "\n".join(header_lines)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def generate(json_path: str, header_path: str) -> None:
    """Read the JSON schema and write the C++ header."""
    with open(json_path) as f:
        data = json.load(f)

    types_raw = data["types"]
    statuses_raw = data.get("statuses", {})
    systems = data["systems"]
    sockets = data["sockets"]

    types = _flatten_types(types_raw)
    statuses = _flatten_statuses(statuses_raw)

    unknown = set(sockets) - set(systems)
    if unknown:
        sys.exit(f"ERROR: sockets reference unknown systems: {unknown}")

    header = emit_header(str(json_path), types, statuses, systems, sockets)
    Path(header_path).parent.mkdir(parents=True, exist_ok=True)
    Path(header_path).write_text(header, encoding="utf-8")
    print(f"Generated {header_path} ({len(types)} types, {len(statuses)} statuses, {len(systems)} systems, {len(sockets)} sockets)")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit("Usage: generate_command_types.py <input.json> <output.hpp>")
    generate(sys.argv[1], sys.argv[2])
