#!/usr/local/bin/python

from bs4 import BeautifulSoup
import requests

opcode_table_url = "http://pastraiser.com/cpu/gameboy/gameboy_opcodes.html"

page = requests.get(opcode_table_url)
tree = BeautifulSoup(page.content, 'html.parser')
tables = tree.find_all("table")

code_table = tables[0]
prefix_table = tables[1]

def parse_table(table):
  rows = table.find_all("tr")
  codes = []
  for i, row in enumerate(rows[1:]):
    cells = row.find_all("td")
    for j, cell in enumerate(cells[1:]):
      if cell.contents[0] == u'\xa0':
        codes.append(None)
        continue
      args = cell.contents[1].contents[0].split(u'\xa0\xa0')
      length = args[0]
      counts = args[1].split("/")
      code = {
        "code": hex(16*i + j),
        "name": cell.contents[0],
        "length": args[0],
        "cycles": counts[0],
        "short_cycles": counts[0]
      }
      if len(counts) > 1:
        code["short_cycles"] = counts[1]
      codes.append(code)
  return codes

def format_code(code):
  if code == None:
    return ""
  return ", ".join([code["code"], code["length"], code["short_cycles"],
      code["cycles"], '"' + code["name"] + '"'])

def format_codes(codes):
  return "{ " + " },\n{ ".join([format_code(c) for c in codes]) + " }"

#with open('opcodes.html', 'w') as f:
#  f.write(page.content)

# Parse and write the regular opcodes.
codes = parse_table(code_table)
with open('opcodes.inl', 'w') as f:
  f.write(format_codes(codes))

# Parse and write the CB prefix opcodes.
prefix_codes = parse_table(prefix_table)
with open('cb_opcodes.inl', 'w') as f:
  f.write(format_codes(prefix_codes))
