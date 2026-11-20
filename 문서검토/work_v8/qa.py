from pathlib import Path
import pypdfium2 as pdf
from PIL import Image
from pypdf import PdfReader
import json
w=Path(__file__).resolve().parent;out=w/'final-render';out.mkdir(exist_ok=True)
d=pdf.PdfDocument(str(w/'v8-final.pdf'));report=[]
for i in range(len(d)):
 d[i].render(scale=1.5).to_pil().convert('RGB').save(out/f'page-{i+1}.png')
for start in range(0,len(d),4):
 imgs=[Image.open(out/f'page-{i+1}.png') for i in range(start,min(start+4,len(d)))];ww,hh=imgs[0].size
 im=Image.new('RGB',(ww*2,hh*2),'#dddddd')
 for j,img in enumerate(imgs):im.paste(img,(j%2*ww,j//2*hh))
 im.save(out/f'sheet-{start//4+1}.jpg',quality=90)
for i,p in enumerate(PdfReader(w/'v8-final.pdf').pages):
 s=p.extract_text();report.append({'page':i+1,'text':s})
(out/'text.json').write_text(json.dumps(report,ensure_ascii=False,indent=1),encoding='utf-8')
print('Actual PDF pages',len(d),'sparse',[(r['page'],len(r['text'])) for r in report if len(r['text'])<200])
