#!/usr/bin/env python3
"""Character and bigram frequencies from a plain-text tweet archive.

Input:  tweets separated by lines containing only '---' (data/raw/tweets_all.txt).
Output: data/freq.json with aggregate counts only, no tweet text, so it is
        safe to commit.

Cleaning: URLs and @mentions are removed (they are pasted or autocompleted, not
typed); hashtags are kept as typed. Newlines count as Enter ("\\n").

Usage: tools/extract_freq.py [archive] [out.json]
"""
import collections
import json
import re
import sys
import unicodedata

URL = re.compile(r"https?://\S+")
MENTION = re.compile(r"@\w+")
SPACES = re.compile(r"[ \t]+")


def clean(tweet: str) -> str:
    t = URL.sub("", tweet)
    t = MENTION.sub("", t)
    t = SPACES.sub(" ", t)
    lines = [ln.strip() for ln in t.split("\n")]
    return "\n".join(ln for ln in lines if ln)


def typeable(c: str) -> bool:
    # Drop emoji and other pictographs: they come from an emoji picker.
    cat = unicodedata.category(c)
    return cat[0] in "LNPZ" or c in "\n+<=>|~^`$€" or cat == "Sm" or cat == "Sc"


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else "data/raw/tweets_all.txt"
    out = sys.argv[2] if len(sys.argv) > 2 else "data/freq.json"
    raw = open(src, encoding="utf-8").read()
    tweets = [clean(t) for t in re.split(r"\n---\n", raw)]
    tweets = [t for t in tweets if t]

    uni = collections.Counter()
    bi = collections.Counter()
    upper = 0
    letters = 0
    for t in tweets:
        chars = [c for c in t if typeable(c)]
        for c in chars:
            if c.isalpha():
                letters += 1
                upper += c.isupper()
        low = [c.lower() for c in chars]
        uni.update(low)
        bi.update(a + b for a, b in zip(low, low[1:]))

    total = sum(uni.values())
    data = {
        "source": "own tweet archive (URLs and @mentions removed)",
        "tweets": len(tweets),
        "chars": total,
        "uppercaseShareOfLetters": upper / letters if letters else 0,
        "unigrams": dict(uni.most_common()),
        "bigrams": dict(bi.most_common(4000)),
    }
    with open(out, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=1)

    print(f"{len(tweets)} tweets, {total} typed characters, "
          f"{100 * data['uppercaseShareOfLetters']:.1f}% of letters uppercase")
    print("top characters (% of all):")
    row = []
    for c, n in uni.most_common(70):
        label = {" ": "␣", "\n": "⏎"}.get(c, c)
        row.append(f"{label} {100 * n / total:5.2f}")
        if len(row) == 7:
            print("   " + "   ".join(row))
            row = []
    if row:
        print("   " + "   ".join(row))


if __name__ == "__main__":
    main()
