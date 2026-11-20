from pathlib import Path
from copy import deepcopy
import re,json,hashlib,math
from docx import Document
from docx.text.paragraph import Paragraph
from docx.table import Table
from docx.shared import Cm,Pt,RGBColor
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_TABLE_ALIGNMENT,WD_CELL_VERTICAL_ALIGNMENT
from docx.enum.style import WD_STYLE_TYPE
from PIL import Image,ImageDraw,ImageFont
W=Path('C:\\JSH\\Unreal\\MissionProj\\ProjGit\\문서검토\\work_v7'); OUT=W.parent
src=OUT/'기술문서_전선현_담당절_수정본_v6.docx'
ref=Path(r'C:/Users/user1/Downloads/기술문서_우진_2장_펫.docx')
d=Document(src); elements=list(d.element.body); blocks=json.loads((W/'blocks.json').read_text(encoding='utf-8'))
# 같은 원본의 관계 ID를 유지한 채 본문만 재편한다.
figs={}; last=None
for e in elements:
 if e.tag==qn('w:p'):
  p=Paragraph(e,d)
  if e.xpath('.//w:drawing'):last=deepcopy(e)
  if p.style.name=='Caption' and re.match(r'그림 \d+\.',p.text):
   figs[int(re.search(r'\d+',p.text).group())]=last
sect=deepcopy(d.sections[0]._sectPr)
for e in list(d.element.body):d.element.body.remove(e)
d.element.body.append(sect)
for name,size in [('Normal',12),('Heading 1',18),('Heading 2',14),('Heading 3',12),('Caption',10),('List Bullet',12),('List Number',12)]:
 if name not in d.styles:
  d.styles.add_style(name,WD_STYLE_TYPE.PARAGRAPH).base_style=d.styles['Normal']
 st=d.styles[name];st.font.name='맑은 고딕';st.font.size=Pt(size);st.font.color.rgb=RGBColor(0,0,0)
 st._element.get_or_add_rPr().rFonts.set(qn('w:eastAsia'),'맑은 고딕')
 pf=st.paragraph_format;pf.line_spacing=1.15;pf.space_before=Pt(0);pf.space_after=Pt(6);pf.keep_with_next=name.startswith('Heading');pf.page_break_before=False
 for x in st._element.xpath('./w:pPr/w:pBdr'):x.getparent().remove(x)
 if name.startswith('Heading'):pf.space_before=Pt(12);pf.space_after=Pt(7)
for s in d.sections:
 s.top_margin=Cm(2.5);s.bottom_margin=Cm(2.54);s.left_margin=Cm(2.54);s.right_margin=Cm(2.54)

step=0
def para(text,style='Normal'):
 global step
 if style=='List Number':
  step+=1;text=f'{step}. {text}'
 p=d.add_paragraph(text,style);p.paragraph_format.widow_control=True
 return p
def bullet(text):
 p=para('• '+text,'List Bullet');p.paragraph_format.left_indent=Cm(.35);p.paragraph_format.first_line_indent=Cm(-.35)
 return p
def heading(text,level=2):return para(text,'Heading '+str(level))
def table(rows,widths=None):
 t=d.add_table(rows=0,cols=len(rows[0]));t.alignment=WD_TABLE_ALIGNMENT.CENTER;t.autofit=False
 widths=widths or ([4,11.9] if len(rows[0])==2 else [15.9/len(rows[0])]*len(rows[0]))
 for row in rows:
  cs=t.add_row().cells
  for c,s in zip(cs,row):c.text=s
 for col,ww in zip(t.columns,widths):col.width=Cm(ww)
 for ri,row in enumerate(t.rows):
  pr=row._tr.get_or_add_trPr();pr.append(OxmlElement('w:cantSplit'))
  if ri==0:pr.append(OxmlElement('w:tblHeader'))
  for ci,c in enumerate(row.cells):
   c.width=Cm(widths[ci]);c.vertical_alignment=WD_CELL_VERTICAL_ALIGNMENT.CENTER
   tc=c._tc.get_or_add_tcPr();sh=OxmlElement('w:shd');sh.set(qn('w:fill'),'EEEEEE' if ri==0 else 'FFFFFF');tc.append(sh)
   bd=OxmlElement('w:tcBorders')
   for key in ['top','bottom','left','right']:
    z=OxmlElement('w:'+key);z.set(qn('w:val'),'single');z.set(qn('w:color'),'D9D9D9');z.set(qn('w:sz'),'4');bd.append(z)
   tc.append(bd)
   mar=OxmlElement('w:tcMar')
   for key in ['top','bottom','left','right']:
    z=OxmlElement('w:'+key);z.set(qn('w:w'),'75');z.set(qn('w:type'),'dxa');mar.append(z)
   tc.append(mar)
   for p in c.paragraphs:
    p.paragraph_format.space_after=Pt(3);p.paragraph_format.line_spacing=1.05;p.paragraph_format.keep_with_next=ri==0
    for r in p.runs:r.font.size=Pt(10.5);r.font.bold=ri==0
 para('').paragraph_format.space_after=Pt(2)
 return t
def oldtable(idx,maxrows=None):
 rows=blocks[idx]['rows'];rows=rows if maxrows is None else rows[:maxrows+1]
 # 元の文面から人名・細かな日時を除き、主語が抜けない範囲で整える。
 rows=[[re.sub(r'\s*\((?:\d{2}-\d{2}[^)]*)\)','',s).replace('10-20 우진 수정, 10-28 이동 컴포넌트로 정리','이동 컴포넌트로 통합') for s in row] for row in rows]
 table(rows,[2.7,3.0,6.0,4.2] if rows[0]==['현상','원인','해결','결과'] else None)
figcount=0
def fig(n,title,ex=''):
 global figcount
 e=deepcopy(figs[n]);d.element.body.insert(-1,e)
 p=Paragraph(e,d);p.paragraph_format.keep_with_next=True;p.alignment=WD_ALIGN_PARAGRAPH.CENTER
 # 同じ比率で本文幅に収める。
 for ext in e.xpath('.//wp:extent'):
  oldw=int(ext.get('cx'));oldh=int(ext.get('cy'));nw=int(Cm(15.7));nh=int(oldh*nw/oldw)
  if nh>Cm(10):nw=int(nw*Cm(10)/nh);nh=int(Cm(10))
  ext.set('cx',str(nw));ext.set('cy',str(nh))
  for a in e.xpath('.//a:xfrm/a:ext'):a.set('cx',str(nw));a.set('cy',str(nh))
 figcount+=1;para(f'그림 {figcount}. {title}','Caption')
 if ex:para(ex)

def sequence(name,actors,msgs):
 width=1500;top=140;gap=95;height=top+gap*len(msgs)+60
 im=Image.new('RGB',(width,height),'white');dr=ImageDraw.Draw(im);font=ImageFont.truetype('C:/Windows/Fonts/malgun.ttf',29);small=ImageFont.truetype('C:/Windows/Fonts/malgun.ttf',26)
 xs=[100+(width-200)*i/(len(actors)-1) for i in range(len(actors))]
 for x,a in zip(xs,actors):
  dr.rectangle((x-95,10,x+95,100),fill='#eeeeee',outline='#777777',width=2)
  lines=a.split('\n')
  for k,s in enumerate(lines):dr.text((x-dr.textlength(s,font=small)/2,20+32*k),s,font=small,fill='black')
  for y in range(105,height-10,18):dr.line((x,y,x,y+9),fill='#aaaaaa',width=2)
 for i,(a,b,label) in enumerate(msgs):
  y=top+gap*i;xa=xs[a];xb=xs[b];dr.line((xa,y,xb,y),fill='#555555',width=3)
  sg=1 if xb>xa else -1;dr.polygon([(xb,y),(xb-sg*14,y-7),(xb-sg*14,y+7)],fill='#555555')
  tw=dr.textlength(label,font=font);xx=(xa+xb-tw)/2
  dr.rectangle((xx-5,y-43,xx+tw+5,y-2),fill='white');dr.text((xx,y-43),label,font=font,fill='black')
 path=W/(name+'.png');im.save(path);return path
def newfig(path,title,ex=''):
 global figcount
 p=para('');p.alignment=WD_ALIGN_PARAGRAPH.CENTER;p.paragraph_format.keep_with_next=True;p.add_run().add_picture(str(path),width=Cm(15.7))
 figcount+=1;para(f'그림 {figcount}. {title}','Caption')
 if ex:para(ex)
def section(title,summary,items):
 heading(title)
 if summary:para(summary)
 for item in items:bullet(item)
def decision(text):para('설계 결정  '+text)

para('통합 기술 문서','Title');para('전선현 담당 시스템')
para('전투 프레임워크와 보스전, 시네마틱, 카메라 가림, 진행 복원을 다룬다. 각 절은 핵심 구조와 호출 순서, 설정 지점, 설계상의 이유를 중심으로 정리한다.')
heading('담당과 기여 범위')
table([['범위','기여'],['전선현 주요 구현','Enemy 공통 구조·패턴, 보스 공통 구조와 Stage3 전투·연출, 시네마틱, 카메라 가림, 시간대·복원 연결, 볼륨 설정'],['최요환 최초 구현 → 전선현 확장','문·이동 기믹의 잠금·연출 제어, 보스 레이저 GA의 공통 부모 통합, 진행 저장과 플래그 게이트 연결'],['강진구 최초 구현 → 전선현 통합·확장','폭탄·터렛 GA의 공통 부모 통합, ITargetableInterface 재사용, RoomMaskVolume의 컷어웨이 확장'],['공동 연결','보스 기믹은 최요환, 펫 내부 동작은 우진 담당이다. 이 문서는 전투·연출과 만나는 연결 계약을 다룬다.'],['레벨·최적화','메인 맵 조립과 Stage3 환경 구성, 관련 비용 측정·정리. 스테이지 전체 콘텐츠의 단독 제작을 뜻하지 않는다.']])
heading('구성')
para('0.4 GAS와 태그 / 3.5~3.6 기믹 확장 / 5 Enemy / 6 보스전 / 7 시네마틱 / 8 가림 / 9 진행·레벨 / 11 사운드·효과 / 12 성능 / 13~14 문제 해결·회고 / 부록 D')

