from pathlib import Path
from io import BytesIO
from xml.sax.saxutils import escape
import zipfile
from docx import Document
from docx.oxml.ns import qn
from reportlab.pdfgen import canvas
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
import pypdfium2 as pdfium
w=Path(__file__).parent
pdfmetrics.registerFont(TTFont('Malgun','C:/Windows/Fonts/malgun.ttf'))
svg=[]
c=canvas.Canvas(str(w/'figure13-corrected.pdf'),pagesize=(1560,690))
def line(x1,y1,x2,y2,color,width):
 c.setStrokeColor(color);c.setLineWidth(width);c.line(x1,690-y1,x2,690-y2)
 svg.append(f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{color}" stroke-width="{width}"/>')
def text(x,y,s,size=30,color='#333333'):
 c.setFont('Malgun',size);c.setFillColor(color);c.drawString(x,690-y-size*.88,s)
 svg.append(f'<text x="{x}" y="{y+size*.88}" font-family="Malgun Gothic" font-size="{size}" fill="{color}">{escape(s)}</text>')
def circle(x,y,r,color):
 c.setFillColor(color);c.circle(x,690-y,r,fill=1,stroke=0)
 svg.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{color}"/>')
def rect(x,y,ww,hh,color,width):
 c.setStrokeColor(color);c.setLineWidth(width);c.rect(x,690-y-hh,ww,hh,fill=0,stroke=1)
 svg.append(f'<rect x="{x}" y="{y}" width="{ww}" height="{hh}" fill="none" stroke="{color}" stroke-width="{width}"/>')
text(40,25,'평면  카메라 방향에 따라 옆면 선택',32)
text(830,25,'단면  발밑 지지면과 상부를 구분',32)
text(60,95,'숨긴 외부 조각',29,'#777777')
rect(125,155,520,350,'#333333',10)
rect(66,211,44,189,'#A9BAC1',3);rect(711,211,44,189,'#A9BAC1',3)
line(120,510,650,510,'#ACC8D1',15)
circle(390,346,25,'#555555');text(300,390,'현재 방 / 캐릭터',29)
line(385,535,385,560,'#555555',3)
line(385,605,385,626,'#555555',3)
c.setFillColor('#555555');p=c.beginPath();p.moveTo(355,12);p.lineTo(385,50);p.lineTo(415,12);p.close();c.drawPath(p,fill=1,stroke=0)
svg.append('<polygon points="355,678 385,640 415,678" fill="#555555"/>')
text(230,565,'카메라 쪽 면 숨김',29,'#456A7A')
text(910,125,'천장 · 윗층 후보 숨김',29,'#456A7A')
line(850,200,1480,200,'#ACC8D1',13)
line(850,521,1480,521,'#333333',10)
circle(1100,406,20,'#555555');line(1100,420,1100,511,'#555555',8)
text(880,560,'발밑 지지면은 제외',30)
text(880,622,'상태 기록 후 메인·깊이 패스만 해제',28,'#555555')
c.showPage();c.save()
(w/'figure13-corrected.svg').write_text('<svg xmlns="http://www.w3.org/2000/svg" width="1560" height="690" viewBox="0 0 1560 690">'+''.join(svg)+'</svg>',encoding='utf-8')
pdf=pdfium.PdfDocument(str(w/'figure13-corrected.pdf'))
pdf[0].render(scale=1.5).to_pil().save(w/'figure13-corrected.png')
docpath=w.parent/'기술문서_전선현_담당절_수정본_v8.docx'
d=Document(docpath);rid=d.inline_shapes[12]._inline.xpath('.//a:blip')[0].get(qn('r:embed'))
target=str(d.part.related_parts[rid].partname).lstrip('/')
# 그림 데이터만 교체한다. 본문·스타일·그림 크기·배치와 다른 그림은 그대로 보존한다.
with zipfile.ZipFile(docpath) as z:items=[(i,z.read(i.filename)) for i in z.infolist()]
with zipfile.ZipFile(docpath,'w',zipfile.ZIP_DEFLATED) as z:
 for i,data in items:z.writestr(i,(w/'figure13-corrected.png').read_bytes() if i.filename==target else data)
print('Updated',target,'in',docpath)
