import socket
import sys

MANAGER_SOCK_ADDR = "/var/tmp/manager_server.sock"
FUNC_CONFIG_FILE = "./function_config.txt"


def connect_server(server_addr):
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)

    try:
        sock.connect(server_addr)
    except socket.error:
        print('connect server fail!')
        sys.exit(1)
    print('connect server success!')

    return sock



def send_request(sock):
    f = open(FUNC_CONFIG_FILE)
    for line in f:
        print(line)
        sock.sendall(line)

        ret = sock.recv(4096)
        print(ret)


def close_socket(sock):
    sock.close()
    print('close socket success!')



def main():
    server_addr = MANAGER_SOCK_ADDR
    sock = connect_server(server_addr)
    send_request(sock)
    close_socket(sock)



if __name__ == '__main__':
    main()