heading('0.4 시스템별 GAS·GameplayTag 사용 현황',1)
table([['구성','Enemy·보스의 사용 방식'],['ASC / AttributeSet','캐릭터가 ASC를 소유한다. Health·MaxHealth·AttackPower와 Damage 메타 속성을 사용한다.'],['공격 GA','UEnemyPatternAbility를 공통 부모로 사용한다. 패턴마다 GA Spec을 부여한다.'],['GameplayTag','근접 판정 이벤트, Enemy.Pattern.Resource.*와 Hazard.*로 자원·위험 충돌을 표현한다.'],['식별자 구분','PatternId·PhaseId는 FName이다. RequestId·PhaseExecutionId는 실행 회차를 식별한다. 시간대 태그는 조명 프리셋과 저장에 사용한다.']])

heading('3. 상호작용과 기믹 프레임워크',1)
section('3.5 문·자물쇠 기믹 확장','전투 중에는 연출이 문을 제어하고, 전투 후에는 일반 신호와 플레이어 조작으로 되돌린다.',[
'강제 제어: ForceDoorState가 신호보다 우선한다. 연출 중에는 끼임 대기 없이 지정 시간에 이동한다.',
'잠금: SetLocked는 플레이어 조작만 차단한다. 문 위치나 신호·연출 제어는 바꾸지 않는다.',
'상태 인계: 해제 전에 신호를 최종 위치와 맞추고 ReleaseForcedDoorState를 호출한다.',
'자물쇠: RequiredKey와 RequiredKeyCount를 검사해 LinkedActors에 신호를 보낸다. 열쇠는 소모하지 않는다.'])
decision('문 이동과 연출 소유권을 구분한다. 잠금 해제와 문 열림도 별도 상태로 취급한다.')
heading('연출 종료 후 문 상태 인계',3)
for x in ['ForceDoorState로 위치를 결정하더라도 잠금 상태는 별도로 적용한다. 문을 열었다는 이유로 플레이어 잠금을 자동 해제하지 않는다.','강제 제어를 해제하기 전에 일반 신호를 최종 위치와 맞춘다. 그렇지 않으면 다음 신호 갱신에서 문이 원래 위치로 되돌아갈 수 있다.','스킵은 중간 이동을 재현하지 않고 최종 위치를 즉시 적용한다. 종료·초기화는 Director의 정리 경로에서 강제 제어를 해제한다.']:bullet(x)

section('3.6 경계 기믹','물리 경계와 진행 연출을 분리한다.',[
'SpikeBoundary: 닫힌 스플라인을 따라 보이지 않는 벽과 가시 ISM을 배치한다. BlockedChannels로 차단 대상을 정한다.',
'피해·넉백: 플레이어 조종 폰에 적용한다. 피해 대기 시간과 넉백은 별도 설정이며 적·동전은 채널에 따라 막힌다.',
'AreaExitBlocker: 트리거 진입 → 외부 컷신 잠금 → 펫 대사 → 포물선 복귀 → 잠금 해제 순서로 처리한다.',
'펫 StateTree는 BrainComponent로 일시정지·재개한다. 종료 시 카메라·말풍선 높이를 복원하고 필요하면 펫도 복귀시킨다.',
'진행 플래그는 ProgressFlagGate가 구독한다. 조건을 만족하면 출구 차단 액터를 제거한다.'])
decision('펫 위치가 매번 달라 고정 시퀀스 대신 런타임 카메라·이동을 사용한다. BeginExternalCutscene과 EndExternalCutscene은 반드시 짝으로 호출한다.')

heading('경계 구성과 연출 복구',3)
table([['구성','구현과 설정'],['SpikeBoundary','BoundarySpline을 따라 보이지 않는 벽과 가시 ISM을 별도 생성한다. 벽은 BlockedChannels, 가시는 메시·간격·줄 수·지터·시드로 설정한다.'],['AreaExitBlocker','BlockingBox는 물리 차단, TriggerBox는 연출 시작, PetViewCamera는 대사 구도를 맡는다. PetLines와 복귀 위치·비행 높이를 설정한다.'],['종료 정리','카메라를 플레이어로 되돌리고 비행 이동 모드를 Falling으로 전환한다. 말풍선 높이·펫 위치·StateTree·외부 컷신 상태를 정리한다.']])
heading('문제 해결',3)
oldtable(77)
bullet('대사나 펫을 얻지 못하면 대사 대기를 생략하고 복귀로 진행한다. 매 프레임 갱신은 포물선 이동 중에만 사용한다.')

heading('5. Enemy 프레임워크',1)
para('적의 판단, 패턴 선택, 실제 공격과 피격 수집을 분리한다. 기존 GA로 표현되는 공격은 데이터로 재사용하고 새로운 동작만 파생 GA로 확장한다.')
fig(2,'Enemy 공통 구조','Config는 설정, BT는 판단, PatternComponent는 실행 관리, GA는 공격 동작을 맡는다.')
section('5.1 캐릭터 구조와 GAS 연결','능력치·생존·사망은 AEnemyCharacter에 모으고 보스가 이를 확장한다.',[
'초기화: ASC의 Owner와 Avatar를 자기 자신으로 연결하고 Config·PatternSet을 적용한 뒤 AI를 시작한다.',
'피격: GE가 Damage 메타 속성을 변경하면 CalculateHealthAfterDamage로 Health를 계산한다. 보스는 이 지점에서 무적·페이즈 하한을 반영한다.',
'사망: Dying 전환 → 패턴·AI·어빌리티 정지 → 사망 몽타주 → FinishDeath → 래그돌 → 시체 수명 종료·제거.',
'사망 종료는 Notify와 몽타주 종료 델리게이트가 같은 함수를 호출한다. Dying 검사로 중복을 막고 재생 실패 시 즉시 마무리한다.'])
decision('싱글 플레이 AI는 캐릭터 소유 ASC로 단순화한다. 피해 정책은 가상 함수에 모아 보스가 공통 피격 경로를 재사용한다.')
section('5.2 데이터 기반 설정','능력치, 공격 목록, GA 전용 값을 서로 다른 편집 단위로 둔다.',[])
oldtable(122)
heading('새 적과 공격 추가',3)
for s in ['적 BP에 외형·애님 BP·사망 몽타주를 지정한다.','Config에 능력치·인지 범위·호환 BT를 지정한다.','PatternSet에 GA·조건·ExecutionSettings를 추가하고 Config.PatternSet에 연결한다.','근접 몽타주에 판정 NotifyState를 배치하고 실제 소스·소켓을 확인한다.']:para(s,'List Number')
bullet('공통 검증은 ID 누락·중복과 수치 범위를 확인한다. GA 전용 설정, 몽타주·소켓·앵커는 실행 경로가 추가 검사한다.')
decision('GA별 값은 인라인 Instanced 설정으로 분리한다. 새 동작이나 공통 BT로 표현할 수 없는 판단까지 무코드로 추가되는 구조는 아니다.')
heading('설정 분리의 이유',3)
for x in ['종류마다 Config 클래스를 늘리는 대신 공통 Config와 패턴 목록을 조합한다. 공격별로 에셋 파일을 하나씩 만드는 방식보다 목록 안에서 조건을 비교하기 쉽다.','GA BP 기본값만 사용하면 같은 GA를 쓰는 두 패턴의 몽타주·조건이 묶인다. ExecutionSettings를 항목에 인라인으로 두어 같은 동작의 변형을 분리한다.','DamageMultiplier는 개별 패턴 항목에 둔다. 최종 공격 값은 AttackPower × 해당 항목의 배율로 계산한다.']:bullet(x)
heading('문제 해결',3)
oldtable(129)

