import commands

a, b = commands.getstatusoutput('ipmctl show -dimm -performance TotalMediaWrites')

for line in b.splitlines():
    print(line, len(line))
    if len(line) == 0:
        continue
    if line[0] == '-':
        continue
    tmp = line.split('=')
    print(tmp)
    print(int(tmp[1], 16) * 64 / 1024.0 / 1024.0)