from pathlib import Path
import json
from docx import Document
w=Path(__file__).parent
toc=Document(r'C:/JSH/Unreal/MissionProj/문서/기술 문서/목차/통합 기술 문서 목차 (초안)_v5.docx')
print('TOC')
for p in toc.paragraphs:
 if p.text:print(p.text)
for t in toc.tables:
 for r in t.rows:print(' | '.join(c.text for c in r.cells))
b=json.loads((w/'blocks.json').read_text(encoding='utf-8'))
print('\nV6 BLOCK INDEX')
for i,x in enumerate(b):
 if x['type']=='t':print(i,'TABLE',str(x['rows'])[:200])
 elif x['text']:print(i,x['style'],x['text'][:150])