section('5.3 AI와 Behavior Tree','BT는 공격 내용을 구현하지 않고 공격·추적·복귀를 결정한다.',[
'EnemyAIController: 초기화와 빙의가 모두 준비되면 BT를 시작한다. 인지 결과를 타깃과 Focus에 반영한다.',
'Service: 거리·시야·공격 가능 여부와 홈 위치 상태를 Blackboard에 기록한다.',
'ExecuteAttack: 패턴 선택 → 방향 정렬 → 실행 요청 → 결과 확인. PatternId를 비우면 자동 선택한다.',
'PatternMovement: IEnemyLocomotionProvider에서 이동 의도를 받아 실행한다. 보스 구체 클래스에 의존하지 않는다.',
'Task·Service는 키 Selector를 사용하고 컨트롤러의 기본 키는 이름 상수와 타입 검사로 검증한다.'])
heading('초기화와 판단 경계',3)
for x in ['BeginPlay와 OnPossess가 모두 준비된 뒤 TryStartEnemyAI를 호출한다. 먼저 호출된 쪽은 초기화가 끝날 때까지 시작을 미룬다.','BT·Blackboard·필수 키 타입을 검증하고 HomeLocation을 기록한다. 필수 계약이 맞지 않으면 오류를 남기고 BT를 시작하지 않는다.','시야에 들어온 플레이어 조종 Pawn을 전투 타깃으로 삼고, 놓치면 타깃 해제와 복귀 분기로 이어진다.','공격 요청 준비, 사거리, 실제 실행 가능 여부를 분리한다. 후딜이나 시야 때문에 접근할 거리 판단까지 흔들리지 않게 한다.']:bullet(x)
heading('문제 해결',3)
table([['현상','원인','해결','결과'],['이동 Task가 보스에 결합됨','구체 보스 컴포넌트에서 이동 정책 조회','IEnemyLocomotionProvider 계약으로 위임','공통 Task가 일반 적과 보스의 이동 의도를 처리']],[3,3,5.5,4.4])
decision('공격 종류마다 Task를 만들지 않는다. BT는 언제 공격할지 정하고, 패턴 ID로 실행을 위임한다.')

section('5.4 이동과 회전 보정','루트 모션 이동을 유지하면서 공격 회전·비행 높이·겹침을 보정한다.',[])
oldtable(158)
bullet('비행은 지상 NavMesh와 높이 Trace를 사용한다. 다층 지형에서는 두 판정이 같은 층을 선택하는지 확인해야 한다.')
bullet('이동·회전 잠금은 참조 횟수로 관리하며, GA는 자신이 획득한 잠금만 해제한다.')
heading('설정과 적용 범위',3)
for x in ['비행형은 Flying 모드·bUseGroundPathForFlight·유효한 지상 NavMesh·지형 콜리전·초기 높이가 함께 필요하다. 자유로운 3차원 경로 탐색은 제공하지 않는다.','루트 모션 전체를 끄면 돌진·내려찍기의 이동이 사라진다. 공격 잠금은 회전만 차단하고 위치 이동과 충돌 해결은 유지한다.','플레이어 밀어내기는 인접 적에 제한된 속도로 적용한다. 보스는 PlayerPushMultiplier=0으로 일반 적과 다르게 취급한다.']:bullet(x)
heading('문제 해결',3)
table([['현상','원인','해결','결과'],['비행 적 자세가 기울어짐','루트 모션 회전 누적','비행 루트 모션에서 회전 성분 제거','이동 중 자세 유지'],['공격 중 몸이 돌아감','Focus 회전과 루트 모션 회전이 함께 적용','잠금 중 FaceRotation 무시·이동 컴포넌트의 새 회전을 현재 회전으로 교체','위치 이동·충돌 해결 유지']],[3,3,5.5,4.4])

section('5.5 패턴 시스템','실행 조건과 수명 관리는 컴포넌트에, 공격 동작은 부모 GA와 파생 GA에 둔다.',[])
newfig(sequence('pattern-sequence',['BT Task','패턴 컴포넌트','패턴 GA'],[(0,1,'① 패턴 선택·실행 요청'),(1,2,'② Spec으로 활성화'),(2,1,'③ 설정·타깃·요청 ID 조회'),(2,1,'④ 종료 결과 반영'),(1,0,'⑤ 후딜 뒤 결과 확인')]),'패턴 실행 시퀀스','BT는 요청 ID로 상태를 확인한다. 같은 클래스의 GA라도 패턴 항목별 Spec과 실행 문맥을 구분한다.')
for s in ['요청 ID: 취소·사망·페이즈 전환 뒤 도착한 늦은 통지는 현재 실행을 변경하지 못한다.','부모 GA: CanStartPattern → StartPattern → StopPattern 계약과 잠금 소유권을 제공한다. 종료는 FinishPattern 또는 CancelPattern으로 통일한다.','슬롯: Spec·타깃·자원·위험도·후딜을 보관한다. 같은 PatternId는 동시에 중복 실행하지 않는다.','시간 초과: MaxExecutionSeconds는 공격 쿨다운이 아니라 종료 누락을 감지하는 안전장치다.','잠금 정리: EndAbility가 파생 자원을 정리하고 자신이 소유한 이동·회전 잠금을 해제한다.']:bullet(s)
decision('클래스별 Task와 복제된 정리 코드 대신 패턴 요청 하나와 부모 GA 계약을 사용한다. 선택 평가 비용과 실행 상태의 가시화는 후속 개선 대상이다.')
heading('실행 수명과 예외 처리',3)
table([['구분','역할'],['실행 제한','MaxExecutionSeconds는 EndAbility 누락을 감지한다. 정상 공격 길이보다 짧게 설정하면 실행 중 실패로 끝날 수 있다.'],['패턴 후딜','RecoverySeconds는 정상 종료 뒤의 추가 대기다. 공격 동작과 다음 요청 사이를 분리한다.'],['공통 공격 간격','EnemyConfig의 MinAttackInterval·MaxAttackInterval로 패턴 후딜 이후의 공통 공격 간격을 설정한다.'],['오래된 완료 통지','요청 ID가 현재 실행과 다르면 무시한다. 지연 시작으로 예약한 슬롯도 취소 경로에서 정리한다.']])
heading('문제 해결',3)
table([['현상','원인','해결','결과'],['같은 GA의 두 패턴 실행이 구분되지 않음','GA 클래스 단위의 실행 문맥 공유','패턴 항목마다 Spec과 요청 ID 부여','각 설정·결과와 정리 대상을 구분'],['취소 뒤 잠금 잔류','GA별 수동 해제 경로 누락','부모 EndAbility에서 소유한 잠금만 해제','정상·취소 정리 경로 통일']],[3,3,5.5,4.4])
heading('실행 관리 내부의 책임',3)
table([['구성','담당 범위'],['UEnemyPatternComponent','실행 요청·슬롯·결과·타이머·후딜과 취소를 조정'],['FEnemyPatternCatalog','패턴 목록·개별 GA Spec·유효 자원·허용 목록·가중치 재정의 보관'],['EnemyPatternOccupancy','슬롯·잔존 생성물의 점유 수집, 자원·위험도 합류 검사, 가중치 추첨'],['상시 패턴 구현 파일','같은 컴포넌트의 상시 시작·재시도·반복 처리 분리. 별도 UObject를 만들지는 않음']])
bullet('목록·선택 설정과 실행 상태를 구분하고, 점유 계산은 배열 뷰를 받는 함수로 분리한다. 요청 번호와 슬롯의 소유권은 컴포넌트에 유지한다.')
heading('새 동작을 추가할 때의 계약',3)
table([['확장 지점','구현 책임'],['CanStartPattern','설정 형식·몽타주·소스·앵커 등 실제 실행 조건 확인'],['StartPattern','실행 문맥을 받아 동작 시작. 이동·회전은 부모 도우미를 통해 소유권을 기록'],['FinishPattern / CancelPattern','성공·포기를 공통 종료 경로로 전달. 독립적으로 완료 상태를 덮어쓰지 않음'],['StopPattern','타이머·구독·몽타주·소환물 등 파생 동작의 자원 정리. 취소 중 재진입을 고려']])
bullet('보조·상시는 몸을 사용하지 않는다. 부모 도우미가 해당 실행의 이동·회전·몽타주 사용을 생략하므로, 파생 GA가 별도 경로로 몸을 조작하지 않아야 한다.')
bullet('패턴 컴포넌트는 프레임 Tick 대신 GAS 종료 이벤트와 타이머로 수명을 관리한다. BT가 결과를 받은 뒤 ReleaseRequest로 주 패턴 결과와 타깃을 해제한다.')

section('5.6 피격 판정과 피해 경로','판정 수집은 대상 후보만 알리고 GA가 중복·시야·피해를 처리한다.',[])
newfig(sequence('hit-sequence',['몽타주','Trace / GA','대상'],[(0,1,'① Notify로 판정 창 열기'),(1,2,'② 이전→현재 소켓 Sphere Sweep'),(1,2,'③ 적대·중복·시야 검사 후 피해 전달'),(0,1,'④ 창 닫기·정리')]),'근접 판정과 피해 전달')
for s in ['PrepareAttack이 소스·소켓을 확인하고 최신 포즈 갱신을 보장한다. 판정 창 동안만 Trace Tick을 켠다.','IsHostileTarget은 생존·인터페이스·같은 편 여부를 확인한다. 근접 검색은 ECC_Pawn이고 캐릭터는 캡슐만 피격 대상으로 삼는다.','한 공격에서 대상별 1회만 처리한다. 재진입으로 GA가 정리될 수 있어 피해 전달 전에 기록과 타격 연출을 수행한다.','EnemyTargetUtils::ApplyAttackHit → ApplyPointDamage → 대상 TakeDamage로 전달한다. 반환 true는 실제 체력 감소를 보장하지 않는다.','플레이어→적은 GE Damage, 적→대상은 TakeDamage 경로다. 다른 팀원이 구현한 레이저·터렛·코인 경로까지 모두 통합된 것은 아니다.']:bullet(s)
decision('이전·현재 위치 사이 Sweep으로 빠른 이동의 누락을 줄인다. 모든 곡선 궤적이나 콜리전 설정에서 무누락을 보장하지는 않는다.')
heading('새 대상과 판정 소스 연결',3)
for x in ['새 대상은 ITargetableInterface의 생존 계약과 TakeDamage 수신 경로를 제공한다. 근접 수집의 Pawn 오브젝트 타입·피격 컴포넌트 조건도 맞춘다.','몽타주 NotifyState는 Gameplay Event로 판정 창을 알린다. GA는 이벤트의 소유자·메시·몽타주가 현재 공격과 일치하는지 확인한다.','같은 편은 Enemy 캐릭터 또는 적을 Instigator로 가진 액터를 기준으로 판정한다. 수집·GA 피격 처리·최종 전달에서 함께 검사한다.']:bullet(x)
heading('문제 해결',3)
table([['현상','원인','해결','결과'],['옆 적에 타격음·피해 이벤트 발생','후보 수집에 같은 편 구분 없음','IsHostileTarget을 수집·GA·피해 전달에 적용','연출과 피해의 대상 판정 일치'],['피해 중 GA가 취소되면 기록 누락','피해 콜백이 사망·정리 유발','피해 전달 전에 대상 기록·타격 연출 수행','대상별 1회 처리 유지']],[3,3,5.5,4.4])

