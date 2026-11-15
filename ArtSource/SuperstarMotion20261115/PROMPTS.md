# 内置 imagegen 提示词与来源

生成日期2026-11-15。全部使用内置image_gen；透明图请求`transparent_background=true`，两张证据图为false。原图文件与SHA256在sources.json。下列为本批生产提示词集，角色参考均为项目现有图像，未改身份。

## 六个角色走路

林晚参考：`Content/SuperstarStory/Animations/S01_linwan_idle/S01_linwan_idle_000001.png`。

Use case: identity-preserve. Production game animation sprite sheet for the EXACT woman in reference, Lin Wan. Preserve her face, brown high ponytail, hoop earrings, white cropped blouse with long transparent sleeves, blue shorts, black heels, slim proportions, same flat illustrated style. Transparent background. 2048x1024 sheet, exactly FOUR equal columns and TWO equal rows, eight separate full-body sprites, generous empty padding within every cell, never overlap. Every character facing screen RIGHT in side-three-quarter view, identical body scale and foot baseline, no ground shadow, no labels, no grid lines. Reading left to right top row then bottom row: cells 1-6 are SIX distinct sequential frames of a seamless natural walking cycle (left contact, left passing, left up, right contact, right passing, right up). Clearly alternating legs, knees bending, heels lifting. Right forearm bent forward with right closed hand at chest height, as if gripping an umbrella shaft, keep that hand almost fixed relative torso throughout walking; empty hand, do not draw umbrella. Other arm swings naturally. Cell7 standing feet together right hand forward closed grip at chest height for holding umbrella. Cell8 standing still photographing: right elbow bent, hand at chin height with fingers ready to hold a phone, but NO phone drawn. Keep full heads and feet inside each cell. These will be split into game frames. No added clothing, no props, no green background.

小北参考：`Animations/S02_xiaobei_idle/S02_xiaobei_idle_000001.png`。

Use case: identity-preserve. Game sprite animation sheet, exactly FOUR columns TWO rows, eight equal cells. Same Xiaobei woman as reference: short brown hair, white short sleeve collared blouse, navy neck ribbon, navy knee length pencil skirt, white lace-up shoes. Same face and flat shaded illustration, absolutely NO design changes. Every sprite fullbody, facing screen RIGHT, consistent scale and grounded foot baseline, transparent background, no shadow, aura, glow, text or grid. Cells1-6 reading order form six successive natural WALK phases: contact, passing, lift, opposite contact, opposite passing, opposite lift. Small natural steps appropriate to skirt, not jogging or high knees. One forearm bent forward with hand closed at lower chest height for holding an umbrella, other arm swings. Hands EMPTY; no props. Cell7 neutral feet together standing with same forward closed hand; cell8 same standing pose. Enough separation to split equal cells. Exact character identity. Flat clean opaque color inside, clean transparent empty pixels outside.

苏瑶参考：`Animations/S03_suyao_idle/S03_suyao_idle_000001.png`。

Use case identity-preserve. Create exactly six sequential animation frames of a gentle natural walking cycle of the exact woman in reference, Su Yao. Same face, brown twin braids, coral pink sleeveless flared knee length dress and heels. THREE columns TWO rows equal cells in one sprite sheet. All face RIGHT in three-quarter side view. Full body same scale and baseline each cell, transparent background, no text, grid, props or shadow. Read left-to-right top row then bottom row: left contact, left passing, left lift, right contact, right passing, right lift. SMALL elegant walking steps, feet stay near ground, do NOT lift knees high, do NOT run. Gentle opposite arm swing, distinct alternating legs. Seamless return last to first with no repeated endpoint. Keep original identity and clothing colors, flat clean cartoon drawing. Enough padding within each cell to slice.

陈默、小周、女主演员各自使用 `Actors/Partial20261115/M_<role>/M_<role>_000001.png`。逐个调用以下模板，替换LOOK：

