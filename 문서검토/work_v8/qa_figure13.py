from pathlib import Path
from PIL import Image, ImageChops
import pypdfium2 as pdfium
from pypdf import PdfReader
w=Path(__file__).parent
p=w/'v8-figure13-verified.pdf'
pdf=pdfium.PdfDocument(str(p));changed=[]
for i in range(len(pdf)):
 im=pdf[i].render(scale=1.5).to_pil().convert('RGB')
 old=Image.open(w/'verified-render'/f'page-{i+1}.png').convert('RGB')
 if im.size!=old.size or ImageChops.difference(im,old).getbbox():
  changed.append(i+1);im.save(w/f'figure13-page-{i+1}.png')
print('pages',len(pdf),'visually changed pages',changed)
print('figure page',[i+1 for i,p in enumerate(PdfReader(p).pages) if '그림 13.' in p.extract_text()])