section('5.7 전투 피드백과 머리 위 체력바','전투 판정과 표시·연출의 수명을 분리한다.',[
'FEnemyFeedbackCue는 소리·Niagara·절차적 스타일을 묶는다. 비어 있는 슬롯은 재생하지 않는다.',
'절차적 연출이 생성되면 이를 사용하고, 생성하지 못했을 때 Effect가 있으면 Niagara를 사용한다.',
'체력바는 체력 변경과 사망을 구독한다. 내용이 변할 때 재그리기를 요청하며 카메라 방향 회전은 별도로 갱신한다.',
'최근 렌더되지 않은 체력바의 회전은 생략한다. 사망 시 숨기고 Tick·구독을 정리한다. 보스 체력바는 별도 HUD 경로다.'])
heading('표시와 피드백 설정',3)
for x in ['적 BP에는 DeathCue·CorpseVanishCue와 체력바 컴포넌트·위젯 클래스를 지정한다. 패턴별 연출은 실행 설정에, 동작 프레임 연출은 Enemy Feedback Cue Notify에 둔다.','PlayAtLocation은 월드 위치에서, PlayAttached는 대상에 붙여 재생한다. 붙인 연출은 반환 핸들을 보관해 취소·초기화에서 정지한다.','체력바 내용 재그리기와 빌보드 회전은 별도 작업이다. 값이 같을 때 재그리기를 생략해도 카메라 방향 갱신까지 자동으로 줄어들지는 않는다.']:bullet(x)
heading('문제 해결',3)
table([['현상','원인','해결','결과'],['이름 변경 후 효과 누락','문자열 이름·경로 매핑에 의존','FEnemyFeedbackCue와 전용 Notify의 에셋 참조로 교체','편집 가능한 슬롯으로 연결'],['체력바 표시 갱신 비용','값이 같아도 계속 다시 그림','체력 변경 시 재그리기 요청','표시 수명·회전 갱신과 분리. 수치는 12장 참조']],[3,3,5.5,4.4])

section('5.8 설계 변천과 리팩토링','중복 실행·정리와 레벨 의존을 줄이는 방향으로 책임을 재배치했다.',[])
table([['이전 문제','현재 구조'],['GA마다 잠금·취소·판정 처리 반복','공통 부모 GA와 Trace 컴포넌트로 실행 계약 통일'],['보스 연출 클래스가 문·컷·페이즈를 모두 소유','CinematicSubsystem, StageEncounterDirector, Stage3 파생으로 분리'],['이름·경로 조회와 개별 효과 필드','프로퍼티·인터페이스·FeedbackCue·공용 설정으로 연결'],['정리 순서가 경로마다 다름','TearDownEncounter와 상태 변경 경로 통일']])
bullet('구조 변경과 기능 추가를 구분한다. 에셋 형식 변경은 호환 읽기·이전·재저장·재조사까지 수행해야 완료된다.')

heading('리팩토링의 범위와 결과',3)
fig(5,'전투와 스테이지 연출의 책임 분리')
for x in ['일반 적·보스·팀원 패턴을 공통 부모 계약으로 연결하되, 폭탄·터렛·레이저의 고유 동작은 각 구현에 남겼다. 통합 대상은 조건·실행 문맥·잠금·취소와 정리다.','범용 시네마틱은 재생 수명만 맡고 문·계단·페이즈 같은 게임 의미는 Director에 둔다. 스테이지 전용 기능이 공통 서브시스템으로 새어 들어가지 않게 했다.','데이터 형식 변경은 조사 → 호환 읽기·이전 → 재저장 → 재조사 순서로 진행했다. 코드가 빌드되는 것과 기존 콘텐츠 값이 보존되는 것은 별도로 확인한다.','책임을 나눈 결과와 성능 개선은 구분한다. 실행 비용은 같은 조건의 측정 결과로 평가하며 12장에 따로 기록한다.']:bullet(x)

heading('6. 보스전',1)
section('6.1 보스 공통 구조와 페이즈','일반 적의 공격 구조 위에 체력 구간, 기믹 진행, 전환 연출을 추가한다.',[])
fig(6,'보스의 공통 구조와 외부 계약')
for s in ['Combat: HealthFloorRatio에 도달하면 다음 페이즈를 요청한다. 마지막 Combat의 하한은 0이다.','Barrier: 직전 Combat의 체력 하한을 유지하고 기믹 완료로 진행한다.','전환 요청: 진입 ID를 무효화하고 패턴·기믹을 중단한다. TransitionPending → Transitioning → Active로 진행한다.','연출 선택: 레벨 담당자 → 페이즈 기본 시퀀스 → 연출 없이 완료 순이다.','실패 구분: 시퀀스 시작 실패는 연출 생략, 다음 페이즈 설정 적용 실패는 전투 실패 통지·재초기화 경로다.']:bullet(s)
decision('페이즈 진행과 연출을 인터페이스로 분리한다. PhaseExecutionId와 TransitionId는 이전 실행의 늦은 완료를 차단한다.')
heading('페이즈 전환 순서',3)
fig(7,'페이즈 전환과 실패 처리','현재 진입을 무효화한 뒤 전투 동작을 정지한다. 연출 완료 후 새 페이즈 설정을 적용하며 설정 실패는 전투 실패로 이어진다.')
heading('설정과 검증',3)
oldtable(287)
for x in ['새 보스 BP에 PhaseConfig·PatternSet·BT를 지정하고 매니저의 BossClass로 연결한다. 첫 페이즈는 Combat, 체력 하한은 엄격히 감소, 마지막 Combat의 하한은 0이어야 한다.','이동 정책은 실행 중 패턴 → 선택된 패턴 → 페이즈 기본 순으로 정한다. Waypoints는 매니저가 FName 경로 키로 전달한다.','전투 전·전환 중·실패 예약 중에는 피해를 무시한다. 재도전은 이전 보스의 복잡한 내부 상태를 되감는 대신 정리 후 새 보스를 생성한다.']:bullet(x)
heading('문제 해결',3)
table([['현상','원인','해결','결과'],['보스 수정 없이 연출 교체 어려움','보스가 시퀀스 직접 생성·재생','IBossPhaseTransitionHandler로 시작·완료 위임','레벨 연출과 전투 상태 분리'],['전환 뒤 늦은 완료가 다음 진입에 영향','이전 기믹·경로의 지연 콜백','진입 ID·전환 ID를 검증','현재 실행에 대한 통지만 수락']],[3,3,5.5,4.4])

section('6.2 패턴 운용','페이즈는 허용 패턴·가중치·동시 실행 조건을 정하고 실제 실행은 공통 패턴 컴포넌트에 맡긴다.',[])
oldtable(321)
bullet('설정 순서: PatternSet에 정의 → 페이즈 Patterns에 ID 연결 → 가중치·동시 실행 수·보조 허용·자원·위험도 조정.')
decision('자원 태그는 기술적 충돌, Threat는 공격 조합의 위험도를 제한한다. 패턴 조합마다 별도 금지 표를 추가하지 않아도 된다.')
heading('동시 실행 모델',3)
fig(4,'주 패턴과 보조·상시 패턴의 제약','주·보조는 예약·실행·후딜 슬롯을 함께 센다. 상시는 수 제한에서 제외하지만 자원·위험도 조건을 따라야 한다.')
for x in ['PatternSet의 정의와 페이즈의 선택 목록을 분리한다. 페이즈 Patterns에서 허용 ID와 가중치를 바꾸며, 자동 선택은 유효 가중치가 양수인 후보만 추첨한다.','생성물은 GA 종료 뒤에도 남을 수 있다. 등록된 생성물이 사라질 때까지 Hazard 태그와 Threat를 점유해 다음 공격의 조합을 제한한다.','상시 목록은 랜덤 선택 목록과 독립이다. 상시 타깃은 AI의 현재 타깃이며, 자원 충돌·시작 실패는 재시도하고 정상 완료는 RestartInterval 설정을 따른다.']:bullet(x)
heading('문제 해결',3)
oldtable(327)

