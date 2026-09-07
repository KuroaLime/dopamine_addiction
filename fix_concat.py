# -*- coding: utf-8 -*-
import re

path = r"C:\Users\benev\Documents\GitHub\dopamine_addiction\build_portfolio.py"
with open(path, encoding="utf-8") as f:
    lines = f.readlines()

out = []
in_case_body = False
seen_plus = False
in_triple = False
fixed_count = 0

case_start_re = re.compile(r'^sys\d\d = case\(')
bare4_re = re.compile(r"^    (['\"])")
plus_re = re.compile(r'^    \+ ')

for line in lines:
    stripped = line.rstrip("\n")

    if case_start_re.match(stripped):
        in_case_body = True
        seen_plus = False
        in_triple = False
        out.append(line)
        continue

    if in_case_body:
        # toggle triple-quote state based on occurrences of triple double-quote
        triple_count = stripped.count('"""')
        currently_in_triple = in_triple

        if not currently_in_triple:
            if plus_re.match(stripped):
                seen_plus = True
            elif bare4_re.match(stripped) and seen_plus:
                # needs a '+' prefix
                new_line = "    + " + stripped[4:] + "\n"
                out.append(new_line)
                fixed_count += 1
                # update triple state and continue to next line
                if triple_count % 2 == 1:
                    in_triple = not in_triple
                # end of case body detection
                if stripped == ")":
                    in_case_body = False
                continue

        # detect end of case body (a lone ')' at column 0, only when not in triple)
        if not currently_in_triple and stripped == ")":
            in_case_body = False

        if triple_count % 2 == 1:
            in_triple = not in_triple

    out.append(line)

with open(path, "w", encoding="utf-8") as f:
    f.writelines(out)

print("fixed lines:", fixed_count)
