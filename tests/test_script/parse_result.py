import sys

if __name__ == '__main__':
    fn = sys.argv[1]
    type = sys.argv[2]
    nf = sys.argv[3]
    max_thread = sys.argv[4]
    with open(fn, "r") as f:
        line_list = f.readlines()

    is_print = False
    for line in line_list:
        if is_print:
            is_print = False
            print(line[:-1])
        if len(line.split(";")) > 4:
            if line.split(";")[0] == type and line.split(";")[2] == nf and line.split(";")[3] == max_thread:
                print(line[:-1])
                is_print = True