section('6.3 보스와 기믹의 연결','보스는 레벨 기믹의 구체 구현 대신 IBossPhaseGimmickInterface와 통신한다.',[])
newfig(sequence('gimmick-sequence',['페이즈','보스','기믹 구현'],[(0,1,'① PhaseId·진입 ID 발급'),(1,2,'② EnterBossPhase(ID 전달)'),(2,1,'③ 완료 통지(ID 반환)'),(1,0,'④ 객체·현재 상태·ID 검증')]),'기믹 시작과 완료 검증')
for s in ['BossEncounterManager가 배치 기믹 참조를 보스 초기화에 전달한다.','보스당 기믹 구현 컴포넌트 하나를 연결한다. Barrier에 구현이 없으면 진입을 실패시킨다.','완료는 연결 객체·전투 상태·생존 여부와 현재 Barrier의 PhaseId·진입 ID가 모두 맞아야 수락한다.','전환·사망·초기화는 StopBossGimmicks로 중단한다. 개별 배리어·코인의 동작은 기믹 원고와 연결한다.']:bullet(s)
heading('기믹 연결 설정',3)
for x in ['매니저의 기믹 액터 참조를 해당 보스의 초기화에 전달한다. 이름으로 월드 전체를 검색해 구현을 고르지 않는다.','기믹은 진입 때 받은 번호를 완료 통지에 그대로 돌려준다. 같은 PhaseId에 다시 진입하더라도 이전 회차의 완료와 구분한다.','Barrier 동안 체력은 직전 Combat 하한에 묶인다. 화면 안내는 보스 체력바가 현재 페이즈 상태를 읽어 표시한다.']:bullet(x)
decision('기믹 구현과 보스 진행을 인터페이스 하나로 연결해 구체 클래스 헤더 의존을 줄인다. 배리어·코인의 내부 동작은 연결된 기믹 원고에서 다룬다.')

section('6.5.1 Stage3 전체 흐름과 보스 체력바','매니저는 전투 수명, Director는 스테이지 연출, 표시 컴포넌트는 체력바·BGM을 맡는다.',[])
fig(11,'Stage3의 공간 진행과 전투 단계','Combat은 체력 조건, Barrier는 기믹 완료로 진행한다. 컷 종료 후 플레이어 도착 대기는 Director가 처리한다.')
for s in ['Trigger는 시작만 요청한다. 도착·자물쇠·마당·등장은 Stage3 오프닝 단계로 관리한다.','실패·재도전은 TearDownEncounter로 정리한 뒤 보스를 재생성한다. 지연 콜백은 EncounterGeneration이 다르면 무시한다.','체력바·BGM은 PresentationComponent가 묶어 표시·해제한다. 정리 경로마다 따로 구현하지 않는다.','페이즈 경계선은 PhaseConfig의 Combat 하한에서 만든다. 잠금 표시는 보스·페이즈 상태에서 계산한다.']:bullet(s)
oldtable(359)
heading('보스방 구성과 재도전',3)
fig(9,'보스방 매니저와 스테이지 연출의 책임')
table([['편집 대상','주요 연결'],['보스방 매니저','BossClass, LevelAfterDefeat, DefeatedProgressFlag, BossBGM, PhaseGimmickActors, MovementRoutes'],['Stage Director','Encounter에 매니저 지정. PhaseStagings, Doors, CinematicCues와 각 시퀀스 연결'],['트리거','매니저에 시작 요청. 중복 요청 처리와 실제 전투 시작은 현재 상태가 결정'],['PresentationComponent','체력바·BGM의 표시와 해제. 전투 정리에서 함께 종료']])
bullet('플레이어 사망 시 전투를 중지하고 Waiting으로 전환한다. 부활 후 초기화하며, 페이즈 적용 실패도 다음 Tick에 ResetEncounter로 연결한다.')
heading('문제 해결',3)
table([['현상','원인','해결','결과'],['체력바·BGM이 남음','실패·재도전 경로마다 별도 정리','TearDownEncounter와 표시 컴포넌트로 통일','같은 정리 경로 사용'],['컷 종료 후 이전 위치로 복귀','플레이어 Transform 트랙 복원','해당 트랙을 KeepState로 구성','컷이 옮긴 위치에서 재개'],['긴 전환 컷이 중간에 종료','페이즈 전환 제한 시간 부족','컷 길이에 맞춰 TransitionTimeout 조정','데이터 설정으로 제한 시간 관리']],[3,3,5.5,4.4])

section('6.5.2 돌진·근접·머니건','근접과 돌진은 몸을 사용하는 공격, 머니건은 별도 소환 오브젝트로 동작한다.',[])
table([['패턴','실행과 종료'],['근접','설정·몽타주 확인 → 판정 준비·잠금 → 몽타주 재생 → Notify 판정 → 종료. 시야 조건이 켜져 있으면 맞은 순간에도 재검사'],['돌진','근접 GA를 상속한다. Rush에서 방향·거리를 확정하고 자세를 유지한 채 직접 이동한다. 거리 도달 또는 장애물에 막히면 Attack 섹션으로 전환'],['머니건','태그로 좌우 앵커를 찾아 총 두 자루를 소환한다. 총이 등장·장전·연사·퇴장을 수행하고 GA는 완료·실패를 확인한다. 취소 시 총·탄환을 정리']])
bullet('돌진 판정은 이동 구간 전체 Sweep으로 수집하며 근접의 공통 피격 처리를 거친다. 직선 이동이므로 장애물을 우회하지 않는다.')
bullet('머니건은 예상 실행 시간과 안전 여유가 MaxExecutionSeconds보다 짧아야 시작한다. 총·탄환·앵커와 연사 값은 설정으로 연결한다.')
decision('돌진은 근접 판정을 재사용하고 이동만 확장한다. 레이저 빔·그리드의 개별 구현은 해당 원고에서 다루며 공통 부모 계약만 공유한다.')

heading('돌진의 이동과 판정',3)
for x in ['준비: OnMontageStarted에서 루트 모션을 무시하고 Rush 뒤에 Attack을 연결한다. Rush 시작 시점까지 타이머로 진행을 확인한다.','돌진: 타깃까지의 수평 거리와 정면 방향을 확정하고 몽타주를 일시정지한다. RushSpeed × DeltaTime만큼 직접 이동하며 자세를 유지한다.','판정: 프레임 이동 구간 전체를 Sphere Sweep하고 근접의 HandleTargetHit으로 전달한다. 순간 위치 Overlap만 사용하는 것보다 빠른 이동의 누락을 줄인다.','종료: 목표 거리 도달 또는 충돌 정지 때 몽타주를 재개해 Attack으로 넘어간다. 막혔을 때만 RushBlockedCue를 재생한다.']:bullet(x)
heading('머니건의 독립 수명',3)
for x in ['보스 BP의 SceneComponent에 좌우 앵커 태그를 지정한다. GA는 태그로 위치를 받아 AMoneyGun 두 자루를 생성한다.','총은 조준·등장·장전·발사 준비·연사·퇴장을 진행한다. GA는 HasFinished·HasFailed를 확인하며 전체 동작을 다시 구현하지 않는다.','탄환은 직선으로 이동하고 첫 유효 충돌에서 한 번만 피해를 전달한다. 실패·타깃 소멸·취소에서는 총과 탄환을 정리한다.','몸을 사용하지 않으므로 보조 패턴으로 실행할 수 있다. 장전·연사 길이를 늘릴 때는 예상 실행 시간과 MaxExecutionSeconds를 함께 조정한다.']:bullet(x)
heading('설정 지점',3)
table([['대상','설정'],['근접','UEnemyMeleePatternSettings의 몽타주·판정 소스·소켓·큐. 몽타주에 EnemyMeleeTrace NotifyState 배치'],['돌진','근접 설정 재사용. GA BP의 RushSpeed, RushSection, AttackSection, RushHitRadius와 큐 지정'],['머니건','UMoneyGunPatternSettings의 총·탄환 클래스·앵커 태그·장전·연사 값 지정']])
heading('문제 해결',3)
table([['현상','원인','해결','결과'],['돌진이 종료되지 않을 수 있음','도착·충돌 종료 조건 부족','목표 거리 또는 장애물 충돌로 종료','몽타주 Attack 단계로 복귀'],['빠른 돌진에서 판정 누락','긴 프레임 이동을 점 위치로 검사','이동 구간 전체 Sweep','직선 이동 구간의 누락 위험 감소'],['벽 너머 근접 타격','후보 접촉만 확인','시야 조건이 있는 패턴은 피해 직전 재검사','시작 조건과 실제 타격 조건 구분']],[3,3,5.5,4.4])

