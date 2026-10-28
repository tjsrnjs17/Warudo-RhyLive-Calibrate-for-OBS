import argparse
import json
from pathlib import Path


def load(path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f"{path.name}: duplicate key {key}")
            if not isinstance(value, str) or not value.strip():
                raise ValueError(f"{path.name}: invalid text for {key}")
            result[key] = value
        return result

    return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique)


def generate(source, output):
    baseline = source / "en-US.json"
    paths = [baseline] + sorted(path for path in source.glob("*.json") if path != baseline)
    reference = load(baseline)
    keys = list(reference)
    entries = []
    for path in paths:
        messages = load(path)
        if set(messages) != set(keys):
            missing = sorted(set(keys) - set(messages))
            extra = sorted(set(messages) - set(keys))
            raise ValueError(f"{path.name}: missing={missing}, extra={extra}")
        values = ",\n            ".join(json.dumps(messages[key], ensure_ascii=False) for key in keys)
        entries.append('    {"' + path.stem + '", {{\n            ' + values + '\n        }}}')
    text = "\n".join([
        "#pragma once",
        "#include <array>",
        "namespace warudo_i18n {",
        "enum class Text { " + ", ".join(keys) + ", Count };",
        "struct Locale {",
        "    const char *name;",
        "    std::array<const char *, static_cast<std::size_t>(Text::Count)> values;",
        "};",
        "inline constexpr std::array<Locale, " + str(len(entries)) + "> Locales = {{",
        ",\n".join(entries),
        "}};",
        "}",
        "",
    ])
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(text, encoding="utf-8", newline="\n")
    print(f"Validated {len(entries)} locales, {len(keys)} messages each")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    generate(args.source, args.output)
