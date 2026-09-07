import re

src = r'C:\Users\benev\.claude\projects\C--Users-benev-Documents-GitHub-dopamine-addiction\5732ec46-b5b1-407c-b853-eebad4e05604\tool-results\mcp-686dfd56-8bec-44a1-bdb9-1512d57d6bae-notion-fetch-1788756271742.txt'
content = open(src, encoding='utf-8').read()
urls = re.findall(r'https://prod-files-secure\.s3\.us-west-2\.amazonaws\.com/[^)"]+', content)
print(len(urls))
with open('image_urls.txt', 'w', encoding='utf-8') as f:
    for u in urls:
        f.write(u + '\n')
        print(u[:150])