heading('7. 시네마틱',1)
section('7.1 범용 CinematicSubsystem','요청자는 재생 데이터와 후속 처리만 넘기고 서브시스템이 재생 수명을 관리한다.',[])
fig(12,'범용 시네마틱의 데이터와 재생 수명')
newfig(sequence('cinematic-sequence',['요청자','서브시스템','시퀀스'],[(0,1,'① Request·바인딩·콜백 전달'),(1,2,'② 플레이어 생성·재생'),(1,0,'③ Handle 반환'),(2,1,'④ 종료 이유 기록'),(1,0,'⑤ 마커·종료 콜백')]),'재생 요청과 종료 시퀀스','자연 종료는 Tick에서 처리한다. 명시적 스킵은 스킵 호출 안에서 종료하며 Cancel은 콜백 없이 정리한다.')
for s in ['FCinematicRequest: Sequence·Bindings·TransformOrigin·컷신/스킵 여부·EndState·제한 시간을 전달한다.','FCinematicHandle: 요청자가 재생을 조회·취소하는 식별자다. 리스너는 플레이어 종료를 해당 핸들에 연결한다.','Marked Frame은 한 번 읽고 시간순으로 전달한다. Door·Cue 같은 명령의 의미는 요청자가 해석한다.','콜백은 재생 추가·취소를 유발할 수 있다. 목록 원소 참조를 유지하지 않고 핸들로 다시 검색한다.','Play 내부에서는 종료 콜백을 부르지 않는다. 시작 실패는 무효 핸들을 반환하고 요청자가 처리한다.']:bullet(s)
decision('재생 관리와 게임 의미를 분리한다. Cancel은 요청자의 정리 중 재진입을 막기 위해 남은 마커·종료 콜백을 부르지 않는다.')
heading('요청 구성과 종료 계약',3)
oldtable(432)
for x in ['Sequence에 바인딩 태그를 지정하고 Request.Bindings에 같은 이름으로 런타임 액터를 전달한다. 시퀀스에 없는 태그는 무시한다.','LI 로컬 좌표로 작성한 시퀀스에는 bUseTransformOrigin과 TransformOrigin을 지정해 현재 배치 변환을 적용한다.','EndPlayback과 Cancel은 재생을 목록에서 먼저 분리한 뒤 정리한다. 콜백이 같은 핸들을 취소하거나 새 재생을 추가해도 같은 항목을 다시 정리하지 않게 한다.','종료 델리게이트 자체에는 핸들이 없으므로 재생별 UCinematicPlaybackListener가 자기 핸들을 기억한다. Finished·Skipped·Interrupted·TimedOut을 요청자에게 구분해 전달한다.']:bullet(x)
heading('문제 해결',3)
oldtable(435)

section('7.2 BP용 Play Cinematic 노드','UPlayCinematicAsyncAction으로 C++과 같은 재생 경로를 BP에 제공한다.',[])
oldtable(440)
bullet('BP는 Request를 만들고 노드의 On Marker·On Ended·On Failed를 연결한다. 실패 후 다음 단계 진행 여부는 호출자가 결정한다.')
heading('BP 연결 순서',3)
for x in ['Make FCinematicRequest에 시퀀스와 바인딩·옵션을 채우고 Play Cinematic을 호출한다. 비동기 액션은 게임 인스턴스에 등록해 재생 중 수명을 유지한다.','On Marker에서 명령을 해석한다. bReachedByPlayback=false이면 이동 애니메이션 대신 최종 상태를 적용한다.','On Ended에서는 종료 이유에 맞춰 다음 단계를 결정한다. On Failed는 시작 실패이므로 진행할지 중단할지 호출자가 처리한다.','액션은 종료 브로드캐스트 전에 핸들을 비운다. 종료 이벤트에서 Cancel을 다시 호출해도 같은 재생을 취소하지 않는다.']:bullet(x)
decision('일반 함수 반환값만으로는 마커와 종료를 이벤트 핀으로 받을 수 없어 전용 비동기 노드로 노출했다.')

section('7.3 스킵과 입력 차단 및 상태 복원','스킵은 연출 시간을 생략하되 진행에 필요한 상태 변화는 적용해야 한다.',[])
table([['단계','처리'],['입력 차단','진행 중 펫 모드 입력을 먼저 취소한다. 게임플레이 IMC의 우선순위를 저장하고 제거한 뒤 스킵 전용 IMC를 추가한다.'],['스킵','홀드 완료 → 가장 최근의 스킵 가능 컷 선택 → GoToEndAndStop → 남은 마커를 bReachedByPlayback=false로 전달 → 종료'],['입력 복원','스킵 IMC를 제거하고 저장한 매핑을 복원한다. 누르고 있던 키는 해제할 때까지 무시해 종료 직후 공격을 막는다.'],['가림 복원','컷신 시작 시 숨긴 메시·조명 그림자를 복원하고 판정을 정지한다. 마지막 컷신이 끝나면 재개한다.']])
oldtable(477)
for s in ['EndState는 섹션 설정 유지·전체 복원·최종 상태 유지 중 선택한다. Stage Director는 플레이어 위치 트랙의 KeepState를 활용한다.','외부 연출은 BeginExternalCutscene/EndExternalCutscene으로 같은 컷신 횟수에 참여한다. 독립 입력 잠금 전체를 자동 통합하는 것은 아니다.','일반 컷신의 입력 차단과 펫 AI 정지는 별개다. 출구 차단·엔딩 등 필요한 연출이 StateTree를 별도 정지·재개한다.']:bullet(s)
decision('중간 이벤트를 상태 마커로 표현한다. 스킵으로 몰려 온 Cue는 기본 생략하고 필요한 큐만 bPlayWhenSkipped로 허용한다.')
heading('복원 책임의 구분',3)
table([['처리 주체','책임'],['서브시스템·컨트롤러','컷신 횟수, 입력 매핑, 스킵 상태와 가림 정지·재개'],['시퀀스 완료 모드','트랙이 변경한 Transform·애니메이션 상태의 유지 또는 복원'],['요청자·Director','문 강제 제어, 마커의 게임 상태, 부착 큐·레벨 전용 효과와 최종 위치 보정'],['명시적 Cancel 호출자','남은 마커·종료 콜백 없이 끝나므로 자신의 후속 처리와 정리 직접 수행']])
bullet('홀드는 키를 떼거나 일시정지하면 초기화한다. 상호작용 키의 짧은 입력이 우발적으로 컷을 건너뛰지 않게 했다.')
heading('문제 해결',3)
table([['현상','원인','해결','결과'],['스킵 뒤 문·계단 상태 누락','중간 상태 변경이 재생 시간에 의존','남은 마커를 전달하고 최종 상태 적용','시청 여부와 진행 상태 분리'],['컷신 입력 해제 시 다른 컷신까지 풀림','개별 EnableInput 호출','외부 연출도 컷신 횟수에 참여','마지막 컷신 종료 시 복원'],['자연 종료가 중단으로 기록됨','OnStop과 OnFinished가 함께 전달','대기 중 Interrupted를 Finished가 덮어쓰게 처리','자연 종료 이유 보존']],[3,3,5.5,4.4])

section('7.4 Stage3 보스 시네마틱과 엔딩','공통 Director는 문·이벤트·큐를, Stage3 파생은 대문·계단·눈 발광·엔딩을 처리한다.',[])
oldtable(505)
for s in ['LI 로컬 좌표로 작성한 시퀀스에 TransformOrigin을 적용하고 동적 보스·플레이어를 태그로 바인딩한다.','페이즈 컷 종료 후 보스 위치와 문 상태를 보정하고 필요하면 플레이어의 다음 아레나 진입을 기다린다.','처치 후 다음 Tick에 엔딩을 시작한다. 실제 보스를 숨기고 시퀀스 대역을 사용해 사망 래그돌과 연기를 분리한다.','HoldsLevelAfterDefeat가 레벨 이동을 보류한다. 엔딩 후 다음 레벨로 이동하고, 다음 레벨이 없으면 페이드와 펫 AI를 복구한다.']:bullet(s)

heading('공통 처리와 Stage3 전용 처리',3)
for x in ['AStageEncounterDirector는 Doors·EventReceivers·CinematicCues와 PlayStageCut을 제공한다. AStage3BossDirector는 대문 자물쇠·계단·눈 발광·엔딩을 확장한다.','Blood:Start/End는 UCinematicTrailDecalComponent가 샘플링 구간으로 처리한다. 스킵 종료는 RevealAll로 최종 경로를 표시한다.','Collapse:Player는 조작·몽타주를 정리하고 쓰러지는 포즈는 시퀀스 애니메이션이 담당한다. 물리 래그돌 결과에 연출 구도를 맡기지 않는다.','문 번호는 Doors 배열 인덱스다. 기존 마커가 가리키는 자리를 삭제해 뒤 인덱스를 당기지 않도록 연결을 관리한다.']:bullet(x)
heading('엔딩 종료와 실패 복구',3)
for x in ['처치 처리 콜스택과 섞이지 않도록 다음 Tick에 엔딩을 시작한다. 실제 보스의 래그돌과 대역의 시퀀스 연기를 분리한다.','엔딩 준비 때 펫 AI를 정지하고 BGM·대역·바인딩을 연결한다. 종료 뒤 EndingBlackHoldSeconds를 거쳐 다음 레벨로 이동한다.','매니저는 HoldsLevelAfterDefeat로 이동을 보류한다. 다음 레벨이 없으면 검은 화면과 펫 AI를 복구해 현재 맵에서 멈추지 않게 한다.']:bullet(x)
decision('공통 재생·마커 처리와 스테이지 연출을 나누어 새 보스방은 Director 파생과 데이터 연결로 구성한다. 새로운 게임 의미의 마커는 확장 지점에서 구현한다.')