Use case identity-preserve. Single game sprite sheet with SIX sequential natural WALK frames, THREE columns TWO rows equal cells. EXACT character from reference: LOOK. Preserve face, body proportions, identity, flat clean cartoon style. All face screen RIGHT three-quarter side view. Full body same scale, consistent baseline, lots of empty padding. Small ordinary steps with knees low, not running. Frames left contact, passing, lift, opposite contact, passing, lift; clearly changing alternating legs. Near forearm bent forward with closed empty hand at lower chest height, as if carrying an object; other arm swings. No objects. Truly transparent background, no shadows, labels, grids or halos. Last frame flows into first. This sheet will be split into six game frames.

- chenmo LOOK: short black hair, black glasses, blue jacket over white T shirt, dark trousers and dark shoes
- xiaozhou LOOK: short dark blue hair, teal hooded sweatshirt, beige trousers and white sneakers
- actress LOOK: young woman with straight black shoulder length hair, pale cream cardigan, dark navy pants and black flats

## 苏瑶持花姿态

参考同苏瑶原帧。

Identity-preserve game sprite edit. The exact Su Yao in reference, brown twin braids, coral pink sleeveless knee-length dress, heels, same face and proportions and flat drawing style. Change only pose: standing feet together, facing screen LEFT in three-quarter view, near arm bent slightly forward with a clearly visible closed hand in front of waist as if holding the stem of a flower. Other hand relaxed. Hand empty, no flower drawn. Full body centered at same scale with plenty of margin. Genuine transparent background, no shadow, no text, no halo.

## 手机与直播支架

Use case stylized-concept. One isolated smartphone game prop, vertical portrait orientation, front view with subtle three-quarter thickness. Dark charcoal case, blue-gray reflective screen, small front camera, no logo, no words, no hands. Clean flat cartoon illustration matching a modern 2D story game. Tall phone centered with generous transparent padding, full silhouette. Genuine transparent background, no green backdrop, no floor shadow.

Use case stylized-concept. One complete livestream smartphone tripod assembly game prop, full-height slender black three-legged floor tripod with a portrait smartphone SECURELY IN ITS CLAMP at the very top. Camera side facing left-three-quarter, whole object centered, long stem reaching from floor to a standing adult's face. Phone must visibly connect to clamp, clamp to stem, stem to three legs. Simple charcoal black metal, dark navy phone, clean flat cartoon art. One continuous assembled object, no hands, no people, no text, no floor shadow, genuinely transparent background. Full object not cropped, generous padding.

## 两张证据图

EV01参考李谋母版和小北原帧。

Use case illustration-story. A fictional in-game evidence photo, landscape 16:9. Two ADULT fictional cartoon characters from reference: older director Li Mou (gray temples, dark jacket, headphones around neck) and young ADULT woman Xiaobei (short brown hair, white collared blouse, navy bow and skirt). Director stands beside Xiaobei with one arm around her shoulders in a quiet backstage corridor, Xiaobei holds papers against her body and looks reserved. Clear readable medium full shot, both faces visible. Match references' flat shaded 2D illustrated game style exactly, preserve identities. This illustrates the script evidence 'director embracing Xiaobei'; do not imply any additional action. Background simple backstage walls and closed door, soft neutral lighting. No text, watermarks, UI or photo border.

EV02参考李谋母版和苏瑶原帧。

Use case illustration-story. Fictional in-game evidence photo landscape 16:9. Two adult cartoon characters exactly from reference: director Li Mou (gray temples, dark jacket, headphones at neck) and adult actress Su Yao (brown twin braids, coral pink sleeveless dress). Seated facing each other at a small table in a backstage lounge, both holding wine glasses in a casual toast, warm conspiratorial smiles. Both faces clearly recognizable. Preserve clothing and identity. Same flat shaded 2D game illustration style. Simple background with couch and dim warm lamp, no other people. Clear composition for tiny evidence thumbnail. No words, logos, captions, UI, border or watermark. Illustrate only the script evidence 'director and Su Yao drinking wine'.
