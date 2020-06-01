import sys

if __name__ == '__main__':
    keyword = sys.argv[1]
    dir_name = "./funcbox4_instance/includes/"
    linelist = []
    with open(dir_name + "funcworker.h", "r") as f:
        line_cnt = 0
        for line in f.readlines():
            line_cnt = line_cnt + 1
            if line[0] != '/' and line.find("include") != -1 and \
            (line.find("firewall") != -1 or line.find("monitor") != -1 \
                or line.find("nat") != -1 or line.find("ids") != -1\
                or line.find("vpn") != -1):
                line = "//" + line
            if line.find(keyword) != -1:
                line = line[2:]
            linelist.append(line)
    with open(dir_name + "funcworker.h", "w") as f:
        for line in linelist:
            f.write(line)

    dir_name = "./funcbox4_instance/"
    linelist = []
    with open(dir_name + "Makefile", "r") as f:
        line_cnt = 0
        for line in f.readlines():
            line_cnt = line_cnt + 1
            if line[0] != '#' and (line.find("SRCS-y") != -1 or \
            line.find("INC") != -1 or line.find("CFLAGS") != -1) and (line.find("firewall") != -1 or line.find("monitor") != -1 \
                or line.find("nat") != -1 or line.find("ids") != -1\
                or line.find("vpn") != -1):
                line = "# " + line
            if line.find(keyword) != -1 and \
                (line.find("set") == -1 or keyword.find("set") != -1):
                line = line[2:]
            linelist.append(line)
    with open(dir_name + "Makefile", "w") as f:
        for line in linelist:
            f.write(line)