heading('8. 카메라 가림 처리',1)
section('8.1 CameraOcclusionComponent','방 안과 방 밖의 가림 판정을 구분한다.',[])
fig(17,'방 단위 컷어웨이의 평면과 단면')
for s in ['방 안: 방 AABB에서 옆면·천장·윗층·외부 조각 후보를 캐시하고 카메라 방향으로 숨길 면을 선택한다.','방 밖: 카메라와 플레이어·펫 사이 Sphere Sweep으로 가리는 조각을 찾는다.','숨김: 메인·깊이 패스를 함께 끄고 원래 상태·마지막 가림 시각을 기록한다. 메시 충돌은 유지한다.','복구: 기본 0.05초 갱신·0.25초 지연으로 경계 깜빡임을 줄인다. 컷신·방 이탈·EndPlay에서는 상태를 복원한다.','조명: 현재 방에 도달하지 않는 후보 조명의 그림자를 끄고 원래 상태로 복구한다. 성능은 12장의 별도 측정으로 평가한다.']:bullet(s)
decision('방 안 Sweep은 가까운 가구·계단까지 숨길 수 있어 방 단위 분류를 사용한다. 발밑 지지면은 제외하고 캐릭터 높이로 천장·바닥을 구분한다.')
heading('방 안과 방 밖의 판정 경로',3)
fig(15,'실내 컷어웨이와 실외 Sweep의 선택')
table([['후보','판정 기준'],['천장·윗층','방 AABB·높이 조건과 캐릭터 위치를 함께 사용. 같은 판도 위층에서는 바닥이 될 수 있음'],['카메라 쪽 옆면','옆면 법선과 카메라 방향의 내적을 FaceDotThreshold와 비교'],['벽 부착 조각','얇은 돌출과 벽에 박힌 조각은 해당 옆면에 포함'],['가구·계단 지지면','깊은 가구는 남기고 발밑 지지 LineTrace로 밟고 있는 면 제외']])
heading('상태 보존과 설정',3)
for x in ['GetOrBuildRoomCache는 처음 필요할 때 방의 후보를 분류한다. 매 판정마다 전체 레벨 메시를 다시 분류하지 않는다.','숨길 때 원래 RenderInMainPass·RenderInDepthPass와 마지막 가림 시간을 저장한다. 복구는 원래 값으로 되돌려 처음부터 숨겨져 있던 상태를 덮어쓰지 않는다.','그림자 최적화는 현재 방에 도달하지 않는 후보 조명만 대상으로 한다. 이미 CastShadows가 꺼진 조명은 복구 목록에 넣지 않는다.','실외 Sweep은 CameraOcclusion Trace 채널에 반응하는 메시가 필요하다. 디버그 표시로 Sweep 경로와 방 박스를 확인한다.']:bullet(x)
heading('문제 해결',3)
oldtable(535)

section('8.2 방 선택과 가림 그룹','RoomMaskVolume은 현재 방, OcclusionGroupSubsystem은 방 밖의 같은 평면 조각을 관리한다.',[
'겹친 방은 박스 안으로 가장 깊이 들어간 방을 선택한다. 단순한 마지막 진입 순서에 의존하지 않는다.',
'그룹은 처음 맞은 조각의 반경과 평면·인접 허용 오차로 제한한다. 기둥·모서리 같은 덩어리는 묶지 않는다.',
'설정: 방 볼륨, CutawayIgnoreActors, bCutaway, 플레이어의 RoomCutaway·GroupSettings.',
'한계: 월드 AABB 기반이라 회전 방의 정밀 표현이 어렵다. 캐시 생성 후 추가·이동한 메시를 자동 재분류하지 않는다.'])
heading('볼륨과 그룹의 적용 범위',3)
for x in ['RoomMaskVolume은 방 박스들의 월드 AABB를 제공한다. 겹침 이벤트의 마지막 순서 대신 현재 플레이어 위치로 방을 재선택한다.','방이 겹치면 박스 면에서 가장 깊이 들어간 방을 고른다. bCutaway로 실내 컷어웨이 사용 여부, CutawayIgnoreActors로 예외를 지정한다.','OcclusionGroupSubsystem은 방 밖 Sweep에서만 같은 평면 조각을 묶는다. 처음 맞은 조각 기준 반경·평면·인접 허용 오차로 옆방까지 연결되는 것을 제한한다.','기존 방 마스크 후처리와 컷어웨이는 별도 기능이다. bRoomMaskEnabled는 방 마스크 사용 여부를 프로퍼티로 표현한다.']:bullet(x)

section('8.3 컬링 방식의 변천','메시·태그를 하나씩 지정하던 방식에서 방 박스와 캐시 중심으로 변경했다.',[])
table([['방식','변경 이유'],['Sweep와 같은 면 묶기','실외에 적합하지만 실내에서 가구·계단과 여러 방의 외벽을 함께 숨길 수 있음'],['예외 태그 추가','방이 늘수록 계단·천장·그룹 제외 설정이 증가'],['방 단위 컷어웨이','실내는 방 경계로 분류하고 실외는 기존 Sweep을 유지. 튜토리얼 전용 외벽 숨김과는 별도 경로']])

heading('9. 게임 진행과 레벨 구조',1)
section('9.2.1 진행 저장과 복원 흐름','저장 데이터는 게임 인스턴스, 조명 적용은 월드 수명으로 구분한다.',[
'체크포인트 기록: 레벨·복귀 위치와 현재 시간대 태그를 저장한다.',
'사망·재입장: 저장을 불러와 맵을 다시 열면 배치 액터가 BeginPlay를 다시 거친다.',
'플래그 게이트와 시간대 컨트롤러는 각자 저장값을 읽는다. 두 BeginPlay의 고정 순서에 의존하지 않는다.'])
fig(18,'체크포인트와 플래그 및 시간대 복원','맵 재생성 후 게이트와 시간대 컨트롤러가 각각 현재 저장값을 읽는다. 두 작업의 BeginPlay가 고정 순서로 실행될 필요는 없다.')

section('9.2.3 진행 저장과 진행 플래그','ProgressFlagGate는 소유 액터의 파괴·비활성·등장을 플래그 조건으로 적용한다.',[
'BeginPlay에서 변경 이벤트를 구독하고 즉시 Evaluate로 현재 값을 반영한다.',
'bApplied로 한 번만 적용한다. RevealOwner는 조건 이전에 숨기고 충돌을 끈다.',
'보스방 매니저는 처치 플래그만 기록한다. 어떤 레벨 액터를 제거할지는 게이트가 결정한다.',
'한계: FName 키의 오타는 컴파일에서 잡지 못하고 적용 뒤 조건을 되돌려도 자동 역복원하지 않는다.'])
heading('플래그 게이트 설정',3)
table([['설정','동작'],['Flag·bActWhenFlagSet','같은 FName 키를 사용하고 설정 또는 해제 상태 중 어느 조건에 반응할지 선택'],['DestroyOwner','조건 충족 시 소유 액터 제거'],['DisableOwner','조건 충족 시 소유 액터 비활성화'],['RevealOwner','조건 이전에 숨김·충돌 해제, 조건 충족 뒤 등장']])
decision('보스방 매니저가 개별 레벨 액터를 제거하지 않고 처치 플래그만 기록한다. 새 소비자는 게이트를 연결해 같은 저장 조건을 재사용한다.')

section('9.2.4 시간대 복원','같은 레벨의 유효한 체크포인트일 때만 저장 태그를 적용한다.',[
'SetCheckpoint·DiscoverCheckpoint가 TimeOfDay 태그를 기록한다.',
'TimeOfDayController 등록과 OnSaveDataReplaced에서 RestoreFromCheckpoint를 호출한다.',
'빈 태그·다른 레벨·유효하지 않은 체크포인트는 DefaultPreset을 사용한다.',
'월드 종료 시 게임 인스턴스의 저장 교체 이벤트 구독을 해제한다.'])
section('9.3 레벨 구성과 환경','스테이지 원본 편집과 메인 맵 조립을 Level Instance로 분리한다.',[
'메인 맵은 마을·스테이지 입구·공용 조명과 LI 배치를 조립한다. 같은 맵 파일의 동시 편집을 줄이되 공용 에셋 충돌까지 해결하지는 않는다.',
'NavMesh는 실제 배치 위치와 빌드 상태를 확인한다. LI 이동·복사 후 펫·보스의 경로를 다시 검증한다.',
'마을은 스테이지 연결 허브, Stage3 저택은 대문·마당·전투 구역·상층 개방을 진행 단계와 연결한다.'])
fig(19,'시간대 요청과 공용 조명 적용')
bullet('Stair:Break 같은 레벨 사건이 태그를 요청하면 컨트롤러가 프리셋의 해·하늘빛 값을 적용한다. Sky가 연결되고 인터페이스를 구현한 경우 하늘 값도 전달한다.')
decision('LI는 조명 액터를 직접 찾지 않고 태그로 요청한다. 현재 전환은 즉시 적용이며 시간 보간은 제공하지 않는다.')

heading('레벨 조립과 경로 검증',3)
for x in ['메인 맵에서 LI 원본과 배치 변환을 관리한다. 복사본을 복귀 지점으로 쓸 때는 시퀀스·배치 액터·데이터 참조도 함께 확인한다.','LI 이동·복사 뒤에는 실제 배치에서 경로를 다시 빌드한다. NavMesh 액터의 위치와 빌드 데이터가 맞는지 확인하며 모든 스테이지에 단일 배치 규칙을 강요하지 않는다.','Stage3는 도착 → 대문 해제 → 마당 → 등장 → 전투 구역 이동 → 상층 개방을 공간 진행과 연결한다. 컷 종료 위치와 다음 구역 진입 조건을 함께 검토한다.']:bullet(x)
heading('시간대 확장과 교체 이유',3)
for x in ['새 시간대는 태그와 Presets 항목을 추가한다. 새로운 하늘 표현은 ITimeOfDayReceiver 구현을 추가한다.','해 회전·밝기·색, 하늘 인터페이스 값과 하늘빛 재캡처는 컨트롤러에 모은다. LI는 공용 조명 액터 이름이나 BP 변수 표시 이름을 알 필요가 없다.','기존 bool 시간대와 이름 기반 조명 조회를 태그·프리셋·명시적 참조로 바꾸었다. 저장에는 값 묶음 대신 선택된 프리셋 태그를 기록한다.']:bullet(x)
heading('문제 해결',3)
table([['현상','원인','해결','결과'],['LI 배치 뒤 펫·보스 이동 실패','NavMesh 액터 위치·배치·경로 빌드 불일치','배치 위치 점검과 경로 재빌드','맵 조립 뒤 경로 검증 절차 확보'],['하늘 BP 변경 후 조명 연결 누락','표시 이름·변수 이름 조회','컨트롤러 참조·인터페이스·프리셋으로 교체','코드의 에셋 이름 의존 제거']],[3,3,5.5,4.4])

