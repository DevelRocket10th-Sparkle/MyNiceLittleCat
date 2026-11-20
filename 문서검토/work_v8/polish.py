from pathlib import Path
from docx import Document
from docx.shared import Cm
w=Path(__file__).parent
p=w.parent/'기술문서_전선현_담당절_수정본_v8.docx';d=Document(p)
drop=[
'비행은 지상 NavMesh와 높이 Trace를 사용한다.',
'PatternSet의 정의와 페이즈의 선택 목록을 분리한다.',
'상시 목록은 랜덤 선택 목록과 독립이다.',
'처치 후 다음 Tick에 엔딩을 시작한다.',
'HoldsLevelAfterDefeat가 레벨 이동을 보류한다.',
'겹친 방은 박스 안으로 가장 깊이 들어간 방을 선택한다.',
'그룹은 처음 맞은 조각의 반경과 평면·인접 허용 오차로 제한한다.',
'NavMesh는 실제 배치 위치와 빌드 상태를 확인한다.',
'마을은 스테이지 연결 허브, Stage3 저택은',
]
removed=[]
for par in list(d.paragraphs):
 t=par.text.removeprefix('• ')
 if any(t.startswith(x) for x in drop):
  removed.append(t);par._p.getparent().remove(par._p)
 elif par.text=='그림 6. 페이즈 전환과 실패 처리':par.text='그림 6. 페이즈 전환 흐름'
for t in d.tables:
 if len(t.columns)==2 and any(len(token)>=20 for row in t.rows for token in row.cells[0].text.split()):
  for col,cm in zip(t.columns,[6.3,9.6]):col.width=Cm(cm)
  for row in t.rows:
   for c,cm in zip(row.cells,[6.3,9.6]):c.width=Cm(cm)
 for row in t.rows:
  for cell in row.cells:
   if cell.text=='UCinematicTrailDecalComponent':cell.text='핏자국 컴포넌트'
   if cell.text=='Flag·bActWhenFlagSet':cell.text='Flag /\nbActWhenFlagSet'
for par in d.paragraphs:
 if par.style.name=='Caption':par.alignment=1
d.save(p)
print('Removed repeated bullets',len(removed))
print('characters',sum(len(p.text) for p in d.paragraphs)+sum(len(c.text) for t in d.tables for r in t.rows for c in r.cells))
