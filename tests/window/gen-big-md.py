#!/usr/bin/env python3
"""Generate tests/window/big.md - the document milestone M7 is measured against.

Deterministic (a fixed seed), and shaped like a real document rather than like a stress test:
the mix below is roughly what the repository's own .adoc/.md files look like, because a
performance number taken on ten thousand identical paragraphs answers a question nobody asked.

    ./gen-big-md.py [lines] [path]
"""
import random, sys, os

WORDS = ("layout scene node label formatter glyph atlas cascade selector document markdown "
         "paragraph heading selection clipboard viewport scroll offset measure shaping run "
         "inline block widget engine frame buffer texture sampler pipeline vertex index").split()


def sentence(rng, n):
    out = rng.choice(WORDS).capitalize()
    for _ in range(n - 1):
        out += " " + rng.choice(WORDS)
    return out + "."


def paragraph(rng):
    text = " ".join(sentence(rng, rng.randint(6, 14)) for _ in range(rng.randint(2, 5)))
    # inline markup at about the density of real prose
    words = text.split(" ")
    for _ in range(rng.randint(0, 3)):
        i = rng.randrange(len(words))
        words[i] = rng.choice(("**%s**", "*%s*", "`%s`", "[%s](https://xenolith.studio)")) % words[i]
    return " ".join(words)


def generate(lines, seed=20260911):
    rng = random.Random(seed)
    out = ["# The generated document", "",
           "This file is produced by gen-big-md.py and is not meant to be read.", ""]
    n = 4
    section = 0
    while n < lines:
        kind = rng.random()
        if kind < 0.10:
            section += 1
            out += [f"## Section {section}", ""]
            n += 2
        elif kind < 0.62:
            out += [paragraph(rng), ""]
            n += 2
        elif kind < 0.74:
            for i in range(rng.randint(3, 7)):
                out.append(f"- {sentence(rng, rng.randint(4, 10))}")
                n += 1
            out.append("")
            n += 1
        elif kind < 0.82:
            for i in range(rng.randint(3, 8)):
                out.append(f"{i + 1}. {sentence(rng, rng.randint(4, 9))}")
                n += 1
            out.append("")
            n += 1
        elif kind < 0.88:
            out += ["> " + sentence(rng, rng.randint(8, 16)), ""]
            n += 2
        elif kind < 0.95:
            out += ["```cpp", "int main() {", "    return 0;", "}", "```", ""]
            n += 6
        else:
            out += ["| Name | Meaning |", "|------|---------|"]
            for _ in range(rng.randint(2, 4)):
                out.append(f"| `{rng.choice(WORDS)}` | {sentence(rng, 3)} |")
                n += 1
            out.append("")
            n += 3
    return "\n".join(out) + "\n"


if __name__ == "__main__":
    lines = int(sys.argv[1]) if len(sys.argv) > 1 else 10000
    path = sys.argv[2] if len(sys.argv) > 2 else os.path.join(
            os.path.dirname(os.path.abspath(__file__)), "big.md")
    text = generate(lines)
    with open(path, "w") as f:
        f.write(text)
    print(f"{path}: {text.count(chr(10))} lines, {len(text)} bytes")
