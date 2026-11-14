"""Bootstrap missing-assets task data; the delivered package also contains reviewed
manual instructions and helper tools. This is not a full delivery reproducer.
Never changes gameplay or previous deliveries; refuses an existing output.
"""
from pathlib import Path
import json, shutil, hashlib, zipfile

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Deliverables/Superstar-MissingAssets-20261115'
OLD = ROOT / 'Deliverables/H3AnimationHandoff-20261115'

def put(path, text):
    p = OUT / path
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(text.rstrip()+'\n', encoding='utf-8')

def save(path, data):
    put(path, json.dumps(data, ensure_ascii=False, indent=2))

def copy(src, dest):
    p = OUT / dest
    p.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, p)

def main():
    if OUT.exists():
        raise SystemExit(f'Preserve existing handoff: {OUT} already exists.')
    OUT.mkdir(parents=True)
    for folder in ['tools','workflows','config','prompts/first_frame','prompts/video','prompts/static','tasks','inputs','docs','sources','integration']:
        (OUT/folder).mkdir(parents=True, exist_ok=True)
    for name in ['comfy_jobs.py','export_animation.py','extract_green_screen.py']:
        copy(OLD/'tools'/name, 'tools/'+name)
    for name in ['h3_i2v_640_api.json','h3_i2v_original_api.json']:
        copy(OLD/'workflows'/name, 'workflows/'+name)
    for name in ['02_环境与工作流.md','04_截帧循环与验收.md']:
        copy(OLD/'docs'/name, 'docs/legacy_'+name)
    copy(OLD/'config/models.json', 'config/models.json')
    copy(OLD/'requirements.txt','requirements.txt')
    copy(OLD/'sources/绿幕动画帧导出工作流.md','sources/绿幕动画帧导出工作流.md')
    copy(OLD/'sources/H3-sprite-workflow.zip','sources/H3-sprite-workflow.zip')
    copy(Path('D:/我的文档/xwechat_files/wxid_e4f3z6tmjjhs21_4f80/msg/file/2026-11/剧本全.docx'),'sources/剧本全.docx')
    copy(Path('C:/Users/Administrator/Desktop/H3Animation-20261115-delivery.zip'),'baseline/original_delivery.zip')
    copy(ROOT/'Deliverables/H3AnimationAcceptance-20261115.zip','baseline/acceptance.zip')
    copy(ROOT/'Docs/SuperstarStory/README.md','integration/当前项目说明.md')
    copy(ROOT/'Content/SuperstarStory/story.json','integration/current_story.json')
    copy(ROOT/'Content/SuperstarStory/clips.json','integration/current_clips.json')
    for p in (ROOT/'Content/SuperstarStory/References').glob('*.png'):
        copy(p,'references/scenes/'+p.name)
    for p in (OLD/'references').iterdir():
        if p.is_file(): copy(p,'references/'+p.name)
    for p in (ROOT/'Content/SuperstarStory/Animations').iterdir():
        frames=sorted(p.glob('*.png'))
        if frames:
            copy(frames[0],f'references/baseline/{p.name}/first.png')
            copy(frames[-1],f'references/baseline/{p.name}/last.png')
    chars = json.loads((OLD/'config/characters.json').read_text(encoding='utf-8-sig'))
    for c in chars:
        c['design_status']='user_confirmed'; c['identity_source']='references/'+Path(c['reference']).name
    newchars = [
        ('limou','李谋 / 片场导演','An adult Chinese man in his late forties, short black hair graying at the temples, broad rectangular face, narrow dark eyes, charcoal open-collar shirt with rolled sleeves, gray trousers, black leather shoes, a headset resting around his neck. No hat.','中年男导演、短黑灰发、炭灰衬衫、灰裤、挂颈耳机'),
        ('chenmo','陈默','An adult Chinese man in his early thirties, neat short black hair, slim rectangular black glasses, calm narrow eyes, slate-blue casual jacket over a white shirt, black trousers and dark brown shoes. No headset.','短黑发、黑框眼镜、蓝灰外套、白衬衫、黑长裤'),
        ('xiaozhou','助理小周','An adult Chinese male assistant in his mid twenties, short slightly tousled black hair, round attentive eyes, muted teal hoodie, beige straight trousers, white sneakers, small work badge. No clipboard when transferring an umbrella.','青年男助理、短乱黑发、蓝绿色连帽衫、米色长裤；性别及外形为制作默认'),
        ('actress','第一幕女主演员','An adult Chinese woman in her late twenties, straight shoulder-length black hair, expressive dark eyes, cream plain knit cardigan, dark plum straight trousers, flat black shoes. Distinct from Lin Wan, Xiao Bei and Su Yao.','黑色齐肩直发、米白针织衫、深梅色长裤、黑平底鞋'),
        ('xiaoxia','小夏','An adult Chinese woman in her late twenties with a short straight black bob, taupe long coat over a gray outfit and black ankle boots, calm watchful eyes. Do not resemble Xiao Bei in a blue skirt.','黑短发、灰褐长外套、黑短靴'),
        ('man','路灯下的男人','An adult Chinese man in his mid thirties, short dark hair, light stubble, olive-brown jacket, pale gray shirt, dark jeans and brown casual shoes. No glasses; visually distinct from Chen Mo and Li Mou.','短黑发、浅胡茬、橄榄棕外套、深色牛仔裤；不是陈默或李谋'),
    ]
    for id,name,identity,zh in newchars:
        chars.append(dict(id=id,name=name,identity=identity,zh=zh,facing='screen right',reference=f'output/masters/REF_{id}.png',identity_source=f'output/masters/REF_{id}.png',design_status='production_default_not_user_approved'))
    charmap={c['id']:c for c in chars}
    save('config/characters.json',chars)
    story=json.loads((ROOT/'Content/SuperstarStory/story.json').read_text(encoding='utf-8'))
    jobs=[]
    def add(id,role,name,beats,action,expression,start='Standing in a grounded action-ready pose, three-quarter view facing screen right.',loop=False,events=None,dependencies=None,kind='action',priority='P1'):
        if role=='suyao': start=start.replace('facing screen right','facing screen left')
        job=dict(id=id,character=role,name=name,beats=beats,source='剧本全.docx + 用户已确认导演=李谋；新增角色设计属制作默认',required=True,kind=kind,priority=priority,
            loop=loop,start_pose=start,action=action,expression=expression,events=events or (['cycle_start','opposite_phase','cycle_return'] if loop else ['action_onset','main_event','settled_end']),
            dependency=None,dependencies=dependencies or [],duration_input=5,source_fps=24,export_fps=5,canvas=[640,640],anchor=[320,570],seed=202611150000+len(jobs)*100,status='planned',
            first_frame=f'inputs/{id}.png',prompt=f'prompts/video/{id}.txt',first_frame_prompt=f'prompts/first_frame/{id}.txt')
        jobs.append(job)
        return job
    # Each row is one single-subject clip. Multi-actor interaction is verified as a synchronized composite.
    rows=[
    ('A01_linwan_walk','linwan','稳定行走',[1,3,15,20,21,39],True,'Walk in place with a steady heel-to-toe gait and natural arm swing, maintaining a fixed root position.','calm controlled confidence'),
    ('A02_linwan_hit_table','linwan','愤怒敲桌',[2],False,'Raise the right hand briefly and strike down once to a fixed invisible tabletop contact point at waist height; settle without repeating. No furniture is baked into the sprite.','brows drawn together, narrowed eyes and tightly pressed lips; anger is visible before contact'),
    ('A03_actress_retreat_table','actress','后退撞桌撑住',[2],False,'Take two short backward steps, stop abruptly as the lower back reaches an invisible desk edge, arch back and plant both palms behind at the same tabletop height. Hold the braced state.','eyes widen, eyebrows rise, lips part with fear'),
    ('A04_linwan_advance','linwan','逼近一步',[3],False,'Advance one small step toward screen right, stop firmly and lean slightly forward.','angry assertive stare without a smile'),
    ('A05_limou_applaud','limou','导演鼓掌',[3],True,'Applaud rhythmically at chest height in place, hands clearly separating between claps.','satisfied approving smile'),
    ('A06_actress_leave_bowed','actress','低头离开',[3],True,'Walk in place facing screen left with the chin lowered, shoulders drawn in and arms close to the body.','subdued frightened eyes looking downward'),
    ('A07_linwan_lookback_smirk','linwan','回头冷笑',[4,15],False,'From a neutral three-quarter stance glance back over the shoulder, then lift just one corner of the mouth and settle.','asymmetrical cold smirk and slightly narrowed eyes, never a friendly broad grin'),
    ('A08_xiaozhou_jog','xiaozhou','助理小跑',[4],True,'Jog in place holding a closed cream umbrella in the right hand; keep the umbrella rigid and intact.','attentive urgency'),
    ('A09_linwan_take_umbrella','linwan','抢过雨伞',[4,39],False,'Reach toward an offscreen partner, grip the curved handle of the closed cream umbrella at the agreed transfer point and pull it back to the torso once.','impatient controlled glance; no extra hand or duplicate umbrella'),
    ('A10_xiaozhou_release_umbrella','xiaozhou','松手后愣住',[4],False,'Extend the held closed umbrella toward an offscreen partner, release at the shared transfer event, retract the empty hand and remain stunned.','raised brows, slightly open mouth, frozen surprise'),
    ('A11_linwan_open_umbrella','linwan','撑开雨伞',[5],False,'Starting with the closed cream umbrella in hand, raise it beside the shoulder and open it once above the head. Keep the entire canopy inside the frame.','neutral detached expression'),
    ('A12_linwan_walk_umbrella','linwan','撑伞行走',[5,39],True,'Walk in place facing screen right with the already open cream umbrella above the head. Maintain a stable grip and a complete uncut canopy.','calm closed-mouth expression'),
    ('A13_xiaoxia_idle','xiaoxia','小夏路灯待机',[6],True,'Stand in place with subtle breathing, looking toward a man to screen right.','quiet serious attention'),
    ('A14_man_idle','man','男人路灯待机',[6],True,'Stand in place facing screen left, subtle breathing and attentive posture.','quiet restrained expression'),
    ('A15_linwan_phone_photo','linwan','拿手机偷拍',[6],False,'Raise a charcoal smartphone vertically from chest height to a photographing position, aim to screen left, tap the shutter once and lower slightly.','focused guarded eyes, no smile'),
    ('A16_xiaobei_rise_empty','xiaobei','扶膝空手起身',[11],False,'From the exact fallen endpoint of C03 with the three papers remaining on the ground, push up using both knees and stand without collecting any paper.','distressed lowered gaze; never cheerful'),
    ('A17_xiaobei_crouch_empty','xiaobei','再蹲下准备收拾',[11,12],False,'From A16 standing endpoint, bend the knees and crouch toward the same three scattered sheets; stop at the ready-to-collect pose matching C04.','urgent lowered gaze'),
    ('A18_suyao_walk','suyao','轻快走近',[16,27],True,'Walk in place facing screen left with light restrained steps; preserve both braids and the short coral dress.','sweet social smile'),
    ('A19_linwan_look_down','linwan','低头冷眼',[18],False,'Lower the eyes and tilt the chin slightly toward a fallen person offscreen right, leaving both arms at rest; do not offer a hand.','cold indifference'),
    ('A20_suyao_adjust_dress','suyao','整理裙摆重新甜笑',[19],False,'Begin standing at the C09 endpoint; smooth the short dress hem with both hands, straighten the torso and bring back a social smile.','visible progression from shame and resentment to a controlled sweet smile'),
    ('A21_linwan_raise_head','linwan','抬头难辨的笑',[20],False,'Begin with lowered chin, remain quiet briefly, lift the head slowly and form a small ambiguous smile.','closed mouth, unreadable eyes, restrained smile without exuberance'),
    ('A22_linwan_listen','linwan','停步贴门偷听',[21],False,'Make a small step toward an invisible door at screen right, lean the ear toward the seam and hold still.','alert narrowed eyes, tense mouth'),
    ('A23_limou_comfort','limou','低沉安抚',[22],False,'Make a small calming downward palm gesture toward an offscreen woman, leaning slightly toward her. Mouth may move softly without recognizable words.','controlled reassuring surface with cold narrowed eyes'),
    ('A24_suyao_angry_talk','suyao','愤怒提及小北',[22],False,'Lean slightly toward an offscreen man and speak urgently with a short frustrated hand motion, ending in a tense hold. No invented audible words.','furrowed brows, tight mouth and flashes of anger'),
    ('A25_limou_cold_talk','limou','再次低声开口',[22],False,'Turn the head slightly toward the offscreen woman and speak in a restrained low manner without readable words, keeping the body still.','cold calculating eyes and compressed brow'),
    ('A26_linwan_wall_fist','linwan','背靠墙攥拳',[23],False,'Recoil a little, press the back toward an invisible wall, lower both hands and slowly clench the fists once.','shock tightening into contained anger'),
    ('A27_linwan_mirror_idle','linwan','镜前坐姿',[24,25,27],True,'Sit on an invisible fixed-height stool facing three-quarter away toward screen right; breathe subtly with feet at the established floor line. No stool or mirror baked into the sprite.','quiet guarded face, eyes directed toward an offscreen mirror'),
    ('A28_chenmo_walk','chenmo','陈默走路',[24,26,32,34],True,'Walk in place with a slow measured gait, hands relaxed, fixed root.','calm supportive expression'),
    ('A29_chenmo_knock','chenmo','轻敲两下门',[24],False,'Raise the right hand and knock exactly twice on an invisible door at shoulder height, then lower the hand.','polite calm expression'),
    ('A30_linwan_mirror_glance','linwan','从镜中看陈默',[24],False,'Keep the A27 seated torso facing away; shift only the eyes and slightly lift the chin to notice someone in the offscreen mirror.','guarded curiosity, no full turn'),
    ('A31_chenmo_encourage','chenmo','赞许示意',[25],False,'Stop, raise one hand into a small clear thumbs-up at chest height, hold, and lower it.','warm restrained encouragement'),
    ('A32_linwan_seated_turn','linwan','回头怀疑动摇',[25],False,'From the same seated pose turn the head and upper torso toward screen left, pause with a questioning look, then soften slightly.','one brow lifts; suspicion gradually eases into uncertainty'),
    ('A33_chenmo_leave_pause','chenmo','后退离开停顿',[26],False,'Take one short backward step, turn toward screen left, initiate a walk, then pause and glance slightly back.','thoughtful reserved hesitation'),
    ('A34_suyao_open_door','suyao','推门入场',[27],False,'Push an invisible door with one palm, take one small step and settle facing screen left.','bright sweet smile'),
    ('A35_suyao_lean_threat','suyao','前倾低声威胁',[27],False,'From a social smile lean the upper body forward slightly toward screen left and speak quietly without invented words.','smile fades, eyes narrow, lips tense with a concealed threat'),
    ('A36_linwan_stand_confront','linwan','站起直视不退',[28],False,'From the seated A27 pose stand upright in one controlled action, face screen right and hold the ground without stepping back.','steady alert eyes and defiant composure'),
    ('A37_staff_f_give_rose','extra_f','工作人员递白玫瑰',[28],False,'Without clipboard or pen for this task, extend one white rose toward an offscreen recipient, release at the transfer event and retract the empty hand. Preserve the uniform and intentionally featureless face.','featureless original face; convey neutral professionalism only through posture'),
    ('A38_staff_m_give_knife','extra_m','工作人员递道具刀',[28],False,'Hold a blunt silver theatrical prop knife by its safe blade housing, offer the handle toward an offscreen recipient and release at the transfer event. Preserve the featureless face.','featureless original face; neutral body posture'),
    ('A39_suyao_receive_rose','suyao','苏瑶接白玫瑰',[28,29],False,'Reach to accept exactly one white rose at the shared contact point and bring it to chest height, keeping it visible.','polite sweet smile with watchful eyes'),
    ('A40_linwan_receive_knife','linwan','林晚接道具刀',[28],False,'Accept the blunt theatrical prop knife by its handle, draw it close at waist height, then turn into an alert ready stance.','composed vigilance'),
    ('A41_linwan_knife_advance','linwan','握刀压低重心',[29],False,'Start holding the prop knife at waist height, tighten the grip, lower the center of mass and move forward half a small step. Never make contact with another person.','alert focused eyes and firm mouth'),
    ('A42_suyao_fake_fall','suyao','捂肩后倒跪地',[30],False,'Before any contact, recoil as if startled, bring one hand to the shoulder and lower safely into a kneeling seated pose. Keep the white rose in the other hand or release it once with a recorded event.','exaggerated fear and sudden alarm, no wound or blood'),
    ('A43_suyao_kneel_accuse','suyao','泪眼指认',[30,31],True,'Remain in the kneeling endpoint of A42, one hand over the shoulder, the other pointing toward screen left; subtle breathing without changing the pointing target.','watery eyes and staged frightened grievance'),
    ('A44_linwan_escorted','linwan','被带离舞台',[31],True,'Walk in place toward screen left with upper arms slightly constrained at two fixed offscreen grip points; hands empty after a separately recorded prop-knife handoff.','shocked and unwilling but composed; no falling or injuries'),
    ('A45_staff_f_escort','extra_f','女工作人员带离',[31],True,'Walk in place toward screen left with one empty hand at a fixed offscreen upper-arm grip point; no clipboard or pen. Match A44 cadence.','featureless face unchanged'),
    ('A46_staff_m_escort','extra_m','男工作人员带离',[31],True,'Walk in place toward screen left with one hand at the opposite upper-arm grip point, the other securing the prop knife away from bodies. Match A44 cadence.','featureless face unchanged'),
    ('A47_chenmo_give_file','chenmo','递交文件',[32],False,'Offer a thin neutral gray document folder with both hands to an offscreen recipient, release at the shared transfer point and retract.','serious supportive expression'),
    ('A48_linwan_receive_file','linwan','接过文件',[32],False,'Reach with both hands to receive the same thin gray folder, then hold it securely against the torso.','quiet concentration and guarded resolve'),
    ('A49_linwan_deep_breath','linwan','直播前深呼吸',[33],False,'Face front three-quarter toward camera, inhale once visibly through a small chest rise, exhale slowly and settle into an upright speaking stance.','determination calming into honest openness'),
    ('A50_linwan_speak','linwan','面对手机坦诚讲述',[33,34,35,36],True,'Look toward a fixed offscreen phone lens with subtle natural mouth movement and breathing; small restrained conversational hand gestures. No invented words or lip-synced sentence.','calm candid resolve; avoid cheerful chatter'),
    ('A51_linwan_send','linwan','按下手机发送',[34,35,36],False,'Hold the charcoal phone in the left hand, extend the right index finger and tap the screen exactly once, then retract and hold.','firm honest resolve'),
    ('A52_xiaobei_brave_walk','xiaobei','勇敢走进直播',[35],True,'Walk in place toward screen left with restrained short steps and empty hands, keeping shoulders upright despite tension.','watery eyes at the edge of tears but determined gaze'),
    ('A53_xiaobei_walk_umbrella','xiaobei','拿伞走近',[38],True,'Walk in place facing screen right carrying the same closed cream umbrella at waist height with its handle offered ahead.','gentle calm willingness'),
    ('A54_xiaobei_release_umbrella','xiaobei','林晚抢伞后松手',[39],False,'Hold the closed cream umbrella at the shared transfer point, release it as the offscreen partner takes it and retract the now empty hand.','brief surprise followed by a quiet neutral look, no invented ending reaction'),
    ('A55_staff_m_take_knife','extra_m','带离前接管道具刀',[31],False,'Secure the prop knife by its handle from an offscreen partner and lower it safely away from bodies; prepare the A46 escort pose.','featureless face unchanged'),
    ('A56_linwan_release_knife','linwan','交出道具刀',[31],False,'Lower the held prop knife, allow an offscreen staff member to grasp the handle, release it once and end with empty hands ready for A44.','shocked restrained protest conveyed without invented speech'),
    ]
    for id,role,name,beats,loop,action,expression in rows:
        add(id,role,name,beats,action,expression,loop=loop)
    expr=[
    ('linwan','angry','愤怒',[2,3],'brows pinched down, narrowed eyes, pressed lips'),
    ('linwan','smirk','冷笑',[4,15],'one corner of the mouth raised, cold narrowed eyes, no friendly grin'),
    ('linwan','ambiguous','难辨的笑',[20],'a small controlled closed-mouth smile with unreadable eyes'),
    ('linwan','suspicious','怀疑与动摇',[24,25],'one eyebrow raised, eyes tracking sideways, slightly uncertain parted lips'),
    ('linwan','vigilant','警觉不退',[21,28,29],'focused steady eyes, firm jaw, no recoil'),
    ('linwan','shock_anger','震惊后克制愤怒',[22,23],'eyes tense and wide with tightly controlled anger in the brow and jaw'),
    ('linwan','resolute','决绝坦诚',[33,34,35],'direct honest gaze, relaxed but firm mouth and upright shoulders'),
    ('linwan','relieved','如释重负',[36,38],'softened eyes, released jaw tension and a slight relieved smile'),
    ('xiaobei','tense','紧张慌乱',[9,11,12,13,14],'raised inner brows, worried eyes and a tight slightly parted mouth'),
    ('xiaobei','escape','逃离不安',[15],'frightened lowered eyes, drawn-in shoulders, pressed lips'),
    ('xiaobei','brave_tears','勇敢欲哭',[35,36],'watery eyes with restrained tears, shaky but determined mouth, chin held up'),
    ('suyao','sweet','表面甜笑',[16,19,27],'polished social smile, soft raised cheeks and attentive eyes'),
    ('suyao','shock','错愕',[17],'suddenly widened eyes, interrupted smile and slightly open mouth'),
    ('suyao','shame','羞愤怨毒',[18,19],'lowered chin, compressed lips, narrowed resentful eyes looking upward'),
    ('suyao','threat','低声威胁',[22,27,28],'smile removed, narrowed threatening eyes, tense low brows'),
    ('suyao','fake_fear','伪装恐惧委屈',[30,31],'exaggerated frightened watery eyes and a visibly staged trembling mouth'),
    ('limou','approval','满意赞许',[3],'relaxed brows and a self-satisfied small smile'),
    ('limou','cold','阴冷安抚',[22],'a reassuring half-smile contradicted by cold narrowed calculating eyes'),
    ('chenmo','encourage','沉静鼓励',[25,32,34],'calm kind eyes, a restrained supportive smile'),
    ('xiaozhou','stunned','愣住',[4],'raised brows, widened eyes, slightly open frozen mouth'),
    ('actress','fear','惊恐示弱',[2,3],'wide frightened eyes, raised inner brows and a small tense mouth'),
    ]
    for i,(role,key,name,beats,expression) in enumerate(expr,1):
        start='Use the exact body, feet, costume, camera and canvas registration of this character\'s approved idle. The target facial expression is already fully present at frame zero.'
        if key=='fake_fear': start='Use the exact kneeling pose at the end of A42. The target expression is already present at frame zero; preserve the kneeling silhouette, do not scale it to standing height.'
        add(f'F{i:02d}_{role}_{key}',role,name+'表情待机',beats,'Maintain the specified facial expression throughout a subtle breathing loop. Natural small eye movement is allowed; keep the expression readable on every 5 fps frame. Never morph between emotions.',expression,start=start,loop=True,kind='expression')
    peel=add('R01_linwan_peel_label','linwan','扯下旧标签（替代撕整张照片）',[8],
        'At frame zero the hand is already near eye level at the raised corner of a small old label on ONE intact contestant photo. Pinch only the peeling label, pull it off in one clear short motion and hold the removed strip. The photo remains whole. No tearing the photo into two pieces.',
        'concentrated disapproval, intent readable but whether helping or sabotaging remains ambiguous',start='Three-quarter view facing screen right, left hand steadies one intact contestant photo near eye height; right fingertips already pinch only the raised label corner. The photo is not torn.',events=['pinch_label','label_removed','photo_intact_end'],priority='P0',kind='story_replacement')
    peel['replaces']='C01_linwan_tear_photo';peel['prop_contract']='P07_photo_intact + P08_old_label; no baked wall'
    oldjobs=json.loads((OLD/'config/jobs.json').read_text(encoding='utf-8-sig'))
    repairs={'C05_xiaobei_rise_papers':('基线偏低','Keep the planted-foot baseline registered to S02; correct the persistent 5–6 pixel endpoint offset without per-frame bbox snapping.'),
        'S04_xiaobei_hold_papers':('待机基线偏低','Match the corrected C05 endpoint and S02 ground baseline, while preserving the breathing loop.'),
        'E04_extra_m_walk':('两侧着地基线','Correct both ground-contact phases to the E03 standing floor line; preserve flight and body bob, never snap each frame by bounding box.'),
        'C06_linwan_push_palm':('手臂黄绿污染','Remove green/yellow edge contamination on fingers and forearm without eroding the hand silhouette. Prefer rematting the existing source.'),
        'C07_linwan_stamp':('悬腿停顿与腿边污染','A single decisive downward stamp must follow the lift promptly. No prolonged raised-leg pose. Preserve the single contact event and clean leg edges.'),
        'C08_suyao_fall_kneel':('摔倒时的表情不符','Preserve the valid losing-balance and kneeling motion, but replace the persistent sweet smile with a visibly interrupted smile, widened eyes and startled shame. If reliable facial editing is unavailable, regenerate this clip with the same action and endpoint. Do not add shoulder clutching from act three.'),
        'C09_suyao_rise':('裙边黄绿污染','Clean contaminated pink dress edges without punching holes or removing fabric. Preserve the existing valid front-foot support action.')}
    for old in oldjobs:
        if old['id'] not in repairs: continue
        j=dict(old); title,fix=repairs[j['id']]
        repair_expressions={'linwan':'controlled guarded coldness, no broad friendly smile','xiaobei':'tense distressed eyes, no cheerful smile','suyao':'shocked shame with tightened lips, no sustained sweet smile during the fall or rise','extra_m':'keep the intentionally featureless face'}
        repair_beats={'C05_xiaobei_rise_papers':[13],'S04_xiaobei_hold_papers':[14],'E04_extra_m_walk':[7],'C06_linwan_push_palm':[10],'C07_linwan_stamp':[17],'C08_suyao_fall_kneel':[17],'C09_suyao_rise':[19]}
        j.update(kind='repair',priority='P0',beats=repair_beats[j['id']],dependencies=[],name=j['name']+' / '+title,expression=repair_expressions[j['character']],repair_instruction=fix,status='planned')
        j['action'] += ' '+fix
        jobs.append(j)
    deps={'A11_linwan_open_umbrella':['A09_linwan_take_umbrella'],'A12_linwan_walk_umbrella':['A11_linwan_open_umbrella'],
        'A16_xiaobei_rise_empty':['baseline:C03_xiaobei_fall_scatter'],'A17_xiaobei_crouch_empty':['A16_xiaobei_rise_empty'],
        'A20_suyao_adjust_dress':['C09_suyao_rise'],'A30_linwan_mirror_glance':['A27_linwan_mirror_idle'],'A32_linwan_seated_turn':['A27_linwan_mirror_idle'],
        'A36_linwan_stand_confront':['A27_linwan_mirror_idle'],'A41_linwan_knife_advance':['A40_linwan_receive_knife'],
        'A42_suyao_fake_fall':['A39_suyao_receive_rose'],'A43_suyao_kneel_accuse':['A42_suyao_fake_fall'],
        'A44_linwan_escorted':['A56_linwan_release_knife'],'A46_staff_m_escort':['A55_staff_m_take_knife'],
        'A50_linwan_speak':['A49_linwan_deep_breath'],'A54_xiaobei_release_umbrella':['A53_xiaobei_walk_umbrella'],
        'C05_xiaobei_rise_papers':['baseline:C04_xiaobei_collect_papers'],'S04_xiaobei_hold_papers':['C05_xiaobei_rise_papers'],
        'C09_suyao_rise':['C08_suyao_fall_kneel'],'C08_suyao_fall_kneel':['baseline:S03_suyao_idle']}
    for j in jobs:
        j['dependencies']=deps.get(j['id'],j.get('dependencies',[]))
        if j['id']=='A16_xiaobei_rise_empty': j['start_pose']='Exactly the accepted C03 fallen endpoint: hands empty and exactly three sheets still on the ground. Maintain the original fixed canvas and body scale.'
        if j['id']=='A17_xiaobei_crouch_empty': j['start_pose']='The accepted A16 standing empty-handed endpoint, with the same three sheets on the ground.'
        if j['id']=='A27_linwan_mirror_idle': j['start_pose']='Already seated on an invisible stool, three-quarter back view toward screen right, feet at the shared floor line, no standing-to-sitting introduction.'
        if j['id']=='A36_linwan_stand_confront': j['start_pose']='The accepted A27 seated pose, ready to rise, with identical seated height and camera.'
        if j['id']=='A43_suyao_kneel_accuse' or j['id']=='F16_suyao_fake_fear':
            j['start_pose']='The exact A42 kneeling seated endpoint; one hand covers the shoulder and the other is ready to point left. Preserve the low body silhouette and the original standing body scale.'
            j['dependencies']=['A42_suyao_fake_fall']
        if j['id']=='F04_linwan_suspicious':
            j['start_pose']='Use the seated mirror pose from A27, three-quarter turned left, with the suspicious expression already present.'
            j['dependencies']=['A27_linwan_mirror_idle']
        if j['id']=='F14_suyao_shame':
            j['start_pose']='Use the baseline S05 kneeling idle pose, with ashamed resentful eyes already present. Keep the low silhouette and scale.'
            j['dependencies']=['baseline:S05_suyao_kneel_idle'];j['beats']=[18]
        if 'facing screen left' in j['action'] or j['id'] in ['A14_man_idle','A44_linwan_escorted','A45_staff_f_escort','A46_staff_m_escort']:
            j['start_pose']=j['start_pose'].replace('facing screen right','facing screen left')
        if charmap[j['character']]['design_status']!='user_confirmed': j['dependencies'].insert(0,'REF_'+j['character'])
        identity=charmap[j['character']]['identity']
        common='Match the supplied 2D character reference exactly: clean hand-drawn cartoon outlines, flat softly shaded colors, adult body proportions. Static full-body camera, 640x640, no camera motion or cut. Uniform pure green #00FF00 background, no ground shadow, no text, border or other person. Keep the entire figure and carried prop inside the canvas. Normal standing floor anchor is (320,570), body height approximately 478 pixels. Keep the same scale throughout crouching and kneeling. Do not crop or recenter individual frames.'
        start=j['start_pose']
        if j['dependencies']: start+=' Where an animation predecessor is listed, use its actual reviewed last compatible source frame rather than inventing a fresh neutral start.'
        put(j['first_frame_prompt'],f'{common}\nIdentity: {identity}\nAction-ready pose: {start}\nFacial acting: {j["expression"]}\nPrepare only the usable first-frame pose; never put a turnaround, contact sheet or neutral reference-card layout into the action input. Freeze character and prop identity before video generation.')
        looptext='Repeat complete cycles with matching direction, speed, foot contact phase, face and prop position at the seam. Do not add a stop or neutral reference pose.' if j['loop'] else 'Play the action once. Begin the action immediately and settle by approximately 4.2 seconds; hold the resulting pose to the end of the five-second clip. Do not repeat or reverse the action.'
        prompt=f'For the target video, at 0.00 seconds into the target video, <Picture 1> (from [Shot 1]) is fully referenced.\n\nintegrated_multimodal_description: [Shot 1] {common} {identity} Opening: {start} From 0.00 to 5.00 seconds: {j["action"]} Facial acting: {j["expression"]}. {looptext} The first output frame already belongs to the intended performance; no introductory transformation, costume change or settling from a character reference card.\n\noverall_soundscape: Quiet incidental cloth and prop movement only; no intelligible dialogue. Animation frame delivery does not include an audio track.\n\nnon_diegetic_music: None.'
        put(j['prompt'],prompt)
        put(f'tasks/{j["id"]}.md',f'# {j["id"]} — {j["name"]}\n\n优先级：{j["priority"]}；类型：{j["kind"]}；人物：{charmap[j["character"]]["name"]}；循环：{j["loop"]}。\n\n复用剧情段落：{j.get("beats",[])}。依赖：{j["dependencies"] or "先锁定角色母版"}。\n\n动作：{j["action"]}\n\n表情：{j["expression"]}\n\n首帧提示词：`{j["first_frame_prompt"]}`；视频提示词：`{j["prompt"]}`。\n\n必须实测的事件：{", ".join(j["events"])}。事件时间不得从提示词抄成测量结果。\n\n交付 `output/animations/{j["id"]}/frames/` + animation.json + 源视频/首帧/提交图/提示词/真实审核记录。若为表情任务，另从已验收序列选一帧保存 expression.png，不额外拼接头像到身体。')
    save('config/jobs.json',jobs)
    save('config/first_frame_reviews.json',{j['id']:dict(approved=False,identity_mapping_checked=False,action_ready=False,notes='接手 Agent 检查真实图像后填写；不是等待人类审批。') for j in jobs})
    save('config/cuts.json',{j['id']:dict(video=f'runs/{j["id"]}/attempt_01/source.mp4',reviewed=False,start_seconds=None,end_seconds_exclusive=None,loop=j['loop'],event_times={},notes='') for j in jobs})
    save('config/final_reviews.json',{j['id']:dict(approved=False,no_intro=False,events_readable_5fps=False,alpha_on_light_dark=False,identity_and_props=False,anchor_and_bounds=False,loop_or_chain_checked=False,facial_acting_checked=False,notes='') for j in jobs})

    static=[]
    def imagejob(id,kind,name,prompt,refs,size,alpha,beats,priority='P1'):
        path=f'prompts/static/{id}.txt'
        put(path,prompt)
        static.append(dict(id=id,kind=kind,name=name,prompt=path,references=refs,canvas=size,alpha=alpha,beats=beats,priority=priority,required=True,output=f'output/static/{id}.png'))
    for id,name,identity,zh in newchars:
        imagejob('REF_'+id,'character_master',name+'统一角色母版',f'Create ONE full-body adult 2D cartoon character on uniform pure green, 640x640. Match the line weight, proportions, soft flat shading and palette of references/linwan.jpg; do not copy her face or outfit. {identity} Grounded neutral three-quarter pose facing screen right. Feet aligned at (320,570), standing body height around 478 pixels. No contact sheet, extra views, lettering, props not specified or scenery. This is a production-default design, not a user-approved likeness. Freeze this exact face and costume before any dependent video.', ['references/linwan.jpg'],[640,640],False,[],priority='P0')
        static[-1]['output']=f'output/masters/REF_{id}.png'
    props=[('P01_umbrella_closed','收拢雨伞','A closed cream umbrella with a black shaft and dark curved handle; no logo.'),('P02_umbrella_open','撑开雨伞','The exact same cream umbrella fully open, black shaft and dark curved handle; full canopy visible.'),('P03_phone','手机','A plain charcoal smartphone, front and slightly angled readable silhouette, neutral blank gray screen, no logos or text.'),('P04_prop_knife','舞台道具刀','A clearly blunt theatrical silver prop knife with a short rounded blade and dark handle, no blood or injury.'),('P05_white_rose','白玫瑰','One white rose with a short stem and two muted green leaves, same proportions in all transfer shots.'),('P06_document_folder','文件','A thin neutral gray folder with three ivory sheets tucked inside, no generated text.'),('P07_photo_intact','完整选手照片','One intact small contestant photo showing the confirmed Xiao Bei, with a small old beige label at one upper edge. The face and blue skirt identity must match references/xiaobei.png.'),('P08_old_label','翘起旧标签','A small thin beige paper label with one lifted corner, compatible with P07; no photo fragment.'),('P09_three_papers','三张资料','Exactly three separate ivory A4 sheets, consistent with baseline C02-C05; no readable text.'),('P10_phone_stand','手机支架','A small simple dark tabletop phone support compatible with P03; no phone included.')]
    for id,name,desc in props:
        imagejob(id,'prop',name,f'Create a clean transparent RGBA 512x512 2D cartoon prop asset. {desc} Match supplied character line art and flat shading. One prop per asset except P09 which explicitly contains three sheets. Keep full bounds and no cast shadow or environment. Export unpremultiplied alpha; no green fringes. Record grip/attachment pixel coordinates in assets.json.', ['references/xiaobei.png'] if id=='P07_photo_intact' else ['references/linwan.jpg'],[512,512],True,[])
    evidence=[('EV01_limou_xiaobei','导演抱小北',[34],'Li Mou / the film director embraces adult Xiao Bei at a film-set location. Both identities are recognizable, the arms and contact are unambiguous. Non-explicit ordinary clothed embrace. Do not substitute Chen Mo, Lin Wan or Su Yao.','studio'),('EV02_limou_suyao','导演和苏瑶喝酒',[35],'Li Mou / the film director and adult Su Yao sit together with visible drinking glasses in the existing backstage room. Both faces and outfits match their frozen masters. Do not use Xiao Bei.','dressing'),('EV03_roster','除苏瑶外都被划掉的名单',[36],'A clear contestant photo board in the existing backstage corridor, matching its established wall colors and frame layout. Exactly four slots in the order 林晚, 小北, 苏瑶, 其他选手. Use the actual approved portraits. Add red X marks over slots 1, 2 and 4 only; slot 3 苏瑶 must remain unmarked. Keep editable layers. Typeset the Chinese labels with a real font after image generation; do not trust model lettering.','dressing')]
    for id,name,beats,desc,scene in evidence:
        refs=['references/scenes/'+scene+'.png','output/masters/REF_limou.png','references/xiaobei.png','references/suyao.jpg']
        imagejob(id,'evidence',name,'Create a fictional in-game evidence illustration, 1280x720 RGB, consistent 2D cartoon style. '+desc+' Reuse the supplied scene composition; do not create a new location. No subtitles, watermark, timestamp or invented allegation text. This is a game story asset, not a real-world news photograph.',refs,[1280,720],False,beats,priority='P0')
    save('config/static_jobs.json',static)
    audio=[('AU01','片场环境','Quiet indoor film-set room tone, subtle distant equipment and footsteps, no recognizable dialogue.',True),('AU02','户外雨夜','Gentle evening rain and distant urban ambience at the same streetlamp location, no music or voices.',True),('AU03','后台环境','Low backstage ventilation, faint movement, no recognizable words.',True),('AU04','掌声欢呼','Short audience applause and cheers, naturally decaying, no words.',False),('AU05','敲桌','One distinct flat hand impact on a wooden desktop.',False),('AU06','敲门两下','Exactly two quiet polite knocks on a door.',False),('AU07','纸张与伞','Separate labeled takes for paper scatter, paper collection, umbrella opening; no additional effects.',False),('AU08','苏瑶尖叫','One brief adult female theatrical alarm cry without words, no injury effects.',False),('AU09','切广告','Adult male offscreen production voice says exactly: 切广告. No additional sentence.',False),('AU10','取消资格','Adult host offscreen announces exactly: 取消林晚资格. No invented surrounding speech.',False)]
    audio=[a for a in audio if a[0]!='AU07']+[('AU07a','资料散落','Exactly three paper sheets fall and scatter, one short isolated take.',False),('AU07b','收拾资料','Gentle rustling of three sheets being collected, one isolated take.',False),('AU07c','雨伞打开','One cream umbrella opening with cloth rustle and a subtle mechanical click.',False)]
    save('config/audio_jobs.json',[dict(id=id,name=name,prompt=prompt,loop=loop,priority='P2',required=True,format='WAV PCM 24-bit 48000 Hz; voice/SFX mono, ambience stereo',output=f'output/audio/{id}.wav',status='planned',note='现有音频后端可用时生成；不可用必须在交付状态标记 blocked，不将缺音频冒充完成。') for id,name,prompt,loop in audio])
    save('config/deferred.json',[dict(id='DIALOGUE_truth',reason='五年前真相没有逐字对白；不自行编造。',blocking_visual_delivery=False),dict(id='DIALOGUE_storage',reason='门后只有语义和图标描述；未提供逐字台词。用无可辨词低声与符号暂代。',blocking_visual_delivery=False)])
    save('integration/reuse_scenes.json',dict(generate_new_scenes=False,scene_mapping={b['scene']:('dressing' if b['scene'] in ['corridor','storage'] else 'studio' if b['scene']=='newstudio' else b['scene']) for b in story['beats']},instruction='复用原有四张场景参考及既有3D布景变体；报纸直接复用。新任务只生产人物/表情/道具/证据，不重生成背景或地图。'))
    save('integration/beat_assets.json',[dict(beat=i,title=b['title'],scene=b['scene'],animation_ids=[j['id'] for j in jobs if i in j.get('beats',[])],static_ids=[s['id'] for s in static if i in s['beats']],current_badges=[dict(actor=a['name'],emotions=a.get('emotions',[])) for a in b['actors'] if a.get('emotion_placeholder')]) for i,b in enumerate(story['beats'],1)])
    save('integration/sync_groups.json',[
        dict(id='G01',clips=['A02_linwan_hit_table','A03_actress_retreat_table'],event='table_contact',note='共享桌沿高度；桌子用既有场景几何，不烘入人物。'),
        dict(id='G02',clips=['A09_linwan_take_umbrella','A10_xiaozhou_release_umbrella'],event='umbrella_transfer',note='手柄接触处对齐，只有一把伞，交接事件前后明确所有者。'),
        dict(id='G03',clips=['A37_staff_f_give_rose','A39_suyao_receive_rose'],event='rose_transfer',note='单支白玫瑰，无遮挡断手。'),
        dict(id='G04',clips=['A38_staff_m_give_knife','A40_linwan_receive_knife'],event='knife_transfer',note='道具刀手柄位置一致。'),
        dict(id='G05',clips=['A42_suyao_fake_fall','A41_linwan_knife_advance'],event='fake_recoil_before_contact',note='苏瑶先反应；刀不接触身体。'),
        dict(id='G06',clips=['A55_staff_m_take_knife','A56_linwan_release_knife'],event='knife_taken_by_staff',note='带离前刀具转给工作人员，不可消失或复制。'),
        dict(id='G07',clips=['A44_linwan_escorted','A45_staff_f_escort','A46_staff_m_escort'],event='left_foot_contact',note='一致步频、接触点和左向移动；在同一固定镜头合成验证完整循环。'),
        dict(id='G08',clips=['A47_chenmo_give_file','A48_linwan_receive_file'],event='file_transfer',note='仅一份文件，交接后双方手势连贯。'),
        dict(id='G09',clips=['A09_linwan_take_umbrella','A54_xiaobei_release_umbrella'],event='umbrella_transfer',note='与开场同一把伞；复用林晚片段前确认小北手柄高度能匹配，不能匹配则交付独立变体并登记。')])
    retained=[j['id'] for j in oldjobs if j['id'] not in repairs and j['id']!='C01_linwan_tear_photo']
    save('config/reuse_existing.json',dict(reuse_not_regenerate=retained,repair_ids=list(repairs),deprecated_by_story={'C01_linwan_tear_photo':'R01_linwan_peel_label'},notes='复用条目仍要做浅深底、循环和链条抽查；发现同类抠像污染才重导。C08备注源帧89改为92，不为文字改动重生成。路人待机走路仍全部必做，但已存在的素材不重复计为新增任务。'))
    summary={kind:sum(j['kind']==kind for j in jobs) for kind in ['action','expression','repair']}
    summary.update(story_replacement=1,animation_jobs=len(jobs),static_jobs=len(static),audio_jobs=len(audio),new_character_masters=len(newchars),director_is_limou=True,generate_new_backgrounds=False)
    save('config/scope.json',summary)
    lines=['# 本轮生成总表','',f'动画任务 {len(jobs)} 项（含动作、独立表情、返修和剧情替代）；静态素材 {len(static)} 项；音频 {len(audio)} 项。复用素材不计入新增生成量。','', '| ID | 内容 | 类型 | 优先级 | 剧情段 | 循环 |','| --- | --- | --- | --- | --- | --- |']
    lines += [f'| {j["id"]} | {j["name"]} | {j["kind"]} | {j["priority"]} | {j.get("beats",[])} | {j["loop"]} |' for j in jobs]
    lines += ['','## 静态素材','', '| ID | 内容 | 类型 | 优先级 |','| --- | --- | --- | --- |']+[f'| {s["id"]} | {s["name"]} | {s["kind"]} | {s["priority"]} |' for s in static]
    put('生成总表.md','\n'.join(lines))
    print(json.dumps(summary,ensure_ascii=False))

if __name__=='__main__': main()