heading('11. 사운드와 이펙트',1)
section('11.2 볼륨·음소거 설정','BGM·SFX의 사용자 값과 음소거를 별도 저장한다.',[
'UI 변경 → SetVolume/SetMuted → SoundMix의 SoundClassOverride → SaveConfig 순서로 적용한다.',
'설정은 UAudioVolumeSubsystem의 Config=GameUserSettings 프로퍼티로 저장한다. 새 월드에서는 ApplyAll로 다시 적용한다.',
'현재 설정은 BGM에 SC_BGM, SFX에 Master를 사용한다. 두 채널의 독립성은 BGM 클래스의 부모 계층과 각 음원의 클래스 연결에 달려 있다.'])
heading('설정 데이터와 사용자 값',3)
table([['구성','책임'],['UAudioVolumeSetting','프로젝트 기본값의 SoundMix와 BGM·SFX SoundClass 참조 지정'],['UAudioVolumeSubsystem','사용자 볼륨·음소거 값 저장. SoundClassOverride와 월드 재적용'],['USoundSettingsWidget','슬라이더·음소거 버튼을 서브시스템에 연결. 일시정지 메뉴에 포함']])
bullet('음소거는 볼륨 값을 0으로 덮어쓰지 않고 적용값만 0으로 만든다. 음소거를 해제하면 사용자가 정한 볼륨을 다시 사용한다.')
heading('문제 해결',3)
oldtable(653)

section('11.3 Stage3 사운드·이펙트 적용','효과의 재생 시점과 정리 책임에 따라 연결 위치를 구분한다.',[])
oldtable(668)
bullet('큐 슬롯의 EffectScale·에셋 참조와 공용 메시·머티리얼은 설정으로 바꾼다. 절차적 스타일 내부의 색·조각 구성은 코드가 정의한다.')

heading('연출의 종료 책임',3)
for x in ['한 번 재생하는 소리·화면 연출은 시퀀스 트랙에 두어 타이밍을 편집한다. 액터에 붙거나 스킵·리셋 때 중단해야 하는 연출은 Cue 마커로 요청한다.','이름 기반 Notify 가로채기는 전용 Enemy Feedback Cue Notify로 교체했다. 패턴·적의 큐는 해당 설정과 BP에 직접 참조한다.','반복·부착 큐는 핸들을 보관한다. 초기화·처치·비정상 종료에서 StopCinematicCues로 중단해 다음 실행에 남지 않게 한다.']:bullet(x)

heading('12. 성능 최적화',1)
section('12.1 측정 방법과 도구','고정 경로를 자동 진행하고 준비 회차를 제외해 변경 전후를 비교한다.',[])
oldtable(680)
bullet('GT−UI는 대기 영향을 구분하기 위한 보조 지표이며 순수 코드 시간이 아니다. 프레임·렌더·GPU·트레이스와 함께 해석한다.')
section('12.2 최적화 항목별 전후 비교','리팩토링 후 기준과 1차 최적화의 각 3회 평균을 비교한다.',[])
oldtable(690)
fig(21,'구간별 프레임 평균 비교')
for s in ['변경: 화면 밖 적은 몽타주만 갱신하고 보스는 포즈·본을 항상 갱신한다. 최근 렌더되지 않은 체력바의 회전은 생략한다.','정확성 보완: 근접 PrepareAttack에서 판정 메시·부착 부모의 본 갱신 정책을 올리고 StopAttack에서 복원한다.','결과: 프레임 평균은 구간별 약 6~14% 감소했다. 기준 회차 범위는 0.45~1.27ms, 적용 후 0.06~0.26ms다. 반복 3회만으로 통계적 유의성을 확정하지 않는다.','이전 별도 측정: 체력바 최적화는 TickActors 2.40→2.08ms였지만 프레임 평균은 거의 같았다. 방 그림자 최적화의 보스 2페이즈 렌더 스레드는 14.64→13.12ms였다. 다른 조건의 효과를 합산하지 않는다.','한계: 에디터 -game 결과다. 여러 변경을 포함하므로 특정 함수 한 줄의 개선으로 귀속하지 않는다. 패키지 빌드와 화면 밖 판정 보완의 단독 비용은 별도 측정이 필요하다.']:bullet(s)

heading('변경 항목과 해석',3)
oldtable(692)
for x in ['AI와 이동 Tick 수 자체는 줄이지 않았다. 활동 단계별 AI 제어와 선택 평가·폴링 비용의 축소는 별도 과제다.','화면 밖 본 갱신 생략의 정확성은 근접 판정과 함께 다룬다. 판정 준비 동안 본 갱신을 보장하고 종료 뒤 원래 정책으로 복원한다.','고정 경로·옵션·준비 회차·반복 수를 유지한다. 트레이스 기록 비용이 있으므로 서로 다른 수집 방식의 절대값을 곧바로 비교하지 않는다.']:bullet(x)

heading('13. 공통 트러블슈팅',1)
table([['현상','원인','해결','결과'],['형식 변경 후 옛 에셋 값 누락','저장된 구조체와 새 필드 형식 불일치','호환 필드·PostLoad 이전 → 재저장 → 재조사','코드 수정과 콘텐츠 이전을 함께 검증'],['복사 맵에서 참조·경로 어긋남','LI·시퀀스·NavMesh가 서로 다른 배치를 참조','원본과 복사본의 연결·로컬 좌표·경로 빌드를 비교','복귀 지점은 파일 복사뿐 아니라 참조 묶음으로 관리']],[2.8,3.2,5.8,4.1])
heading('14. 회고와 향후 과제',1)
for s in ['성과: 요청·진입 ID와 소유권에 따른 정리로 취소·사망·전환·스킵이 겹치는 경로를 공통화했다.','남은 구조: 패턴 실행 상태의 진단·에셋 검증 강화, 보스 UI 이벤트화, 입력 표시의 매핑 연동.','검증 과제: 에셋 연결 검사와 반복 플레이 자동화, 패키지 성능 측정. 시스템 공통화는 피해·입력·연출의 계약부터 합의한다.']:bullet(s)
heading('부록 D 디버그·테스트 도구',1)
oldtable(713)

# 모든 사람 표기는 앞의 기여 표에만 남긴다. 본문에는 작성 지침·촬영 메모를 넣지 않는다.
for p in d.paragraphs:
 if p.style.name.startswith('Heading'):
  p.text=re.sub(r'\s*\[[^]]+\]','',p.text)
 for r in p.runs:
  r.font.name='맑은 고딕';r._element.get_or_add_rPr().rFonts.set(qn('w:eastAsia'),'맑은 고딕');r.font.color.rgb=RGBColor(0,0,0)
# 짧은 비교 표와 부록 표는 페이지 사이에서 한두 행만 남지 않게 묶는다.
for t in d.tables:
 if len(t.rows)<=6:
  for i,row in enumerate(t.rows):
   for c in row.cells:
    for p in c.paragraphs:p.paragraph_format.keep_with_next=i<len(t.rows)-1
for s in d.sections:
 f=s.footer.paragraphs[0];f.clear();f.alignment=WD_ALIGN_PARAGRAPH.CENTER
 fld=OxmlElement('w:fldSimple');fld.set(qn('w:instr'),'PAGE');f._p.append(fld)
d.core_properties.title='통합 기술 문서 전선현 담당 시스템';d.core_properties.author='전선현'
target=OUT/'기술문서_전선현_담당절_수정본_v8.docx';d.save(target)
chars=sum(len(p.text) for p in d.paragraphs)+sum(len(c.text) for t in d.tables for row in t.rows for c in row.cells)
print('characters',chars,'figures',figcount,'tables',len(d.tables),'output',target)
(OUT/'work_v8'/'artifact.md').write_text(f'''# 편집 기준\n참고 문서: {ref}\nSHA256: {hashlib.sha256(ref.read_bytes()).hexdigest()}\n참고 본문 12pt, 하위 제목 14pt, 장 제목 18pt, A4. 원본 참고 문서 보존.\n사용자 요청에 따라 v6의 본문을 전면 재편한다. 짧은 목적문·순서 목록·기술 규칙·관계/시퀀스 그림을 사용한다.\n본문 12pt, 표 10.5pt, A4 좌우 2.54cm, 상단 2.5cm. 참고 문서의 큰 빈 공간은 재현하지 않는다.\n0.4와 담당 절 번호 유지. 사람 이름은 앞의 기여 표로 집중한다. 반복 기록 표·날짜 이력·빈 촬영 상자·중복 흐름을 제거한다.\n그림은 흰 배경, 회색 선·상자, 짧은 주체와 행동 라벨. 시퀀스 그림은 점선 생명선과 번호 있는 호출로 표현한다.\n현재 문서 {chars}자. 원본·참고 문서는 변경하지 않는다. 최종 Word 렌더링으로 모든 페이지를 검수한다.\n''',encoding='utf-8')
